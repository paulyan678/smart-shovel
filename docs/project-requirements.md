# Smart Shovel engineering brief and requirements baseline

## Document purpose

This document turns the surviving 2022 design pitch, firmware, photographs, and calibration files into a traceable baseline for further engineering. It deliberately separates:

1. what the physical prototype and repository actually show;
2. what the 2022 team claimed to have verified;
3. what the pitch called the final concept; and
4. what remains an unbuilt or unvalidated idea.

Historical claims are evidence inputs, not current acceptance results. In particular, this document does **not** assert that the current shovel reports calibrated grams, achieves 10 m GNSS accuracy, carries 5 kg safely, survives a 1.5 m drop, is rain-safe, reduces workload, or costs C$21.89.

### Source set

- Historical pitch archive: `ESC204__Design_Pitch.zip`, supplied separately for
  this refinement and not vendored in the repository. Evidence reviewed includes
  `main.tex`, `_appendix.tex`, `citations.bib`, `Interaction.png`,
  `Electrical component.png`, `calib.pgf`, `95CI.png`, `GPS.png`, `drop.png`,
  `steps.png`, `production_cost.png`, and the prototype/case photographs.
- Current production architecture and pinned toolchain: [`../platformio.ini`](../platformio.ini), configuration under [`../include/smart_shovel/`](../include/smart_shovel/), testable logic under [`../lib/smart_shovel_core/`](../lib/smart_shovel_core/), and the single-board hardware adapter at [`../src/main.cpp`](../src/main.cpp).
- Original integrated sketch, retained only as legacy evidence: [`../legacy/firmware/2022-prototype/main.ino`](../legacy/firmware/2022-prototype/main.ino).
- Historical two-controller experiment: [`../legacy/experiments/master-slave/`](../legacy/experiments/master-slave/).
- Raw calibration and field logs: [`../calibration/data/raw/`](../calibration/data/raw/).
- Historical analysis notebook: [`../legacy/analysis/vis.ipynb`](../legacy/analysis/vis.ipynb).
- Repository prototype photographs: [`img_1.png`](img_1.png) and [`img_2.png`](img_2.png).

Section names below refer to `main.tex` in the pitch archive. The archive is not vendored here, so its claims must remain identifiable by both filename and section.

### Status vocabulary

Only the following status values are used in the traceability table:

- `implemented`: an active code path exists; this alone says nothing about compilation or hardware behavior.
- `host-tested`: a behavior or dataset was exercised on a development host.
- `compile-tested`: the production candidate compiled for the named board and dependency versions. No item receives this status until a reproducible build record exists.
- `hardware-validation-required`: physical wiring, behavior, accuracy, safety, or environmental performance remains to be demonstrated.
- `documented-only`: a requirement or historical claim is recorded, but no conforming implementation/evidence exists.
- `deferred`: intentionally outside the current production baseline.

## Stakeholders, deployment context, and value

The pitch frames the opportunity as collecting data about uncollected recyclable plastic in Accra, Ghana, to support circular-economy and waste-policy decisions (`main.tex`, Executive Summary and sections 1.1, 2.1–2.3). It names a context provider and residents of Ghana as primary stakeholders, and also discusses the Government of Ghana, waste collectors/services, and the design team. Waste collectors are the direct operators even though they are not consistently listed as primary stakeholders.

The intended service environment is outdoor waste collection in Accra. The pitch values data quality and quantity, feasibility, reduced operator workload, durability, affordability, sustainability, and autonomy. Those values remain useful, but they do not substitute for measurable acceptance criteria.

The current technical product is narrower than the social aspiration: it is an offline instrument intended to associate a load-cell reading with GNSS-derived time and position and save a CSV record. It does not identify plastic, classify waste, prove that material was previously uncollected, generate a heat map, or establish that deployment improves policy or worker productivity.

### Current scope language

Until stakeholders define the sampled population and workflow, engineering records shall use **loaded material** or **collection event**, not “uncollected recyclable plastic.” Material type and prior collection status are unknown because the prototype has no classifier and no operator input for either field.

The first deployment remains a supervised engineering trial, not unattended field service. Outdoor/rain use, human-subject workflow claims, and policy-grade data release are prohibited until their requirements below are validated.

## Architecture decision: one Nano is the production baseline

The production architecture is **one Arduino Nano RP2040 Connect** coordinating
the analog weight input, onboard IMU, GNSS UART, SPI microSD module, and a
configurable external status LED on D2.

Evidence for this decision:

- [`../README.md`](../README.md) describes one Nano as the central hub.
- The original integrated sketch, now at [`../legacy/firmware/2022-prototype/main.ino`](../legacy/firmware/2022-prototype/main.ino), demonstrated all active functions in one sketch.
- Pitch `Interaction.png` contains one logical “Arduino Nano (MCU & IMU)” block.
- The pitch BOM in `_appendix.tex`, section “Bill of Materials,” lists two Nanos,
  while the repository photographs contain board-format assemblies consistent
  with a Nano and an L76B GNSS carrier but do not prove the complete architecture.
- [`../legacy/experiments/master-slave/`](../legacy/experiments/master-slave/) is an unfinished experiment, not a viable second-controller implementation: it contains undefined identifiers and syntax/API errors, and `notes.txt` assigns both SCL and SDA to A5 instead of documenting a complete I2C connection.

One Nano is the reversible choice because it matches the only integrated firmware, removes an undocumented inter-MCU protocol and synchronization/failure mode, and reduces wiring, power, cost, and configuration surface. A second controller may be reconsidered only if measured timing, I/O, memory, isolation, or safety requirements cannot be met by one board; that decision would require a versioned interface specification and new verification.

Production logic lives under [`../lib/smart_shovel_core/`](../lib/smart_shovel_core/) with the Arduino adapter at [`../src/main.cpp`](../src/main.cpp), as specified in [`implementation-plan.md`](implementation-plan.md). The 2022 sketches under [`../legacy/`](../legacy/), Wi-Fi/audio/dual-board experiments, widget-lab sketches, and hardware examples are evidence or examples, not alternate production entrypoints. The adapter is implemented and compile-tested; every physical behavior remains pending hardware verification.

## Confirmed prototype hardware and data flow

### What the evidence supports

| Subsystem | Confirmed historical evidence | Current repository behavior | Unknown or unverified |
|---|---|---|---|
| Processor/IMU | Arduino Nano RP2040 Connect named in pitch BOM and software sections; onboard IMU and RGB LED described | PlatformIO pins Raspberry Pi platform 1.20.0, board `nanorp2040connect`, Arduino LSM6DSOX 1.1.2, TinyGPSPlus 1.1.0, and SD 1.3.0; the nonblocking adapter compiles | Exact installed board revision, unexplained second-Nano BOM quantity, installed wiring and on-device behavior |
| Weight chain | Load cell and LM358 custom amplifier; pitch section 3.2.2 claims 910× gain | Production configuration selects `A0`, 16 requested ADC bits, 3,300 mV provisional reference, eight ADC samples/read, and an eight-sample filter; pure conversion/calibration logic exists | Load-cell make/range/sensitivity, bridge wiring/excitation, amplifier schematic/resistors, actual gain/bandwidth/offset, measured ADC reference/input protection |
| Orientation sensing | Onboard IMU; pitch section 5.3.3 correlates voltage with z acceleration | Core applies a tare-relative, same-unit correction using the preserved `calib1.csv` slope and rejects non-finite inputs; host tests cover the formula | Axis convention on the assembled shovel, coefficient units/portability, adapter sample freshness, physical validity |
| GNSS | L76B GNSS module in pitch BOM | Core validates and component-wise merges the best location, date/time, altitude, and satellite evidence; adapter polls `Serial1` nonblockingly; config selects 9,600 baud and 5 s freshness/event-wait limits | Exact module/firmware, antenna placement, electrical levels, actual TX/RX header wiring and on-device behavior |
| Storage | SPI microSD module; pitch says CSV stores time, location, and weight | Schema-v2 rows use device/boot-session/sequence identity; the SD adapter checks the exact header and line boundary, retains one sequence across retries, appends/flushes/closes, and retries from 30–300 s; config selects CS/D10 and `events.csv` | SPI header pins selected by the board core, card voltage compatibility, filesystem/capacity, growing-file latency, write endurance and real power-loss behavior |
| User status | Onboard RGB LED in pitch and historical firmware | Production deliberately uses one active-high external D2 LED with seven pulse/solid patterns plus serial diagnostics; no Wi-Fi/RGB dependency is compiled | D2 LED/resistor wiring, polarity, daylight visibility, operator comprehension, and whether a future bounded onboard-RGB adapter is justified |
| Prototype power | Pitch section 3.2.2: eight 1.5 V AA cells, nominal 12 V, through a 7805 to a claimed 5 V rail | No power topology is encoded in firmware | Current source, regulator implementation, grounding, current draw/runtime, heat, fuse/reverse-polarity/overvoltage protection |
| Structure | Wooden 7/8-inch dowel, dustpan head, M5 fasteners, washer, scrap-metal/load-cell reinforcement, freely rotating handle; exposed breadboards visible | No mechanical design source is in this repository | Dimensions/tolerances, load path, load-cell mounting, fastener grades/torque, total mass/balance, enclosure/ingress safety |

### Historical 2022 data flow

1. Initialize the RGB LED, SD, onboard IMU, ADC resolution, and GNSS UART.
2. Create/write headers for `log.csv` and `rec.csv`.
3. Wait 3 seconds, average 5,000 weight-path samples once, and store that value as `zero_voltage`.
4. Wait 2 seconds and then block until a GNSS location is available because `GPS_INIT_REQUIRED` is `true`.
5. In each loop, average 1,000 analog/IMU iterations, subtract the startup zero, multiply by `MV_TO_GRAM`, append IMU diagnostics and a location record, and increment a success counter.
6. Refresh GNSS after the counter becomes greater than 64, which means 65 logged rows can reuse one location/time fix; the refresh itself blocks for 500 ms.

The source for this flow is [`../legacy/firmware/2022-prototype/main.ino`](../legacy/firmware/2022-prototype/main.ino), not the new production path. Its `rec.csv` has `time,weight,latitude,longitude,alt`; `log.csv` declares `gram,ax,ay,az,gx,gy,gz`. Neither schema carries units, fix validity/age/satellite count, device ID, calibration ID, firmware version, raw ADC value, error flags, or material classification. A row is a loop sample, not a detected collection event.

### Current production design flow

The maintained core, configuration, and `src/main.cpp` implement the following nonblocking flow. Host logic and target compilation are verified; peripheral behavior remains to be verified on hardware:

1. Sample A0 and the onboard IMU at a configured 50 ms interval, averaging eight ADC reads and using explicit motion/sample validity.
2. Establish startup tare from a stable window under an operator empty-head
   precondition. Keep runtime auto-zero disabled because no independent input can
   prove the shovel is unloaded; the bounded core tracker remains host-tested for
   a future explicit unloaded signal.
3. Convert ADC counts to one signal unit before applying the tare-relative orientation relationship; never mix ADC counts and millivolt-labelled values.
4. Filter weight and emit one event only after stable trigger samples, latch it until release, and apply a cooldown to prevent duplicate rows.
5. During a finite GNSS wait, merge the best independently valid location,
   date/time, altitude, and satellite components. On expiry, retain that evidence
   and set `gps_wait_timed_out=true` instead of replacing it with stale data.
6. Format a bounded schema-v2 row with device ID, random 16-hex boot session,
   retry-stable per-session sequence, uptime, UTC when valid, mass/calibration
   state, corrected signal, raw ADC, z acceleration, location/altitude, GNSS
   age/satellites/status, timeout flag, and system health.
7. Append, flush, and close at the event boundary; refuse a mismatched header,
   separate an interrupted tail with a newline, and retry SD initialization with
   30–300 s backoff while exposing degraded state.

The configured gram thresholds are provisional because the preserved `-15
grams/mV` conversion is unverified. They permit deterministic software and
supervised bench evaluation, but not calibrated-gram claims or field decisions
until F-WGT-03 passes. Marking a future mass factor verified does not by itself
enable runtime auto-zero; independent unloaded evidence is also required.

## Pin and electrical evidence

The table distinguishes code evidence from physical-wiring evidence. A pin constant that is unused is not proof of wiring.

| Function | Evidence | Engineering interpretation/status |
|---|---|---|
| Amplified load-cell signal | `kLoadCellPin = A0` in [`../include/smart_shovel/config.hpp`](../include/smart_shovel/config.hpp); `analogRead(A0)` in the legacy integrated sketch | `A0` is the production intent. Physical connection, reference, scaling, and voltage range are hardware-validation-required. |
| GNSS transport | `kGpsBaud = 9600` in production config; [`gps_receiver.cpp`](../src/gps_receiver.cpp) calls `Serial1.begin(baud)` and polls it | Hardware UART at 9,600 baud is implemented in code; physical wiring and on-device behavior are unverified. |
| GNSS pin constants | Legacy [`gps.hpp`](../legacy/firmware/2022-prototype/modules/gps/gps.hpp) declares `RXPin = 1`, `TXPin = 0`, but legacy code never passes them to `Serial1` | Constants are historical, not wiring authority; their names do not safely establish module-to-board crossing. Actual board header pins and GNSS TX→MCU RX / GNSS RX←MCU TX wiring remain unknown. |
| SD chip select | `kSdChipSelectPin = 10` in [`../include/smart_shovel/config.hpp`](../include/smart_shovel/config.hpp) | CS/D10 is the production intent. Physical connection and adapter behavior are unverified. |
| SD SPI signals | Pitch `Electrical component.png` labels module header `1 GND`, `2 VCC`, `3 MISO`, `4 MOSI`, `5 SCK`, `6 CS` | Module-side labels are documented. The Nano header pins for MISO/MOSI/SCK are not explicitly recorded; the adapter will use the board’s default `SPI` mapping. Generic comments under [`../examples/hardware/sd-card/`](../examples/hardware/sd-card/) are not wiring authority. |
| IMU | Production dependency `Arduino_LSM6DSOX@1.1.2`; [`sensor_reader.cpp`](../src/sensor_reader.cpp) checks availability/return values | Onboard, library-managed connection; no external IMU pins are required. Physical axis mapping and timing remain hardware-validation-required. |
| Status LED | [`status_led.cpp`](../src/status_led.cpp) drives configured D2 active-high; [`config.hpp`](../include/smart_shovel/config.hpp) documents the external LED/current-limit-resistor default | Seven single-LED pulse/solid patterns compile. This reversible deviation from the pitch's onboard RGB avoids D13/SPI conflict and coprocessor-dependent RGB calls; electrical/visibility/operator validation is pending. |
| Historical two-Nano I2C | [`notes.txt`](../legacy/experiments/master-slave/notes.txt) mentions ground and A5/A5 | Rejected as an authoritative pin map; incomplete and internally wrong for SDA. |
| Power | Pitch states 8×AA → 7805 → 5 V for all components | Historical intent only. There is no schematic, measured rail, component current budget, common-ground record, or proof that the amplified ADC signal stays within the Nano’s published limits. Do not energize from this description alone. |

Pitch `Interaction.png` is a functional sketch, not a schematic. It incorrectly/ambiguously draws the load-cell “Weight” path directly to the Nano while the op-amp appears to power the load cell. The narrative says the LM358 amplifies the load-cell voltage, which is the expected role, but no circuit values are supplied. The pitch BOM also omits the load cell and every resistor needed for the claimed 910× gain. A reviewed schematic and measured rail/signal limits are release blockers.

Exact historical electrical BOM evidence from `_appendix.tex`, “Bill of Materials,” is: LM358 op-amp ×1; microSD module ×1; L76B GNSS module ×1; “Arduino Nano connect” ×2; 7805 regulator ×1; eight-cell AA holder ×1; 1.5 V AA cells ×8; 0.22 µF ceramic capacitor ×1; 10 µF electrolytic capacitors ×2; and wiring ×3. `Electrical component.png` visually labels a 7805CT, LM358P, AA holder, and the six-pin SD module. Missing from the electrical BOM are the load cell, op-amp feedback/input resistors, breadboards/connectors, microSD card, protection devices, and any PCB. This list records the prototype archive; it is not an approved production BOM.

## Calibration baseline and known defects

### What the raw data can support

The pitch says the shovel was moved with a fixed load and that z acceleration was the only one of six IMU channels with a strong relationship to voltage (`main.tex`, section 5.3.3 and `calib.pgf`). The four calibration CSVs are overlapping snapshots of one 1,415-row stream, not four independent trials: `calib2` is `calib4` rows 5–958, `calib1` is rows 22–639, and `calib3` is rows 824–1200. They must not be concatenated. Host-side ordinary least-squares sensitivity fits of each committed `voltage`/`az` capture produced:

| File | Usable rows | Slope, voltage units per g | Intercept, voltage units | R² |
|---|---:|---:|---:|---:|
| [`calib1.csv`](../calibration/data/raw/calib1.csv) | 618 | -74.7089 | 25.2884 | 0.91518 |
| [`calib2.csv`](../calibration/data/raw/calib2.csv) | 954 | -71.9935 | 38.6403 | 0.71765 |
| [`calib3.csv`](../calibration/data/raw/calib3.csv) | 377 | -73.8033 | 69.3632 | 0.94103 |
| [`calib4.csv`](../calibration/data/raw/calib4.csv) | 1,415 | -70.5066 | 46.8834 | 0.71662 |

The pitch plot labels imply millivolts and g. The CSV headers themselves do not encode units, source transformation, device, applied mass, or trial conditions, and negative “voltage” values show that the data are already transformed rather than raw ADC voltage. The pitch caption’s blanket claim that three trials have R² > 0.9 is not reproducible by treating these files as trials.

[`../legacy/analysis/vis.ipynb`](../legacy/analysis/vis.ipynb) has saved output `slope=-74.7089167`, `intercept=25.2884358`, and R² `0.9151767`, consistent with `calib1.csv`. Its executed state is not replayable as written: it first loads a file named `test.csv`, then later expects `voltage` and `az`, while the committed [`test.csv`](../calibration/data/raw/test.csv) has `time,weight,latitude,longitude,alt`. The maintained [`../tools/analyze_calibration.py`](../tools/analyze_calibration.py) and tests now reproduce `calib1` without modifying the evidence. They report the conventional slope 95% interval `[-76.5086, -72.9093]`; because residual lag-1 correlation is about 0.772, the documented five-lag Newey-West sensitivity interval is wider, approximately `[-77.7539, -71.6640]`. Neither interval establishes unit-to-unit or environmental validity.

Other data limitations:

- [`known.csv`](../calibration/data/raw/known.csv) contains a stray `gz` row and no known-mass or trial-label column. Its first column is called `gram`, but it ranges from approximately -1,035 to +1,396 and cannot establish accuracy against truth.
- [`test.csv`](../calibration/data/raw/test.csv) contains 859 rows and only 11 timestamp strings; it has no known-mass column. Reported `weight` values span approximately 369–2,947 without ground truth, so it demonstrates logging, not gram accuracy.
- Pitch `95CI.png` does not show a confidence interval. It is a residual plot with no residual unit and one conspicuous residual near -31.

### Firmware defects that invalidate calibrated-weight claims

1. **Legacy orientation-correction unit error.** Historical regression is approximately -75 signal units per g. The 2022 `read_weight()` adds `az * 73` to raw ADC **counts**, then converts the sum using `3300 / 65536`; if the slope unit is mV/g, this applies only about 3.68 mV/g. It also ignores IMU sample availability and can use an uninitialized `az`.
2. **Unsupported grams factor.** Legacy `MV_TO_GRAM = -15` has no derivation, paired raw-signal/known-mass artifact, load-cell model, uncertainty, or physical validation. Production config preserves `-15` only as an explicitly provisional value and defaults `SMART_SHOVEL_MASS_CALIBRATION_VERIFIED` to false.
3. **Historical “automatic calibration” means startup zero only.** The 2022
   sketch zeroed once after a fixed delay without proving the shovel was
   unloaded/stationary. The new adapter has a stable-window startup tare under an
   operator empty-head precondition. Its host-tested bounded auto-zero tracker is
   deliberately gated false for every runtime sample because the current
   hardware has no independent unloaded evidence.
4. **Raw provenance remains incomplete.** The new event schema includes raw averaged ADC, z acceleration, corrected signal, calibration status, and event uptime, but not measured ADC reference, tare ID/value, firmware revision, or calibration revision. Those are still required to reconstruct a physical result.

Chosen reversible resolution: the production core first converts counts into one signal unit and applies `(signal - tare) - slope * (az - tare_az)`, eliminating the legacy count/signal-unit mixing in maintained logic. The preserved slope and `-15` conversion remain provisional until hardware provenance and known-mass validation exist. Do not publish, label, or display a production result as calibrated grams until F-WGT-01 through F-WGT-04 pass.

## Prioritized functional requirements

Priority meanings: P0 blocks safe data collection, P1 is required for a useful supervised prototype, and P2 is a later product capability.

| ID | Priority | Requirement and acceptance condition |
|---|---|---|
| F-ARCH-01 | P0 | One Nano shall run the production acquisition path without a second-MCU dependency. `src/main.cpp` shall be the sole production application; reusable pure logic belongs under `lib/smart_shovel_core`, and `legacy/`/`examples/` shall remain outside the production build. |
| F-BOOT-01 | P0 | Boot shall self-test SD, IMU, ADC plausibility, and GNSS transport; each failure shall produce a documented state and shall not silently create apparently valid records. GNSS acquisition shall have a finite, configurable timeout or an explicit degraded mode. |
| F-WGT-01 | P0 | Acquire load-cell amplifier output on A0 and retain raw ADC/count or voltage data with units, ADC reference/resolution, timestamp, and validity. No input may exceed the board’s published operating limits. |
| F-WGT-02 | P0 | Startup zeroing shall occur only after an unloaded-and-stable condition is confirmed; record zero mean, dispersion, sample count, time, calibration ID, and failure reason. It shall be called “startup zero,” not automatic calibration. |
| F-WGT-03 | P0 | Convert voltage to mass only from a versioned, reproducible multi-point known-mass calibration. Sign, factor, intercept, range, temperature, hysteresis, repeatability, and uncertainty shall be documented. Required error tolerance is a stakeholder decision and must be set before release validation. |
| F-WGT-04 | P0 | Any orientation correction shall combine like units, use only fresh IMU samples, define the shovel coordinate frame, and demonstrate corrected error across the operating orientation/load envelope. Until then it shall be disabled for reported mass or flagged invalid. |
| F-GPS-01 | P1 | Parse GNSS time, latitude, longitude, altitude, fix validity, age, and satellite count from the 9600-baud UART. Invalid/stale fixes shall never be serialized as valid positions. |
| F-GPS-02 | P1 | Use UTC in an unambiguous machine-readable timestamp and associate each record with the actual fix age. A provisional open-sky target of ≤10 m horizontal error may be used to design validation, but it is not an achieved fact and requires stakeholder ratification. |
| F-EVT-01 | P1 | Define what creates one collection event. Continuous loop samples shall not be presented as independent waste items. The trigger, debounce/window, aggregation, duplicate handling, and “no load” behavior shall be testable. |
| F-LOG-01 | P0 | Write a versioned CSV (or documented successor) containing device/firmware/calibration IDs, UTC time, weight raw/corrected values and units, position/fix-quality fields, and status flags. Headers shall not be duplicated or silently disagree with data. |
| F-LOG-02 | P1 | Detect card absence, open/write failure, full media, malformed records, and power interruption. Preserve previously committed records and give the operator an actionable indication. |
| F-UI-01 | P1 | Provide a documented LED state table for booting, ready, collecting, GNSS degraded, calibration invalid, storage warning, and fatal failure, plus operator recovery steps. |
| F-EXP-01 | P1 | Provide a host-side, versioned validator/export path that rejects malformed/invalid records before aggregation or mapping. |
| F-CLS-01 | P2 | Material classification, including computer vision, is deferred. No material-type field may be inferred without a validated classifier or explicit operator input. |

## Prioritized nonfunctional requirements

| ID | Priority | Requirement and acceptance condition |
|---|---|---|
| N-ELEC-01 | P0 | Produce a reviewed schematic, wiring table, complete BOM, and measured current/voltage budget. Include load-cell bridge, amplifier feedback network, decoupling, grounding, ADC protection, SD/GNSS levels, fuse/current limiting, reverse-polarity handling, and regulator thermal analysis. |
| N-SAFE-01 | P0 | Do not operate as a field tool with exposed conductors, breadboards, unsecured batteries, sharp reinforcement, or an unqualified load path. A pre-use inspection and safe failure mode are required. |
| N-STR-01 | P0 | Establish working-load and ultimate-load limits with a fixture and documented load cases. The pitch’s 5 kg value is only a provisional test point. Record permanent deformation, fastener movement, load-cell damage, and measurement change before/after. |
| N-DROP-01 | P1 | Define and execute a shovel-specific drop protocol, including 1.5 m as a provisional legacy test height, orientations, surface, battery state, repetitions, exclusion zone, inspection, and post-drop electrical/measurement checks. A photo of static books is not a drop result. |
| N-ENV-01 | P0 | No rain/outdoor service until enclosure, cable entry, condensation, corrosion, cleaning, temperature, and ingress requirements are defined and tested. “Less than 200 mm rain” has no time basis and is not an ingress specification. |
| N-DATA-01 | P0 | Quantitative accuracy, precision, resolution, drift, hysteresis, outlier, and missing-data limits shall be defined for the intended decision. “Adequate for a heat map” is not an acceptance criterion. |
| N-AVAIL-01 | P1 | Blocking waits and fatal loops shall be bounded or intentionally latched with a documented recovery path. The device shall expose whether weight, time, position, and storage are valid independently. |
| N-ERG-01 | P1 | Validate total mass, center of mass, grip/rotation lock, reach, repetitive-use comfort, snag hazards, and cleaning with representative waste collectors. No “no burden/no training” claim is permitted without field evidence and consent. |
| N-PWR-01 | P1 | Define operating time, battery chemistry/range, peak/average current, charging/replacement workflow, low-voltage behavior, storage temperature, and safe shutdown. The historical 8×AA/7805 topology is not the current approved supply. |
| N-PRIV-01 | P0 | Treat precise location/time as potentially sensitive worker and operations data. Define purpose, informed participation, minimization/precision, device identifiers, access, encryption or compensating controls, retention/deletion, incident handling, and aggregate-release rules before field collection. |
| N-SEC-01 | P0 | Do not commit or activate credentials from sketches. Wi-Fi experiments under `legacy/` are non-production and must not become a deployment path without credential provisioning, authentication, and transport security. Local secret files shall remain ignored and current-tree secret scans shall run in CI. |
| N-COST-01 | P1 | Maintain a quantity-complete BOM with model/revision, supplier/date/currency, source, unit/extended cost, spares, enclosure/PCB/fasteners/storage, assembly/test, and exclusions. Set a current cost target with the deployment sponsor before optimizing it. |
| N-MAINT-01 | P1 | Record board/core/library versions, build command, calibration version, wiring revision, and test artifact hashes for every release candidate. |

## Historical verification record: claim versus evidence

| Historical claim | Pitch source | Available evidence | Limitation/current interpretation |
|---|---|---|---|
| GPS is within 10 m | `main.tex`, section 4.4; `GPS.png` | One display showed exact coordinates and distance 6.69 (unit omitted); coordinates are intentionally not republished | No repeat count, truth method, distribution, antenna/context, or Accra test; hardware-validation-required |
| Accurate weight | `main.tex`, sections 4.4 and 6.3; `calib.pgf`; `95CI.png` | Voltage/IMU correlations and a residual plot | No known-mass truth table, residual units, error specification, or valid grams factor; one large residual; raw trials do not all reproduce R² >0.9 |
| Automatically/constantly calibrated | Executive Summary and sections 6.2–6.3 | Startup zero and z-axis correction attempt in the 2022 sketch | Legacy path was startup-only with no unloaded/stability gate and a correction-unit bug; production now has a stable operator-gated startup tare, but runtime auto-zero is disabled until independent unloaded evidence exists |
| Supports 5 kg | `main.tex`, section 4.2 and 6.4; `drop.png` | Photograph of books/static load and textual claim | Load magnitude/procedure not independently documented; maximum is explicitly unknown |
| Survives 1.5 m drop | Executive Summary and section 4.2 | Textual claim and `drop.png` captioned durability | Image does not show drop or post-test results; cited headgear method is not a shovel protocol |
| No workload/training burden and comfortable | sections 4.3, 5.1, 6.2; `steps.png` | Process illustration and team narrative | No representative-user study, task timing, ergonomic measurement, consent, or Ghana field validation |
| Rain-capable service | section 2.3 states intended use below 200 mm rainfall | None; sections 6.4 and prototype photos acknowledge exposed circuits | Prototype is not rain-safe; case remained an aspiration |
| Prototype/final cost is low | sections 4.1, 3.6, and 6.1; `_appendix.tex`; `production_cost.png` | Several estimates | Values conflict and omit material: C$19.46 prototype, C$21.89 variously prototype/final estimate, while listed BOM extended costs sum to C$40.688 including Widget Lab parts (C$9.074 for rows marked project budget) |
| High data quantity suitable for heat map | section 2.6 HL1 and section 4.4 | `test.csv` demonstrates 859 rows | Only 11 timestamp values, repeated positions, no coverage/deployment/heat-map evidence; loop rows are not independent events |

## Prototype, concept aspiration, and deferred work

| Area | 2022 physical prototype/evidence | 2022 “final concept” aspiration | Current reversible resolution |
|---|---|---|---|
| Controller | Pitch BOM says two Nanos; integrated narrative/diagram uses one | Cheaper controller later suggested, including Arduino Mega | One Nano RP2040 Connect; second MCU deferred unless measured need emerges |
| Weight electronics | Load cell + custom LM358 amplifier, claimed 910×; exposed breadboard | Lighter/more accurate load cell and HX711 | Production selects A0 and has unit-safe core math, but physical amplifier/load cell selection and validation remain open |
| Power | 8 AA cells, 7805, nominal 12 V to claimed 5 V | Solar panel wrapped around handle for continuous power | Field power and solar deferred pending measured budget/runtime and safety analysis |
| Structure | Wooden dowel, dustpan, metal reinforcement, free 360° handle | Aluminum dowel/dustpan, ergonomic locking handle | No production mechanical design selected; supervised bench prototype only |
| Enclosure/environment | Electronics exposed | Permanently attached case with snap fit plus screws; rain use | Case CAD is not a built/weatherproof enclosure; outdoor service blocked pending N-ENV-01 |
| GNSS | L76B module | Cheaper NEO-7M suggested | Keep generic Serial1/TinyGPSPlus interface; module and validation target remain hardware decisions |
| Storage/data | MicroSD CSV | Scalable aggregation, filtering, heat maps | Offline schema-v2 SD logging remains baseline; identity is device/session/sequence and a host validator rejects malformed/conflicting records, while file rotation, adapter validation, aggregation, and policy heat-map workflows remain open |
| Waste identification | None | Computer vision classification | Deferred; records describe loaded material only |
| Status | Onboard RGB LED | Maintainability/error guidance | Production uses a reversible external D2 single-LED pulse adapter; validate wiring, patterns, visibility, and recovery guidance before user trials |

## Contradictions and chosen resolutions

| Contradiction or ambiguity | Resolution used by this baseline |
|---|---|
| Pitch BOM says two Nanos, while the interaction diagram uses one and repository photographs do not establish the complete architecture | Select one Nano. Treat the second-controller quantity as unexplained historical evidence and require a new measured architecture decision to reintroduce it. |
| Two historical integrated sketches previously appeared production-like | Both now live under `legacy/firmware/2022-prototype/`; the new `src/main.cpp` adapter is the only production application. |
| Opportunity alternates among uncollected recyclable plastic, general garbage, and waste sent to landfill | Use “loaded material/collection event,” with material and prior-status unknown, until stakeholder workflow is defined. |
| “Automatic/constant calibration” versus a one-time historical startup zero | Describe the 2022 behavior as startup zero only. Keep the new runtime auto-zero gate false until independent unloaded evidence exists; host tests of the bounded algorithm are not proof of physical safety. |
| Pitch/onboard RGB requirement versus recoverable nonblocking status behavior | Use a configurable external active-high D2 LED plus resistor as the reversible fail-safe default. Mark F-UI partial until onboard RGB or the external deviation is physically and operationally accepted. |
| Historical orientation slope is about -75 signal units/g, but 2022 firmware adds 73 ADC counts/g | New core corrects the dimensional formula after count conversion and has deterministic tests; keep results provisional until signal units and physical behavior are verified. |
| `MV_TO_GRAM=-15` versus no known-mass derivation | Preserve only as a `provisional` configuration with verification false; prohibit calibrated-gram claims and field decisions. |
| Rainy-day service and “final case design” versus exposed prototype and future-case wording | Prototype is not weatherproof; enclosure is an unbuilt concept and outdoor use is blocked. |
| GPS ≤10 m, 5 kg capacity, 1.5 m drop, comfort, and workload claims versus thin/no protocols | Retain as historical/provisional validation inputs, not achieved requirements. Re-test with defined protocols. |
| Prototype C$19.46, C$21.89 prototype/final, and C$40.688 sum of listed BOM totals | Do not select a current cost baseline. Produce a complete dated BOM and sponsor-approved target. |
| `GPS.png` reports distance 6.69 without unit | Treat unit as unknown in the evidence record even though the GPS helper returns meters. |
| Pitch says 5 V powers all components, but the ADC signal limits and level compatibility are not documented | No approved power/wiring topology exists; require schematic review and measured limits before energizing. |
| Legacy firmware declares `RXPin=1`, `TXPin=0` but never uses them | Production uses the board UART abstraction; physical crossed UART wiring remains to be recorded and inspected. |

## Assumptions, safety, privacy, and known limitations

### Historical assumptions requiring validation

The pitch explicitly assumes that waste collectors already use/have shovels, that adding the device does not disrupt workflow, and that collectors/services can perform the necessary modifications (`main.tex`, Executive Summary and section 5.2.2). It also implicitly assumes open-sky GNSS reception, access to removable SD cards and a Linux aggregation host, acceptable battery logistics, and that a shovel load is a meaningful sample of the target waste stream. None is accepted without local validation.

### Safety constraints

- Do not lift or drop test with people in the load/impact zone.
- Do not field-handle contaminated waste with exposed breadboards, wiring, batteries, or absorbent/unsealed structures.
- Do not infer a safe working load from the pitch photograph or the load cell’s eventual electrical rating; the shovel structure, fasteners, handle, and load-cell mounting all govern safety.
- Do not connect the historical 8×AA/7805 system or 5 V amplifier output to the Nano until a schematic, regulator thermal calculation, common-ground plan, polarity protection, and ADC-input measurements are reviewed.
- An enclosure must remain inspectable/repairable without weakening the load path, trapping water, or creating snag/sharp hazards. “Permanently attached” and “easy to open for repairs” need a service-design resolution.

### Privacy and governance constraints

Exact timestamped GNSS trails can reveal worker movements, routes, facilities, and operational patterns even when no name is stored. SD removal also makes copying easy. Before any field trial, the data controller, purpose, lawful/consensual participation basis, precision, access, retention, deletion, breach procedure, and aggregation threshold must be agreed with local stakeholders. Public maps should use the least spatial/temporal precision that answers the policy question. Wi-Fi/network export remains out of scope until authentication, encryption, and credential provisioning are designed.

### Technical limitations of the current code

- The hardware adapter and its pinned dependencies compile for `nanorp2040connect`; no physical Nano or connected peripheral was available, so this is not evidence that it runs correctly on the assembly.
- Core logic covers same-unit orientation correction, stable startup tare,
  fail-closed bounded auto-zero, aligned measurement filtering, event
  hysteresis/cooldown/re-arm, component-wise GNSS evidence merge, finite event
  wait, pipeline storage retry, schema-v2 validity/identity, and interruption
  parsing. These deterministic implementations do not validate connected
  sensors or physical thresholds.
- Device ID defaults to `UNCONFIGURED`; production then disables SD logging and
  drops detected events. A fleet-unique, non-personal untracked configuration is
  required before supervised collection.
- The orientation coefficient’s signal unit and portability remain provisional, and the mass conversion is deliberately unverified. The adapter records the numeric legacy conversion with `mass_calibration_status=provisional` and uses it for software/bench event thresholds. Field interpretation is blocked until a traceable calibration is enabled.
- Startup tare passes `explicitly_unloaded=true` solely from the documented
  operator precondition; there is no button or independent unloaded sensor.
  Runtime auto-zero is always disabled in the adapter, even for a verified mass
  factor, until such independent evidence exists.
- Schema v2 carries device/boot-session/sequence identity, raw averaged ADC, z
  acceleration, corrected signal, uptime, GNSS age/satellites/status, a separate
  wait-timeout flag, and calibration status. It does not include measured
  reference, tare/calibration ID, firmware revision, or material type.
- GNSS evidence is merged by component while an event waits, so a later partial
  update cannot erase a better location or UTC component. This is host-tested;
  installed UART/module behavior is not.
- Pending events exist only in a four-entry RAM queue. Reset or power loss before
  a successful append loses those events. A queue overflow increments the drop
  counter and latches the storage-warning pulse code until reboot.
- `events.csv` is not rotated. With pinned SD 1.3.0, close/reopen append seeks
  through the FAT chain, so latency can grow with file size. Archive or replace
  cards at a deployment-defined bounded interval until measured limits and file
  rotation are implemented.
- The logger flushes/closes each completed row, inserts a line boundary after a
  torn tail, assigns a new random boot session after reset, and retains one
  sequence across same-boot retries. Write atomicity, partial-row behavior,
  identity collision risk, full-media faults, and recovery still require real
  SD/power-interruption testing.
- Production status is one external active-high D2 LED, not the pitch's onboard
  RGB path. Wiring, resistor/polarity, pulse visibility, and operator response
  are unvalidated.
- The historical host notebook is stale. Maintained analysis reproduces only the documented orientation relationship; raw datasets still lack metadata and known-mass truth needed for metrology.
- Historical defects—indefinite GNSS startup, ignored IMU freshness, repeated fixes across 65 rows, and fatal LED loops—remain evidence about what must not be reintroduced, not accepted production behavior.

## Requirements traceability

Multiple status values may apply. For example, `implemented; hardware-validation-required` means code exists but is not accepted on hardware.

| Requirement | Evidence / implementation or validation | Status |
|---|---|---|
| F-ARCH-01 | `platformio.ini`, `implementation-plan.md`, and `legacy/README.md` select one Nano and exclude legacy/examples; pure core and one `src/main.cpp` adapter exist | `implemented; host-tested; compile-tested` |
| F-BOOT-01 | SD/IMU initialization and retries, nonblocking GNSS polling/UART-silence diagnostics, and degraded pulse states exist; ADC plausibility self-test and physical GNSS transport proof remain missing | `implemented; compile-tested; hardware-validation-required` |
| F-WGT-01 | Config selects A0/16 bits/3,300 mV; adapter averages raw ADC, core converts counts before correction, and event rows retain raw ADC/z; physical reference/range remain pending | `implemented; host-tested; compile-tested; hardware-validation-required` |
| F-WGT-02 | Core rejects invalid/moving/unstable tare windows; adapter assumes the operator obeyed the empty-head precondition during startup | `implemented; host-tested; compile-tested; hardware-validation-required` |
| F-WGT-03 | `-15` is preserved with `provisional` status for software/bench evaluation; field decisions remain blocked until a traceable factor is configured, and that verification alone still cannot enable auto-zero without unloaded evidence | `implemented; host-tested; compile-tested; hardware-validation-required` |
| F-WGT-04 | Core applies the dimensionally consistent tare-relative formula and rejects invalid samples; calibration analyzer is host-tested, but adapter/physical units remain open | `implemented; host-tested; hardware-validation-required` |
| F-GPS-01 | Core validates coordinates/calendar/freshness and merges location, UTC, altitude, and satellites independently; nonblocking Serial1/TinyGPSPlus adapter captures age and exposes stream silence | `implemented; host-tested; compile-tested; hardware-validation-required` |
| F-GPS-02 | Core represents UTC/freshness and CSV empties invalid optional fields; accuracy evidence remains one historical 6.69-distance screenshot | `implemented; host-tested; compile-tested; documented-only; hardware-validation-required` |
| F-EVT-01 | Core detector and adapter implement aligned filtering, stability, trigger/release hysteresis, latch, cooldown/re-arm, finite GNSS wait, and a four-event RAM queue; overflow is counted/latched, but thresholds and reset-loss behavior need hardware validation | `implemented; host-tested; compile-tested; hardware-validation-required` |
| F-LOG-01 | Schema-v2 formatter/adapter include device/session/sequence identity, uptime, UTC, provisional mass state, corrected signal, raw ADC/z, location, age/satellites, GPS wait/status and health; firmware/calibration/tare IDs remain incomplete | `implemented; host-tested; compile-tested; hardware-validation-required` |
| F-LOG-02 | SD adapter checks the exact schema/boundary, flushes/closes rows, retains an event sequence across retries, and uses a new boot session to avoid restart reuse; RAM-pending loss, growing-file latency, full media, and real interruption remain untested | `implemented; host-tested; compile-tested; hardware-validation-required` |
| F-UI-01 | Seven single-LED pulse/solid patterns and periodic serial diagnostics compile, but this external D2 deviation provides no RGB colors or distinct calibration-invalid pattern and has no operator/hardware validation | `implemented; compile-tested; hardware-validation-required` |
| F-EXP-01 | `tools/validate_events.py` validates schema-v2 rows, ranges, record identity, identical retries, and conflicting payloads; its host tests pass. Aggregation, mapping, and broader export-policy workflows remain deferred | `implemented; host-tested` |
| F-CLS-01 | No classifier implementation | `deferred` |
| N-ELEC-01 | Pitch block/montage and incomplete BOM only; no schematic or measurements | `documented-only; hardware-validation-required` |
| N-SAFE-01 | Prototype photos show exposed assemblies; no safety qualification | `hardware-validation-required` |
| N-STR-01 | Historical 5 kg narrative/static image; maximum explicitly unknown | `documented-only; hardware-validation-required` |
| N-DROP-01 | Historical 1.5 m assertion; `drop.png` does not document a drop result | `documented-only; hardware-validation-required` |
| N-ENV-01 | Case/rain are conceptual/future; exposed prototype | `documented-only; hardware-validation-required` |
| N-DATA-01 | Host regression reproduces some z/voltage correlation and exposes trial inconsistency; no accuracy truth | `host-tested; hardware-validation-required` |
| N-AVAIL-01 | Adapter uses the host-tested pipeline for finite GNSS wait and retry-stable writes, retries IMU at 5 s and SD with 30–300 s backoff, and intentionally latches queue data loss; RAM persistence, file growth, and physical recovery remain open | `implemented; host-tested; compile-tested; hardware-validation-required` |
| N-ERG-01 | Pitch narrative and process graphic only | `documented-only; hardware-validation-required` |
| N-PWR-01 | Historical AA/7805 narrative only | `documented-only; hardware-validation-required` |
| N-PRIV-01 | Schema v2 has non-personal device/session identity and precise location/time; governance, storage protection, retention, and aggregation controls do not yet exist | `implemented; documented-only` |
| N-SEC-01 | Wi-Fi paths are outside production; exposed historical credentials were removed, ignored local examples exist, and targeted secret scanning is a Make/CI gate whose final run is recorded in `implementation-plan.md` | `implemented; documented-only` |
| N-COST-01 | Historical cost records conflict and BOM is incomplete | `documented-only` |
| N-MAINT-01 | PlatformIO pins board/platform/libraries, one production adapter exists, Makefile/CI define gates, `.gitattributes` protects raw evidence bytes, calibration hashes are documented, and native/target builds pass | `implemented; host-tested; compile-tested` |
| Solar power | Pitch final-concept/future language only | `deferred` |
| Aluminum shaft/dustpan and locking ergonomic handle | Pitch final-concept/future language only | `deferred` |
| Protective weather-resistant enclosure | Case rendering only | `deferred; hardware-validation-required` |
| Computer-vision waste classification | Explicitly omitted from prototype | `deferred` |
| Automated heat-map/policy pipeline | Aspiration only | `deferred` |

## Release gates for the next hardware iteration

Before the next instrumented lifting trial:

1. Freeze a complete schematic, wiring/pin table, BOM, board/core/library versions, and single-Nano build command.
2. Bench-test the compile-tested `src/main.cpp` boot/fault behavior on a Nano without a load.
3. Measure every rail and the full A0 signal envelope before connecting the amplifier to the Nano.
4. Verify the core’s same-unit orientation correction and adapter sample-validity handling on hardware, replace provisional `-15` with a versioned known-mass calibration, and log raw/provenance values.
5. Ratify the implemented collection-event definition and schema-v2 fields with deployment/data stakeholders; add calibration/tare/firmware provenance before field use.
6. Execute bench GNSS, SD power-loss, calibrated load, and structural tests with written protocols.
7. Keep the device out of rain and representative-worker workflows until enclosure, safety, privacy, and ergonomic gates pass.
