#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PY32 <-> ATS2853P2 UART 协议测试

帧：CMD + LEN + DATA + CRC16(LE)

用法：
  python py32_uart_test.py set -p COM5 --vol 80 --treble 12 --bass 12 --vib 50
  python py32_uart_test.py crc-demo
"""

from __future__ import print_function

import argparse
import struct
import sys

try:
    import serial
except ImportError:
    serial = None

CMD_SET = 0x01
CMD_STATUS = 0x80


def crc16_modbus(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def build_frame(cmd, payload):
    hdr = bytes([cmd, len(payload)]) + payload
    crc = crc16_modbus(hdr)
    return hdr + struct.pack("<H", crc)


def build_set_frame(vol, treble, bass, vib):
    payload = bytes([
        vol & 0xFF,
        treble & 0xFF,
        bass & 0xFF,
        vib & 0xFF,
    ])
    return build_frame(CMD_SET, payload)


def parse_response(raw):
    if len(raw) < 4:
        return None
    cmd, ln = raw[0], raw[1]
    if cmd != CMD_STATUS or len(raw) < 4 + ln:
        return None
    data = raw[2:2 + ln]
    crc_rx = raw[2 + ln] | (raw[3 + ln] << 8)
    crc_calc = crc16_modbus(raw[:2 + ln])
    if crc_calc != crc_rx:
        return {"crc_ok": False, "crc_calc": crc_calc, "crc_rx": crc_rx}
    return {
        "crc_ok": True,
        "volume": data[0],
        "treble": data[1],
        "bass": data[2],
        "vibration": data[3],
        "bt_connected": data[4],
        "error": data[5],
    }


def send_and_read(port, baud, frame):
    if serial is None:
        print("pip install pyserial", file=sys.stderr)
        return 1
    print("TX:", " ".join("%02X" % b for b in frame))
    ser = serial.Serial(port, baud, timeout=0.5)
    ser.write(frame)
    rsp = ser.read(64)
    ser.close()
    print("RX:", " ".join("%02X" % b for b in rsp))
    info = parse_response(rsp)
    print("Parsed:", info)
    return 0


def main():
    parser = argparse.ArgumentParser(description="PY32 UART protocol test")
    sub = parser.add_subparsers(dest="action")

    p_set = sub.add_parser("set", help="Send CMD 0x01 batch set")
    p_set.add_argument("-p", "--port", required=True)
    p_set.add_argument("--vol", type=int, default=80)
    p_set.add_argument("--treble", type=int, default=12, help="0-24, 12=0dB")
    p_set.add_argument("--bass", type=int, default=12)
    p_set.add_argument("--vib", type=int, default=50)
    p_set.add_argument("--baud", type=int, default=115200)

    sub.add_parser("crc-demo", help="Print doc example frame")

    args = parser.parse_args()

    if args.action == "crc-demo":
        frame = build_set_frame(0x50, 0x0C, 0x0C, 0x32)
        print("Example set frame:")
        print(" ".join("%02X" % b for b in frame))
        return 0

    if args.action == "set":
        frame = build_set_frame(args.vol, args.treble, args.bass, args.vib)
        return send_and_read(args.port, args.baud, frame)

    parser.print_help()
    return 1


if __name__ == "__main__":
    sys.exit(main())
