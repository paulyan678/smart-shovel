# Hardware validation procedure

This is an execution protocol, not a completed hardware report. The firmware's
mass factor remains provisional. Preserve the existing raw calibration captures.

## Capture identity and calibration

For each device, record hardware revision, firmware commit, device ID, power
source, ADC reference measurement, sensor units and capture times. Record the
reference mass source and uncertainty. Keep raw ADC/IMU measurements immutable;
derive tare-relative corrected mV with the exact documented coefficient and
units. An orientation fit with an unknown source unit must not silently become a
verified physical-unit calibration.

Choose application error limits and complete-run train/test assignments before
measurement. At minimum collect two training runs and a separate test run, at
zero plus two nonzero masses spanning the intended operating range, and at two
or more orientations. Randomize repeated loading/unloading and record stable
windows without dropping adverse outcomes. Use the
[known-mass evaluator](../calibration/README.md#held-out-known-mass-evaluation)
and inspect residuals by run and orientation. Repeat under relevant power and
temperature conditions; check creep, saturation, hysteresis and unloaded drift.

The default -15 g/mV factor is not a measured conclusion. Keep
`SMART_SHOVEL_MASS_CALIBRATION_VERIFIED=0` until physical provenance and error
limits are reviewed. If a fitted intercept is significant, investigate tare and
correction assumptions; do not silently add an intercept to a tare-relative
firmware model that does not implement one.

## End-to-end failure scenarios

| Scenario | Evidence to retain | Acceptance boundary |
| --- | --- | --- |
| Cold boot with unloaded shovel | Raw startup samples, tare identity, first event | No event before stable tare; expected configuration/device identity in output |
| Reset during a candidate event | Serial trace and resulting CSV | Candidate state does not leak into the new boot; boot identity changes |
| SD unavailable, then restored | Event IDs before/after retry, status transitions | Bounded queue/retry behavior matches configured policy; retries preserve identity |
| GNSS absent or stale | NMEA input timing, timeout and event row | Event completes within the configured bound; missing/stale evidence is labeled |
| Valid GNSS during event window | Input sentence and output timestamp/location | Output values trace to received evidence; no fabricated position or time |
| Repeated load without release | Sample trace and event count | One qualified event until the release/re-arm condition is met |

Do not remove storage during active writes without protecting the original
data; use a disposable card and retain an independent serial capture. Confirm
behavior at the actual hardware watchdog and storage boundaries, since host
mocks cannot establish peripheral timing.

## Publish an actual validation receipt

Archive source commit, build command/toolchain, firmware hash, capture manifest,
raw-data hashes, predefined limits, evaluator output, scenario observations and
known failures. Label a camera/device recording as a hardware run only when it
shows the device and the resulting records. The browser demo remains a simulation.
Do not infer measured accuracy from host test counts or a successful firmware build.
