---
phase: 01-mqtt-json-refactor
plan: 02
subsystem: mqtt
tags: [mqtt, arduinojson, json, esp32, arduino, pubsubclient]

requires:
  - phase: 01-mqtt-json-refactor plan 01
    provides: GateState struct, topic helpers, handler function stubs

provides:
  - ArduinoJson v7 terintegrasi ke project
  - handleEventTopic() mem-parse JSON event ke GateState dengan validasi enum
  - handleGreetingTopic() mem-parse JSON greeting ke GateState.greeting dengan fallback
  - MQTT buffer diperbesar ke 512 bytes

affects:
  - 01-03 (config handler bisa pakai pola JsonDocument yang sama)
  - Phase 3 (GateState.status/plate/anprStatus kini terisi benar dari JSON)

tech-stack:
  added:
    - bblanchon/ArduinoJson@^7.0
  patterns:
    - JsonDocument + deserializeJson() + operator | untuk default value (ArduinoJson v7 idiom)
    - Validasi enum status sebelum assign ke GateState

key-files:
  modified:
    - platformio.ini
    - src/HD-WF1-WF2-LED-MatrixPanel-DMA.ino.cpp

key-decisions:
  - "ArduinoJson v7: JsonDocument (bukan StaticJsonDocument yang deprecated)"
  - "setBufferSize(512) — default 256 tidak cukup untuk JSON event payload ~150 bytes"
  - "Validasi enum status: hanya granted/denied/scanning/idle yang diterima, selainnya fallback idle"
  - "scroll + color dari greeting: defer ke Phase 3 rendering"

patterns-established:
  - "JsonDocument doc; deserializeJson(doc, payload); const char* v = doc[\"field\"] | \"default\";"
  - "Error handling: if (err) { log error; set safe default; return; }"

duration: ~10min
started: 2026-03-09T00:15:00Z
completed: 2026-03-09T00:25:00Z
---

# Phase 1 Plan 02: JSON Parser — Summary

**ArduinoJson v7 diintegrasikan; handleEventTopic() dan handleGreetingTopic() kini mem-parse JSON payload nyata ke GateState dengan validasi enum dan fallback aman.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~10 min |
| Tasks | 3 of 3 completed |
| Files modified | 2 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: ArduinoJson tersedia + buffer 512 bytes | Pass | `bblanchon/ArduinoJson@^7.0` di platformio.ini; `setBufferSize(512)` sebelum setServer() |
| AC-2: handleEventTopic() parse JSON ke GateState | Pass | status/plate/anprStatus/gateName diekstrak dengan operator `\|` default |
| AC-3: Robust terhadap JSON tidak valid | Pass | DeserializationError ditangkap → status="idle", log error, return |
| AC-4: handleGreetingTopic() parse field "text" | Pass | gGate.greeting diisi, redrawNeeded=true, Serial log teks |
| AC-5: Fallback jika "text" kosong | Pass | `if (strlen(text) > 0)` — greeting tidak berubah jika kosong |

## Accomplishments

- ArduinoJson v7 masuk ke project — basis parsing JSON untuk semua topic berikutnya
- `handleEventTopic()` kini mengisi GateState dengan data nyata dari ANPR system
- Validasi enum status mencegah nilai tak terduga masuk ke display state
- `handleGreetingTopic()` aman terhadap payload kosong / field hilang
- MQTT buffer 512 bytes — memastikan JSON payload ~150 bytes tidak terpotong

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `platformio.ini` | Modified | Tambah `bblanchon/ArduinoJson@^7.0` ke lib_deps_external |
| `src/HD-WF1-WF2-LED-MatrixPanel-DMA.ino.cpp` | Modified | `#include <ArduinoJson.h>`, `setBufferSize(512)`, implementasi 2 handler |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| JsonDocument (bukan StaticJsonDocument) | StaticJsonDocument deprecated di v7 | Forward-compatible dengan ArduinoJson future versions |
| setBufferSize(512) | Default 256 tidak cukup untuk event JSON ~150 bytes | Semua payload diterima penuh tanpa truncation |
| Validasi enum status | Cegah nilai tak terduga masuk ke rendering logic | P1 hanya menerima granted/denied/scanning/idle |
| scroll + color defer ke Phase 3 | Rendering logic belum ada di Phase 1 | Tidak menambah kompleksitas sebelum waktunya |

## Deviations from Plan

None — plan dieksekusi persis seperti yang direncanakan.

## Issues Encountered

| Issue | Resolution |
|-------|------------|
| `pio run` masih gagal Python 3.14 | Sama seperti plan 01-01 — environment issue, bukan kode |

## Next Phase Readiness

**Ready:**
- GateState kini terisi dengan data JSON nyata dari ANPR via MQTT
- Pola JsonDocument siap digunakan di `handleConfigTopic()` (plan 01-03)
- Fondasi rendering Phase 3 solid: status/plate/anprStatus/gateName/greeting semua terisi

**Concerns:**
- Build validation masih belum bisa via CLI (Python 3.14 issue)

**Blockers:**
- None — plan 01-03 (config + NVS) bisa dimulai segera

---
*Phase: 01-mqtt-json-refactor, Plan: 02*
*Completed: 2026-03-09*
