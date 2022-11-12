(function () {
  "use strict";

  var CSV_HEADER =
    "schema_version,device_id,boot_session_id,event_sequence,event_uptime_ms,timestamp_utc," +
    "mass_g,mass_calibration_status,corrected_signal_mv,raw_adc,accel_z_g," +
    "latitude,longitude,altitude_m,gps_age_ms,satellites,gps_status," +
    "gps_wait_timed_out,system_health";

  var DEVICE_ID = "SS-DEMO";
  var BOOT_SESSION_ID = "9F3A7C10B4D2E681";
  var EVENT_SEQUENCE = 1;
  var EVENT_UPTIME_MS = 2600;
  var ADC_REFERENCE_MV = 3300;
  var ADC_LEVELS = 65536;
  var ORIENTATION_SLOPE = -74.7089168184;
  var PROVISIONAL_GRAMS_PER_MV = -15;
  var TARE_SIGNAL_MV = 1547.5752510790549;
  var TARE_AZ_G = 1;
  var LOAD_THRESHOLD_G = 150;
  var PLAY_INTERVAL_MS = 1800;
  var NOISE_MV = [-1.2, 0.7, -0.5, 1.0, -0.3, 0.5, -0.6, 0.4];

  var steps = [
    {
      title: "Boot and component checks",
      description:
        "The watchdog starts before peripherals. The adapter initializes ADC, IMU, GNSS UART, microSD, diagnostics, and the external status LED independently.",
    },
    {
      title: "Stable startup tare",
      description:
        "With the documented empty-shovel precondition, 32 stable samples establish signal and z-axis tare points. Motion, invalid IMU data, or excessive span restarts the window.",
    },
    {
      title: "Load arrives while the shovel moves",
      description:
        "The raw signal contains deterministic synthetic noise and orientation influence. A moving sample is rejected and clears every aligned evidence filter before stability is assessed again.",
    },
    {
      title: "Filter and qualify one event",
      description:
        "Eight stable samples warm the moving average. Five stable detector updates above 150 g then emit one event; the detector latches to prevent duplicates.",
    },
    {
      title: "Validate GNSS evidence",
      description:
        "Location, UTC, altitude, and satellites are merged independently. Fresh complete evidence can proceed immediately; stale or unavailable evidence waits at most five seconds.",
    },
    {
      title: "Attempt a synchronized CSV append",
      description:
        "The logger reserves one event identity, verifies the exact schema-v2 file boundary, and uses an O_SYNC append. A reported success includes the dependency's data and metadata synchronization.",
    },
    {
      title: "Expose the durable outcome",
      description:
        "A durable row with valid coordinates can become one offline map point. Degraded GNSS still permits a local row; a transient SD failure must recover before any successful row or point is shown.",
    },
    {
      title: "Release, cooldown, and re-arm",
      description:
        "The event detector requires five stable readings at or below 50 g, then a one-second cooldown. Only after that independent lifecycle can another load create an event.",
    },
  ];

  var currentStep = 0;
  var playTimer = null;
  var reducedMotionQuery = window.matchMedia("(prefers-reduced-motion: reduce)");

  var elements = {};

  function byId(id) {
    return document.getElementById(id);
  }

  function cacheElements() {
    elements.playToggle = byId("play-toggle");
    elements.playIcon = elements.playToggle.querySelector(".button-icon");
    elements.playLabel = elements.playToggle.querySelector(".button-label");
    elements.nextStep = byId("next-step");
    elements.reset = byId("reset-demo");
    elements.loadInput = byId("load-input");
    elements.loadOutput = byId("load-output");
    elements.orientationInput = byId("orientation-input");
    elements.orientationOutput = byId("orientation-output");
    elements.failureMode = byId("failure-mode");
    elements.motionPreference = byId("motion-preference");
    elements.stepStatus = byId("step-status");
    elements.stepItems = Array.prototype.slice.call(
      document.querySelectorAll("#step-list [data-step]")
    );
    elements.stepCounter = byId("step-counter");
    elements.panelTitle = byId("sensor-panel-title");
    elements.stepDescription = byId("step-description");
    elements.sensorChartDesc = byId("sensor-chart-desc");
    elements.rawLine = byId("raw-line");
    elements.correctedLine = byId("corrected-line");
    elements.filteredLine = byId("filtered-line");
    elements.rawAdc = byId("raw-adc-value");
    elements.rawSignal = byId("raw-signal-value");
    elements.orientationTerm = byId("orientation-term-value");
    elements.corrected = byId("corrected-value");
    elements.filteredMass = byId("filtered-mass-value");
    elements.eventState = byId("event-state-value");
    elements.ledLamp = byId("led-lamp");
    elements.ledState = byId("led-state");
    elements.ledPattern = byId("led-pattern");
    elements.sensorHealth = byId("sensor-health");
    elements.gpsHealth = byId("gps-health");
    elements.storageHealth = byId("storage-health");
    elements.queueHealth = byId("queue-health");
    elements.eventLog = byId("event-log");
    elements.gpsBadge = byId("gps-status-badge");
    elements.gpsExplanation = byId("gps-explanation");
    elements.gpsTime = byId("gps-time");
    elements.gpsPosition = byId("gps-position");
    elements.gpsAge = byId("gps-age");
    elements.gpsTimeout = byId("gps-timeout");
    elements.csvBadge = byId("csv-status-badge");
    elements.csvOutput = byId("csv-output");
    elements.csvNote = byId("csv-note");
    elements.copyCsv = byId("copy-csv");
    elements.copyStatus = byId("copy-status");
    elements.mapMarker = byId("map-marker");
    elements.mapEmpty = byId("map-empty");
    elements.mapCoordinate = byId("map-coordinate-label");
    elements.mapDesc = byId("map-svg-desc");
  }

  function currentConfiguration() {
    var mass = Number(elements.loadInput.value);
    var accelerationZ = Number(elements.orientationInput.value);
    var correctedSignal = mass / PROVISIONAL_GRAMS_PER_MV;
    var orientationTerm = ORIENTATION_SLOPE * (accelerationZ - TARE_AZ_G);
    var signalMv = TARE_SIGNAL_MV + correctedSignal + orientationTerm;
    var rawAdc = Math.round((signalMv * ADC_LEVELS) / ADC_REFERENCE_MV);

    return {
      mass: mass,
      accelerationZ: accelerationZ,
      correctedSignal: correctedSignal,
      orientationTerm: orientationTerm,
      signalMv: signalMv,
      rawAdc: rawAdc,
      eligible: mass >= LOAD_THRESHOLD_G,
      mode: elements.failureMode.value,
    };
  }

  function gpsFor(config) {
    if (config.mode === "gps-stale") {
      return {
        status: "stale",
        timestamp: "",
        latitude: "",
        longitude: "",
        altitude: "",
        age: "6501",
        satellites: "",
        timedOut: true,
        health: "gps_degraded",
      };
    }
    if (config.mode === "gps-unavailable") {
      return {
        status: "no_fix",
        timestamp: "",
        latitude: "",
        longitude: "",
        altitude: "",
        age: "",
        satellites: "",
        timedOut: true,
        health: "gps_degraded",
      };
    }
    return {
      status: "valid",
      timestamp: "2026-07-15T08:30:01Z",
      latitude: "5.6037000",
      longitude: "-0.1870000",
      altitude: "24.30",
      age: "220",
      satellites: "9",
      timedOut: false,
      health: "healthy",
    };
  }

  function eventWritten(config) {
    if (!config.eligible || currentStep < 5) {
      return false;
    }
    return config.mode !== "sd-failure" || currentStep >= 6;
  }

  function csvRow(config) {
    var gps = gpsFor(config);
    return [
      "2",
      DEVICE_ID,
      BOOT_SESSION_ID,
      String(EVENT_SEQUENCE),
      String(EVENT_UPTIME_MS),
      gps.timestamp,
      config.mass.toFixed(3),
      "provisional",
      config.correctedSignal.toFixed(3),
      String(config.rawAdc),
      config.accelerationZ.toFixed(6),
      gps.latitude,
      gps.longitude,
      gps.altitude,
      gps.age,
      gps.satellites,
      gps.status,
      gps.timedOut ? "true" : "false",
      gps.health,
    ].join(",");
  }

  function setText(element, value) {
    element.textContent = value;
  }

  function setBadge(element, text, className) {
    element.className = "status-badge " + className;
    setText(element, text);
  }

  function setLed(state, pattern, className) {
    elements.ledLamp.className = "led-lamp " + className;
    setText(elements.ledState, state);
    setText(elements.ledPattern, pattern);
  }

  function renderStepNavigation() {
    elements.stepItems.forEach(function (item, index) {
      item.classList.toggle("is-complete", index < currentStep);
      if (index === currentStep) {
        item.setAttribute("aria-current", "step");
      } else {
        item.removeAttribute("aria-current");
      }
    });
    setText(elements.stepCounter, "Step " + (currentStep + 1) + " of " + steps.length);
    setText(elements.panelTitle, steps[currentStep].title);
    setText(elements.stepDescription, steps[currentStep].description);
    elements.nextStep.disabled = currentStep >= steps.length - 1;
  }

  function chartY(value) {
    var minimum = -60;
    var maximum = 30;
    var clamped = Math.max(minimum, Math.min(maximum, value));
    return 38 + ((maximum - clamped) / (maximum - minimum)) * 174;
  }

  function chartPoints(values) {
    return values
      .map(function (value, index) {
        var x = 56 + index * (594 / (values.length - 1));
        return x.toFixed(1) + "," + chartY(value).toFixed(1);
      })
      .join(" ");
  }

  function renderChart(config) {
    var rawDelta = config.signalMv - TARE_SIGNAL_MV;
    var noiseScale = currentStep === 2 ? 3.4 : 1;
    var raw = NOISE_MV.map(function (noise) {
      return rawDelta + noise * noiseScale;
    });
    var corrected = raw.map(function (sample) {
      return sample - config.orientationTerm;
    });
    var mean = corrected.reduce(function (total, sample) {
      return total + sample;
    }, 0) / corrected.length;
    var filtered = corrected.map(function (_sample, index) {
      if (index < 2) {
        return corrected[index];
      }
      var running = corrected.slice(0, index + 1);
      return running.reduce(function (total, sample) {
        return total + sample;
      }, 0) / running.length;
    });

    if (currentStep === 0) {
      elements.rawLine.setAttribute("points", "");
      elements.correctedLine.setAttribute("points", "");
      elements.filteredLine.setAttribute("points", "");
      setText(
        elements.sensorChartDesc,
        "No sensor samples have been accepted during the boot step."
      );
      return;
    }

    if (currentStep === 1) {
      var tareNoise = NOISE_MV.map(function (noise) {
        return noise * 0.6;
      });
      elements.rawLine.setAttribute("points", chartPoints(tareNoise));
      elements.correctedLine.setAttribute("points", chartPoints(tareNoise));
      elements.filteredLine.setAttribute("points", chartPoints(tareNoise));
      setText(
        elements.sensorChartDesc,
        "Eight representative samples from the simulated stable startup tare cluster around zero relative signal."
      );
      return;
    }

    elements.rawLine.setAttribute("points", chartPoints(raw));
    elements.correctedLine.setAttribute("points", chartPoints(corrected));
    elements.filteredLine.setAttribute("points", currentStep >= 3 ? chartPoints(filtered) : "");
    setText(
      elements.sensorChartDesc,
      "Eight deterministic synthetic samples show raw relative signal, the orientation-corrected signal, and " +
        (currentStep >= 3 ? "the running filtered result." : "no accepted filtered result while motion persists.") +
        " The final corrected mean is " + mean.toFixed(3) + " millivolts."
    );
  }

  function renderMetrics(config) {
    if (currentStep === 0) {
      setText(elements.rawAdc, "—");
      setText(elements.rawSignal, "—");
      setText(elements.orientationTerm, "—");
      setText(elements.corrected, "—");
      setText(elements.filteredMass, "—");
      setText(elements.eventState, "idle");
      return;
    }
    if (currentStep === 1) {
      setText(elements.rawAdc, "30734 counts");
      setText(elements.rawSignal, TARE_SIGNAL_MV.toFixed(3) + " mV tare");
      setText(elements.orientationTerm, "0.000 mV");
      setText(elements.corrected, "0.000 mV");
      setText(elements.filteredMass, "0.000 g provisional");
      setText(elements.eventState, "idle · tare ready");
      return;
    }

    setText(elements.rawAdc, String(config.rawAdc) + " counts");
    setText(elements.rawSignal, config.signalMv.toFixed(3) + " mV");
    setText(elements.orientationTerm, config.orientationTerm.toFixed(3) + " mV");
    setText(elements.corrected, config.correctedSignal.toFixed(3) + " mV");
    setText(elements.filteredMass, config.mass.toFixed(3) + " g provisional");

    if (currentStep === 2) {
      setText(elements.eventState, "idle · motion reset");
    } else if (!config.eligible) {
      setText(elements.eventState, "idle · below 150 g");
    } else if (currentStep === 3) {
      setText(elements.eventState, "latched · event emitted");
    } else if (currentStep < 7) {
      setText(elements.eventState, "latched · duplicate blocked");
    } else {
      setText(elements.eventState, "cooldown → idle");
    }
  }

  function renderLedAndHealth(config) {
    var gps = gpsFor(config);
    setText(elements.sensorHealth, currentStep === 0 ? "checking" : "ready (simulated)");
    setText(
      elements.queueHealth,
      config.eligible && currentStep >= 3 && currentStep < 5 ? "1 / 4 events" : "0 / 4 events"
    );

    if (currentStep === 0) {
      setLed(
        "Booting pattern defined",
        "Two 100 ms pulses, then a 600 ms pause; full setup cycle not assured",
        "pattern-booting"
      );
      setText(elements.gpsHealth, "not assessed");
      setText(elements.storageHealth, "checking");
      return;
    }
    if (currentStep === 1) {
      setLed("Startup tare", "500 ms on, 500 ms off", "pattern-calibrating");
      setText(elements.gpsHealth, "stream simulated");
      setText(elements.storageHealth, "available");
      return;
    }

    setText(elements.storageHealth, "available");
    if (!config.eligible && currentStep >= 3) {
      if (gps.status !== "valid") {
        setText(elements.gpsHealth, gps.status + " · no event-specific wait");
        setLed("GNSS degraded", "One 150 ms pulse, then a 1200 ms pause", "pattern-gps");
      } else {
        setText(elements.gpsHealth, "valid · no event requested");
        setLed("Ready", "Solid on; load is below event threshold", "pattern-ready");
      }
      return;
    }

    if (gps.status !== "valid" && currentStep === 2) {
      setText(elements.gpsHealth, gps.status + " · no event-specific wait yet");
      setLed("GNSS degraded", "One 150 ms pulse, then a 1200 ms pause", "pattern-gps");
      return;
    }

    if ((config.mode === "gps-stale" || config.mode === "gps-unavailable") && currentStep === 3) {
      setText(elements.gpsHealth, "waiting for fresh evidence");
      setLed("Waiting for GNSS", "125 ms on, 125 ms off; bounded to 5 s", "pattern-waiting");
      if (config.eligible) {
        setText(elements.queueHealth, "1 / 4 events");
      }
      return;
    }

    if (config.mode === "sd-failure" && currentStep === 5 && config.eligible) {
      setText(elements.gpsHealth, "valid · 220 ms");
      setText(elements.storageHealth, "write failed · retry due in 30 s");
      setText(elements.queueHealth, "1 / 4 events · sequence 1 reserved");
      setLed("Storage degraded", "Three 150 ms pulses, then a 600 ms pause", "pattern-storage");
      return;
    }

    if (gps.status !== "valid" && currentStep >= 4) {
      setText(elements.gpsHealth, gps.status + " · wait timed out");
      setLed("GNSS degraded", "One 150 ms pulse, then a 1200 ms pause", "pattern-gps");
      return;
    }

    setText(elements.gpsHealth, "valid · 220 ms");
    if (config.mode === "sd-failure" && currentStep >= 6) {
      setText(elements.storageHealth, "recovered · retry succeeded");
    }
    setLed("Ready", "Solid on", "pattern-ready");
  }

  function logEntry(text) {
    var item = document.createElement("li");
    item.textContent = text;
    elements.eventLog.appendChild(item);
  }

  function renderEventLog(config) {
    elements.eventLog.textContent = "";
    logEntry("[0.000 s] boot: watchdog and independent peripheral checks");
    if (currentStep >= 1) {
      logEntry("[1.600 s] tare=ready,samples=32,empty_precondition=simulated");
    }
    if (currentStep >= 2) {
      logEntry("[1.650 s] motion=unstable,aligned_filters=reset");
    }
    if (currentStep >= 3) {
      if (config.eligible) {
        logEntry(
          "[2.200 s] event=detected,mass_g=" + config.mass.toFixed(3) + ",pending=1"
        );
      } else {
        logEntry(
          "[2.200 s] event=none,mass_g=" + config.mass.toFixed(3) + ",reason=below_threshold"
        );
      }
    }
    if (!config.eligible) {
      return;
    }
    if (currentStep >= 4) {
      if (config.mode === "gps-stale") {
        logEntry("[7.200 s] gps=stale,age_ms=6501,event_wait_timed_out=true");
      } else if (config.mode === "gps-unavailable") {
        logEntry("[7.200 s] gps=no_fix,event_wait_timed_out=true");
      } else {
        logEntry("[2.200 s] gps=valid,age_ms=220,event_wait_timed_out=false");
      }
    }
    if (currentStep >= 5) {
      if (config.mode === "sd-failure" && currentStep === 5) {
        logEntry("[2.250 s] storage=write_failed,event_sequence=1,pending=1");
      } else {
        var writeTime = config.mode === "gps-stale" || config.mode === "gps-unavailable" ? "7.250" : config.mode === "sd-failure" ? "32.250" : "2.250";
        if (config.mode === "sd-failure") {
          logEntry("[32.200 s] storage=recovered,retry_identity=SS-DEMO/9F3A7C10B4D2E681/1");
        }
        logEntry("[" + writeTime + " s] storage=recorded,event_sequence=1");
      }
    }
    if (currentStep >= 6) {
      if (gpsFor(config).status === "valid") {
        logEntry("[offline] validator-ready row can produce one map point");
      } else {
        logEntry("[offline] row retained locally; no valid coordinate to plot");
      }
    }
    if (currentStep >= 7) {
      logEntry("[detector] release=confirmed,cooldown_ms=1000,rearmed=true");
    }
  }

  function renderGps(config) {
    var gps = gpsFor(config);
    if (!config.eligible && currentStep >= 3) {
      setBadge(elements.gpsBadge, "Not requested", "status-neutral");
      setText(elements.gpsExplanation, "No event was emitted, so no event-specific GNSS wait begins.");
      setText(elements.gpsTime, "—");
      setText(elements.gpsPosition, "—");
      setText(elements.gpsAge, "—");
      setText(elements.gpsTimeout, "false");
      return;
    }
    if (currentStep < 4) {
      setBadge(elements.gpsBadge, "Not assessed", "status-neutral");
      setText(elements.gpsExplanation, "Location and UTC are evaluated independently when an event is emitted.");
      setText(elements.gpsTime, "—");
      setText(elements.gpsPosition, "—");
      setText(elements.gpsAge, "—");
      setText(elements.gpsTimeout, "false");
      return;
    }

    if (gps.status === "valid") {
      setBadge(elements.gpsBadge, "Valid", "status-valid");
      setText(elements.gpsExplanation, "Fresh location and UTC were already available, so storage can proceed without waiting.");
      setText(elements.gpsTime, gps.timestamp);
      setText(elements.gpsPosition, gps.latitude + ", " + gps.longitude + " · 24.30 m");
      setText(elements.gpsAge, gps.age + " ms · 9 satellites");
      setText(elements.gpsTimeout, "false");
      return;
    }

    setBadge(
      elements.gpsBadge,
      gps.status === "stale" ? "Stale" : "No fix",
      "status-warning"
    );
    if (gps.status === "stale") {
      setText(
        elements.gpsExplanation,
        "The retained location was 6501 ms old—beyond the 5000 ms limit. No better evidence arrived during the five-second event wait."
      );
      setText(elements.gpsAge, "6501 ms retained; not serialized as a position");
    } else {
      setText(
        elements.gpsExplanation,
        "No location or UTC became available during the five-second event wait. Local logging can still proceed with empty optional fields."
      );
      setText(elements.gpsAge, "—");
    }
    setText(elements.gpsTime, "empty in CSV");
    setText(elements.gpsPosition, "empty in CSV");
    setText(elements.gpsTimeout, "true");
  }

  function renderCsv(config) {
    var written = eventWritten(config);
    var text = CSV_HEADER;
    if (written) {
      text += "\n" + csvRow(config);
    }
    setText(elements.csvOutput, text);
    elements.copyCsv.disabled = !written;

    if (!config.eligible && currentStep >= 3) {
      setBadge(elements.csvBadge, "No event", "status-neutral");
      setText(elements.csvNote, "The filtered mass did not cross 150 g, so the exact header remains without an event row.");
      return;
    }
    if (config.mode === "sd-failure" && currentStep === 5 && config.eligible) {
      setBadge(elements.csvBadge, "Write failed", "status-error");
      setText(
        elements.csvNote,
        "No successful row is shown. The simulated event remains in the four-entry RAM queue with sequence 1 reserved for retry."
      );
      return;
    }
    if (!written) {
      setBadge(elements.csvBadge, "Not written", "status-neutral");
      setText(elements.csvNote, "The exact header is present, but no event row has been written yet.");
      return;
    }

    setBadge(elements.csvBadge, config.mode === "sd-failure" ? "Recovered + written" : "Written", "status-valid");
    if (config.mode === "gps-stale" || config.mode === "gps-unavailable") {
      setText(
        elements.csvNote,
        "The local row is durable. Invalid GNSS fields are empty; gps_status and gps_wait_timed_out preserve why."
      );
    } else if (config.mode === "sd-failure") {
      setText(
        elements.csvNote,
        "The retry succeeded after simulated SD recovery. Device, boot session, and event sequence are unchanged."
      );
    } else {
      setText(
        elements.csvNote,
        "One deterministic synthetic schema-v2 row is durable and ready for host validation."
      );
    }
  }

  function renderMap(config) {
    var gps = gpsFor(config);
    var written = eventWritten(config);
    var plottable = currentStep >= 6 && written && gps.status === "valid";
    elements.mapMarker.toggleAttribute("hidden", !plottable);
    elements.mapEmpty.toggleAttribute("hidden", plottable);

    if (plottable) {
      setText(elements.mapCoordinate, gps.latitude + ", " + gps.longitude);
      setText(
        elements.mapDesc,
        "One synthetic event marker is plotted at latitude " + gps.latitude + " and longitude " + gps.longitude + " after a durable schema-v2 row."
      );
      return;
    }

    if (!config.eligible && currentStep >= 3) {
      setText(elements.mapEmpty, "No event was emitted");
      setText(elements.mapDesc, "No map point exists because the simulated load did not emit an event.");
    } else if (config.mode === "sd-failure" && currentStep === 5) {
      setText(elements.mapEmpty, "Waiting for the durable SD retry");
      setText(elements.mapDesc, "No map point exists while the event is only pending in RAM after an SD write failure.");
    } else if (written && gps.status !== "valid") {
      setText(elements.mapEmpty, "Local row written · no valid coordinate to plot");
      setText(elements.mapDesc, "The event is logged locally, but no marker is plotted because GNSS position is not valid.");
    } else if (written && gps.status === "valid") {
      setText(elements.mapEmpty, "Durable row ready · advance to plot the outcome");
      setText(elements.mapDesc, "The durable row has a valid location; the next documentation step plots its synthetic point.");
    } else {
      setText(elements.mapEmpty, "Waiting for a durable row with valid location");
      setText(elements.mapDesc, "No event point is plotted before a durable row with valid location exists.");
    }
  }

  function renderMotionPreference() {
    if (reducedMotionQuery.matches) {
      elements.motionPreference.hidden = false;
      setText(
        elements.motionPreference,
        "Reduced-motion preference detected: smooth scrolling and interface transitions are disabled. LED timing is always presented statically in text. Play advances through discrete states; Next provides full manual control."
      );
    } else {
      elements.motionPreference.hidden = true;
      setText(elements.motionPreference, "");
    }
  }

  function render(announce) {
    var config = currentConfiguration();
    elements.loadOutput.value = config.mass.toFixed(3) + " g";
    elements.orientationOutput.value = config.accelerationZ.toFixed(6) + " g";
    renderStepNavigation();
    renderChart(config);
    renderMetrics(config);
    renderLedAndHealth(config);
    renderEventLog(config);
    renderGps(config);
    renderCsv(config);
    renderMap(config);
    renderMotionPreference();
    if (announce) {
      setText(
        elements.stepStatus,
        "Step " + (currentStep + 1) + " of " + steps.length + ": " + steps[currentStep].title
      );
    }
  }

  function setPlaying(playing) {
    if (!playing && playTimer !== null) {
      window.clearInterval(playTimer);
      playTimer = null;
    }
    elements.playToggle.setAttribute("aria-pressed", playing ? "true" : "false");
    setText(elements.playIcon, playing ? "❚❚" : "▶");
    setText(elements.playLabel, playing ? "Pause" : "Play");
  }

  function canAdvance(config) {
    if (currentStep === 3 && !config.eligible) {
      setText(
        elements.stepStatus,
        "No event emitted: simulated load is below the 150 gram threshold. Increase the load to continue."
      );
      setPlaying(false);
      return false;
    }
    return true;
  }

  function advance() {
    var config = currentConfiguration();
    if (!canAdvance(config)) {
      return;
    }
    if (currentStep >= steps.length - 1) {
      setPlaying(false);
      return;
    }
    currentStep += 1;
    render(true);
    if (currentStep >= steps.length - 1) {
      setPlaying(false);
    }
  }

  function togglePlay() {
    if (playTimer !== null) {
      setPlaying(false);
      return;
    }
    if (currentStep >= steps.length - 1) {
      currentStep = 0;
      render(true);
    }
    setPlaying(true);
    playTimer = window.setInterval(advance, PLAY_INTERVAL_MS);
  }

  function reset() {
    setPlaying(false);
    currentStep = 0;
    elements.loadInput.value = "412.75";
    elements.orientationInput.value = "0.9821";
    elements.failureMode.value = "normal";
    render(true);
    elements.playToggle.focus();
  }

  function configurationChanged() {
    setPlaying(false);
    if (currentStep > 2) {
      currentStep = 2;
    }
    render(true);
  }

  function copyCsv() {
    var text = elements.csvOutput.textContent;
    if (!text || elements.copyCsv.disabled) {
      return;
    }
    if (navigator.clipboard && navigator.clipboard.writeText) {
      navigator.clipboard.writeText(text).then(
        function () {
          setText(elements.copyStatus, "CSV copied to the clipboard.");
        },
        function () {
          setText(elements.copyStatus, "Clipboard access was unavailable. Select the CSV text manually.");
        }
      );
    } else {
      setText(elements.copyStatus, "Clipboard access is unsupported. Select the CSV text manually.");
    }
  }

  function bindEvents() {
    elements.playToggle.addEventListener("click", togglePlay);
    elements.nextStep.addEventListener("click", advance);
    elements.reset.addEventListener("click", reset);
    elements.loadInput.addEventListener("input", configurationChanged);
    elements.orientationInput.addEventListener("input", configurationChanged);
    elements.failureMode.addEventListener("change", configurationChanged);
    elements.copyCsv.addEventListener("click", copyCsv);
    if (typeof reducedMotionQuery.addEventListener === "function") {
      reducedMotionQuery.addEventListener("change", function () {
        renderMotionPreference();
      });
    } else if (typeof reducedMotionQuery.addListener === "function") {
      reducedMotionQuery.addListener(renderMotionPreference);
    }
  }

  function initialize() {
    cacheElements();
    bindEvents();
    setPlaying(false);
    render(false);
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", initialize);
  } else {
    initialize();
  }
})();
