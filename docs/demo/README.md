# Collection-cycle demo

The demo is a deterministic documentation model of the current firmware flow. It uses synthetic
sensor and Accra GNSS values; it is not live telemetry and does not claim a new hardware test.

Serve the documentation from the repository root so browser security and relative paths match the
published layout:

```sh
python3 -m http.server 8000 --directory docs
```

Then open:

- [the interactive demo](http://127.0.0.1:8000/demo/)
- [the zero-dependency browser smoke test](http://127.0.0.1:8000/demo/smoke.html)

The smoke page drives the normal path and a GNSS-unavailable path in a same-origin fixture. A pass
means the demo produced the expected schema-v2 rows and exposed a map point only for valid GNSS; it
does not replace firmware unit, native, or board-build gates.
