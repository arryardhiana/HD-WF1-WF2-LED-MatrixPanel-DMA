# Project State

## Project Reference

See: .paul/PROJECT.md (updated 2026-03-09)

**Core value:** Staff parking dapat melihat status akses kendaraan (granted/denied), hasil scan ANPR, nomor plat, dan info gate secara real-time via MQTT.
**Current focus:** v0.2 Parking Gate Display — Phase 2 COMPLETE, siap Phase 3

## Current Position

Milestone: v0.2 Parking Gate Display
Phase: 3 of 4 (Panel Rendering P1-P4) — In Progress
Plan: 03-04 created, awaiting approval
Status: PLAN created, ready for APPLY
Last activity: 2026-03-09 — Created .paul/phases/03-panel-rendering/03-04-PLAN.md

Progress:
- Milestone: [█████░░░░░] 50%
- Phase 1:   [██████████] 100% COMPLETE
- Phase 2:   [██████████] 100% COMPLETE
- Phase 3:   [░░░░░░░░░░] 0%

## Loop Position

Current loop state:
```
PLAN ──▶ APPLY ──▶ UNIFY
  ✓        ○        ○     [Plan created, awaiting approval]
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
| gDisplayTimeout disimpan, belum digunakan | Logic idle timer di Phase 4 | 2026-03-09 |
| configTime() di setup() (bukan di connectWiFiIfNeeded) | Hanya perlu dipanggil sekali; SNTP daemon retry otomatis | 2026-03-09 |
| P2 y offsets +1/+12/+23 untuk 3-row layout | 3×8px + 3px gaps = 30px dalam 32px panel | 2026-03-09 |

### Deferred Issues
| Issue | Origin | Effort | Revisit |
|-------|--------|--------|---------|
| `pio run` gagal Python 3.14 vs PlatformIO | 01-01 | M | Downgrade Python ke 3.12 |
| scroll + color dari greeting | 01-02 | S | Phase 3 |
| GFX_Lite font besar untuk P3 | PROJECT.md | S | Phase 3 plan 03-03 |
| Idle timer logic (gDisplayTimeout) | 01-03 | S | Phase 4 plan 04-01 |

### Blockers/Concerns
- Python 3.14 vs PlatformIO: build CLI tidak bisa diverifikasi — perlu diselesaikan sebelum flash.

## Phase 1 Summary (apa yang dibangun)
- **01-01**: GateState struct, 4 MQTT topic helpers, mqttCallback routing
- **01-02**: ArduinoJson v7, JSON parser event + greeting, setBufferSize(512)
- **01-03**: Preferences NVS, loadConfig/saveConfig, handleConfigTopic partial update

## Phase 2 Summary (apa yang dibangun)
- **02-01**: configTime(UTC+7), gClockStr global, updateClock(), 1-second loop timer
- **02-02**: P2 3-row layout: gate name / gClockStr / ANPR status

## Session Continuity

Last session: 2026-03-09
Stopped at: Plan 03-04 created
Next action: Review and approve plan, then run /paul:apply .paul/phases/03-panel-rendering/03-04-PLAN.md
Resume file: .paul/phases/03-panel-rendering/03-04-PLAN.md

---
*STATE.md — Updated after every significant action*
