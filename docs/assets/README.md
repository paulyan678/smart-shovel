# Visual asset provenance

The web assets in this directory explain the Smart Shovel while preserving the
distinction between physical evidence, current firmware, simulation, and future
work. The original pitch ZIP is intentionally not committed.

## Photographs and rendering

| Repository asset | Source | Treatment | Truth status |
| --- | --- | --- | --- |
| `smart-shovel-prototype-overview.webp` | Previously tracked `docs/img_1.png` | Re-encoded at 1600×900, metadata omitted | Historical 2022 prototype photograph |
| `smart-shovel-electronics-closeup.webp` | Previously tracked `docs/img_2.png` | Cropped to the electronics, gallery overlay/unrelated background removed, re-encoded at 1600×779 | Historical 2022 prototype photograph |
| `smart-shovel-upright-prototype.webp` | Pitch archive `cat.jpeg` | Cropped, resized to 750×1199, and re-encoded without embedded camera or location metadata | Historical 2022 prototype photograph |
| `historical-enclosure-concept.webp` | Pitch archive `WhatsApp Image 2022-04-17 at 10.11.47 PM.jpeg` | Resized to 1200×856 and re-encoded without metadata | Historical, unbuilt CAD concept—not a physical case |

The old generic PNG names were removed after conversion so the repository does
not retain duplicate multi-megabyte copies. Do not restore archive metadata or
copy screenshots containing exact historical coordinates into public docs.

## Explanatory SVGs

The SVGs are new documentation derived from maintained firmware,
`docs/project-requirements.md`, and preserved calibration data. They replace the
pitch's visually useful but electrically ambiguous or outdated diagrams. Each
SVG contains a title and description and uses text/shape cues in addition to
color.

- `system-architecture.svg`: current single-controller boundaries plus separated historical/deferred concepts.
- `collection-cycle.svg`: current collection-event and offline validation flow.
- `operating-states.svg`: operational sequence and degraded/recovery branches.
- `status-indicators.svg`: external D2 mode definitions, including the boot-servicing caveat from `src/main.cpp`.
- `orientation-correction.svg`: real `calib1.csv` relationship and provisional model limits.
- `gps-behavior.svg`: valid, partial, stale, and unavailable GNSS evidence behavior.
