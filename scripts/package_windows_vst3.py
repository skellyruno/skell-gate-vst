#!/usr/bin/env python3
"""Build a deterministic, checksummed private-preview ZIP for a Windows VST3 bundle."""
from __future__ import annotations

import argparse
import hashlib
import json
import stat
import sys
import zipfile
from pathlib import Path
from typing import Iterable

REQUIRED_FILES = {
    "release-manifest.json",
    "WINDOWS-VST3-PRIVATE-PREVIEW.txt",
    "Install-SkellGlitchPreview.ps1",
}


def _fail(message: str) -> None:
    raise ValueError(message)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _regular_files(root: Path) -> list[Path]:
    files: list[Path] = []
    for path in root.rglob("*"):
        if path.is_symlink():
            _fail(f"Symlinks are not allowed in a release package: {path.relative_to(root)}")
        if path.is_file():
            files.append(path)
    return sorted(files, key=lambda item: item.relative_to(root).as_posix())


def _validate_package(package_dir: Path) -> tuple[dict, list[Path]]:
    if not package_dir.is_dir():
        _fail(f"Package directory does not exist: {package_dir}")

    for name in REQUIRED_FILES:
        if not (package_dir / name).is_file():
            _fail(f"Missing required preview sidecar: {name}")

    try:
        manifest = json.loads((package_dir / "release-manifest.json").read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        _fail(f"Cannot read release-manifest.json: {exc}")

    required_manifest = (
        "product", "version", "vendor", "platform", "bundle", "module",
        "module_sha256", "moduleinfo_sha256", "source_sha", "workflow_run_id",
        "workflow_run_attempt", "workflow_event", "workflow_ref",
    )
    missing = [field for field in required_manifest if not manifest.get(field)]
    if missing:
        _fail("Release manifest is missing required provenance fields: " + ", ".join(missing))
    if manifest["platform"] != "windows-x64":
        _fail(f"Expected windows-x64 manifest, got {manifest['platform']!r}")
    if not isinstance(manifest["source_sha"], str) or len(manifest["source_sha"]) not in (40, 64):
        _fail("source_sha must be a full 40- or 64-character commit hash")
    if any(ch not in "0123456789abcdefABCDEF" for ch in manifest["source_sha"]):
        _fail("source_sha contains non-hexadecimal characters")
    if not str(manifest["workflow_run_id"]).isdigit():
        _fail("workflow_run_id must be numeric")

    bundles = [path for path in package_dir.iterdir() if path.is_dir() and path.name.lower().endswith(".vst3")]
    if len(bundles) != 1:
        _fail(f"Expected exactly one top-level VST3 bundle, found {len(bundles)}")
    bundle = bundles[0]
    if bundle.name != manifest["bundle"]:
        _fail(f"Bundle name does not match release manifest: {bundle.name!r} != {manifest['bundle']!r}")
    nested_bundles = [path for path in package_dir.rglob("*") if path.is_dir() and path.name.lower().endswith(".vst3")]
    if nested_bundles != [bundle]:
        _fail("Expected exactly one VST3 bundle directory in the package")

    module = bundle / "Contents" / "x86_64-win" / f"{manifest['product']}.vst3"
    moduleinfo = bundle / "Contents" / "Resources" / "moduleinfo.json"
    if not module.is_file() or module.relative_to(bundle).as_posix() != manifest["module"]:
        _fail("Windows VST3 module path does not match release manifest")
    if not moduleinfo.is_file():
        _fail("VST3 moduleinfo.json is missing")
    if _sha256(module) != manifest["module_sha256"]:
        _fail("VST3 module SHA-256 does not match release manifest")
    if _sha256(moduleinfo) != manifest["moduleinfo_sha256"]:
        _fail("VST3 moduleinfo.json SHA-256 does not match release manifest")

    all_files = _regular_files(package_dir)
    if any(path.suffix.lower() == ".exe" for path in all_files):
        _fail("Standalone executables are not allowed in a VST3-only preview package")
    if any(path.suffix.lower() == ".vst3" and path != module for path in all_files):
        _fail("Unexpected duplicate or additional VST3 module file found")
    return manifest, all_files


def _write_checksums(package_dir: Path, files: Iterable[Path]) -> Path:
    checksum_file = package_dir / "SHA256SUMS.txt"
    lines = []
    for path in files:
        relative = path.relative_to(package_dir).as_posix()
        lines.append(f"{_sha256(path)}  {relative}")
    payload = "\n".join(lines) + "\n"

    # Exclusive creation closes the check-then-write race: an existing file is never
    # truncated, even if another process creates it after our initial package check.
    created = False
    try:
        with checksum_file.open("x", encoding="utf-8", newline="\n") as stream:
            created = True
            stream.write(payload)
    except FileExistsError:
        _fail("Refusing to overwrite an existing SHA256SUMS.txt")
    except Exception:
        # A failed write must not strand a partial checksum file that blocks retry.
        if created:
            try:
                checksum_file.unlink()
            except OSError:
                pass
        raise
    return checksum_file


def build_archive(package_dir: Path, output_zip: Path) -> tuple[str, Path]:
    package_dir = package_dir.resolve()
    output_zip = output_zip.resolve()
    sidecar = output_zip.with_suffix(output_zip.suffix + ".sha256")
    if output_zip.suffix.lower() != ".zip":
        _fail("Output filename must end in .zip")
    if output_zip.exists() or output_zip.is_symlink():
        _fail(f"Refusing to overwrite existing preview archive: {output_zip}")
    if sidecar.exists() or sidecar.is_symlink():
        _fail(f"Refusing to overwrite existing checksum sidecar: {sidecar}")
    if output_zip.parent == package_dir or package_dir in output_zip.parents:
        _fail("Preview archive must be written outside the package directory")

    checksum_file = package_dir / "SHA256SUMS.txt"
    checksums_created = False
    archive_created = False
    sidecar_created = False
    try:
        _, files = _validate_package(package_dir)
        _write_checksums(package_dir, files)
        checksums_created = True
        archive_files = _regular_files(package_dir)

        output_zip.parent.mkdir(parents=True, exist_ok=True)
        # ZIP_STORED plus normalized timestamps/permissions makes the archive byte-stable
        # across repeated runs with the same package contents.
        with output_zip.open("xb") as stream:
            archive_created = True
            with zipfile.ZipFile(stream, "w", compression=zipfile.ZIP_STORED, strict_timestamps=True) as archive:
                for path in archive_files:
                    relative = path.relative_to(package_dir).as_posix()
                    info = zipfile.ZipInfo(relative, date_time=(2020, 1, 1, 0, 0, 0))
                    info.compress_type = zipfile.ZIP_STORED
                    info.create_system = 3
                    info.external_attr = (stat.S_IFREG | 0o644) << 16
                    info.flag_bits = 0
                    archive.writestr(info, path.read_bytes())
        archive_hash = _sha256(output_zip)
        with sidecar.open("x", encoding="utf-8", newline="\n") as stream:
            sidecar_created = True
            stream.write(f"{archive_hash}  {output_zip.name}\n")
    except Exception:
        # Remove only files created by this invocation. Pre-existing files remain untouched.
        if archive_created and output_zip.exists():
            output_zip.unlink()
        if sidecar_created and sidecar.exists():
            sidecar.unlink()
        if checksums_created and checksum_file.exists() and not checksum_file.is_symlink():
            checksum_file.unlink()
        raise
    return archive_hash, sidecar


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package-dir", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        digest, sidecar = build_archive(args.package_dir, args.output)
        print(json.dumps({"archive": str(args.output), "sha256": digest, "sidecar": str(sidecar)}, indent=2))
        return 0
    except (OSError, ValueError, zipfile.BadZipFile) as exc:
        sys.stderr.write(f"WINDOWS VST3 PREVIEW PACKAGING FAILED: {exc}\n")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
