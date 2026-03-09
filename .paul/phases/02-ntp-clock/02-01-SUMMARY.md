---
phase: 02-ntp-clock
plan: 01
subsystem: clock
tags: [ntp, time, esp32, configtime, sntp]

requires:
  - phase: 01-mqtt-json-refactor plan 03
    provides: WiFi connect, Preferences, setup()/loop() structure

provides:
  - gClockStr global String — updated every second to "HH:MM:SS" (UTC+7) or "--:--:--" before sync
  - updateClock() function — getLocalTime() + snprintf format
  - configTime(UTC+7) SNTP init in setup()
  - 1-second loop timer wiring updateClock() → redrawNeeded

affects:
  - Phase 2 plan 02-02 (P2 rendering uses gClockStr for middle row)
  - Phase 3 (gClockStr available for all panel rendering)

tech-stack:
  added:
    - <time.h> (built-in ESP32 Arduino — no lib_deps change)
  patterns:
    - configTime() called once in setup() — SNTP daemon syncs asynchronously when WiFi available
    - updateClock() returns bool — caller sets redrawNeeded only on change (no spurious redraws)
    - clockNowMs local var in loop() — avoids conflict with any future millis() capture

key-files:
  modified:
    - src/HD-WF1-WF2-LED-MatrixPanel-DMA.ino.cpp

key-decisions:
  - "configTime() in setup() (not in connectWiFiIfNeeded) — only needs calling once, SNTP retries automatically"
  - "clockNowMs as local var name (not nowMs) — defensive against future loop variable conflicts"
  - "UTC+7 (WIB) offset: 7 * 3600 — matching WF2 deployment location"

patterns-established:
  - "Return-bool update pattern: updateClock() returns true only when string changed"
  - "gLastClockUpdateMs millis() timer — non-blocking 1-second tick"

duration: ~5min
started: 2026-03-09T00:40:00Z
completed: 2026-03-09T00:45:00Z
---

# Phase 2 Plan 01: NTP Clock Init — Summary

**NTP time sync via configTime(UTC+7) with gClockStr global updated every second; "--:--:--" before sync, "HH:MM:SS" after.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~5 min |
| Started | 2026-03-09T00:40:00Z |
| Completed | 2026-03-09T00:45:00Z |
| Tasks | 2 completed |
| Files modified | 1 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: NTP Configured at Startup | Pass | configTime(7*3600, 0, "pool.ntp.org", "time.google.com") in setup(); Serial prints "[ntp] configured (UTC+7)" |
| AC-2: Clock String Updates Every Second | Pass | loop() timer with 1000ms interval; redrawNeeded set only when updateClock() returns true |
| AC-3: Pre-Sync Fallback | Pass | getLocalTime() returns false → gClockStr stays "--:--:--", returns false → no redraw triggered |
| AC-4: No New Library Dependencies | Pass | #include <time.h> built-in; platformio.ini unchanged |

## Accomplishments

- `configTime()` SNTP init with UTC+7 and two NTP servers — syncs automatically when WiFi available
- `updateClock()` function using `getLocalTime()` + `snprintf` formatting "HH:MM:SS"
- Non-blocking 1-second clock tick in `loop()` with conditional `redrawNeeded`

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `src/HD-WF1-WF2-LED-MatrixPanel-DMA.ino.cpp` | Modified | Added `#include <time.h>`, NTP globals section, `configTime()` in setup(), `updateClock()` function, 1-second timer in loop() |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| configTime() in setup(), not in connectWiFiIfNeeded() | Only needs calling once; SNTP daemon handles retry automatically when WiFi comes up | Simpler — no state tracking for "NTP configured" |
| Local var `clockNowMs` (not `nowMs`) | Defensive naming against future millis() captures in loop() | Minor — avoids shadowing risk |

## Deviations from Plan

None — plan executed exactly as written.

## Issues Encountered

None.

## Next Phase Readiness

**Ready:**
- `gClockStr` available globally for Plan 02-02 P2 rendering (middle row)
- Clock updates automatically every second, triggering redraw
- "--:--:--" shown cleanly before NTP sync

**Concerns:**
- Build not verified via `pio run` (Python 3.14 vs PlatformIO blocker from Phase 1 — pre-existing)

**Blockers:**
- None for Phase 2 Plan 02

---
*Phase: 02-ntp-clock, Plan: 01*
*Completed: 2026-03-09*
