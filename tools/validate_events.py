#!/usr/bin/env python3
"""Validate smart-shovel CSV exports before aggregation.

The firmware deliberately reuses an event identity after an ambiguous SD write
failure. Identical duplicate rows are therefore reported as retry duplicates,
while conflicting payloads for one identity are rejected.
"""

from __future__ import annotations

import argparse
import csv
import math
import re
import sys
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Dict, Optional, Sequence, TextIO, Tuple


EXPECTED_HEADER: Tuple[str, ...] = (
    "schema_version",
    "device_id",
    "boot_session_id",
    "event_sequence",
    "event_uptime_ms",
    "timestamp_utc",
    "mass_g",
    "mass_calibration_status",
    "corrected_signal_mv",
    "raw_adc",
    "accel_z_g",
    "latitude",
    "longitude",
    "altitude_m",
    "gps_age_ms",
    "satellites",
    "gps_status",
    "gps_wait_timed_out",
    "system_health",
)
DEVICE_ID = re.compile(r"[A-Za-z0-9_.-]{1,24}\Z")
BOOT_SESSION_ID = re.compile(r"[0-9A-Fa-f]{16}\Z")
UINT32_MAX = (1 << 32) - 1


class EventValidationError(ValueError):
    """Raised when an event export violates schema-v2 invariants."""


@dataclass(frozen=True)
class ValidationSummary:
    path: Path
    data_rows: int
    unique_events: int
    identical_retry_rows: int


def _error(line: int, column: str, message: str) -> EventValidationError:
    return EventValidationError(f"line {line} column {column}: {message}")


def _required_uint32(value: str, line: int, column: str, minimum: int = 0) -> int:
    if not value or not value.isascii() or not value.isdecimal():
        raise _error(line, column, f"expected an unsigned decimal integer; got {value!r}")
    parsed = int(value)
    if parsed < minimum or parsed > UINT32_MAX:
        raise _error(line, column, f"value {parsed} is outside {minimum}..{UINT32_MAX}")
    return parsed


def _optional_finite(value: str, line: int, column: str) -> Optional[float]:
    if value == "":
        return None
    try:
        parsed = float(value)
    except ValueError as exc:
        raise _error(line, column, f"expected a finite number; got {value!r}") from exc
    if not math.isfinite(parsed):
        raise _error(line, column, f"expected a finite number; got {value!r}")
    return parsed


def _optional_uint32(value: str, line: int, column: str) -> Optional[int]:
    return None if value == "" else _required_uint32(value, line, column)


def _validate_timestamp(value: str, line: int) -> None:
    if value == "":
        return
    try:
        parsed = datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ")
    except ValueError as exc:
        raise _error(line, "timestamp_utc", f"expected a valid UTC second; got {value!r}") from exc
    if parsed.year < 2000:
        raise _error(line, "timestamp_utc", "year must be 2000 or later")


def _validate_row(row: Sequence[str], line: int) -> Tuple[str, str, int]:
    if len(row) != len(EXPECTED_HEADER):
        raise EventValidationError(
            f"line {line}: expected {len(EXPECTED_HEADER)} fields; got {len(row)}"
        )
    values = dict(zip(EXPECTED_HEADER, row))
    if values["schema_version"] != "2":
        raise _error(line, "schema_version", "expected schema version 2")
    if DEVICE_ID.fullmatch(values["device_id"]) is None:
        raise _error(line, "device_id", "expected 1..24 letters, digits, '.', '_', or '-'")
    if BOOT_SESSION_ID.fullmatch(values["boot_session_id"]) is None:
        raise _error(line, "boot_session_id", "expected exactly 16 hexadecimal characters")

    sequence = _required_uint32(values["event_sequence"], line, "event_sequence", minimum=1)
    _required_uint32(values["event_uptime_ms"], line, "event_uptime_ms")
    _validate_timestamp(values["timestamp_utc"], line)

    mass = _optional_finite(values["mass_g"], line, "mass_g")
    calibration_status = values["mass_calibration_status"]
    if calibration_status not in {"unavailable", "provisional", "verified"}:
        raise _error(line, "mass_calibration_status", "unknown status")
    if mass is None and calibration_status != "unavailable":
        raise _error(line, "mass_calibration_status", "mass is blank but calibration is available")
    _optional_finite(values["corrected_signal_mv"], line, "corrected_signal_mv")
    _optional_uint32(values["raw_adc"], line, "raw_adc")
    _optional_finite(values["accel_z_g"], line, "accel_z_g")

    latitude = _optional_finite(values["latitude"], line, "latitude")
    longitude = _optional_finite(values["longitude"], line, "longitude")
    if (latitude is None) != (longitude is None):
        raise _error(line, "latitude/longitude", "both coordinates must be present or both blank")
    if latitude is not None and not -90.0 <= latitude <= 90.0:
        raise _error(line, "latitude", "value is outside -90..90")
    if longitude is not None and not -180.0 <= longitude <= 180.0:
        raise _error(line, "longitude", "value is outside -180..180")
    _optional_finite(values["altitude_m"], line, "altitude_m")
    _optional_uint32(values["gps_age_ms"], line, "gps_age_ms")
    _optional_uint32(values["satellites"], line, "satellites")

    gps_status = values["gps_status"]
    if gps_status not in {"valid", "location_only", "no_fix", "invalid", "stale", "timeout"}:
        raise _error(line, "gps_status", "unknown status")
    if gps_status == "valid" and (latitude is None or values["timestamp_utc"] == ""):
        raise _error(line, "gps_status", "valid requires coordinates and UTC")
    if values["gps_wait_timed_out"] not in {"true", "false"}:
        raise _error(line, "gps_wait_timed_out", "expected lowercase true or false")
    if values["system_health"] not in {
        "healthy",
        "gps_degraded",
        "storage_degraded",
        "gps_and_storage_degraded",
    }:
        raise _error(line, "system_health", "unknown status")
    return values["device_id"], values["boot_session_id"].lower(), sequence


def validate_stream(source: TextIO, path: Path = Path("<stream>")) -> ValidationSummary:
    reader = csv.reader(source, strict=True)
    try:
        header = next(reader, None)
        if header is None:
            raise EventValidationError(f"{path}: export is empty")
        if tuple(header) != EXPECTED_HEADER:
            raise EventValidationError(f"{path}: schema-v2 header mismatch")

        identities: Dict[Tuple[str, str, int], Tuple[str, ...]] = {}
        rows = 0
        duplicates = 0
        for row in reader:
            rows += 1
            identity = _validate_row(row, reader.line_num)
            payload = tuple(row)
            prior = identities.get(identity)
            if prior is None:
                identities[identity] = payload
            elif prior == payload:
                duplicates += 1
            else:
                raise EventValidationError(
                    f"line {reader.line_num}: identity {identity!r} has conflicting payloads"
                )
    except csv.Error as exc:
        raise EventValidationError(f"{path}: invalid CSV near line {reader.line_num}: {exc}") from exc

    return ValidationSummary(path, rows, len(identities), duplicates)


def validate_file(path: Path) -> ValidationSummary:
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as source:
            return validate_stream(source, path)
    except (OSError, UnicodeError) as exc:
        raise EventValidationError(f"cannot read {path}: {exc}") from exc


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="schema-v2 events.csv to validate")
    return parser


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = _parser().parse_args(argv)
    try:
        summary = validate_file(args.input)
    except EventValidationError as exc:
        print(f"event validation failed: {exc}", file=sys.stderr)
        return 2
    print(
        f"event validation passed: rows={summary.data_rows}, "
        f"unique_events={summary.unique_events}, "
        f"identical_retry_rows={summary.identical_retry_rows}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
