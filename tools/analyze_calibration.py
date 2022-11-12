#!/usr/bin/env python3
"""Analyze one immutable smart-shovel orientation-calibration CSV.

The implementation intentionally uses only the Python standard library so that
the preserved evidence can be checked in a fresh checkout without a notebook or
scientific-Python environment.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple


EXPECTED_SCHEMA: Tuple[str, ...] = (
    "voltage",
    "ax",
    "ay",
    "az",
    "gx",
    "gy",
    "gz",
)
NORMAL_975 = 1.959963984540054


class CalibrationError(ValueError):
    """Raised when an input cannot support the requested analysis."""


@dataclass(frozen=True)
class CalibrationData:
    path: Path
    sha256: str
    total_rows: int
    blank_rows: int
    voltage: Tuple[float, ...]
    az: Tuple[float, ...]

    @property
    def usable_rows(self) -> int:
        return len(self.voltage)


def _file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as source:
            for block in iter(lambda: source.read(1024 * 1024), b""):
                digest.update(block)
    except OSError as exc:
        raise CalibrationError(f"cannot read {path}: {exc}") from exc
    return digest.hexdigest()


def load_calibration_csv(path: Path) -> CalibrationData:
    """Load and strictly validate one calibration capture without changing it."""

    if not path.exists():
        raise CalibrationError(f"input does not exist: {path}")
    if not path.is_file():
        raise CalibrationError(f"input is not a regular file: {path}")

    voltage: List[float] = []
    az: List[float] = []
    total_rows = 0
    blank_rows = 0

    try:
        with path.open("r", encoding="utf-8-sig", newline="") as source:
            reader = csv.reader(source, strict=True)
            header = next(reader, None)
            if header is None:
                raise CalibrationError(f"input is empty: {path}")
            if tuple(header) != EXPECTED_SCHEMA:
                actual = ",".join(header)
                expected = ",".join(EXPECTED_SCHEMA)
                raise CalibrationError(
                    f"schema mismatch in {path}: expected {expected}; got {actual}"
                )

            for row in reader:
                total_rows += 1
                if not row or all(not field.strip() for field in row):
                    blank_rows += 1
                    continue
                if len(row) != len(EXPECTED_SCHEMA):
                    raise CalibrationError(
                        f"line {reader.line_num} in {path} has {len(row)} fields; "
                        f"expected {len(EXPECTED_SCHEMA)}"
                    )

                values: List[float] = []
                for column, field in zip(EXPECTED_SCHEMA, row):
                    try:
                        value = float(field)
                    except ValueError as exc:
                        raise CalibrationError(
                            f"line {reader.line_num} column {column} in {path} "
                            f"is not numeric: {field!r}"
                        ) from exc
                    if not math.isfinite(value):
                        raise CalibrationError(
                            f"line {reader.line_num} column {column} in {path} "
                            f"is not finite: {field!r}"
                        )
                    values.append(value)

                voltage.append(values[0])
                az.append(values[3])
    except UnicodeError as exc:
        raise CalibrationError(f"input is not valid UTF-8 text: {path}: {exc}") from exc
    except csv.Error as exc:
        raise CalibrationError(f"invalid CSV in {path}: {exc}") from exc
    except OSError as exc:
        raise CalibrationError(f"cannot read {path}: {exc}") from exc

    if len(voltage) < 3:
        raise CalibrationError(
            f"{path} has {len(voltage)} usable rows; at least 3 are required"
        )

    return CalibrationData(
        path=path,
        sha256=_file_sha256(path),
        total_rows=total_rows,
        blank_rows=blank_rows,
        voltage=tuple(voltage),
        az=tuple(az),
    )


def _beta_continued_fraction(a: float, b: float, x: float) -> float:
    """Evaluate the continued fraction used by the incomplete beta function."""

    max_iterations = 300
    epsilon = 3.0e-14
    minimum = sys.float_info.min / epsilon
    qab = a + b
    qap = a + 1.0
    qam = a - 1.0
    c = 1.0
    d = 1.0 - qab * x / qap
    if abs(d) < minimum:
        d = minimum
    d = 1.0 / d
    result = d

    for iteration in range(1, max_iterations + 1):
        doubled = 2 * iteration
        coefficient = (
            iteration
            * (b - iteration)
            * x
            / ((qam + doubled) * (a + doubled))
        )
        d = 1.0 + coefficient * d
        if abs(d) < minimum:
            d = minimum
        c = 1.0 + coefficient / c
        if abs(c) < minimum:
            c = minimum
        d = 1.0 / d
        result *= d * c

        coefficient = -(
            (a + iteration)
            * (qab + iteration)
            * x
            / ((a + doubled) * (qap + doubled))
        )
        d = 1.0 + coefficient * d
        if abs(d) < minimum:
            d = minimum
        c = 1.0 + coefficient / c
        if abs(c) < minimum:
            c = minimum
        d = 1.0 / d
        delta = d * c
        result *= delta
        if abs(delta - 1.0) <= epsilon:
            return result

    raise CalibrationError("Student-t confidence interval calculation did not converge")


def _regularized_incomplete_beta(x: float, a: float, b: float) -> float:
    if x <= 0.0:
        return 0.0
    if x >= 1.0:
        return 1.0
    front = math.exp(
        math.lgamma(a + b)
        - math.lgamma(a)
        - math.lgamma(b)
        + a * math.log(x)
        + b * math.log1p(-x)
    )
    if x < (a + 1.0) / (a + b + 2.0):
        return front * _beta_continued_fraction(a, b, x) / a
    return 1.0 - front * _beta_continued_fraction(b, a, 1.0 - x) / b


def _student_t_cdf(value: float, degrees_of_freedom: int) -> float:
    if degrees_of_freedom <= 0:
        raise CalibrationError("Student-t degrees of freedom must be positive")
    if value == 0.0:
        return 0.5
    ratio = degrees_of_freedom / (degrees_of_freedom + value * value)
    tail = 0.5 * _regularized_incomplete_beta(
        ratio, degrees_of_freedom / 2.0, 0.5
    )
    return 1.0 - tail if value > 0.0 else tail


def _student_t_quantile(probability: float, degrees_of_freedom: int) -> float:
    """Invert the Student-t CDF by deterministic bisection."""

    if not 0.5 < probability < 1.0:
        raise CalibrationError("Student-t probability must be between 0.5 and 1")
    lower = 0.0
    upper = 1.0
    while _student_t_cdf(upper, degrees_of_freedom) < probability:
        upper *= 2.0
        if not math.isfinite(upper):
            raise CalibrationError("Student-t quantile calculation did not converge")

    for _ in range(100):
        middle = (lower + upper) / 2.0
        if _student_t_cdf(middle, degrees_of_freedom) < probability:
            lower = middle
        else:
            upper = middle
    return (lower + upper) / 2.0


def _lag1_correlation(residuals: Sequence[float]) -> Optional[float]:
    first = residuals[:-1]
    second = residuals[1:]
    count = len(first)
    if count < 2:
        return None
    first_mean = math.fsum(first) / count
    second_mean = math.fsum(second) / count
    covariance = math.fsum(
        (left - first_mean) * (right - second_mean)
        for left, right in zip(first, second)
    )
    first_sum_squares = math.fsum((value - first_mean) ** 2 for value in first)
    second_sum_squares = math.fsum((value - second_mean) ** 2 for value in second)
    denominator = math.sqrt(first_sum_squares * second_sum_squares)
    return covariance / denominator if denominator > 0.0 else None


def _newey_west_slope_standard_error(
    x: Sequence[float], residuals: Sequence[float], lags: int
) -> float:
    """Return Bartlett-kernel HAC slope SE with n/(n-2) correction."""

    count = len(x)
    if lags < 0:
        raise CalibrationError("HAC lag count must be nonnegative")
    if lags >= count:
        raise CalibrationError(
            f"HAC lag count {lags} must be smaller than usable row count {count}"
        )

    sum_x = math.fsum(x)
    sum_x_squared = math.fsum(value * value for value in x)
    determinant = count * sum_x_squared - sum_x * sum_x
    if determinant <= 0.0:
        raise CalibrationError("az has zero variance; regression is undefined")
    bread_01 = -sum_x / determinant
    bread_11 = count / determinant

    meat_00 = math.fsum(value * value for value in residuals)
    meat_01 = math.fsum(
        residual * residual * value for residual, value in zip(residuals, x)
    )
    meat_11 = math.fsum(
        residual * residual * value * value
        for residual, value in zip(residuals, x)
    )

    for lag in range(1, lags + 1):
        weight = 1.0 - lag / (lags + 1.0)
        lag_00 = 0.0
        lag_01 = 0.0
        lag_11 = 0.0
        for index in range(lag, count):
            product = residuals[index] * residuals[index - lag]
            current_x = x[index]
            lagged_x = x[index - lag]
            lag_00 += 2.0 * product
            lag_01 += product * (current_x + lagged_x)
            lag_11 += 2.0 * product * current_x * lagged_x
        meat_00 += weight * lag_00
        meat_01 += weight * lag_01
        meat_11 += weight * lag_11

    slope_variance = (
        bread_01 * bread_01 * meat_00
        + 2.0 * bread_01 * bread_11 * meat_01
        + bread_11 * bread_11 * meat_11
    )
    slope_variance *= count / (count - 2.0)
    if slope_variance < 0.0:
        raise CalibrationError(
            "HAC slope variance is negative; input is ill-conditioned"
        )
    return math.sqrt(slope_variance)


def analyze_file(path: Path, hac_lags: Optional[int] = None) -> Dict[str, object]:
    data = load_calibration_csv(path)
    x = data.az
    y = data.voltage
    count = data.usable_rows
    x_mean = math.fsum(x) / count
    y_mean = math.fsum(y) / count
    x_deviations = tuple(value - x_mean for value in x)
    y_deviations = tuple(value - y_mean for value in y)
    sum_xx = math.fsum(value * value for value in x_deviations)
    sum_yy = math.fsum(value * value for value in y_deviations)
    if sum_xx <= 0.0:
        raise CalibrationError(
            f"az has zero variance in {path}; regression is undefined"
        )
    if sum_yy <= 0.0:
        raise CalibrationError(
            f"voltage has zero variance in {path}; R-squared is undefined"
        )

    sum_xy = math.fsum(
        x_value * y_value
        for x_value, y_value in zip(x_deviations, y_deviations)
    )
    slope = sum_xy / sum_xx
    intercept = y_mean - slope * x_mean
    residuals = tuple(
        response - (intercept + slope * predictor)
        for predictor, response in zip(x, y)
    )
    sum_squared_error = math.fsum(value * value for value in residuals)
    degrees_of_freedom = count - 2
    residual_variance = sum_squared_error / degrees_of_freedom
    rmse = math.sqrt(sum_squared_error / count)
    residual_standard_error = math.sqrt(residual_variance)
    slope_standard_error = math.sqrt(residual_variance / sum_xx)
    intercept_standard_error = math.sqrt(
        residual_variance * (1.0 / count + x_mean * x_mean / sum_xx)
    )
    t_critical = _student_t_quantile(0.975, degrees_of_freedom)
    r_squared = 1.0 - sum_squared_error / sum_yy
    lag1_correlation = _lag1_correlation(residuals)
    durbin_watson = (
        math.fsum(
            (residuals[index] - residuals[index - 1]) ** 2
            for index in range(1, count)
        )
        / sum_squared_error
        if sum_squared_error > 0.0
        else None
    )

    result: Dict[str, object] = {
        "input": str(path),
        "sha256": data.sha256,
        "schema": list(EXPECTED_SCHEMA),
        "counts": {
            "total_rows": data.total_rows,
            "blank_rows": data.blank_rows,
            "usable_rows": count,
        },
        "model": {
            "formula": "voltage = slope * az + intercept",
            "slope": slope,
            "intercept": intercept,
            "r_squared": r_squared,
            "rmse": rmse,
            "residual_standard_error": residual_standard_error,
            "degrees_of_freedom": degrees_of_freedom,
            "t_critical_95": t_critical,
            "slope_standard_error": slope_standard_error,
            "slope_ci95": [
                slope - t_critical * slope_standard_error,
                slope + t_critical * slope_standard_error,
            ],
            "intercept_standard_error": intercept_standard_error,
            "intercept_ci95": [
                intercept - t_critical * intercept_standard_error,
                intercept + t_critical * intercept_standard_error,
            ],
        },
        "diagnostics": {
            "residual_lag1_correlation": lag1_correlation,
            "durbin_watson": durbin_watson,
        },
        "hac": None,
    }

    if hac_lags is not None:
        hac_standard_error = _newey_west_slope_standard_error(x, residuals, hac_lags)
        result["hac"] = {
            "lags": hac_lags,
            "kernel": "Bartlett",
            "small_sample_correction": "n/(n-2)",
            "critical_value_95": NORMAL_975,
            "slope_standard_error": hac_standard_error,
            "slope_ci95": [
                slope - NORMAL_975 * hac_standard_error,
                slope + NORMAL_975 * hac_standard_error,
            ],
        }
    return result


def _format_number(value: object) -> str:
    if value is None:
        return "n/a"
    if isinstance(value, float):
        return format(value, ".15g")
    return str(value)


def format_text(result: Dict[str, object]) -> str:
    counts = result["counts"]
    model = result["model"]
    diagnostics = result["diagnostics"]
    assert isinstance(counts, dict)
    assert isinstance(model, dict)
    assert isinstance(diagnostics, dict)
    slope_ci = model["slope_ci95"]
    intercept_ci = model["intercept_ci95"]
    assert isinstance(slope_ci, list)
    assert isinstance(intercept_ci, list)
    lines = [
        f"Input: {result['input']}",
        f"SHA-256: {result['sha256']}",
        f"Schema: {','.join(result['schema'])}",
        f"Rows after header: {counts['total_rows']}",
        f"Blank rows ignored: {counts['blank_rows']}",
        f"Usable finite rows: {counts['usable_rows']}",
        f"Model: {model['formula']}",
        f"Slope: {_format_number(model['slope'])}",
        f"Intercept: {_format_number(model['intercept'])}",
        f"R-squared: {_format_number(model['r_squared'])}",
        f"RMSE: {_format_number(model['rmse'])}",
        "Residual standard error: "
        f"{_format_number(model['residual_standard_error'])}",
        f"Degrees of freedom: {model['degrees_of_freedom']}",
        f"95% Student-t critical value: {_format_number(model['t_critical_95'])}",
        f"Slope standard error: {_format_number(model['slope_standard_error'])}",
        "Slope 95% CI: "
        f"[{_format_number(slope_ci[0])}, {_format_number(slope_ci[1])}]",
        "Intercept standard error: "
        f"{_format_number(model['intercept_standard_error'])}",
        "Intercept 95% CI: "
        f"[{_format_number(intercept_ci[0])}, {_format_number(intercept_ci[1])}]",
        "Residual lag-1 correlation: "
        f"{_format_number(diagnostics['residual_lag1_correlation'])}",
        f"Durbin-Watson: {_format_number(diagnostics['durbin_watson'])}",
    ]
    hac = result["hac"]
    if isinstance(hac, dict):
        hac_ci = hac["slope_ci95"]
        assert isinstance(hac_ci, list)
        lines.extend(
            [
                f"Newey-West lags: {hac['lags']}",
                f"Newey-West kernel: {hac['kernel']}",
                "Newey-West slope standard error: "
                f"{_format_number(hac['slope_standard_error'])}",
                "Newey-West slope 95% CI: "
                f"[{_format_number(hac_ci[0])}, {_format_number(hac_ci[1])}]",
            ]
        )
    return "\n".join(lines)


def _nonnegative_integer(value: str) -> int:
    try:
        parsed = int(value)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("must be an integer") from exc
    if parsed < 0:
        raise argparse.ArgumentTypeError("must be nonnegative")
    return parsed


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Fit voltage = slope * az + intercept for one calibration CSV."
    )
    parser.add_argument(
        "--input",
        required=True,
        type=Path,
        help="explicit path to one immutable calibration CSV",
    )
    parser.add_argument(
        "--hac-lags",
        type=_nonnegative_integer,
        help="also report Bartlett-kernel Newey-West slope uncertainty",
    )
    parser.add_argument(
        "--format",
        choices=("text", "json"),
        default="text",
        help="output format (default: text)",
    )
    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = build_parser()
    arguments = parser.parse_args(argv)
    try:
        result = analyze_file(arguments.input, arguments.hac_lags)
    except CalibrationError as exc:
        parser.error(str(exc))
    if arguments.format == "json":
        print(json.dumps(result, indent=2, sort_keys=True, allow_nan=False))
    else:
        print(format_text(result))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
