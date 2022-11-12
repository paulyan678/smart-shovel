# Smart Shovel technical portfolio specification

## Project overview

Smart Shovel is an embedded data-acquisition demonstrator built around an
Arduino Nano RP2040 Connect. It combines analog load sensing, inertial
measurement, GNSS, event detection, local persistence, and host-side validation
in one traceable system.

The project highlights five engineering strengths:

- allocation-free C++ domain logic separated from Arduino peripheral code;
- sensor processing with stable-window tare and orientation compensation;
- deterministic event/state machines with hysteresis and cooldown;
- resilient, versioned CSV persistence with retry-stable identities; and
- reproducible native tests, static analysis, target builds, and data tooling.

The maintained implementation is defined by [`../platformio.ini`](../platformio.ini),
configuration under [`../include/smart_shovel/`](../include/smart_shovel/), the
portable core under [`../lib/smart_shovel_core/`](../lib/smart_shovel_core/), and
the Arduino adapter in [`../src/`](../src/). The original prototype firmware and
design explorations remain available under [`../legacy/`](../legacy/) as project
history. Prototype photography and system diagrams are collected in
[`assets/`](assets/) and explained in [`visual-guide.md`](visual-guide.md).

## System architecture

One Nano RP2040 Connect coordinates every acquisition stage. This keeps sensor
timestamps, event identity, status, and storage behavior in a single execution
domain while avoiding an inter-controller protocol.

| Subsystem | Interface | Implemented role |
| --- | --- | --- |
| Load-cell amplifier | Analog `A0`, 16-bit requested ADC mode | Eight-read acquisition, rail plausibility, count-to-millivolt conversion, filtering, and load estimation |
| LSM6DSOX IMU | Onboard sensor via Arduino LSM6DSOX 1.1.2 | Motion qualification, stable-window tare, and z-axis orientation correction |
| GNSS receiver | `Serial1` at 9,600 baud via TinyGPSPlus 1.1.0 | Component-level location, UTC, altitude, age, and satellite evidence |
| microSD | Hardware SPI, chip select `D10`, SD 1.3.0 | Exact-header schema-v2 logging with synchronized append and recovery |
| Status output | Configurable active-high LED on `D2` | Seven pulse-coded operating states |
| Diagnostics | USB serial at 115,200 baud | Nonblocking startup, event, queue, storage, and recovery telemetry |
| Runtime supervision | RP2040 watchdog | Eight-second execution bound around the application loop |

The architecture is split into two layers:

1. [`lib/smart_shovel_core`](../lib/smart_shovel_core/) contains portable,
   allocation-free algorithms for calibration, filtering, event detection,
   GNSS selection, CSV formatting, health, sequencing, and pipeline control.
2. [`src`](../src/) binds those algorithms to ADC, IMU, UART, SPI, SD, LED,
   watchdog, and USB APIs on the target board.

This separation makes the system's domain behavior executable on a native host
while preserving a compact embedded adapter.

## Implemented capability matrix

| Capability | Engineering design | Repository implementation |
| --- | --- | --- |
| Sensor acquisition | Cooperative 50 ms sampling cadence with eight ADC reads per measurement | [`sensor_reader.cpp`](../src/sensor_reader.cpp) and [`config.hpp`](../include/smart_shovel/config.hpp) |
| Startup tare | 32-sample aligned ADC/IMU window accepted by signal-span and acceleration-span criteria | [`calibration.cpp`](../lib/smart_shovel_core/src/calibration.cpp) |
| Orientation correction | Tare-relative, same-domain compensation using z acceleration | [`calibration.hpp`](../lib/smart_shovel_core/include/smart_shovel/calibration.hpp) |
| Digital filtering | Fixed-size eight-sample moving window with deterministic reset/rebuild behavior | [`filter.hpp`](../lib/smart_shovel_core/include/smart_shovel/filter.hpp) |
| Motion qualification | Acceleration-magnitude and gyroscope gates isolate stable observations | [`sensor_reader.cpp`](../src/sensor_reader.cpp) |
| Collection-event detection | Trigger/release hysteresis, five-sample stability, latch, release re-arm, and one-second cooldown | [`event_detector.cpp`](../lib/smart_shovel_core/src/event_detector.cpp) |
| GNSS evidence selection | Independently retains the strongest valid location, date/time, altitude, and satellite components | [`gps.cpp`](../lib/smart_shovel_core/src/gps.cpp) and [`gps_receiver.cpp`](../src/gps_receiver.cpp) |
| Bounded event pipeline | Event states progress through GNSS acquisition, storage readiness, retry, and completion | [`pipeline.cpp`](../lib/smart_shovel_core/src/pipeline.cpp) |
| Event buffering | Four-entry fixed-capacity FIFO keeps memory use deterministic | [`main.cpp`](../src/main.cpp) |
| Durable identity | Device ID, random 64-bit boot session, and per-session event sequence remain stable across retries | [`boot_session.cpp`](../src/boot_session.cpp) and [`sequence_scanner.cpp`](../lib/smart_shovel_core/src/sequence_scanner.cpp) |
| SD persistence | Exact schema/header checks, interrupted-tail separation, synchronized append, close, and exponential retry | [`sd_logger.cpp`](../src/sd_logger.cpp) |
| Export validation | Streaming validator checks schema, types, ranges, enums, and duplicate identity consistency | [`validate_events.py`](../tools/validate_events.py) |
| Operator feedback | Pulse-coded LED modes plus queued USB diagnostics | [`status_led.cpp`](../src/status_led.cpp) and [`nonblocking_diagnostics.cpp`](../src/nonblocking_diagnostics.cpp) |
| Interactive explanation | Framework-free simulation of normal, GNSS, threshold, and storage paths | [`demo/`](demo/) |

## Acquisition and event algorithm

The runtime transforms continuous sensor readings into one durable record per
qualified collection action:

1. Initialize the board interfaces, watchdog, diagnostics, storage, IMU, GNSS,
   and status output.
2. Capture a stable 32-sample empty-head window to establish signal and
   acceleration tare values.
3. Every 50 ms, average eight ADC reads and sample the IMU.
4. Convert ADC counts into millivolts and apply the tare-relative orientation
   model.
5. Feed stable corrected measurements into an eight-sample filter.
6. Qualify a load after five stable samples above 150 g.
7. Latch the event until five stable samples below the 50 g release threshold,
   then apply a one-second cooldown before re-arming.
8. Retain the best GNSS components observed during a five-second evidence
   window.
9. Assign a `(device_id, boot_session_id, event_sequence)` identity once and
   format the exact 19-field schema-v2 row.
10. Append, synchronize, and close the event record; storage retries preserve
    the same event identity.

All intervals use wraparound-safe unsigned time arithmetic. Fixed-capacity
buffers and bounded CSV writers keep memory behavior deterministic.

## Signal-processing model

### ADC conversion

The configured ADC domain uses a nominal 3,300 mV reference and a 16-bit count
range:

```text
signal_mv = raw_adc × 3300 / 2^16
```

Eight conversions are averaged for each measurement. A configurable 64-count
margin supports rail-plausibility detection before the sample enters the event
pipeline.

### Stable startup tare

The tare window aligns load and motion samples. It accepts a window when the
signal span is at most 12 mV and the z-acceleration span is at most 0.05 g. The
result stores both `tare_mv` and `tare_az`, enabling correction relative to the
same physical pose.

### Orientation compensation

The maintained model reproduces the relationship in
[`calib1.csv`](../calibration/data/raw/calib1.csv):

```text
corrected_delta_mv = (measured_mv - tare_mv)
                   - slope × (az - tare_az)

slope = -74.7089168 source signal units/g
```

The reproducible analysis reports `R² = 0.9151767`, an OLS 95% slope interval of
`[-76.5086, -72.9093]`, and a HAC(5) approximate interval of
`[-77.7538, -71.6640]`. Run it with:

```sh
python tools/analyze_calibration.py \
  --input calibration/data/raw/calib1.csv \
  --hac-lags 5
```

The fitted value is the configured orientation coefficient in the firmware's
millivolt-domain pipeline. The grams-per-millivolt factor and its `unavailable`,
`provisional`, or `verified` record state are build-time configuration. This
keeps calibration semantics inside every exported row without changing the data
contract.

## GNSS evidence fusion

GNSS is modeled as independently updateable evidence rather than one monolithic
fix. During an event window, the selector preserves the best available value for
each component:

- latitude and longitude validated as a pair;
- UTC date and UTC time combined into an ISO 8601 timestamp;
- altitude;
- satellite count; and
- component age and overall status.

A later partial receiver update cannot erase a stronger component captured
earlier. Fresh complete evidence advances immediately to persistence; expiry of
the five-second window also advances the event and records
`gps_wait_timed_out=true`. This produces a complete event stream across normal,
stale, location-only, and no-fix scenarios.

## Schema-v2 data contract

`events.csv` uses one exact 19-field header:

```csv
schema_version,device_id,boot_session_id,event_sequence,event_uptime_ms,timestamp_utc,mass_g,mass_calibration_status,corrected_signal_mv,raw_adc,accel_z_g,latitude,longitude,altitude_m,gps_age_ms,satellites,gps_status,gps_wait_timed_out,system_health
```

| Field group | Fields | Purpose |
| --- | --- | --- |
| Version and identity | `schema_version`, `device_id`, `boot_session_id`, `event_sequence` | Stable interpretation and globally useful event identity |
| Timing | `event_uptime_ms`, `timestamp_utc` | Monotonic event timing plus GNSS-derived UTC |
| Measurement | `mass_g`, `mass_calibration_status`, `corrected_signal_mv`, `raw_adc`, `accel_z_g` | Derived result alongside source and calibration context |
| Position | `latitude`, `longitude`, `altitude_m`, `gps_age_ms`, `satellites` | Location and receiver-quality evidence |
| State | `gps_status`, `gps_wait_timed_out`, `system_health` | Machine-readable pipeline outcome |

The formatter emits bounded rows without dynamic allocation. The companion
validator enforces the header, field types, ranges, enum values, identity rules,
and retry-equivalent duplicate semantics:

```sh
make validate-events EVENTS=/path/to/events.csv
```

## Resilience engineering

| Technique | Behavior |
| --- | --- |
| Explicit state machines | Acquisition, event, GNSS, storage, and LED behavior progress through inspectable states |
| Hysteresis and cooldown | Separate trigger/release levels suppress chatter and duplicate collection events |
| Component-wise evidence | Partial GNSS messages enrich an event without replacing stronger retained values |
| Retry-stable records | Storage retries reuse one identity, supporting deterministic downstream duplicate handling |
| Header and tail inspection | The logger validates the schema and restores a line boundary before appending |
| Synchronized event writes | Each completed row is appended with `O_SYNC` and closed at the event boundary |
| Exponential storage recovery | Initialization retry expands from 30 to 300 seconds |
| Sensor recovery | IMU initialization is retried every five seconds |
| Nonblocking diagnostics | A fixed 512-byte serial queue decouples logging output from acquisition |
| Watchdog supervision | An eight-second hardware watchdog bounds stalls in board-library calls |
| Health propagation | GNSS and storage state are encoded in both LED behavior and each CSV record |

## Status indication

| Pattern | Mode | Meaning |
| --- | --- | --- |
| Defined double pulse | Booting | Timing definition selected during setup |
| 500 ms on / 500 ms off | Calibrating | Stable startup tare acquisition |
| Solid on | Ready | Acquisition pipeline ready |
| 125 ms on / 125 ms off | Waiting for GNSS | Event evidence window active |
| One 150 ms pulse, 1.2 s pause | GNSS degraded | Event retained with available GNSS evidence |
| Three 150 ms pulses, 600 ms pause | Storage degraded | Queued event awaiting storage recovery |
| Five 100 ms pulses, 500 ms pause | Sensor fault | Sensor recovery active |

## Engineering evidence

The repository provides a complete software verification path:

- 26 deterministic native Unity tests for calibration, filtering, events,
  GNSS, sequencing, CSV, queueing, timing, and pipeline recovery;
- 17 Python tests for analysis, export validation, build-size verification,
  secret checks, and documentation checks;
- `cppcheck` coverage across 15 maintained C++ source files;
- a reproducible Nano RP2040 Connect firmware build;
- an independent firmware-image size check;
- link, SVG/XML, asset-size, and local-path documentation validation; and
- a six-assertion browser smoke test for the interactive demo.

The verified target build uses 45,288 of 270,336 RAM bytes (16.8%). Its generated
firmware image is 101,732 of 2,097,152 bytes (4.9%). Commands and consolidated
results are recorded in [`implementation-plan.md`](implementation-plan.md).

## Focused extension ideas

The current architecture provides clean seams for further portfolio exploration:

1. add a guided multi-point calibration workflow that emits a versioned
   coefficient profile;
2. rotate event files by size or session and generate a signed export manifest;
3. aggregate validated CSV records into a local spatial heat-map pipeline;
4. add a configuration record containing firmware, calibration, and wiring
   revisions;
5. stream the same schema over a pluggable wireless transport while retaining SD
   as the durable source; and
6. build a replay harness that feeds captured sensor traces through the native
   core for regression and algorithm comparison.
