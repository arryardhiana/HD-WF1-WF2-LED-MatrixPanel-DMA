# Project State

## Project Reference

See: .paul/PROJECT.md (updated 2026-03-09)

**Core value:** Staff parking dapat melihat status akses kendaraan (granted/denied), hasil scan ANPR, nomor plat, dan info gate secara real-time via MQTT.
**Current focus:** v0.2 Parking Gate Display — MILESTONE COMPLETE

## Current Position

Milestone: v0.2 Parking Gate Display — COMPLETE
Phase: 4 of 4 (Polish & Robustness) — Complete
Plan: All plans complete
Status: v0.2 milestone complete — ready for field deploy or next milestone
Last activity: 2026-03-09 — Phase 4 complete, v0.2 milestone complete

Progress:
- Milestone: [██████████] 100% COMPLETE
- Phase 1:   [██████████] 100% COMPLETE
- Phase 2:   [██████████] 100% COMPLETE
- Phase 3:   [██████████] 100% COMPLETE
- Phase 4:   [██████████] 100% COMPLETE

## Loop Position

Current loop state:
```
PLAN ──▶ APPLY ──▶ UNIFY
  ✓        ✓        ✓     [Milestone complete]
```

## Accumulated Context

### Decisions
| Decision | Rationale | Date |
|----------|-----------|------|
| MQTT schema: `parking/{gate_id}/*` (4 topics) | Clean separation per concern | 2026-03-09 |
| ArduinoJson v7 JsonDocument | StaticJsonDocument deprecated | 2026-03-09 |
| NVS Preferences namespace "gate-cfg" | Built-in ESP32, no extra lib | 2026-03-09 |
| GATE_ID tetap compile-time constant | NVS hanya untuk runtime config | 2026-03-09 |
| NTP (bukan BM8563 RTC) untuk jam P2 | Lebih simpel — configTime() cukup | 2026-03-09 |
| P3 textSize(2) 2-row split on 2nd space | Max readability for plate numbers | 2026-03-09 |
| gDisplayTimeout=0 skips timeout check | Condition handles it — no special case | 2026-03-09 |
| gMqttWasConnected guard for OFFLINE indicator | No false OFFLINE on first boot | 2026-03-09 |
| ArduinoOTA.handle() WiFi-guarded | Clean no-op when offline | 2026-03-09 |

### Deferred Issues
| Issue | Origin | Effort | Revisit |
|-------|--------|--------|---------|
| `pio run` gagal Python 3.14 vs PlatformIO | 01-01 | M | Downgrade Python ke 3.12 — MUST FIX before flash |
| esp_task_wdt_init() API mismatch (ESP-IDF v5.x) | 04-01 | S | Verify/update API before flash |

### Blockers/Concerns
- Python 3.14 vs PlatformIO: build CLI tidak bisa diverifikasi — perlu diselesaikan sebelum flash.
- esp_task_wdt_init(30, true) — new ESP-IDF v5.x API may need struct config parameter.

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
- **03-03**: P3 large plate — textSize(2), 2-row split, drawCenteredText2(); P4 scroll+color
- **03-04**: Auto-timeout idle — gLastEventMs global, millis()-based loop() check

## Phase 4 Summary (apa yang dibangun)
- **04-01**: esp_task_wdt watchdog (30s panic); MQTT offline indicator P2 row 3
- **04-02**: ArduinoOTA — setupOTA() helper, handle() in loop(), hostname "parking-{GATE_ID}"

## Session Continuity

Last session: 2026-03-09
Stopped at: v0.2 milestone complete
Next action: Resolve Python 3.14/PlatformIO, verify esp_task_wdt_init() API, then flash to hardware
Resume file: .paul/ROADMAP.md

---
*STATE.md — Updated after every significant action*
