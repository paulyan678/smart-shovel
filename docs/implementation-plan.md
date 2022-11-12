# Smart Shovel implementation and verification

## Delivered system

The Smart Shovel demonstration is implemented as one Arduino Nano RP2040
Connect application with a portable C++ core and thin hardware adapters. It
integrates analog load acquisition, onboard inertial sensing, orientation
compensation, event qualification, GNSS evidence fusion, schema-v2 SD logging,
status indication, diagnostics, and host-side export validation.

The design emphasizes deterministic embedded behavior: fixed-capacity buffers,
bounded formatters, explicit state machines, retry-stable record identity,
wraparound-safe time arithmetic, and no dynamic allocation in the domain core.

## Architecture decisions

| Decision | Implementation value |
| --- | --- |
| One Nano RP2040 Connect | Keeps sensor timing, GNSS evidence, event identity, and persistence in one execution domain |
| Portable core plus adapters | Runs domain logic natively while isolating Arduino APIs under [`../src/`](../src/) |
| Cooperative 50 ms acquisition | Interleaves sensing, GNSS polling, event progression, diagnostics, LED updates, and retries |
| Fixed-capacity memory | Makes queue, filter, formatter, and diagnostics memory use explicit |
| Event-oriented records | Produces one meaningful row per qualified load/release cycle |
| Versioned schema | Keeps firmware output and host validation synchronized |
| Component-wise GNSS merge | Preserves the best location, UTC, altitude, and satellite evidence independently |
| Retry-stable identity | Supports idempotent processing of storage retries and exported files |
| Pinned dependencies | Reproduces the board build across local development and CI |

## Completed work packages

| Work package | Delivered result | Evidence |
| --- | --- | --- |
| Repository architecture | One production application, reusable core library, hardware examples, archived explorations, and automated tools | [`../src/`](../src/), [`../lib/smart_shovel_core/`](../lib/smart_shovel_core/), [`../platformio.ini`](../platformio.ini) |
| Sensor processing | ADC averaging, count conversion, stable tare, IMU motion qualification, orientation correction, and moving filter | [`../src/sensor_reader.cpp`](../src/sensor_reader.cpp), [`../lib/smart_shovel_core/src/calibration.cpp`](../lib/smart_shovel_core/src/calibration.cpp) |
| Event semantics | Threshold, stability, hysteresis, latch, release re-arm, cooldown, and fixed event queue | [`../lib/smart_shovel_core/src/event_detector.cpp`](../lib/smart_shovel_core/src/event_detector.cpp), [`../src/main.cpp`](../src/main.cpp) |
| GNSS integration | Nonblocking UART parsing, freshness checks, component merge, event evidence window, and outcome status | [`../src/gps_receiver.cpp`](../src/gps_receiver.cpp), [`../lib/smart_shovel_core/src/gps.cpp`](../lib/smart_shovel_core/src/gps.cpp) |
| Persistence | Exact schema-v2 formatting, boot/session sequencing, header inspection, synchronized append, and storage retry | [`../lib/smart_shovel_core/src/csv.cpp`](../lib/smart_shovel_core/src/csv.cpp), [`../src/sd_logger.cpp`](../src/sd_logger.cpp) |
| Runtime resilience | Health state, watchdog, queued diagnostics, IMU retry, SD backoff, and LED pulse codes | [`../src/main.cpp`](../src/main.cpp), [`../src/status_led.cpp`](../src/status_led.cpp) |
| Data tooling | Reproducible calibration analysis, schema-v2 validation, and independent firmware image-size check | [`../tools/`](../tools/) |
| Automated quality | Native/Python tests, format check, static analysis, secret scan, docs validation, and target build | [`../Makefile`](../Makefile), [`../.github/workflows/ci.yml`](../.github/workflows/ci.yml) |
| Visual demonstration | Responsive system guide, diagrams, prototype assets, deterministic simulator, and browser smoke test | [`visual-guide.md`](visual-guide.md), [`demo/`](demo/) |

## Verification gates

The aggregate gate is:

```sh
make verify
```

| Gate | Command | Verified result |
| --- | --- | --- |
| Python tooling | `make test-python` | 17/17 tests passed |
| Calibration reproduction | `make calibration-check` | `calib1` hash and fitted model reproduced |
| Documentation and assets | `make docs-check` | Links, local targets, SVG/XML, asset sizes, and paths passed |
| Credential scan | `make secret-check` | Maintained tree passed |
| C++ formatting | `make format-check` | Passed |
| Static analysis | `make static-check` | 15 maintained source files passed `cppcheck` |
| Native domain tests | `make test-native` | 26/26 Unity tests passed |
| Target firmware build | `make build-firmware` | Nano RP2040 Connect build passed |
| Aggregate software gate | `make verify` | Passed |
| Patch hygiene | `git diff --check` | Passed |
| Interactive demo | Open [`demo/smoke.html`](demo/smoke.html) through the local server | 6/6 browser assertions passed |

## Build profile

The verified Nano RP2040 Connect build produced:

| Resource | Used | Capacity | Utilization |
| --- | ---: | ---: | ---: |
| RAM | 45,288 bytes | 270,336 bytes | 16.8% |
| Generated firmware image | 101,732 bytes | 2,097,152 bytes | 4.9% |

PlatformIO's Mbed estimate separately reported 4,410 bytes for its measured
program segment. The repository also checks the complete generated image against
the configured flash budget.

## Test coverage map

The native Unity suite exercises the portable core at state and boundary level:

- ADC conversion, plausibility margins, tare stability, and orientation math;
- filter fill, reset, replacement, and aligned-sample handling;
- event threshold crossings, stability counts, hysteresis, cooldown, and re-arm;
- GNSS coordinate/calendar validation, component freshness, and evidence merge;
- pipeline GNSS timing, storage retry, recovery, and health composition;
- schema-v2 formatting, optional fields, enum text, and bounded buffers;
- device/session/sequence validation and interrupted CSV scanning;
- fixed-ring-buffer capacity and FIFO behavior; and
- unsigned-millisecond wraparound behavior.

The Python suite covers calibration reproduction, schema-v2 export validation,
identity conflict detection, firmware image sizing, repository secret patterns,
and documentation integrity.

## Reproduce the portfolio demonstration

Build and verify the firmware:

```sh
make setup
make verify
```

Run the interactive system walkthrough:

```sh
python3 -m http.server 8000 --directory docs
```

Then open `http://localhost:8000/demo/`. The controls expose normal collection,
GNSS-stale, GNSS-unavailable, transient-storage, and below-threshold paths while
showing state progression, LED status, the exact CSV row, and the valid-location
map rule.

Validate an exported event file:

```sh
make validate-events EVENTS=/path/to/events.csv
```

Reproduce the orientation model:

```sh
python tools/analyze_calibration.py \
  --input calibration/data/raw/calib1.csv \
  --hac-lags 5
```

## Extension roadmap

| Extension | Technical focus |
| --- | --- |
| Guided calibration profiles | Versioned multi-point fits, residual reporting, and generated firmware configuration |
| Trace replay | Deterministic playback of captured ADC/IMU/GNSS streams through the native core |
| Storage rotation | Size/session-based files plus export manifests and integrity hashes |
| Local mapping | Validated CSV aggregation, spatial binning, and interactive heat-map rendering |
| Configuration provenance | Firmware, calibration, and assembly revisions embedded in each export |
| Pluggable telemetry | Schema-preserving wireless transport alongside the SD event log |
