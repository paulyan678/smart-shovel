from __future__ import annotations

import io
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
TOOLS_DIRECTORY = REPOSITORY_ROOT / "tools"
sys.path.insert(0, str(TOOLS_DIRECTORY))

import validate_events  # noqa: E402


HEADER = ",".join(validate_events.EXPECTED_HEADER) + "\n"
VALID_ROW = (
    "2,SS-001,0123456789ABCDEF,1,1234,2026-07-16T12:34:56Z,123.456,"
    "provisional,-8.500,54321,0.998000,5.6037000,-0.1870000,42.50,250,9,"
    "valid,false,healthy\n"
)


class EventValidationTests(unittest.TestCase):
    def test_accepts_schema_v2_and_classifies_identical_retry(self) -> None:
        summary = validate_events.validate_stream(io.StringIO(HEADER + VALID_ROW + VALID_ROW))
        self.assertEqual(summary.data_rows, 2)
        self.assertEqual(summary.unique_events, 1)
        self.assertEqual(summary.identical_retry_rows, 1)

    def test_rejects_conflicting_payload_for_one_identity(self) -> None:
        conflicting = VALID_ROW.replace("123.456", "999.000")
        with self.assertRaisesRegex(validate_events.EventValidationError, "conflicting payloads"):
            validate_events.validate_stream(io.StringIO(HEADER + VALID_ROW + conflicting))

    def test_rejects_bad_header_range_and_partial_coordinates(self) -> None:
        with self.assertRaisesRegex(validate_events.EventValidationError, "header mismatch"):
            validate_events.validate_stream(io.StringIO("wrong\n"))

        bad_latitude = VALID_ROW.replace("5.6037000", "95.0")
        with self.assertRaisesRegex(validate_events.EventValidationError, "latitude"):
            validate_events.validate_stream(io.StringIO(HEADER + bad_latitude))

        partial = VALID_ROW.replace("-0.1870000", "")
        with self.assertRaisesRegex(validate_events.EventValidationError, "both coordinates"):
            validate_events.validate_stream(io.StringIO(HEADER + partial))

    def test_cli_reports_summary(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "events.csv"
            path.write_text(HEADER + VALID_ROW, encoding="utf-8")
            completed = subprocess.run(
                [sys.executable, str(TOOLS_DIRECTORY / "validate_events.py"), str(path)],
                check=True,
                capture_output=True,
                text=True,
            )
        self.assertIn("rows=1, unique_events=1", completed.stdout)


if __name__ == "__main__":
    unittest.main()
