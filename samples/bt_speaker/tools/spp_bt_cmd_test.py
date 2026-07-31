#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成蓝牙 SPP 测试用十六进制帧（在手机上用 Hex 发送）。

裸命令（推荐）：2 字节  D2 + subcmd
带帧头：A5 5A | len_hi | len_lo | D2 | subcmd

用法：
  python spp_bt_cmd_test.py next
  python spp_bt_cmd_test.py vol_up --framed
  python spp_bt_cmd_test.py --list
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
    parser = argparse.ArgumentParser(description="SPP 蓝牙命令 Hex 生成")
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
    print("模式: %s" % ("A5 5A 帧" if args.framed else "裸 D2（SPP 推荐）"))
    print("Hex:  %s" % " ".join("%02X" % b for b in frame))
    print("\n在手机 Serial Bluetooth Terminal 中选 Hex 发送上述字节。")
    print("固件日志应出现: [spp] RX ... [remote_cmd] src=spp ... ret=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
