#!/usr/bin/env python3
"""Fail when a generated firmware image exceeds its physical flash budget."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Optional, Sequence


def check_image(path: Path, maximum_bytes: int) -> int:
    if maximum_bytes <= 0:
        raise ValueError("maximum size must be positive")
    try:
        size = path.stat().st_size
    except OSError as exc:
        raise ValueError(f"cannot inspect {path}: {exc}") from exc
    if not path.is_file():
        raise ValueError(f"image is not a regular file: {path}")
    if size > maximum_bytes:
        raise ValueError(f"image is {size} bytes; physical limit is {maximum_bytes} bytes")
    return size


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--maximum-bytes", type=int, required=True)
    args = parser.parse_args(argv)
    try:
        size = check_image(args.image, args.maximum_bytes)
    except ValueError as exc:
        print(f"firmware size check failed: {exc}", file=sys.stderr)
        return 2
    percentage = 100.0 * size / args.maximum_bytes
    print(
        f"firmware size check passed: image={size} bytes, "
        f"limit={args.maximum_bytes} bytes, utilization={percentage:.1f}%"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
