# 远程控制协议（D2 命令）

手机/PC 经 **SPP**、**BLE GATT** 或 **UART** 下发命令，音箱解析后调用 `bt_manager_remote_cmd_dispatch()` 执行媒体/音量/通话等任务。

## 命令字

| 字节 | 含义 |
|------|------|
| `payload[0]` | 固定 `0xD2` |
| `payload[1]` | 子命令（见下表） |

### 子命令表

| sub | 功能 | API |
|-----|------|-----|
| `0x00` | 上一曲 | `bt_manager_media_play_previous()` |
| `0x01` | 下一曲 | `bt_manager_media_play_next()` |
| `0x02` | 暂停 | `bt_manager_media_pause()` |
| `0x03` | 播放 | `bt_manager_media_play()` |
| `0x04` | 播放/暂停 | `bt_manager_media_playpause()` |
| `0x05` | 音量+ | `bt_manager_volume_up()` |
| `0x06` | 音量- | `bt_manager_volume_down()` |
| `0x07` | 音效开关 | `bt_manager_audio_effect_switch()` |
| `0x08` | 接听 | `bt_manager_call_accept()` |
| `0x09` | 挂断/拒接 | `bt_manager_call_terminate()` |

## ACK 回包（可选）

| 字节 | 含义 |
|------|------|
| `0xD2` | 命令字 |
| `0x80` | ACK 标记 |
| 子命令 | 原命令 `payload[1]` |
| 状态 | `0`=成功；失败为 `(uint8_t)(-errno)` |

默认关闭。开启方式：

- SPP：`CONFIG_SYSTEM_APP_SPP_CMD_ACK=y`
- BLE：`CONFIG_SYSTEM_APP_BLE_CMD_ACK=y`（须先订阅 Notify）

---

## 通道 1：SPP（经典蓝牙 RFCOMM）

| 项目 | 值 |
|------|-----|
| UUID | `00001101-0000-1000-8000-00805F9B34FB` |
| 实现 | `system_app_spp_uart_bridge.c` |
| 测试 | `tools/SPP_CMD_TEST.md`、`spp_bt_cmd_test.py` |

**裸命令（推荐）：** `D2 01`（2 字节）

**带帧头：**

```
A5 5A | LEN_H | LEN_L | D2 | subcmd | ...
```

例下一曲：`A5 5A 00 02 D2 01`

---

## 通道 2：BLE GATT

| 项目 | 值 |
|------|-----|
| Service | `0xFFE0` |
| RX Write | `0xFFE1` |
| TX Notify | `0xFFE2` + CCC |
| 广播名 | `BT_LE_NAME`（如 `285L_LE`） |
| 广播 UUID | `0xFFE0`（Complete 16-bit UUID List） |
| 实现 | `system_app_ble_remote_cmd.c` |
| 测试 | `tools/BLE_CMD_TEST.md`、`ble_cmd_test.py` |

**裸命令：** 向 `0xFFE1` Write `D2 01`

**带帧头（与 SPP 相同）：** Write `A5 5A 00 02 D2 01`

---

## 通道 3：UART 二进制帧

| 项目 | 值 |
|------|-----|
| 帧格式 | `D3 \| VER(0x02) \| LEN_H \| LEN_L \| payload \| CRC8` |
| payload | `D2 \| subcmd` |
| 实现 | `bt_manager_uart_rx.c` |
| 测试 | `tools/uart_bt_cmd_test.py` |

例音量+：`D3 02 00 02 D2 05 CRC`

---

## 蓝牙名称（双模）

| 属性 | 示例 | 用途 |
|------|------|------|
| `BT_NAME` | `285L_BR` | 经典蓝牙配对列表 |
| `BT_LE_NAME` | `285L_LE` | BLE 扫描/广播 |

配置：`app_conf/285l2_mc_bt/nvram.prop`

---

## 固件配置（285l2_mc_bt）

```
CONFIG_BT_SPP=y
CONFIG_BT_BLE=y
CONFIG_SYSTEM_APP_BLE_REMOTE_CMD=y
# CONFIG_SYSTEM_APP_SPP_CMD_ACK=y
# CONFIG_SYSTEM_APP_BLE_CMD_ACK=y
```

## 日志关键字

```
[spp] init OK / connected / RX
[ble_cmd] init OK / connected / RX / notify on
[remote_cmd] src=spp|ble|uart cmd D2 sub=0x.. ret=0 (ok)
```
