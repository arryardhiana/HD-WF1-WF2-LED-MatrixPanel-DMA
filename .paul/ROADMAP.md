# Roadmap: HD-WF1-WF2-LED-MatrixPanel-DMA

## Overview

Transformasi firmware dari sensor display generik menjadi sistem display parking gate yang lengkap — dengan MQTT schema baru berbasis JSON, RTC BM8563 aktif untuk jam real-time, dan tiap panel (P1-P4) menampilkan informasi spesifik yang relevan untuk operator parking gate.

## Current Milestone

**v0.2 Parking Gate Display** (v0.2.0)
Status: In progress
Phases: 3 of 4 complete

## Phases

| Phase | Name | Plans | Status | Completed |
|-------|------|-------|--------|-----------|
| 1 | MQTT & JSON Refactor | 3 | Complete | 2026-03-09 |
| 2 | NTP Clock Integration | 2 | Complete | 2026-03-09 |
| 3 | Panel Rendering P1-P4 | 4 | Complete | 2026-03-09 |
| 4 | Polish & Robustness | 2 | Not started | - |

## Phase Details

### Phase 1: MQTT & JSON Refactor

**Goal:** Ganti MQTT schema lama (sensor generik, plain string) ke schema baru berbasis JSON untuk parking gate.
**Depends on:** Nothing (refactor dari baseline)
**Research:** Unlikely (PubSubClient sudah ada, hanya schema baru)

**Scope:**
- Define dan dokumentasikan topic schema baru (`parking/{gate_id}/event`, dll)
- Ganti `SensorBinding[]` dengan struct baru `GateConfig` dan event handler
- Implement JSON parser (ArduinoJson atau manual parse) untuk payload event
- Subscribe ke 4 topic baru, unsubscribe dari topic lama
- Simpan `gate_id` dan `gate_name` ke NVS

**Plans:**
- [ ] 01-01: Refactor struct dan konfigurasi MQTT (gate_id, topics)
- [ ] 01-02: Implement JSON payload parser untuk topic `event` dan `greeting`
- [ ] 01-03: Implement topic `config` dan `reset` handler + NVS persistence

### Phase 2: RTC Integration

**Goal:** Aktifkan BM8563 RTC untuk menampilkan jam dan tanggal real-time di P2, dengan NTP sync saat WiFi tersedia.
**Depends on:** Phase 1 (WiFi/MQTT sudah stabil)
**Research:** Unlikely (library BM8563 sudah ada di platformio.ini, pin I2C sudah diketahui)

**Scope:**
- Init BM8563 saat startup (I2C SDA=41, SCL=42)
- NTP sync ke RTC setelah WiFi connect
- Baca waktu dari RTC setiap detik
- Format tampilan: `"HH:MM  DD/MM/YY"`

**Plans:**
- [ ] 02-01: Init BM8563, NTP sync, dan simpan ke RTC
- [ ] 02-02: Loop baca RTC setiap detik, format string untuk P2 baris 2

### Phase 3: Panel Rendering P1-P4

**Goal:** Setiap panel menampilkan konten spesifik sesuai desain parking gate display.
**Depends on:** Phase 1 (event data tersedia), Phase 2 (waktu tersedia)
**Research:** Unlikely (GFX_Lite API sudah dipahami dari kode existing)

**Scope:**
- P1: Full color block hijau/merah/kuning/gelap berdasarkan status event
- P2: 3 baris — gate name (baris 1), jam+tanggal RTC (baris 2), status ANPR (baris 3)
- P3: Nomor plat besar di tengah panel, font besar/bold
- P4: Teks greeting, bisa scroll jika panjang, warna configurable
- Auto-timeout: kembali ke idle setelah N detik dari event terakhir

**Plans:**
- [ ] 03-01: P1 — full color status block (granted/denied/scanning/idle)
- [ ] 03-02: P2 — 3-baris gate name + RTC time + ANPR status
- [ ] 03-03: P3 — large plate number display
- [ ] 03-04: P4 — greeting display dengan scroll support + idle state

### Phase 4: Polish & Robustness

**Goal:** Sistem siap deploy di lapangan — auto-reconnect robust, brightness configurable, dan idle state yang bersih.
**Depends on:** Phase 3 (semua panel berfungsi)
**Research:** Unlikely (internal patterns)

**Scope:**
- Auto-reconnect WiFi dan MQTT tanpa blocking
- Brightness control via MQTT config topic
- Watchdog timer / heartbeat
- Graceful idle state saat tidak ada koneksi
- OTA update support (sudah ada partisi OTA)

**Plans:**
- [ ] 04-01: Robust auto-reconnect, watchdog, dan idle display state
- [ ] 04-02: Brightness MQTT config + OTA update verification

---
*Roadmap created: 2026-03-09*
*Last updated: 2026-03-09*
