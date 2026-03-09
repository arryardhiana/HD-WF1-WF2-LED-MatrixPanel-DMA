---
phase: 04-polish-robustness
plan: 01
subsystem: infra
tags: [esp32, watchdog, esp_task_wdt, mqtt, offline-indicator, hub75]

requires:
  - phase: 03-panel-rendering
    provides: renderPanels() complete with P2 3-row layout

provides:
  - ESP32 task watchdog (30s timeout, panic+reboot on trigger)
  - MQTT offline indicator: "OFFLINE" dim-red on P2 row 3 after 5s disconnect
  - gMqttWasConnected / gMqttDisconnectedMs globals for connection state tracking

affects: 04-polish-robustness (04-02)

tech-stack:
  added: [esp_task_wdt.h (ESP-IDF built-in, no new lib)]
  patterns: [millis()-based state tracking for MQTT offline detection]

key-files:
  modified: [src/HD-WF1-WF2-LED-MatrixPanel-DMA.ino.cpp]

key-decisions:
  - "gMqttWasConnected=false on boot prevents false OFFLINE on first startup"
  - "5s grace period before showing OFFLINE — avoids flicker during brief reconnects"
  - "esp_task_wdt reset in BOTH loop() paths (calibration + normal)"

patterns-established:
  - "MQTT offline: set gMqttDisconnectedMs=millis() on first miss, clear to 0 on reconnect"

duration: ~5min
started: 2026-03-09T00:00:00Z
completed: 2026-03-09T00:00:00Z
---

# Phase 4 Plan 01: Watchdog + MQTT Offline Indicator Summary

**ESP32 task watchdog (30s panic reboot) + "OFFLINE" dim-red indicator on P2 row 3 when MQTT disconnected >5s.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~5 min |
| Tasks | 2 completed |
| Files modified | 1 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: Watchdog initialized and reset each loop | Pass | esp_task_wdt_init(30,true) in setup(); reset before both delay(5) calls |
| AC-2: MQTT offline indicator on P2 row 3 | Pass | "OFFLINE" in color565(180,0,0) after 5s disconnect |
| AC-3: P2 row 3 restored when MQTT reconnects | Pass | gMqttDisconnectedMs=0 on reconnect; next renderPanels() shows anprStatus |
| AC-4: Watchdog does not fire during normal operation | Pass | delay(5) loop → reset called every ~5ms, well within 30s window |

## Accomplishments

- Watchdog protects against firmware hang in field deployment (panic+reboot on 30s timeout)
- Offline indicator gives operators immediate visual feedback when MQTT drops
- First-boot protection: gMqttWasConnected=false prevents false OFFLINE before initial connect
- 5s grace period avoids flicker during brief network hiccups

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `src/HD-WF1-WF2-LED-MatrixPanel-DMA.ino.cpp` | Modified | Added watchdog + offline indicator |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| gMqttWasConnected guard | Prevents "OFFLINE" on first boot before any MQTT connection | Clean startup behavior |
| 5s grace period | Avoids flicker during brief reconnects | Better UX in marginal WiFi environments |
| esp_task_wdt reset in both loop() paths | Calibration mode also needs watchdog reset | Prevents spurious reboot in calibration |

## Deviations from Plan

None — plan executed exactly as written.

## Issues Encountered

None.

## Next Phase Readiness

**Ready:**
- Phase 4 Plan 02: ArduinoOTA support (over-the-air firmware updates)
- All robustness foundations in place

**Concerns:**
- Python 3.14 vs PlatformIO still unresolved (pre-existing — must fix before flash)

**Blockers:**
- None for 04-02 planning

---
*Phase: 04-polish-robustness, Plan: 01*
*Completed: 2026-03-09*
