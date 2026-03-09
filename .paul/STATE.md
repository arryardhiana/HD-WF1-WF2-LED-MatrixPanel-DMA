# Project State

## Project Reference

See: .paul/PROJECT.md (updated 2026-03-09)

**Core value:** Staff parking dapat melihat status akses kendaraan (granted/denied), hasil scan ANPR, nomor plat, dan info gate secara real-time via MQTT.
**Current focus:** v0.2 Parking Gate Display — Phase 3 COMPLETE, siap Phase 4

## Current Position

Milestone: v0.2 Parking Gate Display
Phase: 4 of 4 (Polish & Robustness) — Not started
Plan: Not started
Status: Ready to plan
Last activity: 2026-03-09 — Phase 3 complete, transitioned to Phase 4

Progress:
- Milestone: [███████░░░] 75%
- Phase 1:   [██████████] 100% COMPLETE
- Phase 2:   [██████████] 100% COMPLETE
- Phase 3:   [██████████] 100% COMPLETE
- Phase 4:   [░░░░░░░░░░] 0%

## Loop Position

Current loop state:
```
PLAN ──▶ APPLY ──▶ UNIFY
  ✓        ✓        ✓     [Loop complete - ready for next PLAN]
```

## Accumulated Context

### Decisions
| Decision | Rationale | Date |
|----------|-----------|------|
| MQTT schema: `parking/{gate_id}/*` (4 topics) | Clean separation per concern | 2026-03-09 |
| ArduinoJson v7 JsonDocument | StaticJsonDocument deprecated | 2026-03-09 |
| NVS Preferences namespace "gate-cfg" | Built-in ESP32, no extra lib | 2026-03-09 |
| GATE_ID tetap compile-time constant | NVS hanya untuk runtime config | 2026-03-09 |
| NTP (bukan BM8563 RTC) untuk jam P2 | Lebih simpel — tidak perlu I2C/library tambahan, cukup configTime() | 2026-03-09 |
| configTime() di setup() (bukan di connectWiFiIfNeeded) | Hanya perlu dipanggil sekali; SNTP daemon retry otomatis | 2026-03-09 |
| P2 y offsets +1/+12/+23 untuk 3-row layout | 3×8px + 3px gaps = 30px dalam 32px panel | 2026-03-09 |
| P3 textSize(2) 2-row split: split on 2nd space | Fits "D 1234 ABC" as 2 rows for max readability | 2026-03-09 |
| gDisplayTimeout=0 skips timeout check entirely | Condition handles it — no special case needed | 2026-03-09 |

### Deferred Issues
| Issue | Origin | Effort | Revisit |
|-------|--------|--------|---------|
| `pio run` gagal Python 3.14 vs PlatformIO | 01-01 | M | Downgrade Python ke 3.12 — must fix before flash |

### Blockers/Concerns
- Python 3.14 vs PlatformIO: build CLI tidak bisa diverifikasi — perlu diselesaikan sebelum flash.

## Phase 1 Summary (apa yang dibangun)
- **01-01**: GateState struct, 4 MQTT topic helpers, mqttCallback routing
- **01-02**: ArduinoJson v7, JSON parser event + greeting, setBufferSize(512)
- **01-03**: Preferences NVS, loadConfig/saveConfig, handleConfigTopic partial update

## Phase 2 Summary (apa yang dibangun)
- **02-01**: configTime(UTC+7), gClockStr global, updateClock(), 1-second loop timer
- **02-02**: P2 3-row layout: gate name / gClockStr / ANPR status

## Phase 3 Summary (apa yang dibangun)
- **03-01**: P1 full color block — getStatusColor(), fillRect() per status, idle=black
- **03-02**: P2 rendering polish — centered 3-row text dengan labelColor/valueColor
- **03-03**: P3 large plate — textSize(2), 2-row split, drawCenteredText2(); P4 scroll + color support
- **03-04**: Auto-timeout idle — gLastEventMs global, millis()-based loop() check, gDisplayTimeout=0 disables

## Session Continuity

Last session: 2026-03-09
Stopped at: Phase 3 complete, transitioned to Phase 4
Next action: /paul:plan for Phase 4 (Polish & Robustness)
Resume file: .paul/ROADMAP.md

---
*STATE.md — Updated after every significant action*
