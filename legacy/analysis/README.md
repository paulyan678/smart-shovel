# Archived analysis notebook

`vis.ipynb` is the original prototype notebook, preserved byte-for-byte for
historical evidence. It is stale and is not runnable from top to bottom:

Its SHA-256 is
`aeb08c3f6acb43e3bee0a509a2dda6f94daf0766a552d400ace4d361a655cb98`.

- its source currently loads `test.csv`, whose schema is
  `time,weight,latitude,longitude,alt`;
- earlier cells then request `gram`, `voltage`, and `az`;
- later cells return to `weight` and `longitude`;
- cached outputs and nonsequential execution counts reflect prior interactive
  state rather than the current source order.

The cached slope is reproducible from `calibration/data/raw/calib1.csv`, but the
notebook does not identify that input in its current source. Use
[`tools/analyze_calibration.py`](../../tools/analyze_calibration.py) and the
[calibration documentation](../../calibration/README.md) for the maintained,
validated workflow. Do not treat cached notebook output as a fresh execution.
