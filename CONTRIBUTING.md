# Contributing

Contributions should keep the production firmware small, testable, and honest
about what has and has not been validated on physical hardware.

## Set up

Use Python 3.11 or later and a C++17 compiler:

```sh
python3 -m venv .venv
. .venv/bin/activate
make setup
```

PlatformIO Core and both PlatformIO platforms are version-pinned. Native tests
use the host compiler, so Linux also needs `build-essential`; formatting and
static checks need `clang-format` and `cppcheck`.

## Before proposing a change

Run the same gates as CI:

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

`make verify` runs the complete sequence. Hardware-dependent changes must also
state the exact board, wiring, procedure, and observed result. Never describe a
compile check or host test as physical validation.

Visual documentation must label historical evidence, simulation, provisional
units, and deferred work explicitly. Keep the framework-free demo usable by
keyboard and at phone widths, preserve reduced-motion behavior, and do not add
exact historical coordinates or camera metadata from the pitch archive.

Keep Arduino calls in the production adapter and put deterministic domain logic
in `lib/smart_shovel_core`. Add or update Unity tests for behavior changes. Run
`make format` for maintained C/C++ rather than manually restyling archived code.

Calibration files under `calibration/data/raw` are immutable evidence. Add a new
capture with provenance instead of editing an existing file. Do not commit local
credentials, `.env` files, or any `secrets.hpp`; copy the relevant
`secrets.example.hpp` locally when an archived experiment requires one. The
credentials removed from the 2022 prototype must be treated as exposed and
rotated rather than restored.

Keep pull requests focused, explain assumptions and hardware limitations, and
include the commands and results used to verify the change.
