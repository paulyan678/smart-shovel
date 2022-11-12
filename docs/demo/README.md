# Collection-cycle demo

This deterministic walkthrough presents the Smart Shovel firmware pipeline from
sensor acquisition to a durable, location-aware schema-v2 event. Adjustable
sample values make the normal, GNSS recovery, and storage retry paths easy to
explore and reproduce.

Serve the documentation from the repository root:

```sh
python3 -m http.server 8000 --directory docs
```

Then open:

- [the interactive demo](http://127.0.0.1:8000/demo/)
- [the zero-dependency browser smoke test](http://127.0.0.1:8000/demo/smoke.html)

The smoke page drives the normal path and a GNSS-unavailable path in a
same-origin fixture. Its six assertions verify schema-v2 output and the rule
that a map point appears only for a durable event with valid coordinates.
