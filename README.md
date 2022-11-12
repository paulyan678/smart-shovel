# Smart Shovel

Smart Shovel is an embedded sensing platform that turns a collection motion into
a structured waste-measurement event. An Arduino Nano RP2040 Connect combines
load sensing, inertial orientation correction, GNSS evidence, and microSD
logging in one deterministic firmware pipeline.

The project demonstrates production-minded embedded engineering: clear hardware
boundaries, allocation-free domain logic, testable state machines, explicit data
quality, failure recovery, reproducible calibration analysis, and a pinned CI
toolchain.

![Smart Shovel prototype laid horizontally, showing the shovel head, wooden shaft, T-handle, and electronics.](docs/assets/smart-shovel-prototype-overview.webp)

## Engineering highlights

- **Deterministic event detection** — moving-average filtering, motion gating,
  stability windows, debounce, hysteresis, release detection, and cooldown.
- **Sensor fusion** — tare-relative load measurement corrected with the onboard
  LSM6DSOX z-acceleration signal.
- **Evidence-aware GNSS** — independently retains the best valid location, UTC,
  altitude, and satellite data during each bounded event window.
- **Durable data identity** — schema version, device ID, random boot session,
  and retry-stable event sequence make records aggregation-ready.
- **Resilient peripherals** — bounded retries, exponential SD backoff, a
  four-event queue, degraded operating modes, and hardware-watchdog coverage.
- **Portable core logic** — Arduino-independent, allocation-free C++ domain
  code runs unchanged in deterministic native Unity tests.
- **Reproducible analytics** — calibration source data, hashes, OLS and HAC
  uncertainty analysis, and CSV validation are committed alongside the code.
- **Automated quality gates** — Python tests, native firmware tests, static
  analysis, formatting, secret scanning, documentation checks, and a pinned
  Nano RP2040 Connect build run in CI.

## Interactive demonstration

[Open the Smart Shovel demo](docs/demo/index.html) to step through boot, tare,
load sensing, event qualification, GNSS capture, SD persistence, degraded
states, and release/re-arm behavior.

The web demo uses deterministic sample data. Serve it locally for the complete
interactive experience:

```sh
python3 -m http.server 8000 --directory docs
# open http://localhost:8000/demo/
```

The [visual engineering guide](docs/visual-guide.md) provides the full photo,
architecture, state, indicator, calibration, and data-flow walkthrough.

## System architecture

![Smart Shovel architecture showing the load-cell analog path, onboard IMU, Nano RP2040 Connect, GNSS, event queue, microSD logger, status LED, and host validator.](docs/assets/system-architecture.svg)

One Nano RP2040 Connect owns the complete sensing and logging path:

| Function | Interface | Role |
| --- | --- | --- |
| Amplified load signal | `A0` | High-resolution tare-relative measurement |
| LSM6DSOX IMU | Onboard | Motion qualification and orientation correction |
| L76B-compatible GNSS | `Serial1`, 9600 baud | UTC, location, altitude, and satellite evidence |
| microSD | Hardware SPI, CS `D10` | Synchronized schema-v2 CSV persistence |
| Status LED | External active-high LED on `D2` | Compact operational-state display |
| Serial diagnostics | USB serial, 115200 baud | State, event, retry, and recovery tracing |

## Collection pipeline

![Smart Shovel collection cycle: stable empty-shovel tare, filtered and orientation-corrected load measurement, event qualification, bounded GNSS evidence, schema-v2 SD record, host validation, and aggregation.](docs/assets/collection-cycle.svg)

1. Initialize sensors, GNSS, storage, diagnostics, and the watchdog.
2. Build a 32-sample stable tare for both load signal and orientation.
3. Sample every 50 ms and maintain an eight-sample aligned filter window.
4. Correct the load signal relative to the measured z-acceleration delta.
5. Qualify an event across mass, motion, stability, debounce, and cooldown gates.
6. Merge the strongest GNSS components observed during the five-second window.
7. Assign a stable `(device_id, boot_session_id, event_sequence)` identity.
8. Append and close one schema-v2 row with synchronized SD semantics.
9. Require release before re-arming the detector for the next collection.

Invalid sensor samples clear aligned filters and candidate continuity. GNSS and
storage operate independently, so sensing continues through recoverable
peripheral states. Pending records retain their original identity across SD
retries, which makes downstream duplicate handling deterministic.

## Event model

Defaults are centralized in
[`include/smart_shovel/config.hpp`](include/smart_shovel/config.hpp):

| Parameter | Default |
| --- | ---: |
| Sample interval | 50 ms |
| Moving-average window | 8 samples |
| Trigger / release | 150 g / 50 g |
| Trigger / release confirmation | 5 / 5 samples |
| Candidate delta ceiling | 75 g |
| Stable acceleration band | ±0.15 g around gravity |
| Stable gyro ceiling | 15 degrees/s |
| Post-release cooldown | 1 s |
| GNSS wait / freshness | 5 s / 5 s |
| Pending event capacity | 4 |
| SD retry backoff | 30–300 s |
| Hardware watchdog | 8 s |

The state machine and its boundary conditions are exercised by the native test
suite, including threshold crossings, motion resets, filter rebuilding,
wraparound-safe timing, GNSS selection, queue behavior, and retry identity.

## CSV data contract

`events.csv` uses an exact 19-field schema. Here is a representative deterministic
record:

```csv
schema_version,device_id,boot_session_id,event_sequence,event_uptime_ms,timestamp_utc,mass_g,mass_calibration_status,corrected_signal_mv,raw_adc,accel_z_g,latitude,longitude,altitude_m,gps_age_ms,satellites,gps_status,gps_wait_timed_out,system_health
2,SS-001,9F3A7C10B4D2E681,42,123456,2026-07-15T08:30:01Z,412.750,provisional,-27.517,30214,0.982100,5.6037000,-0.1870000,24.30,220,9,valid,false,healthy
```

The schema makes measurement quality explicit through
`mass_calibration_status`, `gps_status`, `gps_wait_timed_out`, and
`system_health`. Optional unavailable values remain empty rather than being
silently reused. Component-level GNSS merging prevents a partial later update
from erasing stronger evidence captured earlier in the event window.

Each boot generates a random 64-bit session rendered as 16 hexadecimal
characters. Once an event receives a sequence number, that identity remains
stable across storage retries. The logger validates the exact header, repairs a
missing tail newline before append, and uses synchronized event-boundary writes.

Validate an exported file with:

```sh
make validate-events EVENTS=/path/to/events.csv
```

The validator checks the schema, types, ranges, enum values, identity conflicts,
and retry-equivalent duplicate rows.

## Calibration analysis

The orientation model is reproducible directly from the preserved calibration
dataset:

```sh
python tools/analyze_calibration.py \
  --input calibration/data/raw/calib1.csv \
  --hac-lags 5
```

For `calib1.csv`, the analysis reports:

```text
voltage = -74.7089168 × az + 25.2884374
R² = 0.9151767
OLS 95% slope interval = [-76.5086, -72.9093]
HAC(5) approximate interval = [-77.7538, -71.6640]
```

Firmware uses the fitted value as the configured orientation coefficient in its
millivolt-domain signal pipeline, while startup tare cancels the intercept:

```text
corrected_delta_mv = (measured_mv - tare_mv)
                   - slope × (az - tare_az)
```

The record-level `mass_calibration_status` supports `unavailable`,
`provisional`, and `verified`, allowing datasets to carry their calibration
state without changing the schema. See
[`calibration/README.md`](calibration/README.md) for the complete data catalog,
hashes, schemas, and analysis notes.

## Status indication

| LED pattern | State | Firmware behavior |
| --- | --- | --- |
| Defined double pulse | Booting | Timing definition selected during setup |
| Slow blink | Calibrating | Builds the stable startup tare |
| Solid on | Ready | Sensors, GNSS, and storage available |
| Fast blink | Event pending | Collects bounded GNSS evidence |
| One short pulse, long pause | GNSS degraded | Preserves the best available event evidence |
| Three short pulses, then pause | Storage degraded | Queues events and retries storage |
| Five very short pulses, then pause | Sensor fault | Suppresses invalid readings and retries the IMU |

Serial diagnostics complement the LED with exact startup, state, event, write,
retry, queue, and recovery messages.

## Repository layout

```text
src/                         production Arduino application and adapters
include/smart_shovel/        firmware and device configuration
lib/smart_shovel_core/       allocation-free portable domain logic
test/test_core/              deterministic PlatformIO Unity host tests
calibration/data/raw/        source calibration datasets
tools/analyze_calibration.py reproducible calibration CLI
tools/validate_events.py     schema-v2 CSV validator
examples/hardware/           focused sensor and peripheral diagnostics
legacy/                      archived design explorations
docs/assets/                 optimized photographs and SVG explanations
docs/demo/                   framework-free interactive collection demo
docs/                        engineering guide, traceability, and test gates
.github/workflows/ci.yml     complete automated verification pipeline
```

[`src/main.cpp`](src/main.cpp) is the single firmware entry point. Arduino calls
and the runtime event queue stay in the application adapter; the stateful
measurement, event detection, GNSS selection, and schema logic lives in the
portable core.

## Build and verify

Requirements:

- Python 3.11 or later
- A C++17 host compiler
- `clang-format` and `cppcheck`
- A data-capable USB cable for upload and serial monitoring

Create an isolated environment and install the pinned toolchain:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements-dev.txt
```

Install the system analysis tools with `brew install clang-format cppcheck` on
macOS or `sudo apt-get install clang-format cppcheck` on Ubuntu.

Run the full CI-equivalent gate:

```sh
make verify
```

Or invoke individual stages:

```sh
make test-python
make calibration-check
make docs-check
make secret-check
make format-check
make static-check
make test-native
make build-firmware
```

PlatformIO Core, the board platforms, and Arduino libraries are pinned in
`requirements-dev.txt` and `platformio.ini` for repeatable builds.

## Configure, upload, and monitor

Create the local device configuration from the tracked example:

```sh
cp include/smart_shovel/device_config.example.hpp \
  include/smart_shovel/device_config.hpp
```

Set a fleet-unique device ID of at most 24 characters using letters, digits,
`.`, `_`, or `-`, then build and upload:

```sh
pio run -e nanorp2040connect
pio run -e nanorp2040connect --target upload
pio device monitor --baud 115200
```

Start the firmware with the shovel head empty and motionless while it builds the
startup tare. The LED changes from its calibration blink to the current
operating state, and serial diagnostics report `tare=ready`.

## Documentation

- [Visual engineering guide](docs/visual-guide.md)
- [Interactive collection demo](docs/demo/index.html)
- [Requirements and implementation trace](docs/project-requirements.md)
- [Implementation and verification plan](docs/implementation-plan.md)
- [Calibration data and analysis](calibration/README.md)
- [Hardware diagnostics](examples/hardware/README.md)

## Contributing and license

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for architecture conventions and the
verification workflow. Smart Shovel is available under the
[MIT License](LICENSE).
