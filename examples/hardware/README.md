# Hardware diagnostics

These sketches isolate individual peripherals for bench diagnosis. They are not
the production application, are excluded from PlatformIO's firmware build, and
require the operator to verify wiring and electrical limits before upload.

- `load-cell/load-cell.ino` reports averaged A0 ADC counts and nominal voltage.
  It does not perform tare, orientation compensation, or grams calibration.
- `sd-card/` preserves Arduino SD inspection/logger examples. Their example pin
  comments are not authoritative wiring for the Smart Shovel.

Record the board revision, wiring, supply/reference voltage, library versions,
and observed serial output when using a diagnostic. Passing a diagnostic on one
bench setup does not validate load capacity, measurement accuracy, weather
resistance, durability, or field behavior. Promote no example into production
without adapting it to the maintained interfaces and adding appropriate tests.
