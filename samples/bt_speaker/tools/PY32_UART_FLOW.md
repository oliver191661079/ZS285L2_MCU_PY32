# PY32 ↔ ATS2853P2 通讯流程（285L2）

## 角色与方向

```
  PY32  --CMD 0x01-->  ATS2853 执行音量/音效/震感
  PY32  <--CMD 0x80--  ATS2853 回传状态（仅响应）
```

## 两路灯带

| 灯带 | GPIO | 角色 |
|------|------|------|
| WS2812 | **GPIO8** | 氛围灯 |
| AMBIC1010 | **GPIO14** (LED_DO) | 音乐律动 4×44 矩阵 |

PY32 UART 协议 **不含** 灯带指令。

## 配置

```
CONFIG_BT_MUSIC_LED_STRIP=y
CONFIG_BT_MUSIC_LED_STRIP_AMBIENT=y
CONFIG_BT_MUSIC_LED_STRIP_GPIO=8
CONFIG_BT_MUSIC_LED_STRIP2=y
CONFIG_BT_MUSIC_LED_RHYTHM=y
CONFIG_BT_MUSIC_LED_STRIP2_GPIO=14
CONFIG_BT_MUSIC_LED_STRIP2_COUNT=176
```

## 代码

| 功能 | 文件 |
|------|------|
| PY32 UART | `system_app_py32_uart.c` |
| GPIO8 氛围灯 | `btmusic_ws2812.c` → `btmusic_ws2812_ambient_set` |
| GPIO14 律动 | `btmusic_ws2812.c` → `btmusic_ws2812_show_spectrum` |
