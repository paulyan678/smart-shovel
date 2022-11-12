# Smart Shovel visual guide

This guide explains the product without treating a simulation, a 2022 photograph,
or a compile-tested code path as new physical evidence. The complete engineering
baseline remains in [`project-requirements.md`](project-requirements.md).

## Evidence labels

| Label | Meaning in these visuals |
| --- | --- |
| **Current** | Implemented in maintained firmware and covered by host tests or a target compile. |
| **Provisional** | Implemented, but the physical units, thresholds, or behavior still require hardware validation. |
| **Historical 2022** | A photograph, rendering, or claim from the original design pitch; not re-tested here. |
| **Simulated** | A deterministic teaching example, not live telemetry or a field measurement. |
| **Deferred** | A future concept that is not implemented in the current repository. |

## Historical prototype

The photographs below show the exposed 2022 prototype. They do not demonstrate
that the refined firmware has run on this assembly or that the shovel is safe,
weatherproof, calibrated, or ready for field work.

<table>
  <tr>
    <td width="62%">
      <img src="assets/smart-shovel-prototype-overview.webp" width="1600" height="900" alt="Historical 2022 Smart Shovel laid horizontally, with a black shovel head, wooden shaft, T-handle, and exposed breadboard electronics strapped near the head.">
      <br><strong>Historical 2022 — complete prototype.</strong> The electronics are exposed and the physical assembly has not been revalidated.
    </td>
    <td width="38%">
      <img src="assets/smart-shovel-upright-prototype.webp" width="750" height="1199" alt="Historical Smart Shovel standing upright with exposed electronics strapped to its shaft and an orange cat beside the shovel.">
      <br><strong>Historical 2022 — upright view.</strong> Re-encoded from the pitch archive with embedded camera and location metadata removed.
    </td>
  </tr>
</table>

![Close view of exposed breadboards and wiring strapped to the historical Smart Shovel, including controller, GNSS, storage, analog electronics, and battery assemblies.](assets/smart-shovel-electronics-closeup.webp)

**Historical 2022 — electronics close-up.** The photograph documents a prototype
assembly, not an approved schematic or production wiring reference.

![Historical red CAD rendering of a rectangular electronics enclosure with a hinged lid.](assets/historical-enclosure-concept.webp)

**Historical 2022 concept — unbuilt enclosure rendering.** It is not evidence of
a manufactured case, ingress protection, or rain suitability.

## Current system architecture

![Current Smart Shovel architecture: load-cell analog path and onboard IMU feed one Nano RP2040 Connect; GNSS evidence joins a queued event; schema-v2 records are synchronously written to microSD and later host-validated. Historical power and RGB concepts and deferred mapping are separated.](assets/system-architecture.svg)

The current application uses one Nano RP2040 Connect and an external, active-high
D2 status LED. The pitch's two-Nano BOM quantity, onboard RGB indication, and
8×AA-to-7805 power concept remain historical evidence rather than current wiring
authority. SD operations are synchronous; the watchdog bounds a dependency stall
by resetting the board, which can lose RAM-only events.

## Collection cycle and data outcome

![Collection cycle from empty-shovel startup tare through stable measurement, orientation correction, event qualification, bounded GNSS evidence, schema-v2 SD write, validation, and a deferred multi-device map.](assets/collection-cycle.svg)

The firmware ends at a validated per-device CSV export. Multi-device aggregation,
policy analysis, and field-scale heat maps remain deferred. The demo's point plot
is a simulated explanation of the intended downstream use, not an implemented
mapping service or an Accra field dataset.

## Firmware states and visible status

![Operating-state diagram showing boot and component checks, stable empty-shovel tare, ready acquisition, filtering and event latch, bounded GNSS wait, synchronized storage, recovery, release, and cooldown, with degraded branches.](assets/operating-states.svg)

![External D2 LED definitions: a boot double-pulse that is selected but not continuously serviced during setup, plus six loop-serviced modes for calibrating, ready, GNSS wait, GNSS degraded, storage degraded, and sensor fault.](assets/status-indicators.svg)

The current status output is a single external LED, so meaning comes from pulse
count and timing rather than color. There is no dedicated successful-write flash:
serial reports `storage=recorded,event_sequence=…`, then a healthy device returns
to solid ready. The boot double-pulse is defined, but setup calls the renderer
only once before the main loop selects another mode, so the full waveform is not
currently guaranteed. Wiring, visibility, and operator interpretation remain
untested.

## Orientation evidence and provisional mass

![Calibration evidence plot showing the real calib1 voltage-versus-z-acceleration relationship, fitted slope, uncertainty notes, tare-relative correction, and the unsupported provisional grams conversion.](assets/orientation-correction.svg)

The plotted relationship comes from the preserved `calib1.csv` capture. Its 618
usable rows support the software model but not calibrated grams: signal units and
trial provenance are incomplete, the four captures overlap, and no raw signal is
paired with traceable known masses. Runtime auto-zero remains disabled because
the current shovel cannot independently prove that it is unloaded.

## GNSS evidence at event time

![GNSS behavior matrix comparing valid, location-only, stale, and unavailable evidence, with optional CSV fields and the separate gps_wait_timed_out flag.](assets/gps-behavior.svg)

Location, UTC, altitude, and satellite evidence are merged independently during
a bounded five-second event window. A timeout does not overwrite the retained
evidence status; it is recorded separately as `gps_wait_timed_out=true`.

## Interactive collection-cycle demonstration

Open [`demo/index.html`](demo/index.html) to explore a deterministic collection
cycle with Play/Pause, Next, Reset, simulated load and z-acceleration controls,
and GNSS/SD failure modes. All generated readings, location, time, CSV rows, and
map marks are labeled **simulated**. The orientation coefficient is derived from
checked-in calibration evidence; the grams conversion remains provisional.

GitHub does not execute JavaScript inside README pages. From the repository root,
serve the `docs/` directory locally and open `http://localhost:8000/demo/`:

```sh
python3 -m http.server 8000 --directory docs
```

The committed [`demo/smoke.html`](demo/smoke.html) page drives a successful
event and a GNSS-unavailable event in a same-origin fixture. It reports six
browser assertions covering the schema-v2 row and valid-location-only map rule.

To publish the same static files with GitHub Pages, a repository administrator
can select **Settings → Pages → Deploy from a branch → `main` → `/docs`**. This
repository includes `.nojekyll` for that configuration, but this guide does not
claim that Pages is currently enabled.
