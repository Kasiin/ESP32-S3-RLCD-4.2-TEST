"""Offline packaging checks. No USB access, no flashing."""
import hashlib
from pathlib import Path
import re
import shutil
import tempfile
import unittest
from urllib.parse import unquote

from flash import verify

ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / "firmware/v0.2.0"


class ReleaseTests(unittest.TestCase):
    def test_checksums(self):
        verify(FIRMWARE)

    def test_hardware_validated_application(self):
        digest = hashlib.sha256((FIRMWARE / "rlcd_hardware_test.bin").read_bytes()).hexdigest()
        self.assertEqual(digest, "0f8467e6cb20f6865b45e2d2bcf839270629c33d27015e3381f556dbff4b450f")

    def test_merged_offsets(self):
        merged = (FIRMWARE / "rlcd-hardware-test-v0.2.0-merged.bin").read_bytes()
        app = (FIRMWARE / "rlcd_hardware_test.bin").read_bytes()
        partition = (FIRMWARE / "partition-table.bin").read_bytes()
        self.assertEqual(merged[0], 0xE9)
        self.assertEqual(merged[0x8000:0x8000 + len(partition)], partition)
        self.assertEqual(merged[0x10000:], app)
        self.assertEqual(merged[0x9000:0x10000], b"\xff" * 0x7000)

    def test_corruption_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            for source in FIRMWARE.iterdir():
                if source.is_file():
                    shutil.copy2(source, directory / source.name)
            app = directory / "rlcd_hardware_test.bin"
            contents = bytearray(app.read_bytes())
            contents[-1] ^= 1
            app.write_bytes(contents)
            with self.assertRaisesRegex(ValueError, "SHA256 mismatch"):
                verify(directory)

    def test_local_document_links(self):
        docs = list(ROOT.glob("*.md")) + list((ROOT / "docs").glob("*.md")) + list(FIRMWARE.glob("*.md"))
        for path in docs:
            for link in re.findall(r"\]\(([^)]+)\)", path.read_text(encoding="utf-8")):
                if "://" in link or link.startswith("#"):
                    continue
                target = path.parent / unquote(link.split("#", 1)[0])
                self.assertTrue(target.exists(), f"Broken link in {path.name}: {link}")


if __name__ == "__main__":
    unittest.main()
