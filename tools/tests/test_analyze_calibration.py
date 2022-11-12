from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
TOOLS_DIRECTORY = REPOSITORY_ROOT / "tools"
sys.path.insert(0, str(TOOLS_DIRECTORY))

import analyze_calibration  # noqa: E402


RAW_DATA = REPOSITORY_ROOT / "calibration" / "data" / "raw"
CALIB1 = RAW_DATA / "calib1.csv"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


class CalibrationAnalysisTests(unittest.TestCase):
    def test_calib1_golden_fit_and_input_is_unchanged(self) -> None:
        before = sha256(CALIB1)
        result = analyze_calibration.analyze_file(CALIB1, hac_lags=5)
        after = sha256(CALIB1)

        self.assertEqual(
            before,
            "13a7f5e0f5fc300f62521f9b5ce161a70229fc79c9a331a25cd4cbc6148e12a7",
        )
        self.assertEqual(after, before)
        self.assertEqual(result["sha256"], before)
        self.assertEqual(
            result["counts"],
            {"total_rows": 619, "blank_rows": 1, "usable_rows": 618},
        )

        model = result["model"]
        self.assertAlmostEqual(model["slope"], -74.70891681836552, places=11)
        self.assertAlmostEqual(model["intercept"], 25.28843736594002, places=11)
        self.assertAlmostEqual(model["r_squared"], 0.9151766665138769, places=13)
        self.assertAlmostEqual(
            model["slope_standard_error"], 0.9164046458205017, places=12
        )
        self.assertAlmostEqual(
            model["intercept_standard_error"], 0.960613275482504, places=12
        )
        self.assertAlmostEqual(model["slope_ci95"][0], -76.5085729023959, places=10)
        self.assertAlmostEqual(model["slope_ci95"][1], -72.9092607343352, places=10)

        diagnostics = result["diagnostics"]
        self.assertAlmostEqual(
            diagnostics["residual_lag1_correlation"], 0.7721495548104585, places=12
        )
        self.assertAlmostEqual(
            diagnostics["durbin_watson"], 0.4475997663906329, places=12
        )

        hac = result["hac"]
        self.assertEqual(hac["lags"], 5)
        self.assertAlmostEqual(hac["slope_standard_error"], 1.55355114, places=7)

    def test_cli_emits_valid_json(self) -> None:
        completed = subprocess.run(
            [
                sys.executable,
                str(TOOLS_DIRECTORY / "analyze_calibration.py"),
                "--input",
                str(CALIB1),
                "--format",
                "json",
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        result = json.loads(completed.stdout)
        self.assertEqual(result["counts"]["usable_rows"], 618)
        self.assertIsNone(result["hac"])

    def test_rejects_known_schema(self) -> None:
        with self.assertRaisesRegex(
            analyze_calibration.CalibrationError, "schema mismatch"
        ):
            analyze_calibration.analyze_file(RAW_DATA / "known.csv")

    def test_rejects_test_schema(self) -> None:
        with self.assertRaisesRegex(
            analyze_calibration.CalibrationError, "schema mismatch"
        ):
            analyze_calibration.analyze_file(RAW_DATA / "test.csv")


if __name__ == "__main__":
    unittest.main()
