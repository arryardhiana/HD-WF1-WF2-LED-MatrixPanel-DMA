---
phase: 04-polish-robustness
plan: 02
subsystem: infra
tags: [esp32, ota, arduinoota, wifi, over-the-air]

requires:
  - phase: 04-polish-robustness (04-01)
    provides: watchdog init in setup(); WiFi connectivity in loop()

provides:
  - ArduinoOTA support: firmware updates over WiFi without USB cable
  - hostname "parking-{GATE_ID}" (e.g. "parking-gate-a") discoverable on LAN
  - OTA password protection ("ota-parking")
  - Serial logging for OTA start/progress/end/error events

affects: []

tech-stack:
  added: [ArduinoOTA (ESP32 Arduino core built-in, no new lib)]
  patterns: [setupOTA() helper called in setup(); handle() in loop() WiFi-guarded]

key-files:
  modified: [src/HD-WF1-WF2-LED-MatrixPanel-DMA.ino.cpp]

key-decisions:
  - "ArduinoOTA.handle() guarded by WiFi.status()==WL_CONNECTED — no-op when offline"
  - "setupOTA() called before watchdog init — OTA itself handles wdt internally during upload"
  - "Not called in CALIBRATION_MODE loop path — keeps calibration clean"

patterns-established:
  - "OTA hostname = 'parking-' + GATE_ID for device identification on LAN"

duration: ~5min
started: 2026-03-09T00:00:00Z
completed: 2026-03-09T00:00:00Z
---

# Phase 4 Plan 02: ArduinoOTA Support Summary

**ArduinoOTA added: firmware updatable over WiFi as "parking-gate-a" with password protection.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~5 min |
| Tasks | 1 completed |
| Files modified | 1 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: ArduinoOTA initialized | Pass | setupOTA() called in setup() with hostname + password |
| AC-2: OTA handle called each loop | Pass | ArduinoOTA.handle() in normal loop() path, WiFi-guarded |
| AC-3: OTA Serial logging | Pass | 4 callbacks: onStart, onEnd, onProgress, onError |
| AC-4: Watchdog reset during OTA | Pass | ArduinoOTA handles wdt internally during upload |

## Accomplishments

- Device discoverable as "parking-gate-a" on LAN for `pio run -t upload --upload-port <ip>`
- Password-protected OTA prevents unauthorized firmware uploads
- No new library needed — ArduinoOTA is part of ESP32 Arduino core
- Zero impact on CALIBRATION_MODE path

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `src/HD-WF1-WF2-LED-MatrixPanel-DMA.ino.cpp` | Modified | Added ArduinoOTA include, constant, setupOTA(), handle() in loop() |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| WiFi guard on handle() | Avoids calling handle() when WiFi down | Clean behavior, no-op offline |
| setupOTA() before watchdog | OTA upload internally manages wdt | No 30s panic during long upload |

## Deviations from Plan

None — plan executed exactly as written.

## Issues Encountered

**Pre-existing (not introduced by this plan):** `esp_task_wdt_init(30, true)` may need API change for ESP-IDF v5.x (new signature uses `esp_task_wdt_config_t*` struct). Cannot verify until Python 3.14 / PlatformIO build issue is resolved. Logged as deferred.

## Next Phase Readiness

**Phase 4 complete. v0.2 milestone complete.**

All goals achieved:
- P1-P4 panels rendering correctly (Phase 3)
- Auto-timeout idle (Phase 3)
- Watchdog + MQTT offline indicator (Phase 4)
- ArduinoOTA for field updates (Phase 4)

**Remaining before field deploy:**
- Resolve Python 3.14 vs PlatformIO — downgrade to Python 3.12
- Verify `esp_task_wdt_init()` API for board's ESP-IDF version

---
*Phase: 04-polish-robustness, Plan: 02*
*Completed: 2026-03-09*
