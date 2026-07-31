# ZS285L2 BTM (ATS2853P2 蓝牙音箱)

基于 Zephyr RTOS 的 ATS2853P2 蓝牙音箱固件，负责音频播放、WS2812 灯带控制、以及与低压板 PY32F005 MCU 的 UART 通信。

## 目录结构

```
ZS285L2/
├── samples/bt_speaker/          # 应用层代码
│   └── src/
│       ├── btmusic/             # 蓝牙音乐播放 + WS2812 氛围灯
│       │   ├── btmusic_main.c   #   音频播放、频谱采集、律动数据发送
│       │   ├── btmusic_ws2812.c #   GPIO8 WS2812 氛围灯（GPIO bitbang + 8 色循环）
│       │   └── btmusic_ws2812_rhythm.c  # 律动灯（已移至外部 MCU）
│       ├── system_app/          # 系统应用
│       │   └── system_app_py32_uart.c   # PY32 MCU UART 协议（50ms 频谱+状态帧）
│       └── common/app_tws.c     # TWS 真无线立体声
├── boards/csky/ats2853p2_evb/   # 板级配置
│   ├── board.c                  #   GPIO pinmux（UART1 TX=GPIO21, WS2812=GPIO8）
│   └── board_285l2.h            #   引脚宏定义
├── drivers/                     # 硬件驱动（SPI、UART、DMA、I2S、GPIO 等）
├── arch/csky/                   # CSKY 架构适配
├── include/                     # 公共头文件
├── subsys/                      # 子系统（蓝牙协议栈、文件系统等）
├── ext/                         # 第三方库
└── scripts/                     # 构建脚本
```

## 与 PY32F005 MCU 的通信

UART1（TX=GPIO21, RX=GPIO22, 115200bps, 硬件 TX）每 50ms 发送一帧：

| 字段 | 长度 | 说明 |
|------|------|------|
| Addr | 1B | 0x02（本机地址） |
| CMD | 1B | 0x10 |
| LEN | 1B | 0x11（17 字节） |
| 状态 | 6B | 音量、高音、低音、震感、连接状态、错误码 |
| 频谱 | 10B | band0~band9，DSP 实时频谱能量 |
| 播放 | 1B | 0=停止, 1=播放中 |
| CRC16 | 2B | 覆盖 Addr+CMD+LEN+DATA |

详细协议见 `PY32F005E1xM7-ATS2853P2通讯协议.md`。

## 关键配置

- **芯片**：ATS2853P2 (CSKY CK802, 264MHz)
- **RTOS**：Zephyr（定制版）
- **音频**：I2S TX0 → ACM8635 codec，支持 A2DP / TWS
- **灯带**：GPIO8 WS2812 × 25 颗（氛围灯 8 色循环，10s 切换）
- **构建**：`bash build.sh`（选择 `285l2_mc_bt` 配置）

## 开发环境

- Windows 10/11 + MSYS2
- CSKY 工具链
- 烧录工具：Actions 专用烧录器
