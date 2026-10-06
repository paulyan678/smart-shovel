from __future__ import annotations

import csv
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from evaluate_known_mass import FIELDS, evaluate


class KnownMassTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / "measurements.csv"
        # Analytical fixtures test the evaluator only; they are not hardware measurements.
        self.rows = [["test-device", run, split, mass, mass / 2, angle]
                     for run, split in [("a", "train"), ("b", "train"), ("c", "test")]
                     for mass in [0, 100, 200] for angle in [0, 45]]

    def run_evaluation(self):
        with self.path.open("w", newline="") as output:
            writer = csv.writer(output)
            writer.writerow(FIELDS)
            writer.writerows(self.rows)
        return evaluate(self.path, 5, 10)

    def test_known_answer_fit_and_no_input_mutation(self):
        result = self.run_evaluation()
        before = self.path.read_bytes()
        self.assertEqual(result["model"], {"grams_per_mv": 2, "intercept_g": 0})
        self.assertEqual(result["held_out"]["rmse_g"], 0)
        self.assertTrue(result["within_limits"])
        self.assertEqual(result["firmware_calibration_status"], "unchanged")
        evaluate(self.path, 5, 10)
        self.assertEqual(self.path.read_bytes(), before)

    def test_held_out_failure_does_not_refit_training(self):
        for row in self.rows:
            if row[2] == "test":
                row[4] += 20
        result = self.run_evaluation()
        self.assertEqual(result["model"]["grams_per_mv"], 2)
        self.assertEqual(result["held_out"]["rmse_g"], 40)
        self.assertFalse(result["within_limits"])

    def test_acquisition_run_leakage_is_rejected(self):
        self.rows[-1][1] = "a"
        with self.assertRaisesRegex(ValueError, "both splits"):
            self.run_evaluation()

    def test_nonfinite_values_are_rejected(self):
        for value in [float("nan"), float("inf"), -float("inf")]:
            with self.subTest(value=value):
                self.rows[-1][4] = value
                with self.assertRaisesRegex(ValueError, "finite"):
                    self.run_evaluation()

    def test_missing_test_run_is_rejected(self):
        self.rows = [row for row in self.rows if row[2] == "train"]
        with self.assertRaisesRegex(ValueError, "independent acquisition runs"):
            self.run_evaluation()

    def test_one_mass_or_orientation_cannot_establish_calibration(self):
        for row in self.rows:
            row[3] = 0
        with self.assertRaisesRegex(ValueError, "nonzero known masses"):
            self.run_evaluation()

    def test_multiple_devices_must_be_evaluated_separately(self):
        self.rows[-1][0] = "second-device"
        with self.assertRaisesRegex(ValueError, "exactly one device"):
            self.run_evaluation()

    def test_zero_variance_is_rejected(self):
        for row in self.rows:
            row[4] = 0
        with self.assertRaisesRegex(ValueError, "variance"):
            self.run_evaluation()


if __name__ == "__main__":
    unittest.main()
