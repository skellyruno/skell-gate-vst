import hashlib
import json
import sys
import tempfile
import unittest
import zipfile
from unittest.mock import patch
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from package_windows_vst3 import build_archive


class PreviewArchiveTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.package = self.root / "package"
        bundle = self.package / "SkellGlitch.vst3"
        module = bundle / "Contents" / "x86_64-win" / "SkellGlitch.vst3"
        module.parent.mkdir(parents=True)
        module.write_bytes(b"MZ" + b"synthetic-amd64-module")
        moduleinfo = bundle / "Contents" / "Resources" / "moduleinfo.json"
        moduleinfo.parent.mkdir(parents=True)
        moduleinfo.write_text('{"Name":"SkellGlitch"}\n', encoding="utf-8")
        self.manifest = {
            "schema_version": 1,
            "product": "SkellGlitch",
            "version": "1.7.3",
            "vendor": "Skelly",
            "platform": "windows-x64",
            "bundle": "SkellGlitch.vst3",
            "module": "Contents/x86_64-win/SkellGlitch.vst3",
            "module_sha256": hashlib.sha256(module.read_bytes()).hexdigest(),
            "moduleinfo_sha256": hashlib.sha256(moduleinfo.read_bytes()).hexdigest(),
            "source_sha": "2935ee64180a4a9dde69399a0b4f86747206ce80",
            "workflow_run_id": "43936168153",
            "workflow_run_attempt": "1",
            "workflow_event": "pull_request",
            "workflow_ref": "refs/pull/8/merge",
        }
        (self.package / "release-manifest.json").write_text(json.dumps(self.manifest), encoding="utf-8")
        (self.package / "WINDOWS-VST3-PRIVATE-PREVIEW.txt").write_text(
            "private preview and install/rollback instructions\n", encoding="utf-8"
        )
        (self.package / "Install-SkellGlitchPreview.ps1").write_text(
            "# synthetic installer fixture\n", encoding="utf-8"
        )
        self.output = self.root / "SkellGlitch-Windows-VST3-Preview-1.7.3.zip"

    def tearDown(self):
        self.temp.cleanup()

    def test_builds_archive_with_manifest_notice_installer_and_internal_checksums(self):
        digest, sidecar = build_archive(self.package, self.output)
        self.assertTrue(self.output.is_file())
        self.assertTrue(sidecar.is_file())
        self.assertEqual(hashlib.sha256(self.output.read_bytes()).hexdigest(), digest)
        self.assertIn(digest, sidecar.read_text(encoding="utf-8"))
        with zipfile.ZipFile(self.output) as archive:
            self.assertIsNone(archive.testzip())
            names = set(archive.namelist())
            self.assertIn("SkellGlitch.vst3/Contents/x86_64-win/SkellGlitch.vst3", names)
            self.assertIn("release-manifest.json", names)
            self.assertIn("WINDOWS-VST3-PRIVATE-PREVIEW.txt", names)
            self.assertIn("Install-SkellGlitchPreview.ps1", names)
            self.assertIn("SHA256SUMS.txt", names)
            sums = archive.read("SHA256SUMS.txt").decode("utf-8").splitlines()
            for line in sums:
                expected, relative = line.split("  ", 1)
                self.assertEqual(hashlib.sha256(archive.read(relative)).hexdigest(), expected)

    def test_archive_is_byte_deterministic_for_same_contents(self):
        first_hash, _ = build_archive(self.package, self.output)
        import shutil
        second_package = self.root / "package-copy"
        shutil.copytree(self.package, second_package)
        (second_package / "SHA256SUMS.txt").unlink()
        second_output = self.root / "second.zip"
        second_hash, _ = build_archive(second_package, second_output)
        self.assertEqual(first_hash, second_hash)
        self.assertEqual(self.output.read_bytes(), second_output.read_bytes())

    def test_failed_archive_write_cleans_generated_checksums_and_can_retry(self):
        original_writestr = zipfile.ZipFile.writestr
        write_calls = 0

        def fail_first_write(archive, *args, **kwargs):
            nonlocal write_calls
            write_calls += 1
            if write_calls == 1:
                raise OSError("simulated archive write failure")
            return original_writestr(archive, *args, **kwargs)

        with patch.object(zipfile.ZipFile, "writestr", new=fail_first_write):
            with self.assertRaisesRegex(OSError, "simulated archive write failure"):
                build_archive(self.package, self.output)

        self.assertFalse((self.package / "SHA256SUMS.txt").exists())
        self.assertFalse(self.output.exists())
        self.assertFalse(self.output.with_suffix(self.output.suffix + ".sha256").exists())

        # A retry in the same workspace must succeed without manual cleanup.
        digest, sidecar = build_archive(self.package, self.output)
        self.assertTrue(self.output.is_file())
        self.assertTrue(sidecar.is_file())
        self.assertEqual(hashlib.sha256(self.output.read_bytes()).hexdigest(), digest)

    def test_workflow_actions_use_node24_compatible_majors(self):
        workflow = Path(__file__).resolve().parents[1] / ".github" / "workflows" / "build.yml"
        content = workflow.read_text(encoding="utf-8")
        self.assertIn("actions/checkout@v7", content)
        self.assertIn("actions/setup-python@v7", content)
        self.assertIn("actions/upload-artifact@v7", content)
        self.assertNotIn("actions/checkout@v4", content)
        self.assertNotIn("actions/setup-python@v5", content)
        self.assertNotIn("actions/upload-artifact@v4", content)

    def test_release_branches_trigger_push_ci(self):
        workflow = Path(__file__).resolve().parents[1] / ".github" / "workflows" / "build.yml"
        content = workflow.read_text(encoding="utf-8")
        self.assertIn("  push:\n    branches:\n      - main\n      - 'release/**'", content)

    def test_verified_preview_upload_is_visibility_independent(self):
        workflow = Path(__file__).resolve().parents[1] / ".github" / "workflows" / "build.yml"
        content = workflow.read_text(encoding="utf-8")
        upload_step = content.split(
            "name: Upload verified Windows VST3 preview archive\n", 1
        )[1].split("      - name:", 1)[0]
        self.assertNotIn("github.event.repository.private", content)
        self.assertNotIn("if:", upload_step)
        self.assertIn("if-no-files-found: error", upload_step)
        self.assertIn(".zip.sha256", upload_step)
        self.assertLess(content.index("Validate packaged VST3 with pluginval"), content.index("name: Upload verified Windows VST3 preview archive"))
        self.assertLess(content.index("Build deterministic private-preview ZIP and checksums"), content.index("name: Upload verified Windows VST3 preview archive"))

    def test_workflow_runs_windows_installer_regressions_before_archive_creation(self):
        workflow = Path(__file__).resolve().parents[1] / ".github" / "workflows" / "build.yml"
        content = workflow.read_text(encoding="utf-8")
        copy_step = "Copy-Item -LiteralPath 'scripts/Install-SkellGlitchPreview.ps1' -Destination 'release-package/Install-SkellGlitchPreview.ps1'"
        test_step = "pwsh -NoLogo -NoProfile -File tests/Test-SkellGlitchPreviewInstaller.ps1"
        package_step = "python scripts/package_windows_vst3.py"
        self.assertIn(copy_step, content)
        self.assertIn(test_step, content)
        self.assertLess(content.index(copy_step), content.index(test_step))
        self.assertLess(content.index(test_step), content.index(package_step))

    def test_requires_the_per_user_installer(self):
        (self.package / "Install-SkellGlitchPreview.ps1").unlink()
        with self.assertRaisesRegex(ValueError, "Missing required preview sidecar"):
            build_archive(self.package, self.output)
        self.assertFalse(self.output.exists())

    def test_refuses_to_overwrite_existing_archive(self):
        self.output.write_bytes(b"protected archive")
        with self.assertRaises(ValueError):
            build_archive(self.package, self.output)
        self.assertEqual(self.output.read_bytes(), b"protected archive")

    def test_refuses_to_overwrite_existing_checksum_sidecar(self):
        sidecar = self.output.with_suffix(self.output.suffix + ".sha256")
        sidecar.write_text("protected sidecar\n", encoding="utf-8")
        with self.assertRaises(ValueError):
            build_archive(self.package, self.output)
        self.assertEqual(sidecar.read_text(encoding="utf-8"), "protected sidecar\n")
        self.assertFalse(self.output.exists())

    def test_refuses_to_overwrite_internal_checksums(self):
        sums = self.package / "SHA256SUMS.txt"
        sums.write_text("protected internal checksum\n", encoding="utf-8")
        with self.assertRaises(ValueError):
            build_archive(self.package, self.output)
        self.assertEqual(sums.read_text(encoding="utf-8"), "protected internal checksum\n")

    def test_rejects_wrong_module_hash(self):
        self.manifest["module_sha256"] = "0" * 64
        (self.package / "release-manifest.json").write_text(json.dumps(self.manifest), encoding="utf-8")
        with self.assertRaises(ValueError):
            build_archive(self.package, self.output)
        self.assertFalse(self.output.exists())

    def test_rejects_standalone_executable(self):
        (self.package / "SkellGlitch.exe").write_bytes(b"MZ")
        with self.assertRaises(ValueError):
            build_archive(self.package, self.output)
        self.assertFalse(self.output.exists())

    def test_rejects_duplicate_vst3_bundle(self):
        duplicate = self.package / "SkellGlitch-old.vst3"
        duplicate.mkdir()
        with self.assertRaises(ValueError):
            build_archive(self.package, self.output)
        self.assertFalse(self.output.exists())

    def test_rejects_nested_duplicate_vst3_bundle_directory(self):
        nested = self.package / "Extras" / "Unexpected.vst3"
        nested.mkdir(parents=True)
        with self.assertRaises(ValueError):
            build_archive(self.package, self.output)
        self.assertFalse(self.output.exists())

    def test_rejects_symlinked_package_file(self):
        target = self.package / "WINDOWS-VST3-PRIVATE-PREVIEW.txt"
        link = self.package / "NOTICE-LINK.txt"
        try:
            link.symlink_to(target)
        except (OSError, NotImplementedError):
            self.skipTest("Symlinks are unavailable on this filesystem")
        with self.assertRaises(ValueError):
            build_archive(self.package, self.output)
        self.assertFalse(self.output.exists())

    def test_rejects_incomplete_provenance(self):
        self.manifest["workflow_run_id"] = ""
        (self.package / "release-manifest.json").write_text(json.dumps(self.manifest), encoding="utf-8")
        with self.assertRaises(ValueError):
            build_archive(self.package, self.output)
        self.assertFalse(self.output.exists())


if __name__ == "__main__":
    unittest.main()
