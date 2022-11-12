# Legacy material

This directory preserves prototype history without placing it on the production
build path. Files here are nonproduction, unvalidated, and may be incomplete or
fail to compile against current dependencies.

- `firmware/2022-prototype/` contains the original single-board sketches and
  helper modules.
- `experiments/` contains audio, Wi-Fi, and master/slave explorations that are
  outside the MVP.
- `analysis/` contains the stale interactive notebook; use the maintained
  calibration CLI instead.

The accepted production architecture uses one Arduino Nano RP2040 Connect.
Nothing under `legacy/` is compiled by `platformio.ini`, and moving a legacy
feature into production requires a documented requirement, a maintained design,
and tests.

Hard-coded Wi-Fi credentials were removed from the archived `wlab3` experiment.
They must be considered exposed and rotated. For local historical investigation,
copy `experiments/wlab3/secrets.example.hpp` to `secrets.hpp` and supply only
local disposable values. `secrets.hpp` is ignored repository-wide and must never
be committed.
