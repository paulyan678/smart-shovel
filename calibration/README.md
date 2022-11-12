# Calibration evidence and procedure

This directory preserves the prototype's original calibration and debug captures
without changing their bytes. The files under `data/raw/` are evidence, not a
complete physical calibration: do not edit them in place, append rows, or treat
their filenames as labels for known masses.

Repository [`.gitattributes`](../.gitattributes) marks the imported CSV and
notebook evidence `-text`, preventing checkout-time line-ending conversion from
changing the documented bytes on another platform.

## Provenance and schemas

All six raw data files were first committed in Git commit
`d506f3e19ea6161c1f1a0964e28256272fa6c02d` on
`2022-11-11T19:06:04-05:00`. The repository records no device identifier,
sample interval, applied mass, tare state, supply/reference-voltage measurement,
or test procedure for these captures.

| File | Numeric samples | Schema or issue | SHA-256 |
| --- | ---: | --- | --- |
| `data/raw/calib1.csv` | 618 | `voltage,ax,ay,az,gx,gy,gz` | `13a7f5e0f5fc300f62521f9b5ce161a70229fc79c9a331a25cd4cbc6148e12a7` |
| `data/raw/calib2.csv` | 954 | Same calibration schema | `3285cdcbfa316e931b9aa0d5ae742a530c32420b9d8a9a8fca3699e8ba24dd5b` |
| `data/raw/calib3.csv` | 377 | Same calibration schema | `e3cfeeff9249d884390303f2376c8d1a1c9f2d137473a0a16a781bab134b54a4` |
| `data/raw/calib4.csv` | 1,415 | Same calibration schema | `13320d5257dc2431ed82a7afb712e217dadc121be3e918549b524374f56c28e3` |
| `data/raw/known.csv` | 164 usable | `gram,ax,ay,az,gx,gy,gz`; also contains a malformed singleton `gz` row | `c01c425aaf6589410c3181ba0b8f7854ccabaafa9c830e72d79d633462ea0c9a` |
| `data/raw/test.csv` | 859 | `time,weight,latitude,longitude,alt` | `cb4b43204a3b4f70e132f62494c6da21def6d2ebcdcc5a398a6ef1b50c7eb1e9` |

Each CSV contains one blank record after its header. The analysis tool reports
that record separately and rejects any nonblank row that is not finite numeric
data in the exact calibration schema.

### Overlapping captures

The four calibration files are overlapping snapshots of one 1,415-row stream,
not four independent trials:

- `calib2.csv` is exactly `calib4.csv` data rows 5 through 958.
- `calib1.csv` is exactly `calib4.csv` data rows 22 through 639.
- `calib3.csv` is exactly `calib4.csv` data rows 824 through 1200.

Never concatenate these files. Doing so would duplicate observations and produce
misleading confidence intervals. Analyze one explicitly selected capture at a
time.

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

The conventional interval assumes independent residuals. These sequential
samples have residual lag-1 correlation of about `0.77215`; a Bartlett-kernel
Newey-West sensitivity calculation with five lags gives slope standard error
about `1.55355` and an asymptotic 95% interval of approximately
`[-77.7539, -71.6640]`. Neither interval describes unit-to-unit, environmental,
or long-term uncertainty.

For a tare-relative correction, with fitted slope `s`, use a consistent signal
unit on both sides:

```text
corrected_delta = (measured - tare) - s * (az - tare_az)
```

With the fitted negative slope this adds about `74.7089` signal units for each
one-unit increase in `az` relative to tare. The fitted intercept is specific to
this capture and should not replace runtime unloaded tare.

### Unit and mass-calibration limitations

The CSV header says `voltage`, while adjacent legacy firmware describes its
output as millivolts. However, the capture contains negative values and does not
record raw ADC counts, reference voltage, or the transformation that produced
them. Treat the fitted coefficient as signal-units per `az` unit until the
millivolt provenance is confirmed on hardware.

`known.csv` and `test.csv` contain already-computed `gram` or `weight` values.
Neither includes paired raw signal and traceable applied mass. They therefore
cannot validate the legacy `-15 grams/mV` constant or provide a replacement.
The default firmware retains that finite factor only to exercise the complete
event pipeline and writes `mass_calibration_status=provisional` beside every
derived mass. A finite provisional value is not a physically valid gram claim
and must be excluded from field decisions. Set the status to `verified` only
after the ground-truth procedure below; a zero or non-finite configured factor
is treated as unavailable.

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
uncertainty. It intentionally rejects `known.csv`, `test.csv`, reordered
headers, nonnumeric values, non-finite values, and zero-variance inputs.

Run its regression tests with:

```sh
python3 -m unittest discover -s tools/tests -v
```

## Ground-truth hardware calibration

1. Assign a device ID and run ID. Record firmware revision, load-cell/amplifier
   hardware, excitation supply, requested ADC resolution, and measured ADC
   reference voltage.
2. Log timestamped raw ADC counts, averaged counts and spread, converted signal,
   all three acceleration axes, gyro/stability state, tare ID, and explicitly
   labeled applied mass. Keep raw captures immutable.
3. With the shovel confirmed empty, collect repeated stable samples at multiple
   static orientations in randomized order. Reject motion using independently
   justified acceleration-norm and gyro limits; do not derive those limits from
   this unlabeled capture.
4. Repeat the orientation sequence at multiple known loads. Fit the orientation
   term on training runs and verify residual bias on held-out runs. A portable
   slope must remain stable while the per-run intercept/tare is allowed to vary.
5. After orientation correction, apply traceable masses spanning the intended
   range, including zero. Repeat loading and unloading to measure linearity,
   hysteresis, noise, saturation, creep, and zero drift. Fit grams from the
   corrected signal and retain coefficient uncertainty and residual error.
6. Repeat across devices, power conditions, and relevant temperatures. Validate
   on held-out devices/runs before enabling gram-valued event thresholds.
7. Automatic zero maintenance may update tare only when independent evidence
   shows the shovel is stable and unloaded. It must never use a loaded sample as
   zero. The maintained production adapter has no such independent signal and
   therefore disables runtime auto-zero even when the mass calibration is
   verified; only the bounded fail-closed algorithm is host-tested.
