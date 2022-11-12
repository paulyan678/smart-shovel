# Implementation plan and acceptance gates

This plan records the accepted scope and the evidence required to close it. A
software item is `[PASS]` only after its named inspection or command succeeds.
Physical procedures are `[BLOCKED: physical hardware unavailable]`, not passed
by host tests or a target compile.

## Accepted architecture and scope

The production MVP uses one Arduino Nano RP2040 Connect to read the analog load
cell amplifier and onboard LSM6DSOX IMU, acquire GNSS location/time, and write
event CSV records to microSD. This matches the only integrated historical data
flow. The pitch BOM's unexplained second-Nano quantity does not justify putting
the unfinished master/slave sketches on the production path.

Production scope is load sensing, orientation compensation, an operator-gated
stable startup tare, collection-event detection, GNSS validity/freshness, CSV
logging, and serial/status indication. Runtime auto-zero is deliberately
disabled because the current hardware cannot independently prove that the shovel
is unloaded. The bounded auto-zero core remains host-tested for a future explicit
unloaded input. Audio, Wi-Fi serving, dual-board communication, computer vision,
solar charging, and alternate sensors remain deferred.

The production status output is a configurable, active-high external LED on D2
through a current-limit resistor. It uses pulse codes rather than the pitch's
onboard RGB colors. This is a reversible, compile-tested F-UI deviation whose
wiring, visibility, and operator interpretation remain hardware-blocked.

Pure logic belongs in `lib/smart_shovel_core` and is tested on the native host.
Arduino-specific peripheral access belongs in the single application under
`src/`. Hardware examples and all historical sketches remain outside that build.
Schema v2 identifies a record by device ID, random 16-hex-digit boot session, and
per-session sequence; an assigned sequence is retained across SD write retries.

## Work plan

| Work item | Acceptance condition | Status |
| --- | --- | --- |
| Repository structure | One production `src/` application; legacy and examples excluded | `[PASS: tree and PlatformIO paths inspected]` |
| Core logic | Calibration/ADC plausibility, filtering, event cooldown/re-arm, component-wise GNSS evidence, schema-v2 identity, interruption parsing, and degraded states have deterministic tests | `[PASS: 26/26 native Unity tests]` |
| Firmware adapter | ADC/IMU recovery, stable-only aligned windows, GNSS merge/wait, retry-stable synchronous SD writes, nonblocking USB diagnostics, watchdog, external LED, and configuration integrate and compile | `[PASS: Nano RP2040 Connect compile; physical behavior blocked below]` |
| Calibration/export tooling | Raw inputs remain byte-stable, the fit is reproducible, firmware image size is independently checked, and schema-v2 exports are validated before aggregation | `[PASS: calibration smoke test and 10/10 Python tests]` |
| Documentation | Setup, wiring, calibration, schema v2, behavior, limitations, and historical claims are traceable and links resolve | `[PASS: final content audit and 47-link local target check]` |
| Security | No tracked credentials; local secret files are ignored; exposed credentials are documented for rotation | `[PASS: staged-tree and targeted credential scans]` |
| Publication | Intentional diff reviewed, dated commit verified, and normal push to `origin/main` succeeds | `[PENDING: publication has not occurred]` |

## Executable acceptance gates

| Gate | Exact command | Status |
| --- | --- | --- |
| Install pinned tools | `make setup` | `[PASS]` |
| Python tooling tests | `make test-python` | `[PASS: 10/10 tests]` |
| Calibration smoke test | `make calibration-check` | `[PASS: documented calib1 fit/hash reproduced]` |
| Credential/current-tree scan | `make secret-check` | `[PASS: staged tree]` |
| C++ formatting | `make format-check` | `[PASS]` |
| Practical static analysis | `make static-check` | `[PASS: cppcheck covered 15 maintained source files]` |
| Native Unity tests | `make test-native` | `[PASS: 26/26 tests]` |
| Nano RP2040 Connect compile and image limit | `make build-firmware` | `[PASS: RAM 45,288/270,336 bytes (16.8%); PlatformIO's Mbed estimate was 4,410 bytes, while the independent generated image was 101,732/2,097,152 bytes (4.9%)]` |
| Aggregate software gate | `make verify` | `[PASS]` |
| Whitespace/error check | `git diff --check` | `[PASS]` |

## Hardware validation still required

| Procedure | Evidence required | Status |
| --- | --- | --- |
| ADC/reference characterization | Raw counts, measured reference, noise, input range, and saturation across supply conditions | `[BLOCKED: physical hardware unavailable]` |
| Ground-truth mass calibration | Traceable masses, loading/unloading repeats, residuals, hysteresis, drift, and coefficient uncertainty | `[BLOCKED: physical hardware unavailable]` |
| Orientation compensation | Held-out orientations, loads, runs, and devices with recorded stability/motion criteria | `[BLOCKED: physical hardware unavailable]` |
| Automatic tare safety | Add independent unloaded evidence, then demonstrate that loaded samples can never update tare | `[BLOCKED: no independent unloaded input and physical hardware unavailable]` |
| GNSS behavior | Cold/warm acquisition, UART silence, component freshness, timeout, and no-fix logging on the installed module | `[BLOCKED: physical hardware unavailable]` |
| SD interruption/restart | Real-card absence/full/write faults, power-loss tails, boot-session identity, retry-stable sequence, and recovery | `[BLOCKED: physical hardware unavailable]` |
| Integrated event collection | Representative shovel motions produce intended events without duplicate/noise events | `[BLOCKED: physical hardware unavailable]` |
| Status indication | D2 wiring/resistor/polarity, all pulse codes, daylight visibility, and operator recovery comprehension | `[BLOCKED: physical hardware unavailable]` |
| Physical/environmental claims | Separate procedures for capacity, drop, enclosure/rain, temperature, vibration, ergonomics, power, and field acceptance | `[BLOCKED: physical hardware unavailable]` |

## Release implications

- The default `UNCONFIGURED` device ID disables SD logging and drops detected
  events; a non-personal fleet-unique ID is mandatory for a supervised unit.
- Finite masses derived with the preserved `-15 g/mV` factor are explicitly
  `provisional`, not calibrated field measurements.
- A queue overflow means data was lost. Its three-pulse warning remains latched
  until reboot even if storage later recovers and the queue drains.
- SD initialization retries back off from 30 to 300 seconds; IMU recovery is
  attempted every 5 seconds. These timings are compile/host evidence only.
- USB diagnostics use a fixed 512-byte queue and the pinned target's nonblocking
  CDC send call. When the queue fills, bytes are dropped and counted rather than
  blocking acquisition; target-specific behavior still needs hardware testing.
- Every mutating SD open uses `FILE_WRITE | O_SYNC`, so reported write success
  includes the data/FAT/directory sync exposed by pinned SD 1.3.0. The SD calls
  remain synchronous: they may delay 50 ms sampling and GNSS polling, and the
  8 s watchdog can reset a dependency stall at the cost of the RAM queue.
- No hardware, safety, environmental, ergonomic, accuracy, or field-acceptance
  claim is released by the completed software gates.
