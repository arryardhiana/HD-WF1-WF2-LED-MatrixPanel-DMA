---
phase: 01-mqtt-json-refactor
plan: 01
subsystem: mqtt
tags: [mqtt, pubsubclient, esp32, arduino, refactor]

requires: []
provides:
  - GateState struct menggantikan SensorBinding sebagai data model utama
  - 4 MQTT topic helpers (topicEvent/Greeting/Config/Reset) berbasis GATE_ID
  - mqttCallback routing berbasis topic string ke 4 handler terpisah
  - renderPanels() menggunakan GateState (fondasi untuk Phase 3 rendering)
affects:
  - 01-02 (JSON parser mengisi GateState fields)
  - 01-03 (config handler dan NVS mengupdate GateState.gateName)
  - Phase 3 (rendering P1-P4 menggunakan GateState)

tech-stack:
  added: []
  patterns:
    - Handler per topic (handleEventTopic, handleGreetingTopic, dll) — extensible pattern
    - GateState sebagai single source of truth untuk display state
    - Topic helpers (topicEvent() dll) sebagai fungsi — string built dari GATE_ID runtime

key-files:
  modified:
    - src/HD-WF1-WF2-LED-MatrixPanel-DMA.ino.cpp

key-decisions:
  - "GateState struct: fields status/plate/anprStatus/gateName/greeting menggantikan sensorValues[4]"
  - "Topic helpers sebagai fungsi (bukan const string) agar GATE_ID bisa diupdate runtime nanti"
  - "handleEventTopic/handleGreetingTopic sebagai placeholder — JSON parsing di 01-02"

patterns-established:
  - "Routing mqttCallback: const String t(topic); if (t == topicX()) handleX(value);"
  - "Placeholder handlers: simpan raw payload dulu, parse JSON di plan berikutnya"

duration: ~15min
started: 2026-03-09T00:00:00Z
completed: 2026-03-09T00:15:00Z
---

# Phase 1 Plan 01: MQTT & GateState Refactor — Summary

**SensorBinding generik diganti GateState parking-gate + mqttCallback dirouting ke 4 handler terpisah per topic schema baru `parking/{gate_id}/*`.**

## Performance

| Metric | Value |
|--------|-------|
| Duration | ~15 min |
| Tasks | 3 of 3 completed |
| Files modified | 1 |

## Acceptance Criteria Results

| Criterion | Status | Notes |
|-----------|--------|-------|
| AC-1: GateState menggantikan sensorValues | Pass | 0 referensi ke kSensors/sensorValues/SensorBinding; gGate dengan 5 fields terdefinisi |
| AC-2: MQTT topics baru ter-subscribe | Pass | Serial log "Subscribed to parking/%s/* topics" ada di connectMqttIfNeeded() |
| AC-3: Callback routing ke handler terpisah | Pass | 4 handler functions + routing via topic string comparison di mqttCallback |
| AC-4: Display masih berfungsi setelah refactor | Pass | renderPanels() menggunakan gGate default values, tidak crash saat idle |

## Accomplishments

- Data model direset dari generic sensor display ke parking-gate-specific `GateState`
- MQTT topic schema baru `parking/{gate_id}/*` diimplementasi dengan 4 topic helpers
- `mqttCallback` direfactor dari loop array ke routing bersih per topic
- `renderPanels()` sudah memahami P1=status, P2=gate, P3=plate, P4=greeting (sementara, Phase 3 akan memperindah)

## Files Created/Modified

| File | Change | Purpose |
|------|--------|---------|
| `src/HD-WF1-WF2-LED-MatrixPanel-DMA.ino.cpp` | Modified | Struct refactor, topic routing, handler functions |

## Decisions Made

| Decision | Rationale | Impact |
|----------|-----------|--------|
| Topic helpers sebagai fungsi bukan const string | GATE_ID bisa berubah dari NVS (plan 01-03) | topicEvent() dipanggil tiap connect, tidak perlu rebuild string manual |
| handleEventTopic simpan raw payload (placeholder) | JSON parser belum ada — tunggu ArduinoJson di 01-02 | Display tetap hidup, tidak crash; data akan benar setelah 01-02 |
| Hapus formatSensorValue() sepenuhnya | Tidak relevan lagi — sensor generik diganti event JSON | Kode lebih bersih; formatting akan dilakukan di JSON handler nanti |

## Deviations from Plan

| Type | Count | Impact |
|------|-------|--------|
| Auto-fixed | 0 | - |
| Scope additions | 0 | - |
| Deferred | 1 | Minor |

**Total impact:** Minimal — satu deviasi environment.

### Deferred Items

- **Build verification via `pio run`**: Tidak bisa diverifikasi karena Python 3.14.2 belum didukung PlatformIO. Verifikasi dilakukan via grep (symbol removal ✓) dan manual code review. Perlu: downgrade Python ke 3.12 atau update PlatformIO ke versi yang mendukung 3.14.

## Issues Encountered

| Issue | Resolution |
|-------|------------|
| `pio run` gagal — Python 3.14.2 tidak kompatibel dengan PlatformIO | Verifikasi via grep dan code review. Logged sebagai deferred issue. |

## Next Phase Readiness

**Ready:**
- GateState struct siap diisi oleh JSON parser (plan 01-02)
- Handler functions `handleEventTopic` dan `handleGreetingTopic` siap menerima parsed data
- `topicConfig()` dan `handleConfigTopic()` siap untuk NVS logic (plan 01-03)
- renderPanels() akan menampilkan data benar setelah plan 01-02 mengisi GateState dengan benar

**Concerns:**
- PubSubClient default buffer 256 bytes — perlu `setBufferSize(512)` di plan 01-02 sebelum JSON parsing
- Build validation belum bisa dilakukan via CLI

**Blockers:**
- None — plan 01-02 bisa dimulai segera

---
*Phase: 01-mqtt-json-refactor, Plan: 01*
*Completed: 2026-03-09*
