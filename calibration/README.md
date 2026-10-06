# Calibration analysis and procedure

This directory preserves the prototype's original calibration and debug captures
byte-for-byte and turns them into a reproducible engineering workflow. The files
under `data/raw/` remain immutable inputs; analysis is performed by the maintained
command-line tooling in `tools/`.

Repository [`.gitattributes`](../.gitattributes) marks the imported CSV and
notebook evidence `-text`, preventing checkout-time line-ending conversion from
changing the documented bytes on another platform.

## Provenance and schemas

All six raw data files were first committed in Git commit
`d506f3e19ea6161c1f1a0964e28256272fa6c02d` on
`2022-11-11T19:06:04-05:00`. Content hashes make every analysis result traceable
to its source capture.

| File | Numeric samples | Schema and notes | SHA-256 |
| --- | ---: | --- | --- |
| `data/raw/calib1.csv` | 618 | `voltage,ax,ay,az,gx,gy,gz` | `13a7f5e0f5fc300f62521f9b5ce161a70229fc79c9a331a25cd4cbc6148e12a7` |
| `data/raw/calib2.csv` | 954 | Same calibration schema | `3285cdcbfa316e931b9aa0d5ae742a530c32420b9d8a9a8fca3699e8ba24dd5b` |
| `data/raw/calib3.csv` | 377 | Same calibration schema | `e3cfeeff9249d884390303f2376c8d1a1c9f2d137473a0a16a781bab134b54a4` |
| `data/raw/calib4.csv` | 1,415 | Same calibration schema | `13320d5257dc2431ed82a7afb712e217dadc121be3e918549b524374f56c28e3` |
| `data/raw/known.csv` | 164 usable | `gram,ax,ay,az,gx,gy,gz`; also contains a malformed singleton `gz` row | `c01c425aaf6589410c3181ba0b8f7854ccabaafa9c830e72d79d633462ea0c9a` |
| `data/raw/test.csv` | 859 | `time,weight,latitude,longitude,alt` | `cb4b43204a3b4f70e132f62494c6da21def6d2ebcdcc5a398a6ef1b50c7eb1e9` |

Each CSV contains one blank record after its header. The analysis tool reports
that record separately and enforces finite numeric data in the exact calibration
schema.

### Overlapping captures

The four calibration files are overlapping snapshots of one 1,415-row stream:

- `calib2.csv` is exactly `calib4.csv` data rows 5 through 958.
- `calib1.csv` is exactly `calib4.csv` data rows 22 through 639.
- `calib3.csv` is exactly `calib4.csv` data rows 824 through 1200.

The analysis workflow selects one capture at a time so overlapping observations
are counted once.

## Supported orientation relationship

The preserved notebook's cached result is reproduced by `calib1.csv`. A fresh
double-precision ordinary least-squares fit of `voltage` on `az` is:

```text
voltage = -74.7089168184 * az + 25.2884373659
n = 618
R^2 = 0.9151766665
slope standard error = 0.9164046458
slope 95% confidence interval = [-76.5085729, -72.9092607]
intercept standard error = 0.9606132755
intercept 95% confidence interval = [23.4019634, 27.1749114]
```

The sequential samples have residual lag-1 correlation of about `0.77215`, so
the analyzer also reports a Bartlett-kernel Newey-West sensitivity calculation.
With five lags, it gives a slope standard error of about `1.55355` and an
asymptotic 95% interval of approximately `[-77.7539, -71.6640]`.

For a tare-relative correction, with fitted slope `s`, use a consistent signal
unit on both sides:

```text
corrected_delta = (measured - tare) - s * (az - tare_az)
```

With the fitted negative slope this adds about `74.7089` signal units for each
one-unit increase in `az` relative to tare. Runtime uses an unloaded tare in
place of the capture-specific intercept.

### Signal and mass-calibration model

The CSV names its signal column `voltage`; the analyzer deliberately expresses
the fitted coefficient in source signal units per `az` unit so the model stays
faithful to the captured data.

`known.csv` and `test.csv` contain already-computed `gram` or `weight` values.
The runtime mass transform is configurable and carries an explicit
`mass_calibration_status` value of `unavailable`, `provisional`, or `verified` in
every event row. The default `provisional` factor exercises the complete sensing,
event, storage, and telemetry pipeline while keeping calibration maturity visible
in the data contract.

## Reproducible analysis

The analyzer uses only the Python standard library, reads one explicit file, and
never writes to its input:

```sh
python3 tools/analyze_calibration.py \
  --input calibration/data/raw/calib1.csv

python3 tools/analyze_calibration.py \
  --input calibration/data/raw/calib1.csv \
  --hac-lags 5 \
  --format json
```

It reports the source SHA-256, record counts, OLS coefficients and uncertainty,
R-squared, RMSE, residual diagnostics, and optional Newey-West slope
uncertainty. It rejects `known.csv`, `test.csv`, reordered
headers, nonnumeric values, non-finite values, and zero-variance inputs.

Run its regression tests with:

```sh
python3 -m unittest discover -s tools/tests -v
```

## Ground-truth calibration workflow

1. Assign a device ID and run ID. Record firmware revision, load-cell/amplifier
   hardware, excitation supply, requested ADC resolution, and measured ADC
   reference voltage.
2. Log timestamped raw ADC counts, averaged counts and spread, converted signal,
   all three acceleration axes, gyro/stability state, tare ID, and explicitly
   labeled applied mass. Keep raw captures immutable.
3. With the shovel confirmed empty, collect repeated stable samples at multiple
   static orientations in randomized order. Gate motion with documented
   acceleration-norm and gyro thresholds.
4. Repeat the orientation sequence at multiple known loads. Fit the orientation
   term on training runs and verify residual bias on held-out runs. A portable
   slope must remain stable while the per-run intercept/tare is allowed to vary.
5. After orientation correction, apply traceable masses spanning the intended
   range, including zero. Repeat loading and unloading to measure linearity,
   hysteresis, noise, saturation, creep, and zero drift. Fit grams from the
   corrected signal and retain coefficient uncertainty and residual error.
6. Repeat across devices, power conditions, and relevant temperatures. Validate
   on held-out devices/runs before enabling gram-valued event thresholds.
7. Use explicit unloaded tare in the maintained firmware. A bounded automatic
   zero-maintenance algorithm is also host-tested as an extension point for a
   future adapter with an independent stable-and-unloaded signal.

## Held-out known-mass evaluation

Copy [known-mass-template.csv](known-mass-template.csv) for a new device. The
template deliberately contains no sample measurements. Log actual stable-window
observations after applying the documented tare/orientation correction in mV.
Keep original ADC/IMU traces and a capture manifest alongside the derived CSV:
device/hardware identifiers, firmware revision, ADC reference and units, tare
and correction coefficients, acquisition times, traceable mass identities, and
source-file hashes. Historical `known.csv` contains computed grams, so it is not
a substitute for raw signal paired with independently known masses.

Before collecting test data, assign whole acquisition runs to `train` or `test`
and choose the maximum acceptable RMSE and absolute error for the intended use.
Use at least two training runs and one untouched test run, each split covering
zero and two or more nonzero masses at multiple orientations. Randomize the
loading/unloading sequence; do not split neighboring samples from one run across
training and test. Evaluate different devices separately.

```sh
python3 tools/evaluate_known_mass.py --input measurements.csv \
  --max-rmse-g 10 --max-error-g 25 > mass-evaluation.json
```

The limits above are an example, not a certified accuracy specification. The
tool fits slope and intercept using only training runs, then reports held-out
bias, RMSE, worst error, and per-run/per-orientation errors. It rejects missing
splits, run leakage, invalid values and insufficient load/orientation coverage.
Exit status is 0 within the supplied limits, 1 outside them, and 2 for invalid
input. The report includes the immutable input hash and never changes firmware
configuration or its calibration-status flag.

A passing report alone does not verify physical provenance, temperature/power
robustness, creep or saturation. Review the capture evidence and complete the
[hardware procedure](../docs/hardware-validation.md) before setting
`SMART_SHOVEL_MASS_CALIBRATION_VERIFIED`. The evaluator's analytical test data is
only a known-answer software fixture, not a reported hardware result.
