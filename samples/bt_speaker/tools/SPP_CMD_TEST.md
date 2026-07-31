# 蓝牙 SPP 远程命令测试说明

## 路径说明（不是 UART）

```
手机/PC APP  --[蓝牙 SPP RFCOMM]-->  音箱固件
                                      bt_manager_spp
                                      system_app_spp_uart_bridge.c
                                      bt_manager_remote_cmd_dispatch()
                                      bt_manager_media_* / volume_* / ...
                                      --[AVRCP/HFP]-->  手机执行切歌/音量等
```

**UART 仅用于看日志**（`printk`），命令数据走蓝牙 SPP。

## 前提

1. `prj.conf` 已开启 `CONFIG_BT_SPP=y`
2. 音箱已与手机/PC **配对并连接**（A2DP 播放中更易验证媒体键）
3. 手机/PC 侧打开 **SPP 串口**（配对后会出现虚拟 COM 或 SPP 专用 App）

## 命令格式

### 方式 A：裸命令（推荐调试）

直接发送 **2 字节**（十六进制）：

| 功能     | 十六进制发送 |
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
| 进入 ADFU | `D2 0A`        |

### 方式 B：带帧头（适合长数据/多包）

```
A5 5A | LEN_H | LEN_L | payload...
```

例：下一曲，payload = `D2 01`，长度 = 2：

```
A5 5A 00 02 D2 01
```

## 手机测试

1. 安装 **Serial Bluetooth Terminal**（或类似 SPP 调试 App）
2. 与音箱配对，在 App 里选择音箱的 **SPP / Serial** 服务连接
3. 选择 **Hex** 模式，发送 `D2 01`
4. USB 接音箱调试串口，查看日志：

```
[spp] connected ch=...
[spp] RX ch=... len=2
[spp] bare cmd len=2 sub=0x01
[remote_cmd] src=spp cmd D2 sub=0x01
[remote_cmd] src=spp media_play_next ret=0 (ok)
```

## Windows PC 测试

1. 设置 → 蓝牙 → 配对音箱
2. 部分驱动会生成 **传出 COM 口**（SPP）；在设备管理器查看 COM 号
3. 用 **PuTTY** / **sscom** 打开该 COM，波特率通常无关（RFCOMM）
4. 以 **十六进制发送** `D2 05` 测音量加；`D2 0A` 重启进入 ADFU（设备会断开蓝牙）

若无 COM 口，可用 **nRF Connect Desktop** 或 Python `pyserial` 连蓝牙 SPP（需额外库）。

## SPP 回包（可选）

默认 **不回包**。需要时在 `prj.conf` 打开：

```
CONFIG_SYSTEM_APP_SPP_CMD_ACK=y
```

| 你发送（裸） | 音箱回包（裸，4 字节） | 含义 |
|--------------|------------------------|------|
| `D2 01` 下一曲 | `D2 80 01 00` | 子命令 0x01 成功 |
| `D2 01`（未连 A2DP） | `D2 80 01 16` | 失败，状态字节常为 `-errno`（如 22=EINVAL） |

- 第 2 字节固定 `0x80` 表示 ACK  
- 第 3 字节为 **原命令子码**  
- 第 4 字节：`0` 成功，非 `0` 失败码  

若用 **A5 5A 帧** 发命令，回包同样带帧头，例如：

```
A5 5A 00 04 D2 80 01 00
```

日志：`[spp] TX ACK bare ...` 或 `TX ACK framed ...`

## 成功 / 失败判断

| 日志 | 含义 |
|------|------|
| `[spp] init OK` | SPP 服务已注册 |
| `[spp] connected` | 手机已连上 SPP |
| `[spp] RX len=...` | 收到蓝牙数据 |
| `ret=0 (ok)` | 命令已交给 bt_manager |
| `ret=-22` 等 | 无 A2DP 连接或当前状态不允许 |

## 与 UART 命令的区别

| 入口 | 物理链路 | 帧格式 |
|------|----------|--------|
| **SPP（本说明）** | 蓝牙 RFCOMM | 裸 `D2 xx` 或 `A5 5A`+payload |
| UART RX | TTL/USB 串口线 | `D3 02 len payload CRC8` |

两者最终都调用 `bt_manager_remote_cmd_dispatch()`。

## 扩展自定义任务

在 `bt_manager_remote_cmd.c` 的 `switch (payload[1])` 中增加 `case 0x10:` 等，
或提供强符号覆盖 `bt_manager_remote_cmd_dispatch()`。
