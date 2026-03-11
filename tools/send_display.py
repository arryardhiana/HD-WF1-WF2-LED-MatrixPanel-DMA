#!/usr/bin/env python3
"""
send_display.py — Kirim perintah ke LED matrix display via USB serial.

Protokol: <cmd>:<json>\\n
Compatible dengan 1-line CLI:
  printf 'event:{...}\\n' > /dev/ttyUSB0

Contoh penggunaan:
  python3 tools/send_display.py event --status granted --plate "D 1234 ABC" --anpr detected
  python3 tools/send_display.py event --status denied --plate "B 9999 XYZ" --anpr detected
  python3 tools/send_display.py event --status scanning
  python3 tools/send_display.py event --status idle
  python3 tools/send_display.py greeting --text "Selamat Datang" --color yellow
  python3 tools/send_display.py greeting --text "Selamat Datang di Gedung Utama" --scroll
  python3 tools/send_display.py config --brightness 128
  python3 tools/send_display.py config --timeout 10 --gate-name "Gate B"
  python3 tools/send_display.py reset
  python3 tools/send_display.py event --status granted --plate "D 1234 ABC" --port /dev/ttyUSB1

Dependencies:
  pip install pyserial
"""

import argparse
import json
import sys
import time

CANDIDATE_PORTS = ["/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyACM0", "/dev/ttyACM1"]


def find_port():
    """Auto-detect serial port dari candidate list."""
    import serial
    for port in CANDIDATE_PORTS:
        try:
            s = serial.Serial(port, timeout=0.1)
            s.close()
            return port
        except Exception:
            continue
    return None


def send(port, baud, cmd, payload):
    """Kirim cmd:json\\n ke serial port."""
    import serial
    line = f"{cmd}:{json.dumps(payload, ensure_ascii=False)}\n"
    try:
        with serial.Serial(port, baud, timeout=2) as s:
            time.sleep(0.05)  # beri waktu port ready
            s.write(line.encode("utf-8"))
            s.flush()
    except serial.SerialException as e:
        print(f"ERROR: Tidak bisa buka port {port}: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"Sending to {port} ({baud} baud):")
    print(f"  {line.strip()}")
    print("OK")


def main():
    parser = argparse.ArgumentParser(
        description="Kirim perintah ke LED matrix display via USB serial.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("--port", default=None, help="Serial port (default: auto-detect)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")

    sub = parser.add_subparsers(dest="command", required=True)

    # --- event ---
    p_event = sub.add_parser("event", help="Kirim event akses kendaraan")
    p_event.add_argument(
        "--status", required=True,
        choices=["granted", "denied", "scanning", "idle"],
        help="Status akses"
    )
    p_event.add_argument("--plate", default="", help='Nomor plat, contoh: "D 1234 ABC"')
    p_event.add_argument(
        "--anpr", default="",
        choices=["", "detected", "not_detected", "low_confidence"],
        metavar="ANPR",
        help="Status ANPR: detected | not_detected | low_confidence"
    )
    p_event.add_argument("--gate", default="Gate A", help="Nama gate (default: Gate A)")

    # --- greeting ---
    p_greet = sub.add_parser("greeting", help="Update teks greeting di P4")
    p_greet.add_argument("--text", required=True, help="Teks yang ditampilkan")
    p_greet.add_argument(
        "--color", default="white",
        choices=["white", "yellow", "cyan"],
        help="Warna teks (default: white)"
    )
    p_greet.add_argument("--scroll", action="store_true", help="Aktifkan scroll horizontal")

    # --- config ---
    p_cfg = sub.add_parser("config", help="Update konfigurasi display")
    p_cfg.add_argument("--brightness", type=int, metavar="0-255", help="Brightness panel")
    p_cfg.add_argument("--timeout", type=int, metavar="DETIK", help="Display timeout sebelum idle")
    p_cfg.add_argument("--gate-name", metavar="NAMA", help="Nama gate")

    # --- reset ---
    sub.add_parser("reset", help="Reset display ke idle state")

    args = parser.parse_args()

    # Resolve port
    port = args.port
    if not port:
        port = find_port()
        if not port:
            print(
                f"ERROR: Tidak menemukan serial port. Coba kandidat: {', '.join(CANDIDATE_PORTS)}\n"
                "Gunakan --port /dev/ttyUSBx untuk specify manual.",
                file=sys.stderr,
            )
            sys.exit(1)

    # Build payload
    if args.command == "event":
        payload = {
            "status": args.status,
            "plate": args.plate,
            "anpr_status": args.anpr,
            "gate_name": args.gate,
            "timestamp": int(time.time()),
        }
        send(port, args.baud, "event", payload)

    elif args.command == "greeting":
        payload = {
            "text": args.text,
            "scroll": args.scroll,
            "color": args.color,
        }
        send(port, args.baud, "greeting", payload)

    elif args.command == "config":
        payload = {}
        if args.brightness is not None:
            if not 0 <= args.brightness <= 255:
                print("ERROR: --brightness harus antara 0-255", file=sys.stderr)
                sys.exit(1)
            payload["brightness"] = args.brightness
        if args.timeout is not None:
            payload["display_timeout"] = args.timeout
        if args.gate_name is not None:
            payload["gate_name"] = args.gate_name
        if not payload:
            print("ERROR: config butuh minimal satu opsi (--brightness, --timeout, atau --gate-name)", file=sys.stderr)
            sys.exit(1)
        send(port, args.baud, "config", payload)

    elif args.command == "reset":
        send(port, args.baud, "reset", {})


if __name__ == "__main__":
    main()
