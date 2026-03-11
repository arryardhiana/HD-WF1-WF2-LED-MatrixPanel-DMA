# Parking Gate LED Matrix Display

Firmware untuk **Huidu WF1/WF2** (ESP32-S2/S3) yang menampilkan status akses kendaraan, hasil scan ANPR, nomor plat, dan informasi gate secara real-time pada 4 panel LED HUB75 64×32 yang disusun vertikal — dikendalikan via MQTT JSON.

---

## Daftar Isi

- [Hardware](#hardware)
- [Layout Panel](#layout-panel)
- [Cara Flash Pertama Kali](#cara-flash-pertama-kali)
- [Cara Flash via OTA](#cara-flash-via-ota)
- [Konfigurasi Firmware](#konfigurasi-firmware)
- [MQTT Topics & Payload](#mqtt-topics--payload)
- [Tampilan per Panel](#tampilan-per-panel)
- [Testing dengan Node-RED](#testing-dengan-node-red)
- [Kontrol via USB Serial](#kontrol-via-usb-serial)
- [Libraries](#libraries)

---

## Hardware

| Komponen | Spesifikasi |
|----------|-------------|
| Board | Huidu WF1 (ESP32-S2) atau WF2 (ESP32-S3) |
| Panel LED | HUB75 64×32 pixel, 4 buah |
| Susunan | 1×4 vertikal ke bawah (serpentine ZZ chain) |
| Total resolusi | 64×128 pixel |
| RTC | BM8563 built-in (I2C SDA=41, SCL=42) |
| HUB75 port | 75EX1 |

### Huidu WF1 (ESP32-S2)

Board murah ~$6, dilengkapi baterai, RTC BM8563, dan tombol di GPIO 11.

- [Seller #1 AliExpress](https://www.aliexpress.com/item/1005005038544582.html)
- [Seller #2 AliExpress](https://www.aliexpress.com/item/1005005556957598.html)

### Huidu WF2 (ESP32-S3)

Lebih mudah di-flash — cukup kabel USB-A ke USB-C standar.

> **Catatan:** WF2 punya 2 port HUB75 (`75EX1` dan `75EX2`) tapi **tidak bisa digunakan bersamaan**. Firmware ini menggunakan `75EX1`.

---

## Layout Panel

```
┌──────────────────────────────────┐
│  P1 — Status Akses (64×32)       │  Traffic lamp 3 lingkaran:
│  ( )  ( )  ( )                   │  Merah=Denied | Kuning=Scanning | Hijau=Granted
│  dim  dim  dim  ← idle           │  Idle: semua dim
└──────────────────────────────────┘
┌──────────────────────────────────┐
│  P2 — Info Gate (64×32)          │  Baris 1: Nama gate (biru)
│  Gate A                          │  Baris 2: Jam real-time NTP (hijau)
│  14:32:05  10/03/2026            │  Baris 3: Status ANPR / "OFFLINE" (jika disconnect)
│  detected                        │
└──────────────────────────────────┘
┌──────────────────────────────────┐
│  P3 — Nomor Plat (64×32)         │  Font besar (textSize 2), 2 baris jika perlu
│     D 1234                       │  Split otomatis di spasi ke-2
│       ABC                        │  Default: "------" saat idle
└──────────────────────────────────┘
┌──────────────────────────────────┐
│  P4 — Greeting / Info (64×32)    │  Idle/Granted: greeting MQTT (2 baris jika panjang)
│    Selamat                       │  Denied: "Gunakan / QRCODE / SAUNPAD"
│     Datang                       │  Scanning: kosong (blank)
└──────────────────────────────────┘
```

---

## Cara Flash Pertama Kali

### WF2 (ESP32-S3) — Lebih mudah

1. Hubungkan kabel USB-A ke USB-C dari board ke laptop
2. Build & upload:
   ```bash
   ~/.platformio/penv/bin/platformio run --target upload -e huidu_hd_wf2
   ```
3. Monitor serial:
   ```bash
   ~/.platformio/penv/bin/platformio device monitor --baud 115200
   ```

### WF1 (ESP32-S2) — Perlu mode download

1. **Bridge 2 pad** di dekat port MicroUSB untuk masuk Download Mode
2. Hubungkan kabel **USB-A ke USB-A** ke port USB-A di board (bukan MicroUSB)
3. Upload firmware

> **Perhatian Python:** Jika `pio run` gagal karena Python 3.14, gunakan langsung:
> ```bash
> ~/.platformio/penv/bin/platformio run --target upload
> ```

---

## Cara Flash via OTA

Setelah firmware pertama berhasil di-flash, update selanjutnya bisa via WiFi tanpa kabel.

**Konfigurasi `platformio.ini`:**
```ini
upload_protocol = espota
upload_port = parking-gate-01.local
upload_flags = --auth=ota-parking
```

**Upload:**
```bash
~/.platformio/penv/bin/platformio run --target upload -e huidu_hd_wf2
```

**Info OTA:**
| Parameter | Value |
|-----------|-------|
| Hostname | `parking-{GATE_ID}` (contoh: `parking-gate-01`) |
| Password | `ota-parking` |
| Port | 3232 (ArduinoOTA default) |

> Jika hostname tidak resolve, gunakan IP address langsung sebagai `upload_port`.

---

## Konfigurasi Firmware

Edit konstanta di bagian atas `src/HD-WF1-WF2-LED-MatrixPanel-DMA.ino.cpp`:

```cpp
// WiFi
static const char* WIFI_SSID = "NamaWiFi";
static const char* WIFI_PASS = "PasswordWiFi";

// MQTT Broker
static const char* MQTT_HOST = "10.10.6.99";
static const uint16_t MQTT_PORT = 1883;
static const char* MQTT_USER = "app";
static const char* MQTT_PASS = "password";

// OTA
static const char* OTA_PASSWORD = "ota-parking";

// Identitas gate — ubah per device
static const char* GATE_ID   = "gate-01";   // digunakan di MQTT topic
static const char* GATE_NAME = "Gate A";    // nama default di P2
```

> `GATE_ID` menentukan MQTT topic yang di-subscribe: `parking/{GATE_ID}/*`
> Setiap device di gate yang berbeda harus punya `GATE_ID` unik.

---

## MQTT Topics & Payload

Semua topic menggunakan prefix `parking/{gate_id}/`.

### Subscribe Topics

| Topic | Fungsi |
|-------|--------|
| `parking/{gate_id}/event` | Event akses kendaraan (status, plat, ANPR) |
| `parking/{gate_id}/greeting` | Update teks greeting di P4 |
| `parking/{gate_id}/config` | Update konfigurasi runtime (disimpan ke flash) |
| `parking/{gate_id}/reset` | Reset display ke idle |

---

### `parking/{gate_id}/event`

Dikirim oleh sistem ANPR setiap ada kendaraan terdeteksi.

```json
{
  "status": "granted",
  "plate": "D 1234 ABC",
  "anpr_status": "detected",
  "gate_name": "Gate A",
  "timestamp": 1741478400
}
```

| Field | Tipe | Nilai | Keterangan |
|-------|------|-------|------------|
| `status` | string | `granted` \| `denied` \| `scanning` \| `idle` | Menentukan warna P1 |
| `plate` | string | `"D 1234 ABC"` | Nomor plat, ditampilkan di P3 |
| `anpr_status` | string | `detected` \| `not_detected` \| `low_confidence` | Ditampilkan di P2 baris 3 |
| `gate_name` | string | `"Gate A"` | Opsional — override nama gate di P2 |
| `timestamp` | int | Unix timestamp | Opsional |

**Perilaku P1 berdasarkan `status`:**

| Status | P1 | P4 |
|--------|----|----|
| `granted` | Lingkaran hijau menyala | Greeting normal |
| `denied` | Lingkaran merah menyala | "Gunakan / QRCODE / SAUNPAD" |
| `scanning` | Lingkaran kuning menyala | Blank (kosong) |
| `idle` | Semua lingkaran dim | Greeting normal |

---

### `parking/{gate_id}/greeting`

Update teks P4 secara dinamis.

```json
{
  "text": "Selamat Datang",
  "scroll": false,
  "color": "white"
}
```

| Field | Tipe | Default | Keterangan |
|-------|------|---------|------------|
| `text` | string | — | Teks yang ditampilkan di P4 |
| `scroll` | bool | `false` | Scroll horizontal jika teks panjang |
| `color` | string | `"white"` | `"white"` \| `"yellow"` \| `"cyan"` |

> Partial update didukung — cukup kirim field yang mau diubah.

---

### `parking/{gate_id}/config`

Update konfigurasi runtime, **tersimpan permanen ke flash (NVS)** — bertahan setelah reboot.

```json
{
  "gate_name": "Gate A - Main Entrance",
  "display_timeout": 5,
  "brightness": 96
}
```

| Field | Tipe | Default | Keterangan |
|-------|------|---------|------------|
| `gate_name` | string | `"Gate A"` | Nama gate di P2 baris 1 |
| `display_timeout` | int | `5` | Detik sebelum kembali ke idle. `0` = tidak timeout |
| `brightness` | int | `96` | Kecerahan panel 0–255 |

**Contoh — ubah nama gate saja:**
```json
{"gate_name": "Gate B - Basement"}
```

**Contoh — redup untuk malam hari:**
```json
{"brightness": 30}
```

---

### `parking/{gate_id}/reset`

Reset display ke idle. Payload kosong atau `{}`.

```json
{}
```

---

## Tampilan per Panel

### P1 — Status Akses

Traffic lamp 3 lingkaran horizontal. Yang tidak aktif tampil dim (gelap).

```
Idle:     ( )  ( )  ( )   semua gelap
Denied:   (R)  ( )  ( )   merah menyala
Scanning: ( )  (Y)  ( )   kuning menyala
Granted:  ( )  ( )  (G)   hijau menyala
```

### P2 — Info Gate

3 baris teks:
- **Baris 1:** Nama gate (warna biru)
- **Baris 2:** Jam real-time dari NTP, format `HH:MM:SS  DD/MM/YYYY`, update tiap detik
- **Baris 3:** Status ANPR terakhir, atau `OFFLINE` (merah) jika MQTT disconnect > 5 detik

### P3 — Nomor Plat

Font besar (`textSize 2`). Teks di-split otomatis di spasi ke-2:
- `"D 1234 ABC"` → baris 1: `D 1234`, baris 2: `ABC`
- Jika terlalu panjang untuk textSize 2 → fallback ke textSize 1 satu baris
- Default saat idle: `------`

### P4 — Greeting / Info

| Kondisi | Tampilan |
|---------|----------|
| `idle` / `granted` | Teks greeting dari MQTT, 2 baris otomatis jika panjang |
| `denied` | "Gunakan / QRCODE / SAUNPAD" (3 baris, orange-red, centered) |
| `scanning` | Blank / kosong |

---

## Testing dengan Node-RED

File flow tersedia di `tools/nodered-mqtt-test-flow.json`.

### Import Flow

1. Buka Node-RED
2. Menu (≡) → **Import** → pilih file `tools/nodered-mqtt-test-flow.json`
3. Klik **Import** → **Deploy**

### Isi Flow

| Section | Fungsi |
|---------|--------|
| **Auto Every 4s** | Generate event acak (status, plat, ANPR) tiap 4 detik |
| **Manual Trigger** | Trigger 1 event acak |
| **GRANTED / DENIED / SCANNING / IDLE** | Kirim status spesifik secara manual |
| **Random Greeting** | Generate greeting acak (teks + warna + scroll) |
| **Selamat Datang** | Reset greeting ke default |
| **Scroll Text** | Test teks panjang dengan scroll |
| **RESET Display** | Kirim reset ke display |
| **Update Config** | Update gate_name / brightness / timeout |

### Format Plat yang Di-generate

```
{PREFIX} {1000-9999} {SUFFIX}
Contoh: D 4521 BCD, B 1337 XYZ, H 9001 ASD
```

---

## Kontrol via USB Serial

Display dapat dikontrol langsung via kabel USB (kabel yang sama untuk flashing) **tanpa WiFi dan tanpa MQTT broker**. Cocok untuk testing di meja atau debugging lapangan.

**Batasan kabel USB:**
| Tipe | Panjang Maks |
|------|-------------|
| Passive (biasa) | 5 meter |
| Active extension | 25 meter |

---

### Protokol Serial

Format: `<command>:<json_payload>` diakhiri newline (`\n`), baud rate `115200`.

| Command | Fungsi |
|---------|--------|
| `event` | Event akses kendaraan |
| `greeting` | Update teks P4 |
| `config` | Update konfigurasi |
| `reset` | Reset display ke idle |

---

### Opsi A — Python CLI (`tools/send_display.py`)

**Install dependency (sekali saja):**
```bash
pip install pyserial
# macOS dengan Homebrew Python:
pip install pyserial --break-system-packages
```

**Event akses kendaraan:**
```bash
# Granted
python3 tools/send_display.py event --status granted --plate "D 1234 ABC" --anpr detected

# Denied
python3 tools/send_display.py event --status denied --plate "B 9999 XYZ" --anpr detected

# Scanning (kamera sedang proses)
python3 tools/send_display.py event --status scanning

# Idle (reset ke standby)
python3 tools/send_display.py event --status idle
```

**Update greeting P4:**
```bash
# Teks pendek, warna kuning
python3 tools/send_display.py greeting --text "Selamat Datang" --color yellow

# Teks panjang dengan scroll
python3 tools/send_display.py greeting --text "Selamat Datang di Gedung Utama" --scroll

# Warna cyan
python3 tools/send_display.py greeting --text "Have a Nice Day" --color cyan
```

**Konfigurasi:**
```bash
# Ubah brightness
python3 tools/send_display.py config --brightness 128

# Ubah timeout idle (detik)
python3 tools/send_display.py config --timeout 10

# Ubah nama gate
python3 tools/send_display.py config --gate-name "Gate B"

# Kombinasi sekaligus
python3 tools/send_display.py config --brightness 50 --timeout 5 --gate-name "Gate C"
```

**Reset display:**
```bash
python3 tools/send_display.py reset
```

**Specify port manual (jika auto-detect gagal):**
```bash
# macOS
python3 tools/send_display.py --port /dev/tty.usbmodem101 event --status granted --plate "D 1234 ABC"

# Linux
python3 tools/send_display.py --port /dev/ttyUSB0 event --status granted --plate "D 1234 ABC"
```

Auto-detect port mencoba (berurutan): `/dev/tty.usbmodem101`, `/dev/tty.usbmodem1101`, `/dev/tty.SLAB_USBtoUART`, `/dev/ttyUSB0`, `/dev/ttyACM0`.

---

### Opsi B — 1-line CLI (tanpa Python)

Tidak perlu install apapun — langsung kirim ke port serial.

```bash
# macOS — cari port dulu:
ls /dev/tty.*
# biasanya: /dev/tty.usbmodem101

# Linux — port biasanya:
# /dev/ttyUSB0 atau /dev/ttyACM0

# Granted
printf 'event:{"status":"granted","plate":"D 1234 ABC","anpr_status":"detected","gate_name":"Gate A","timestamp":0}\n' > /dev/tty.usbmodem101

# Denied
printf 'event:{"status":"denied","plate":"B 9999 XYZ","anpr_status":"detected","gate_name":"Gate A","timestamp":0}\n' > /dev/tty.usbmodem101

# Scanning
printf 'event:{"status":"scanning","plate":"","anpr_status":"","gate_name":"Gate A","timestamp":0}\n' > /dev/tty.usbmodem101

# Idle
printf 'event:{"status":"idle","plate":"","anpr_status":"","gate_name":"Gate A","timestamp":0}\n' > /dev/tty.usbmodem101

# Update greeting
printf 'greeting:{"text":"Selamat Datang","scroll":false,"color":"yellow"}\n' > /dev/tty.usbmodem101

# Scroll teks panjang
printf 'greeting:{"text":"Selamat Datang di Gedung Utama","scroll":true,"color":"white"}\n' > /dev/tty.usbmodem101

# Update brightness
printf 'config:{"brightness":128}\n' > /dev/tty.usbmodem101

# Reset
printf 'reset:{}\n' > /dev/tty.usbmodem101
```

> MQTT dan serial berjalan **paralel** — keduanya bisa digunakan sekaligus tanpa konflik.

---

## Libraries

| Library | Versi | Fungsi |
|---------|-------|--------|
| `ESP32-HUB75-MatrixPanel-DMA` | latest | Driver LED matrix HUB75 |
| `GFX_Lite` | latest | Rendering teks & grafis |
| `PubSubClient` | ^2.8 | MQTT client |
| `ArduinoJson` | ^7.0 | JSON parser payload |
| `Preferences` | built-in | NVS config persistence |
| `ArduinoOTA` | built-in | OTA firmware update |
| `WiFi` + `time.h` | built-in | WiFi + NTP clock |

---

*Last updated: 2026-03-11*
