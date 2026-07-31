# BLE GATT 远程命令测试说明

## 路径说明

```
手机 APP (GATT Client)  --[BLE Write 0xFFE1]-->  音箱 (GATT Server)
                                                    system_app_ble_remote_cmd.c
                                                    bt_manager_remote_cmd_dispatch()
                                                    bt_manager_media_* / volume_* / ...
                                                    --[AVRCP/HFP]-->  手机执行切歌/音量等

手机 APP  --[订阅 Notify 0xFFE2]-->  可选 ACK：D2 80 | 子命令 | 状态
```

与 **SPP**、**UART** 共用同一套 `D2` 子命令表，见 `SPP_CMD_TEST.md`。

## GATT 定义

| 项目 | UUID (16-bit) | 属性 |
|------|---------------|------|
| Service | `0xFFE0` | Primary |
| RX 写命令 | `0xFFE1` | Write / Write Without Response |
| TX 状态 | `0xFFE2` | Notify + CCC |

完整 128-bit UUID（Bluetooth Base）：`0000FFE0-0000-1000-8000-00805F9B34FB` 等。

## 前提

1. `prj.conf`：`CONFIG_BT_BLE=y`、`CONFIG_SYSTEM_APP_BLE_REMOTE_CMD=y`
2. 手机 **nRF Connect** 等工具扫描 BLE 广播名（`BT_LE_NAME`，如 `285L_LE`），广播中含 Service UUID `0xFFE0`
3. 连接后展开 **Unknown Service `0xFFE0`**
4. 对 **0xFFE2** 打开 Notify（若需 ACK）
5. 向 **0xFFE1** 以 **Write** 发送十六进制 `D2 xx`

## 命令格式（与 SPP 相同）

| 功能     | 十六进制 Write |
|----------|----------------|
| 上一曲   | `D2 00`        |
| 下一曲   | `D2 01`        |
| 暂停     | `D2 02`        |
| 播放     | `D2 03`        |
| 播放/暂停| `D2 04`        |
| 音量+    | `D2 05`        |
| 音量-    | `D2 06`        |
| 音效切换 | `D2 07`        |
| 接听     | `D2 08`        |
| 挂断     | `D2 09`        |

单次 Write 有效载荷 2 字节即可；也支持 **A5 5A 帧**（与 SPP 相同），例如下一曲：`A5 5A 00 02 D2 01`。

可用脚本生成 Hex：`python tools/ble_cmd_test.py next`

完整协议见 `tools/REMOTE_CMD_PROTOCOL.md`。

## nRF Connect 测试步骤

1. 扫描并连接 `285L_LE`（或 `nvram.prop` 中 `BT_LE_NAME`）
2. 找到 Service `0xFFE0`
3. 点 **0xFFE2** → 打开 **Notify**（小铃铛图标）
4. 点 **0xFFE1** → **Write**，类型选 **BYTE ARRAY**，输入 `D2-01`（下一曲）
5. USB 调试串口查看日志：

```
[ble_cmd] init OK svc=0xFFE0 rx=0xFFE1 tx=0xFFE2 ack=0
[ble_cmd] connected conn=...
[ble_cmd] notify on
[ble_cmd] RX len=2 D2 sub=0x01
[remote_cmd] src=ble cmd D2 sub=0x01
[remote_cmd] src=ble media_play_next ret=0 (ok)
```

## Notify 回 ACK（可选）

默认 **不 Notify**。需要时在 `prj.conf` 打开：

```
CONFIG_SYSTEM_APP_BLE_CMD_ACK=y
```

| 你 Write | 音箱 Notify（4 字节） | 含义 |
|----------|----------------------|------|
| `D2 01`  | `D2 80 01 00`        | 下一曲成功 |
| `D2 01`（无 A2DP） | `D2 80 01 16` | 失败，第 4 字节为错误码 |

- 第 2 字节固定 `0x80` 表示 ACK
- 第 3 字节为 **原子命令**
- 第 4 字节：`0` 成功，非 `0` 失败

须先对 **0xFFE2** 开启 Notify，否则固件跳过 ACK 并打印 `[ble_cmd] ACK skip`。

## 经典蓝牙名 vs BLE 广播名

| 配置项 | 示例 | 用途 |
|--------|------|------|
| `BT_NAME` | `285L_BR` | 经典蓝牙配对列表 |
| `BT_LE_NAME` | `285L_LE` | BLE 扫描/广播 Complete Name |

可在 `app_conf/285l2_mc_bt/nvram.prop` 分别修改。首次出厂若未写 `BT_LE_NAME`，固件会在经典名后追加 `_LE`。

## 与 SPP / UART 对比

| 入口 | 链路 | 帧格式 |
|------|------|--------|
| **BLE GATT（本说明）** | BLE Write/Notify | 裸 `D2 xx` |
| SPP | 经典蓝牙 RFCOMM | 裸 `D2 xx` 或 `A5 5A`+payload |
| UART RX | TTL/USB 串口 | `D3 02 len payload CRC8` |

三者均调用 `bt_manager_remote_cmd_dispatch()`。

## 常见问题

| 现象 | 处理 |
|------|------|
| 扫描不到 `0xFFE0` | 确认已连上且 GATT 已发现；重启后看 `[ble_cmd] init OK` |
| Write 无反应 | 确认写的是 **0xFFE1** 而非 0xFFE2 |
| 无 ACK | 打开 `CONFIG_SYSTEM_APP_BLE_CMD_ACK` 并对 0xFFE2 开 Notify |
| 命令 ret≠0 | 需手机已通过经典蓝牙连 A2DP（媒体键类命令） |
