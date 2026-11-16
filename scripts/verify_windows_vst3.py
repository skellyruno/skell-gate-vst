#!/usr/bin/env python3
"""Fail-closed verifier for the SkellGlitch Windows x64 VST3 release bundle."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import struct
import sys
from pathlib import Path
from typing import Any


def _fail(message: str) -> None:
    raise ValueError(message)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _strip_steinberg_json5_trailing_commas(source: str) -> str:
    """Remove only trailing commas emitted by Steinberg's official JSON5Writer."""
    output: list[str] = []
    in_string = False
    escaped = False
    index = 0
    while index < len(source):
        char = source[index]
        if in_string:
            output.append(char)
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            index += 1
            continue
        if char == '"':
            in_string = True
            output.append(char)
            index += 1
            continue
        if char == ",":
            lookahead = index + 1
            while lookahead < len(source) and source[lookahead].isspace():
                lookahead += 1
            if lookahead < len(source) and source[lookahead] in "}]":
                index += 1
                continue
        output.append(char)
        index += 1
    return "".join(output)


def _cmake_identity(cmake_file: Path) -> tuple[str, str, str]:
    source = cmake_file.read_text(encoding="utf-8")
    project = re.search(
        r"\bproject\s*\(\s*SkellGlitch\s+VERSION\s+([^\s\)]+)",
        source,
        re.IGNORECASE,
    )
    company = re.search(r'\bCOMPANY_NAME\s+"([^"]+)"', source)
    product = re.search(r'\bPRODUCT_NAME\s+"([^"]+)"', source)
    if not project or not company or not product:
        _fail("CMakeLists.txt is missing the SkellGlitch project version/company/product identity")
    return product.group(1), project.group(1), company.group(1)


def _verify_pe32_plus_x64(binary: Path) -> None:
    data = binary.read_bytes()
    if len(data) < 0x100 or data[:2] != b"MZ":
        _fail(f"VST3 module is not a valid PE image: {binary}")
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if pe_offset + 26 > len(data) or data[pe_offset : pe_offset + 4] != b"PE\0\0":
        _fail(f"VST3 module has no valid PE signature: {binary}")
    machine = struct.unpack_from("<H", data, pe_offset + 4)[0]
    optional_magic = struct.unpack_from("<H", data, pe_offset + 24)[0]
    if machine != 0x8664:
        _fail(f"VST3 module must be AMD64 (0x8664), got 0x{machine:04x}")
    if optional_magic != 0x20B:
        _fail(f"VST3 module must use PE32+ (0x20b), got 0x{optional_magic:04x}")


def verify_bundle(bundle: Path, cmake_file: Path, source_sha: str = "") -> dict[str, Any]:
    product, version, vendor = _cmake_identity(cmake_file)
    if bundle.name != f"{product}.vst3" or not bundle.is_dir():
        _fail(f"Expected a VST3 bundle directory named {product}.vst3")

    contents = bundle / "Contents"
    moduleinfo = contents / "Resources" / "moduleinfo.json"
    binary = contents / "x86_64-win" / f"{product}.vst3"
    if not moduleinfo.is_file():
        _fail(f"Missing VST3 module metadata: {moduleinfo}")
    if not binary.is_file():
        _fail(f"Missing Windows x64 VST3 module: {binary}")

    module_files = [path for path in contents.rglob("*.vst3") if path.is_file()]
    if module_files != [binary]:
        listed = ", ".join(str(path.relative_to(bundle)) for path in module_files)
        _fail(f"Expected exactly one VST3 module at {binary.relative_to(bundle)}; found: {listed or 'none'}")
    executables = [path for path in bundle.rglob("*.exe") if path.is_file()]
    if executables:
        _fail("Standalone executables are not allowed inside the VST3 release bundle")

    try:
        metadata_text = moduleinfo.read_text(encoding="utf-8")
        metadata = json.loads(_strip_steinberg_json5_trailing_commas(metadata_text))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        _fail(f"moduleinfo.json is not valid Steinberg JSON5-compatible metadata: {exc}")
    if not isinstance(metadata, dict):
        _fail("moduleinfo.json root must be an object")
    if metadata.get("Name") != product:
        _fail(f"VST3 metadata product mismatch: expected {product!r}, got {metadata.get('Name')!r}")
    if metadata.get("Version") != version:
        _fail(f"VST3 metadata version mismatch: expected {version!r}, got {metadata.get('Version')!r}")
    classes = metadata.get("Classes")
    if not isinstance(classes, list):
        _fail("VST3 metadata Classes must be an array")
    audio_classes = [
        item for item in classes
        if isinstance(item, dict)
        and item.get("Category") == "Audio Module Class"
        and item.get("Name") == product
    ]
    if len(audio_classes) != 1:
        _fail(f"Expected exactly one {product!r} Audio Module Class in moduleinfo.json")
    class_vendor = audio_classes[0].get("Vendor")
    factory_info = metadata.get("Factory Info")
    factory_vendor = factory_info.get("Vendor") if isinstance(factory_info, dict) else None
    actual_vendor = class_vendor or factory_vendor
    if actual_vendor != vendor:
        _fail(f"VST3 metadata vendor mismatch: expected {vendor!r}, got {actual_vendor!r}")

    _verify_pe32_plus_x64(binary)
    return {
        "schema_version": 1,
        "product": product,
        "version": version,
        "vendor": vendor,
        "platform": "windows-x64",
        "bundle": bundle.name,
        "module": binary.relative_to(bundle).as_posix(),
        "module_size_bytes": binary.stat().st_size,
        "module_sha256": _sha256(binary),
        "moduleinfo_sha256": _sha256(moduleinfo),
        "source_sha": source_sha or os.environ.get("GITHUB_SHA", ""),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path, help="Built SkellGlitch.vst3 bundle directory")
    parser.add_argument("--cmake-file", type=Path, default=Path("CMakeLists.txt"))
    parser.add_argument("--source-sha", default="")
    parser.add_argument("--manifest-out", type=Path)
    args = parser.parse_args(argv)
    try:
        manifest = verify_bundle(args.bundle, args.cmake_file, args.source_sha)
        rendered = json.dumps(manifest, indent=2, sort_keys=True) + "\n"
        if args.manifest_out:
            args.manifest_out.parent.mkdir(parents=True, exist_ok=True)
            args.manifest_out.write_text(rendered, encoding="utf-8")
        sys.stdout.write(rendered)
        return 0
    except (OSError, ValueError) as exc:
        sys.stderr.write(f"VST3 RELEASE VERIFICATION FAILED: {exc}\n")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
