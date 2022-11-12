# Smart Shovel visual guide

Smart Shovel turns a familiar hand tool into a location-aware embedded sensing
platform. The project combines load sensing, inertial compensation, GNSS, durable
storage, deterministic event detection, and a versioned data contract in one
firmware architecture.

## Project prototype

The prototype makes the complete sensing stack visible: sensing, positioning,
storage, controller, and supporting electronics are mounted directly along the
shovel shaft for an easy-to-follow engineering demonstration.

<table>
  <tr>
    <td width="62%">
      <img src="assets/smart-shovel-prototype-overview.webp" width="1600" height="900" alt="Smart Shovel prototype laid horizontally, with a black shovel head, wooden shaft, T-handle, and breadboard electronics mounted near the head.">
      <br><strong>Complete Smart Shovel prototype.</strong> The exposed layout makes each subsystem easy to identify.
    </td>
    <td width="38%">
      <img src="assets/smart-shovel-upright-prototype.webp" width="750" height="1199" alt="Smart Shovel prototype standing upright with its electronics mounted along the shaft and an orange cat beside it.">
      <br><strong>Upright prototype view.</strong> A full-length view of the sensing assembly and tool form factor.
    </td>
  </tr>
</table>

![Close view of the breadboards and wiring mounted on the Smart Shovel, including controller, GNSS, storage, analog electronics, and battery assemblies.](assets/smart-shovel-electronics-closeup.webp)

**Electronics close-up.** The modular layout exposes the signal chain and makes
the hardware architecture easy to explain subsystem by subsystem.

![Red CAD rendering of a rectangular electronics enclosure with a hinged lid.](assets/historical-enclosure-concept.webp)

**Enclosure concept.** The CAD study explores how the electronics could be
packaged as a compact shaft-mounted module.

## System architecture

![Smart Shovel architecture: load-cell analog path and onboard IMU feed one Nano RP2040 Connect; GNSS evidence joins a queued event; schema-v2 records are synchronously written to microSD and then validated by host tooling.](assets/system-architecture.svg)

One Nano RP2040 Connect coordinates the complete pipeline. Analog load-cell data
and onboard IMU measurements feed aligned filters and a deterministic event
detector. Each accepted event receives GNSS evidence, a retry-stable identity,
and an exact schema-v2 representation before an `O_SYNC` microSD append. An
external D2 LED and nonblocking USB diagnostics expose device health.

## Collection cycle and data outcome

![Collection cycle from empty-shovel startup tare through stable measurement, orientation correction, event qualification, GNSS evidence, schema-v2 SD writing, validation, and an aggregation preview.](assets/collection-cycle.svg)

The event pipeline applies four aligned eight-sample filters, motion gates,
five-sample trigger and release confirmation, a four-entry FIFO, and a bounded
five-second GNSS window. The result is a durable, validated event row ready for
analysis and mapping workflows.

## Firmware states and visible status

![Operating-state diagram showing boot and component checks, stable empty-shovel tare, ready acquisition, filtering and event latch, bounded GNSS wait, synchronized storage, recovery, release, and cooldown.](assets/operating-states.svg)

![External D2 LED definitions for booting, calibrating, ready, GNSS wait, GNSS status, storage status, and sensor status.](assets/status-indicators.svg)

Three independent state systems keep responsibilities clear: a priority-based
status selector, a duplicate-resistant collection detector, and a per-event
GNSS/storage lifecycle. The single external LED communicates the selected device
status through distinct pulse counts and timings, while the diagrams expose the
detector and event lifecycles in full.

## Orientation model and demo mass conversion

![Calibration analysis showing the calib1 voltage-versus-z-acceleration relationship, fitted slope, model statistics, tare-relative correction, and the demo mass conversion.](assets/orientation-correction.svg)

The checked-in `calib1.csv` dataset contains 618 usable rows and produces an OLS
slope of `-74.7089168` with `R² = 0.9151767`. Firmware combines that coefficient
with a stable 32-sample startup tare to compensate the load signal for shovel
orientation. The interactive walkthrough then applies the project’s demo
`-15 g/mV` mass conversion to exercise thresholding and record generation.

## GNSS evidence at event time

![GNSS behavior matrix comparing valid, location-only, stale, and unavailable evidence, with optional CSV fields and the separate gps_wait_timed_out flag.](assets/gps-behavior.svg)

Location, UTC, altitude, and satellite data are merged independently during a
five-second event window. The component-wise merge retains the strongest
snapshot seen for each field, while `gps_status` and `gps_wait_timed_out` preserve
exactly how the record was assembled.

## Interactive collection-cycle demonstration

Open [`demo/index.html`](demo/index.html) to explore the collection cycle with
Play/Pause, Next, Reset, adjustable demo load and z-axis acceleration, and
GNSS/SD recovery scenarios. The walkthrough uses deterministic sample readings
so every control path and schema-v2 row is reproducible.

Serve the `docs/` directory locally and open `http://localhost:8000/demo/`:

```sh
python3 -m http.server 8000 --directory docs
```

The committed [`demo/smoke.html`](demo/smoke.html) page drives a successful
event and a GNSS-unavailable event in a same-origin fixture. It reports six
browser assertions covering the schema-v2 row and valid-location-only map rule.

The same static files are ready for GitHub Pages through **Settings → Pages →
Deploy from a branch → `main` → `/docs`**.
