#!/usr/bin/env python3
"""Fit a single-device mass calibration and evaluate untouched acquisition runs."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
import statistics
import sys


FIELDS = ["device_id", "run_id", "split", "known_mass_g", "corrected_signal_mv", "orientation_deg"]


def evaluate(path: Path, max_rmse_g: float, max_error_g: float) -> dict:
    if any(not math.isfinite(x) or x <= 0 for x in (max_rmse_g, max_error_g)):
        raise ValueError("Error limits must be finite and positive")
    raw = path.read_bytes()
    with path.open(newline="", encoding="utf-8") as source:
        reader = csv.DictReader(source)
        if reader.fieldnames != FIELDS:
            raise ValueError("Expected columns: " + ",".join(FIELDS))
        rows = []
        run_splits = {}
        for number, row in enumerate(reader, 2):
            if None in row or any(value is None or not value.strip() for value in row.values()):
                raise ValueError(f"Row {number}: missing or extra fields")
            row = {key: value.strip() for key, value in row.items()}
            if row["split"] not in {"train", "test"}:
                raise ValueError(f"Row {number}: split must be train or test")
            for key in FIELDS[3:]:
                row[key] = float(row[key])
                if not math.isfinite(row[key]):
                    raise ValueError(f"Row {number}: {key} must be finite")
            if row["known_mass_g"] < 0:
                raise ValueError(f"Row {number}: known mass must be nonnegative")
            identity = (row["device_id"], row["run_id"])
            if run_splits.setdefault(identity, row["split"]) != row["split"]:
                raise ValueError("An acquisition run cannot appear in both splits")
            rows.append(row)
    if len({row["device_id"] for row in rows}) != 1:
        raise ValueError("Supply measurements from exactly one device")
    train = [row for row in rows if row["split"] == "train"]
    test = [row for row in rows if row["split"] == "test"]
    for label, subset, minimum_runs in (("train", train, 2), ("test", test, 1)):
        if len({row["run_id"] for row in subset}) < minimum_runs:
            raise ValueError(f"{label}: need at least {minimum_runs} independent acquisition runs")
        masses = {row["known_mass_g"] for row in subset}
        if len(masses) < 3 or 0 not in masses:
            raise ValueError(f"{label}: need zero and at least two nonzero known masses")
        if len({row["orientation_deg"] for row in subset}) < 2:
            raise ValueError(f"{label}: need at least two recorded orientations")
    x_mean = statistics.fmean(row["corrected_signal_mv"] for row in train)
    y_mean = statistics.fmean(row["known_mass_g"] for row in train)
    variance = math.fsum((row["corrected_signal_mv"] - x_mean) ** 2 for row in train)
    if not math.isfinite(variance) or variance == 0:
        raise ValueError("Training signal variance must be finite and nonzero")
    slope = math.fsum((row["corrected_signal_mv"] - x_mean) * (row["known_mass_g"] - y_mean)
                      for row in train) / variance
    intercept = y_mean - slope * x_mean
    if not all(math.isfinite(x) for x in (slope, intercept)):
        raise ValueError("Calibration coefficients are not finite")

    def errors(subset: list[dict]) -> dict:
        residuals = [slope * row["corrected_signal_mv"] + intercept - row["known_mass_g"]
                     for row in subset]
        result = {"rows": len(subset), "bias_g": statistics.fmean(residuals),
                  "rmse_g": math.sqrt(statistics.fmean(error * error for error in residuals)),
                  "max_absolute_error_g": max(abs(error) for error in residuals)}
        if not all(math.isfinite(value) for value in result.values()):
            raise ValueError("Evaluation errors are not finite")
        return result

    metrics = errors(test)
    return {
        "schema_version": 1, "input_sha256": hashlib.sha256(raw).hexdigest(),
        "device_id": rows[0]["device_id"], "model": {"grams_per_mv": slope, "intercept_g": intercept},
        "train_runs": sorted({row["run_id"] for row in train}),
        "test_runs": sorted({row["run_id"] for row in test}),
        "training": errors(train), "held_out": metrics,
        "held_out_by_run": {run: errors([row for row in test if row["run_id"] == run])
                            for run in sorted({row["run_id"] for row in test})},
        "held_out_by_orientation": {str(angle): errors([row for row in test if row["orientation_deg"] == angle])
                                    for angle in sorted({row["orientation_deg"] for row in test})},
        "limits": {"rmse_g": max_rmse_g, "absolute_error_g": max_error_g},
        "within_limits": metrics["rmse_g"] <= max_rmse_g and metrics["max_absolute_error_g"] <= max_error_g,
        "firmware_calibration_status": "unchanged",
        "scope": "Supplied measurements only; input provenance and physical calibration require review.",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--max-rmse-g", type=float, required=True)
    parser.add_argument("--max-error-g", type=float, required=True)
    args = parser.parse_args()
    try:
        result = evaluate(args.input, args.max_rmse_g, args.max_error_g)
    except (ValueError, OverflowError, OSError) as error:
        parser.error(str(error))
    print(json.dumps(result, indent=2, allow_nan=False))
    return 0 if result["within_limits"] else 1


if __name__ == "__main__":
    sys.exit(main())
