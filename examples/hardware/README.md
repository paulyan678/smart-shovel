# Hardware diagnostics

These focused sketches isolate individual peripherals for fast bench diagnosis.
They are kept outside the PlatformIO application build so each sensor path can be
exercised independently.

- `load-cell/load-cell.ino` reports averaged A0 ADC counts and nominal voltage.
- `sd-card/` provides focused SD inspection and logging utilities.

Record the board revision, wiring, supply/reference voltage, library versions,
and observed serial output with each diagnostic run. This creates a compact,
repeatable evidence trail for peripheral integration work.
