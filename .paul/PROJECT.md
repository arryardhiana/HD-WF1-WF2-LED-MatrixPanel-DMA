# Project: HD-WF1-WF2-LED-MatrixPanel-DMA

## What This Is

Sistem display LED matrix dinamis untuk parking gate menggunakan ESP32-S3 (Huidu WF2), menampilkan status akses kendaraan, hasil scan ANPR, nomor plat, dan informasi gate pada 4 panel LED 64x32 pixel yang disusun vertikal (P1-P4) dengan cabling serpentine, dikendalikan via MQTT. Waktu diambil dari modul RTC BM8563 yang sudah terpasang di board.

## Core Value

Staff parking dapat melihat status akses kendaraan (granted/denied), hasil scan ANPR, nomor plat, dan info gate secara real-time pada 4-panel LED matrix yang dikendalikan via MQTT dari sistem ANPR.

## Current State

| Attribute | Value |
|-----------|-------|
| Version | 0.1.0 |
| Status | Prototype |
| Last Updated | 2026-03-09 |

## Panel Layout

| Panel | Ukuran | Konten |
|-------|--------|--------|
| P1 | 64x32 | Full color status — Hijau (Granted) / Merah (Denied) / Kuning (Scanning) |
| P2 | 64x32 | 3 baris: Nama gate / Jam & Tanggal (dari RTC) / Status ANPR scan |
| P3 | 64x32 | Full display nomor plat (format: "D 1234 ABC") |
| P4 | 64x32 | Greeting / informasi lain (dapat diupdate via MQTT) |

**Konfigurasi hardware:**
- Board: Huidu WF2 (ESP32-S3)
- Modul: 64x32 pixel per panel
- Layout: 1x4 vertikal ke bawah
- Cabling: Serpentine (ZZ chain — config index 4)
- Total resolusi: 64x128 pixel
- RTC: BM8563 pada I2C SDA=41, SCL=42
- HUB75 port: 75EX1

## MQTT Design

### Subscribe Topics

| Topic | Deskripsi |
|-------|-----------|
| `parking/{gate_id}/event` | Event utama: akses kendaraan + hasil ANPR |
| `parking/{gate_id}/greeting` | Update teks greeting di P4 |
| `parking/{gate_id}/config` | Update konfigurasi gate (nama, timeout, dll) |
| `parking/{gate_id}/reset` | Reset display ke idle state |

> `{gate_id}` dikonfigurasi per-device (contoh: `gate-a`, `gate-lobby-1`)

### Payload JSON

#### Topic: `parking/{gate_id}/event`
Event utama dikirim oleh sistem ANPR setiap kali kendaraan terdeteksi.

```json
{
  "status": "granted",
  "plate": "D 1234 ABC",
  "anpr_status": "detected",
  "gate_name": "Gate A",
  "timestamp": 1741478400
}
```

| Field | Type | Nilai | Keterangan |
|-------|------|-------|------------|
| `status` | string | `"granted"` \| `"denied"` \| `"scanning"` \| `"idle"` | Status akses — menentukan warna P1 |
| `plate` | string | `"D 1234 ABC"` | Nomor plat terbaca, ditampilkan di P3 |
| `anpr_status` | string | `"detected"` \| `"not_detected"` \| `"low_confidence"` | Status scan ANPR, ditampilkan di P2 baris 3 |
| `gate_name` | string | `"Gate A"` | Nama gate (opsional, override config lokal) |
| `timestamp` | int | Unix timestamp | Opsional — fallback ke RTC jika tidak ada |

**Warna P1 berdasarkan `status`:**
- `"granted"` → Hijau penuh
- `"denied"` → Merah penuh
- `"scanning"` → Kuning (animasi)
- `"idle"` → Mati / gelap

#### Topic: `parking/{gate_id}/greeting`
Update teks P4 secara dinamis.

```json
{
  "text": "Selamat Datang!",
  "scroll": false,
  "color": "white"
}
```

| Field | Type | Default | Keterangan |
|-------|------|---------|------------|
| `text` | string | — | Teks yang ditampilkan di P4 |
| `scroll` | bool | `false` | Scroll horizontal jika teks panjang |
| `color` | string | `"white"` | Warna teks (`"white"`, `"yellow"`, `"cyan"`) |

#### Topic: `parking/{gate_id}/config`
Konfigurasi runtime (disimpan ke flash/NVS).

```json
{
  "gate_name": "Gate A - Main Entrance",
  "display_timeout": 5,
  "brightness": 96
}
```

| Field | Type | Default | Keterangan |
|-------|------|---------|------------|
| `gate_name` | string | — | Nama gate ditampilkan di P2 baris 1 |
| `display_timeout` | int | `5` | Detik sebelum kembali ke idle setelah event |
| `brightness` | int | `96` | Brightness panel 0–255 |

#### Topic: `parking/{gate_id}/reset`
Payload kosong atau `{}` — reset display ke idle state.

## RTC BM8563

- **Hardware**: BM8563 sudah terpasang di board Huidu WF1/WF2 (I2C SDA=41, SCL=42)
- **Library**: `lewisxhe/I2C BM8563 RTC` sudah ada di `platformio.ini`
- **Status saat ini**: Library tersedia tapi belum diinisialisasi di firmware
- **Rencana implementasi**:
  1. Init BM8563 saat startup
  2. Sync NTP ke RTC jika WiFi tersedia
  3. Baca waktu dari RTC setiap detik untuk P2 baris 2
  4. Fallback ke RTC lokal jika NTP tidak tersedia
- **Format tampilan P2**: `"HH:MM:SS  DD/MM/YYYY"`

## Requirements

### Validated (Shipped)
- [x] Basic LED matrix display working — v0.1.0
- [x] MQTT subscribe + display sensor value — v0.1.0
- [x] Serpentine chain (ZZ) config working — v0.1.0
- [x] MQTT schema `parking/{gate_id}/*` (4 topics) — Phase 1
- [x] JSON payload parser (ArduinoJson v7) untuk event + greeting + config + reset — Phase 1
- [x] NVS config persistence (gate_name, brightness, display_timeout) — Phase 1
- [x] NTP real-time clock di P2 via configTime() UTC+7 — Phase 2 (NTP dipilih vs BM8563 RTC)
- [x] P2 3-row layout: gate name / jam / ANPR status — Phase 2
- [x] P1: Full color status block (granted=hijau / denied=merah / scanning=kuning / idle=gelap) — Phase 3
- [x] P3: Display nomor plat besar (textSize 2, 2-row split, centered) — Phase 3
- [x] P4: Greeting dinamis via MQTT (scroll horizontal + color configurable) — Phase 3
- [x] Auto-timeout idle: display reset ke idle setelah gDisplayTimeout detik — Phase 3

- [x] Watchdog timer ESP32 task WDT (30s, panic+reboot) — Phase 4
- [x] MQTT offline indicator on P2 row 3 ("OFFLINE" dim-red after 5s disconnect) — Phase 4
- [x] ArduinoOTA over-the-air firmware update support — Phase 4

### Active (In Progress)
- [ ] Resolve Python 3.14 vs PlatformIO (downgrade to 3.12) — before flash
- [ ] Verify esp_task_wdt_init() API for board ESP-IDF version

### Out of Scope
- ANPR system (diasumsikan sudah ada, hanya integrasi via MQTT)
- Backend/server MQTT broker setup
- Web dashboard / remote management

## Constraints

### Technical Constraints
- Board: Huidu WF2 (ESP32-S3), bukan board ESP32 generik
- Protocol: MQTT (PubSubClient v2.8)
- Panel: HUB75 64x32, chain ZZ 4-scan
- RTC: BM8563 (bukan DS3231 atau PCF8563)
- Library rendering: GFX_Lite (bukan Adafruit GFX standar)
- Max payload MQTT: default PubSubClient 256 bytes (perlu `setBufferSize` jika lebih)

### Business Constraints
- Integrasi dengan sistem ANPR yang sudah ada
- Deploy di parking gate — perlu auto-reconnect WiFi/MQTT yang robust

## Tech Stack

| Layer | Technology |
|-------|------------|
| MCU | ESP32-S3 (Huidu WF2) |
| LED Driver | ESP32-HUB75-MatrixPanel-DMA |
| Graphics | GFX_Lite |
| MQTT | PubSubClient v2.8 |
| RTC | BM8563 via I2C |
| Build | PlatformIO |

## Success Criteria
- P1 tampil warna hijau/merah/kuning sesuai status dalam < 500ms setelah event MQTT
- P2 menampilkan nama gate, jam RTC real-time, dan status ANPR terbaru
- P3 menampilkan nomor plat dengan font besar dan terbaca jelas
- P4 menampilkan greeting yang dapat diupdate via MQTT
- Display kembali ke idle otomatis setelah `display_timeout` detik
- Auto-reconnect WiFi dan MQTT tanpa perlu reboot

---
*Created: 2026-03-09*
*Last updated: 2026-03-09 after Phase 4 (v0.2 complete)*
