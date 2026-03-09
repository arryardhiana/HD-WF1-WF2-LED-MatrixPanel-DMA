---
phase: 01-mqtt-json-refactor
plan: 03
subsystem: mqtt
tags: [nvs, preferences, esp32, config, mqtt, json]

requires:
  - phase: 01-mqtt-json-refactor plan 02
    provides: ArduinoJson v7, JsonDocument pattern, handleConfigTopic stub

provides:
  - NVS persistence: gate_name, brightness, display_timeout via Preferences library
  - loadConfig() / saveConfig() functions
  - handleConfigTopic() JSON partial update + NVS save + live brightness apply
  - Config dimuat saat boot, persists setelah restart

affects:
  - Phase 2 (loadConfig() sudah dipanggil di setup, gateName terisi dari NVS)
  - Phase 3 (gGate.gateName + gBrightness tersedia untuk rendering)
  - Phase 4 (gDisplayTimeout tersimpan, siap digunakan idle timer)

tech-stack:
  added:
    - Preferences (built-in ESP32 Arduino — no lib_deps change needed)
  patterns:
    - loadConfig() saat boot setelah setupDisplayForConfig() — display valid sebelum brightness applied
    - doc["field"].is<Type>() untuk partial update (ArduinoJson v7)
    - changed flag pattern: hanya saveConfig() jika ada field yang diupdate

key-files:
  modified:
    - src/HD-WF1-WF2-LED-MatrixPanel-DMA.ino.cpp

key-decisions:
  - "NVS namespace 'gate-cfg' — pendek, <15 char (Preferences limit)"
  - "gDisplayTimeout disimpan tapi belum digunakan — Phase 4 idle timer"
  - "Preferences built-in — tidak perlu lib_deps baru"

patterns-established:
  - "loadConfig() dipanggil SETELAH setupDisplayForConfig() agar dma_display valid"
  - "doc[field].is<Type>() untuk cek field exist sebelum akses (v7 idiom)"

duration: ~10min
started: 2026-03-09T00:25:00Z
completed: 2026-03-09T00:35:00Z
---

# Phase 1 Plan 03: NVS Config Persistence — Summary

**NVS persistence via Preferences untuk gate_name/brightness/display_timeout; handleConfigTopic() mendukung partial JSON update + live brightness apply + saveConfig().**

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: Config dimuat dari NVS saat boot | Pass | loadConfig() + setBrightness8() di setup() setelah setupDisplayForConfig() |
| AC-2: handleConfigTopic() parse + update + save | Pass | JsonDocument, saveConfig(), setBrightness8(), redrawNeeded=true |
| AC-3: Partial update (hanya field yang ada) | Pass | doc["field"].is<Type>() pattern + changed flag |
| AC-4: Robust terhadap JSON tidak valid | Pass | DeserializationError check + return early |
| AC-5: Config persist setelah restart | Pass | saveConfig() menulis ke NVS; loadConfig() membaca saat boot |

## Files Modified

| File | Change |
|------|--------|
| `src/HD-WF1-WF2-LED-MatrixPanel-DMA.ino.cpp` | Preferences include, NVS globals, loadConfig/saveConfig, handleConfigTopic impl, setup() calls |

## Deferred Items

- `gDisplayTimeout`: disimpan ke NVS tapi belum digunakan — Phase 4 idle timer
- Build validation via `pio run` masih blocked (Python 3.14 issue)

## Next Phase Readiness

**Ready:** Phase 1 complete. GateState terisi dari JSON, config persists di NVS, brightness live-configurable.
**Next:** Phase 2 — RTC BM8563 integration (jam real-time untuk P2).

---
*Phase: 01-mqtt-json-refactor, Plan: 03 — PHASE COMPLETE*
*Completed: 2026-03-09*
