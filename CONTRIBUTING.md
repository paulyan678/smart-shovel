# Contributing

Contributions should strengthen Smart Shovel as a compact, deterministic, and
well-tested embedded-systems reference project.

## Set up

Use Python 3.11 or later and a C++17 compiler:

```sh
python3 -m venv .venv
. .venv/bin/activate
make setup
```

PlatformIO Core, both PlatformIO platforms, the Nano RP2040 Connect target, and
Arduino libraries are version-pinned. Native tests use the host compiler;
formatting and static analysis require `clang-format` and `cppcheck`.

## Architecture conventions

- Keep Arduino and peripheral APIs in the application adapters under `src/`.
- Put deterministic measurement and event behavior in
  `lib/smart_shovel_core/` so it remains host-testable.
- Keep the core allocation-free and preserve wraparound-safe time comparisons.
- Centralize default behavior in `include/smart_shovel/config.hpp`.
- Preserve the 19-field schema-v2 contract or introduce an explicitly versioned
  migration with validator coverage.
- Maintain retry-stable `(device_id, boot_session_id, event_sequence)` identity.
- Add or update native Unity tests for state-machine behavior changes.
- Use focused sketches under `examples/hardware/` for board-level diagnostics.

## Verification

Run the complete CI-equivalent gate before proposing a change:

```sh
make verify
```

Individual stages are also available:

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

For firmware changes, include the relevant test case and serial or hardware
observation. Run `make format` for maintained C/C++ instead of restyling archived
code manually.

## Calibration and data

Treat files under `calibration/data/raw/` as source datasets. Add new captures
with provenance and hashes, then extend the reproducible analysis or tests to
cover them. Keep measurement units explicit in code, output fields, plots, and
documentation.

Schema changes belong in the firmware serializer, host validator, fixture data,
demo, documentation, and tests as one coherent update.

## Documentation and demo

- Keep architecture, control flow, and indicator diagrams synchronized with the
  firmware.
- Keep the framework-free demo keyboard-operable, responsive at phone widths,
  and compatible with reduced-motion preferences.
- Optimize visual assets, remove camera metadata, and keep all documentation
  dependencies local to the repository.
- Run `make docs-check` after changing Markdown, HTML, CSS, JavaScript, SVG, or
  asset paths.

## Configuration and secrets

Commit example configuration only. Keep local device configuration, credentials,
`.env` files, and secret headers outside version control. Use stable,
non-personal device IDs so exported events remain easy to aggregate.

## Pull requests

Keep changes focused and explain the engineering decision, affected state or
data contract, and verification commands. Include screenshots for visual work
and concise before/after evidence for behavior changes.
