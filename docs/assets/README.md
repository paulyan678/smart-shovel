# Visual asset catalog

The web assets in this directory present the Smart Shovel's physical prototype,
firmware architecture, signal-processing pipeline, and interactive demonstration.
Each optimized image is metadata-free, and every SVG includes an accessible title
and description.

## Photographs and rendering

| Repository asset | Source | Treatment | Portfolio role |
| --- | --- | --- | --- |
| `smart-shovel-prototype-overview.webp` | Earlier repository photograph | Re-encoded at 1600×900 | Full prototype overview |
| `smart-shovel-electronics-closeup.webp` | Earlier repository photograph | Cropped to the electronics and re-encoded at 1600×779 | Controller and sensor integration |
| `smart-shovel-upright-prototype.webp` | Project archive photograph | Cropped, resized to 750×1199, and re-encoded | Upright form-factor view |
| `historical-enclosure-concept.webp` | Project archive rendering | Resized to 1200×856 and re-encoded | Enclosure design concept |

The optimized WebP files replace the earlier multi-megabyte source copies while
keeping the documentation fast to load.

## Explanatory SVGs

The SVGs are derived from maintained firmware, the engineering specification,
and the preserved calibration dataset. Text and shape cues supplement color for
clear, accessible reading.

- `system-architecture.svg`: controller boundary, sensor buses, and data paths.
- `collection-cycle.svg`: deterministic acquisition, correction, event, and storage flow.
- `operating-states.svg`: operational sequence, fault handling, and recovery paths.
- `status-indicators.svg`: seven external D2 status-mode definitions.
- `orientation-correction.svg`: the `calib1.csv` regression and correction model.
- `gps-behavior.svg`: valid, partial, stale, and unavailable GNSS state handling.
