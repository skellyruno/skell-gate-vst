import json
import os
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from verify_windows_vst3 import verify_bundle


PRODUCT = "SkellGlitch"
VERSION = "1.7.3"
VENDOR = "Skelly"


def synthetic_pe(machine: int = 0x8664, magic: int = 0x20B, signature: bytes = b"PE\0\0") -> bytes:
    data = bytearray(512)
    data[0:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = signature
    struct.pack_into("<H", data, 0x84, machine)
    struct.pack_into("<H", data, 0x94, 0xF0)
    struct.pack_into("<H", data, 0x98, magic)
    return bytes(data)


class WindowsVST3PackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.cmake = self.root / "CMakeLists.txt"
        self.cmake.write_text(
            'project(SkellGlitch VERSION 1.7.3 LANGUAGES CXX)\n'
            'juce_add_plugin(SkellGlitch COMPANY_NAME "Skelly" PRODUCT_NAME "SkellGlitch")\n',
            encoding="utf-8",
        )
        self.bundle = self.root / "SkellGlitch.vst3"
        self.module = self.bundle / "Contents" / "x86_64-win" / "SkellGlitch.vst3"
        self.module.parent.mkdir(parents=True)
        self.module.write_bytes(synthetic_pe())
        resources = self.bundle / "Contents" / "Resources"
        resources.mkdir(parents=True)
        self.metadata = resources / "moduleinfo.json"
        self.write_metadata()

    def tearDown(self):
        self.temp.cleanup()

    def write_metadata(self, **changes):
        value = {
            "Name": PRODUCT,
            "Version": VERSION,
            "Factory Info": {"Vendor": VENDOR},
            "Classes": [
                {"CID": "00112233445566778899AABBCCDDEEFF", "Category": "Audio Module Class", "Name": PRODUCT, "Vendor": VENDOR, "Version": VERSION},
                {"CID": "FFEEDDCCBBAA99887766554433221100", "Category": "Component Controller Class", "Name": PRODUCT, "Vendor": VENDOR, "Version": VERSION},
            ],
        }
        value.update(changes)
        self.metadata.write_text(json.dumps(value), encoding="utf-8")

    def assert_rejected(self):
        with self.assertRaises(ValueError):
            verify_bundle(self.bundle, self.cmake, "test-source-sha")

    def test_valid_bundle_emits_binary_and_metadata_hashes(self):
        result = verify_bundle(self.bundle, self.cmake, "0123456789abcdef")
        self.assertEqual(result["product"], PRODUCT)
        self.assertEqual(result["version"], VERSION)
        self.assertEqual(result["vendor"], VENDOR)
        self.assertEqual(result["platform"], "windows-x64")
        self.assertEqual(len(result["module_sha256"]), 64)
        self.assertEqual(len(result["moduleinfo_sha256"]), 64)
        self.assertEqual(result["source_sha"], "0123456789abcdef")

    def test_manifest_records_workflow_run_provenance(self):
        env = {
            "GITHUB_RUN_ID": "43935558026",
            "GITHUB_RUN_ATTEMPT": "2",
            "GITHUB_EVENT_NAME": "pull_request",
            "GITHUB_REF": "refs/pull/8/merge",
        }
        with patch.dict(os.environ, env):
            result = verify_bundle(self.bundle, self.cmake, "head-commit-sha")
        self.assertEqual(result["source_sha"], "head-commit-sha")
        self.assertEqual(result["workflow_run_id"], "43935558026")
        self.assertEqual(result["workflow_run_attempt"], "2")
        self.assertEqual(result["workflow_event"], "pull_request")
        self.assertEqual(result["workflow_ref"], "refs/pull/8/merge")

    def test_rejects_missing_module(self):
        self.module.unlink()
        self.assert_rejected()

    def test_rejects_wrong_pe_machine(self):
        self.module.write_bytes(synthetic_pe(machine=0x14C))
        self.assert_rejected()

    def test_rejects_pe32_not_pe32_plus(self):
        self.module.write_bytes(synthetic_pe(magic=0x10B))
        self.assert_rejected()

    def test_rejects_bad_pe_signature(self):
        self.module.write_bytes(synthetic_pe(signature=b"NOPE"))
        self.assert_rejected()

    def test_accepts_official_steinberg_json5_trailing_commas(self):
        value = json.dumps({
            "Name": PRODUCT,
            "Version": VERSION,
            "Factory Info": {"Vendor": VENDOR},
            "Classes": [
                {"CID": "00112233445566778899AABBCCDDEEFF", "Category": "Audio Module Class", "Name": PRODUCT, "Vendor": VENDOR, "Version": VERSION},
                {"CID": "FFEEDDCCBBAA99887766554433221100", "Category": "Component Controller Class", "Name": PRODUCT, "Vendor": VENDOR, "Version": VERSION},
            ],
        }, indent=2)
        value = value.replace("\n  }", ",\n  }").replace("\n  ]", ",\n  ]")
        value = value[:-1] + ",\n}"
        self.metadata.write_text(value, encoding="utf-8")
        result = verify_bundle(self.bundle, self.cmake, "json5-test")
        self.assertEqual(result["product"], PRODUCT)

    def test_rejects_malformed_json5_metadata(self):
        self.metadata.write_text('{"Name" "SkellGlitch"}', encoding="utf-8")
        self.assert_rejected()

    def test_rejects_wrong_product_identity(self):
        self.write_metadata(Name="OtherPlugin")
        self.assert_rejected()

    def test_rejects_wrong_version(self):
        self.write_metadata(Version="9.9.9")
        self.assert_rejected()

    def test_rejects_missing_audio_module_class(self):
        self.write_metadata(Classes=[])
        self.assert_rejected()

    def test_rejects_duplicate_module_binaries(self):
        duplicate = self.bundle / "Contents" / "arm64-macos" / "Other.vst3"
        duplicate.parent.mkdir()
        duplicate.write_bytes(b"not a Windows module")
        self.assert_rejected()

    def test_rejects_standalone_executable_inside_bundle(self):
        (self.bundle / "SkellGlitch.exe").write_bytes(b"MZ")
        self.assert_rejected()


if __name__ == "__main__":
    unittest.main()
