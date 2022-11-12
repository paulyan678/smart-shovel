from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
TOOLS_DIRECTORY = REPOSITORY_ROOT / "tools"
sys.path.insert(0, str(TOOLS_DIRECTORY))

import check_firmware_size  # noqa: E402


class FirmwareSizeTests(unittest.TestCase):
    def test_accepts_image_within_limit_and_rejects_oversize(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            image = Path(directory) / "firmware.bin"
            image.write_bytes(b"x" * 8)
            self.assertEqual(check_firmware_size.check_image(image, 8), 8)
            with self.assertRaisesRegex(ValueError, "physical limit"):
                check_firmware_size.check_image(image, 7)

    def test_rejects_missing_image_and_invalid_limit(self) -> None:
        with self.assertRaisesRegex(ValueError, "positive"):
            check_firmware_size.check_image(Path("missing.bin"), 0)
        with self.assertRaisesRegex(ValueError, "cannot inspect"):
            check_firmware_size.check_image(Path("missing.bin"), 1)


if __name__ == "__main__":
    unittest.main()
