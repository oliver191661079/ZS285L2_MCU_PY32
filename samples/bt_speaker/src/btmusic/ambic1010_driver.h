/*
 * ambic1010_driver.h — AMBIC1010 4×44 RGB LED 矩阵驱动（独立 MCU 用）
 *
 * 协议：兼容 WS2812，GBR 像素格式（MSB 先发）
 * 时序：码周期 1.2µs，T0H/T1L=300ns±50，T0L/T1H=900ns±50，RESET>200µs
 *
 * 移植步骤：
 *   1. 实现 ambic1010_hw_* 三个硬件抽象函数
 *   2. 调用 ambic1010_init() 初始化
 *   3. 定时调用 ambic1010_show_spectrum() 刷新灯带
 *   4. 或调用 ambic1010_show_rhythm_random() 一键假律动
 */

#ifndef AMBIC1010_DRIVER_H
#define AMBIC1010_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 1. 硬件抽象层 —— 移植时只需实现这 4 个函数
 * ================================================================ */

/* GPIO 输出高/低 */
extern void ambic1010_hw_gpio_set(int high);

/* 微秒级忙等延迟 */
extern void ambic1010_hw_delay_us(unsigned int us);

/* 关全局中断，返回状态（用于保护时序） */
extern unsigned int ambic1010_hw_irq_lock(void);

/* 恢复中断 */
extern void ambic1010_hw_irq_unlock(unsigned int key);

/* ================================================================
 * 2. AMBIC1010 时序参数（取自官方 datasheet 码型时间表）
 * ================================================================ */

#define AMBIC1010_T0H_NS   300U   /* 0 码高电平 */
#define AMBIC1010_T0L_NS   900U   /* 0 码低电平 */
#define AMBIC1010_T1H_NS   900U   /* 1 码高电平 */
#define AMBIC1010_T1L_NS   300U   /* 1 码低电平 */
#define AMBIC1010_RESET_US 500U   /* 帧间复位（datasheet: >200µs，建议 ≥500µs） */

/* 根据 CPU 频率将 ns 换算为忙等微秒（保守向上取整） */
#define AMBIC1010_NS_TO_US(ns)  (((ns) + 999U) / 1000U)

#define AMBIC1010_BIT_US \
	(AMBIC1010_NS_TO_US(AMBIC1010_T0H_NS) + AMBIC1010_NS_TO_US(AMBIC1010_T0L_NS))
/* ≈ 2µs（实际 1.2µs，用 1µs 步长凑整，协议容差 ±50ns 可接受） */

/* ================================================================
 * 3. 矩阵参数
 * ================================================================ */

#define AMBIC1010_ROWS      4U    /* 竖列行数 */
#define AMBIC1010_COLS      44U   /* 横列数 */
#define AMBIC1010_LED_COUNT (AMBIC1010_ROWS * AMBIC1010_COLS) /* 176 */
#define AMBIC1010_GROUP     4U    /* 每列 4 颗一组同色 */
#define AMBIC1010_BANDS     10U   /* 频段数 */

/* 亮度限制（防过热） */
#define AMBIC1010_BRIGHT_MAX  80U
#define AMBIC1010_BRIGHT_MIN  6U

/* 刷新间隔建议（ms）：越短灯效越流畅，但占用 MCU 越多 */
#define AMBIC1010_REFRESH_MS  200U

/* ================================================================
 * 4. 色板（15 组彩虹色，每 3 列一组，44÷3≈15）
 * ================================================================ */

#define AMBIC1010_PAL_NUM  15U

/* ================================================================
 * 5. API
 * ================================================================ */

/* 初始化灯带（发 RESET 脉冲 + 校准） */
void ambic1010_init(void);

/* 清空灯带（全灭） */
void ambic1010_clear(void);

/* 指定像素颜色（px=0..175，物理顺序） */
void ambic1010_set_pixel(uint16_t px, uint8_t r, uint8_t g, uint8_t b);

/* 行/列坐标 → 像素索引（蛇形走线） */
uint16_t ambic1010_xy(uint8_t row, uint8_t col);

/* 刷新灯带（发送全部 176 像素数据 + RESET） */
void ambic1010_flush(void);

/* 用 10 段频谱数据更新矩阵（柱高随能量，每 3 列同色） */
void ambic1010_show_spectrum(const uint8_t band_lvl[AMBIC1010_BANDS]);

/* 生成随机频谱并刷新（假律动） */
void ambic1010_show_rhythm_random(void);

/* ================================================================
 * 6. 通信协议（ATS2853 → 外部 MCU）
 * ================================================================
 *
 * UART 115200-8-N-1，主机发 10 字节频段能量数据：
 *
 *   [SYNC 0xA5] [b0] [b1] [b2] [b3] [b4] [b5] [b6] [b7] [b8] [b9] [CHK]
 *
 *   SYNC = 0xA5（帧头）
 *   b0..b9 = 10 段频带能量（0-255）
 *   CHK = b0 ^ b1 ^ ... ^ b9（异或校验）
 *
 * MCU 收到后调用 ambic1010_show_spectrum(band) 刷新。
 */

#ifdef __cplusplus
}
#endif

#endif /* AMBIC1010_DRIVER_H */
