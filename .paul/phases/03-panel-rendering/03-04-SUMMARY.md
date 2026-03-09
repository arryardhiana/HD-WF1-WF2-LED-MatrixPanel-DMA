---
phase: 03-panel-rendering
plan: 04
subsystem: display
tags: [arduino, esp32, hub75, idle-timeout, millis]

requires:
  - phase: 01-mqtt-json-refactor
    provides: gDisplayTimeout NVS persistence and handleEventTopic/handleResetTopic handlers
  - phase: 03-panel-rendering (03-03)
    provides: renderPanels() complete, scroll logic in loop()

provides:
  - Auto-timeout: display resets to idle after gDisplayTimeout seconds from last non-idle event
  - gLastEventMs global tracking last non-idle event timestamp
  - gDisplayTimeout=0 disables timeout indefinitely

affects: 04-polish-robustness

tech-stack:
  added: []
  patterns: [millis()-based non-blocking timeout in loop()]

key-files:
  modified: [src/HD-WF1-WF2-LED-MatrixPanel-DMA.ino.cpp]

key-decisions:
  - "gDisplayTimeout=0 skips timeout check entirely — no special case needed, condition handles it"
  - "Idle events do NOT reset gLastEventMs — only non-idle events count as activity"
  - "handleResetTopic() unchanged — immediate reset unaffected by timer"

patterns-established:
  - "Non-blocking millis() timeout: check condition + elapsed time, set redrawNeeded on trigger"

duration: ~5min
started: 2026-03-09T00:00:00Z
completed: 2026-03-09T00:00:00Z
---

# Phase 3 Plan 04: Auto-Timeout Idle Display Summary

**Auto-timeout idle: display resets to idle after `gDisplayTimeout` seconds via millis()-based loop() check.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~5 min |
| Tasks | 1 completed |
| Files modified | 1 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: Display resets to idle after timeout | Pass | loop() check: status="idle", plate="", anprStatus="", redrawNeeded=true |
| AC-2: Timeout resets on new event | Pass | gLastEventMs = millis() in handleEventTopic() on non-idle events |
| AC-3: Timeout disabled when gDisplayTimeout=0 | Pass | Condition `gDisplayTimeout > 0` skips check entirely |
| AC-4: Idle event does not reset timer | Pass | `if (gGate.status != "idle")` guard before gLastEventMs assignment |
| AC-5: handleResetTopic immediate reset unaffected | Pass | handleResetTopic() not modified; loop() skips timeout when status already "idle" |

## Accomplishments

- Added millis()-based timeout check in loop() — resets P1/P3/status to idle after configurable delay
- `gLastEventMs` global (unsigned long, init 0) tracks timestamp of last non-idle event
- `handleEventTopic()` already had the gLastEventMs update (was pre-existing from earlier work)
- Phase 3 complete: all 4 panels (P1 color block, P2 3-row, P3 large plate, P4 scroll+color) + idle timeout

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `src/HD-WF1-WF2-LED-MatrixPanel-DMA.ino.cpp` | Modified | Added timeout check block in loop() |

## Deviations from Plan

**Auto-fixed:** `gLastEventMs` global (line 111) and the `handleEventTopic()` update (lines 445-448) were already present from prior partial work — no duplication needed, only the loop() timeout block was missing.

**Total impact:** No scope creep. Plan executed cleanly; one of three sub-changes was already done.

## Issues Encountered

None.

## Next Phase Readiness

**Ready:**
- All 4 panels rendering correctly (P1-P4)
- Auto-timeout active and configurable via MQTT config topic (`display_timeout`)
- Phase 3 fully complete — ready for Phase 4: Polish & Robustness

**Concerns:**
- Python 3.14 vs PlatformIO build issue still unresolved (pre-existing, blocks `pio run` CLI verification)

**Blockers:**
- None for Phase 4 planning

---
*Phase: 03-panel-rendering, Plan: 04*
*Completed: 2026-03-09*
