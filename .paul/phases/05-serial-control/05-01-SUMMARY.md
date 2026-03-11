# Summary: 05-01 Serial Control

## What Was Built

### Firmware — Serial Input Parser
- Tambah `gSerialBuffer` (String global) di `src/HD-WF1-WF2-LED-MatrixPanel-DMA.ino.cpp`
- Tambah serial reader di `loop()` — baca karakter per karakter, proses saat `\n`
- Routing `cmd:payload` ke handler yang sudah ada: `handleEventTopic`, `handleGreetingTopic`, `handleConfigTopic`, `handleResetTopic`
- Error handling: format invalid (tanpa colon) dan command tidak dikenal
- MQTT tetap berjalan paralel — tidak ada konflik

### Python CLI — tools/send_display.py
- 4 subcommands: `event`, `greeting`, `config`, `reset`
- Auto-detect port: `/dev/ttyUSB0`, `/dev/ttyUSB1`, `/dev/ttyACM0`, `/dev/ttyACM1`
- `--port` override manual, `--baud` override baud rate
- Output informatif: tampilkan port, baud, dan payload yang dikirim

## Protokol Serial
```
event:{"status":"granted","plate":"D 1234 ABC","anpr_status":"detected","gate_name":"Gate A","timestamp":0}
greeting:{"text":"Selamat Datang","scroll":false,"color":"white"}
config:{"brightness":128}
reset:{}
```

Compatible dengan 1-line tanpa Python:
```bash
printf 'event:{"status":"granted","plate":"D 1234 ABC","anpr_status":"detected","gate_name":"Gate A","timestamp":0}\n' > /dev/ttyUSB0
```

## Decisions
- Protokol `cmd:json\n` — simpel, human-readable, compatible 1-line CLI
- Handler functions tidak dimodifikasi — dipakai bersama MQTT dan serial
- `gSerialBuffer` bersifat global untuk persist antar iterasi loop()

## Files Modified
- `src/HD-WF1-WF2-LED-MatrixPanel-DMA.ino.cpp` — +gSerialBuffer, +serial reader block di loop()
- `tools/send_display.py` — file baru, Python CLI

## Install Dependency
```bash
pip install pyserial
```
