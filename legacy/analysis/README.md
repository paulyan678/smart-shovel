# Original analysis notebook

`vis.ipynb` is the original prototype notebook, preserved byte-for-byte for
traceability. Its SHA-256 is
`aeb08c3f6acb43e3bee0a509a2dda6f94daf0766a552d400ace4d361a655cb98`.

The notebook captures the project's exploratory analysis sequence:

- its source currently loads `test.csv`, whose schema is
  `time,weight,latitude,longitude,alt`;
- earlier cells then request `gram`, `voltage`, and `az`;
- later cells return to `weight` and `longitude`;
- cached outputs and nonsequential execution counts preserve the original
  interactive state.

The cached slope is reproducible from `calibration/data/raw/calib1.csv`, but the
maintained workflow is now the deterministic
[`tools/analyze_calibration.py`](../../tools/analyze_calibration.py) and the
[calibration documentation](../../calibration/README.md).
