#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成 BLE GATT Write 测试用十六进制载荷（nRF Connect 等工具 Hex 发送）。

GATT：Service 0xFFE0，Write 特征 0xFFE1

裸命令：D2 + subcmd
带帧头：A5 5A | len_hi | len_lo | D2 | subcmd（与 SPP 相同）

用法：
  python ble_cmd_test.py next
  python ble_cmd_test.py vol_up --framed
  python ble_cmd_test.py --list
"""

from __future__ import print_function

import argparse
import sys

SUBCMDS = {
    "prev": 0x00,
    "next": 0x01,
    "pause": 0x02,
    "play": 0x03,
    "playpause": 0x04,
    "vol_up": 0x05,
    "vol_down": 0x06,
    "effect": 0x07,
    "answer": 0x08,
    "hangup": 0x09,
}


def build_bare(subcmd):
    return bytes([0xD2, subcmd & 0xFF])


def build_framed(subcmd):
    payload = build_bare(subcmd)
    plen = len(payload)
    return bytes([0xA5, 0x5A, (plen >> 8) & 0xFF, plen & 0xFF]) + payload


def main():
    parser = argparse.ArgumentParser(description="BLE GATT Write Hex 生成")
    parser.add_argument("--framed", action="store_true", help="生成 A5 5A 帧（默认裸 D2）")
    parser.add_argument("--list", action="store_true", help="列出子命令")
    parser.add_argument("cmd", nargs="?", choices=list(SUBCMDS.keys()))
    args = parser.parse_args()

    if args.list:
        for name, code in SUBCMDS.items():
            print("  %-12s  bare: D2 %02X" % (name, code))
        return 0

    if not args.cmd:
        parser.print_help()
        return 1

    frame = build_framed(SUBCMDS[args.cmd]) if args.framed else build_bare(SUBCMDS[args.cmd])
    print("命令: %s" % args.cmd)
    print("模式: %s" % ("A5 5A 帧" if args.framed else "裸 D2（推荐）"))
    print("Hex:  %s" % " ".join("%02X" % b for b in frame))
    print("\n步骤：")
    print("  1. nRF Connect 扫描 BT_LE_NAME（如 285L_LE）")
    print("  2. 连接后打开 Service 0xFFE0")
    print("  3. 向 Characteristic 0xFFE1 Write 上述 Hex")
    print("  4. 串口日志应出现: [ble_cmd] RX ... [remote_cmd] src=ble ... ret=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
