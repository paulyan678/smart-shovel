# Prototype archive

This directory preserves the project's design evolution while keeping the
maintained firmware path focused and reproducible.

- `firmware/2022-prototype/` contains the original single-board sketches and
  helper modules.
- `experiments/` contains audio, Wi-Fi, and master/slave explorations that are
  outside the MVP.
- `analysis/` contains the original interactive notebook and its cached results.

The accepted production architecture uses one Arduino Nano RP2040 Connect.
Nothing under `legacy/` is compiled by `platformio.ini`; the archive instead
shows the experiments that informed the single-controller design.

Hard-coded Wi-Fi credentials were removed from the archived `wlab3` experiment.
Its `secrets.example.hpp` template and repository-wide ignore rule demonstrate
the local-secret workflow used by the project.
