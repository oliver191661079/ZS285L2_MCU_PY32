/*
 * ambic1010_hw_port.c — 硬件抽象层移植示例（以 PY32F005 / STM32 为例）
 *
 * 将此文件复制到你的 MCU 工程中，根据实际 GPIO 修改即可。
 */

#include "ambic1010_driver.h"

/* ================================================================
 * 配置区 —— 根据实际 MCU 修改
 * ================================================================ */

#define LED_GPIO_PORT    GPIOA       /* 灯带数据脚所在的 GPIO 端口 */
#define LED_GPIO_PIN     GPIO_PIN_0  /* 灯带数据脚对应的引脚号 */
#define CPU_FREQ_MHZ     24          /* MCU 主频（MHz），用于精确延迟校准 */

/* ================================================================
 * 硬件抽象实现
 * ================================================================ */

/* ---- GPIO 输出 ---- */
void ambic1010_hw_gpio_set(int high)
{
	if (high) {
		HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_SET);
	} else {
		HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_PIN_RESET);
	}
}

/* ---- 微秒延迟（用 NOP 或硬件定时器） ---- */
void ambic1010_hw_delay_us(unsigned int us)
{
	/*
	 * 方法 1：硬件定时器微秒延迟（推荐）
	 *   timer_delay_us(us);
	 *
	 * 方法 2：NOP 循环（适用于简单 MCU）
	 *   volatile uint32_t n = us * CPU_FREQ_MHZ / 4;
	 *   while (n--) { __NOP(); }
	 *
	 * 方法 3：SysTick 轮询（ARM Cortex-M）
	 *   uint32_t t0 = SysTick->VAL;
	 *   uint32_t load = SysTick->LOAD;
	 *   uint32_t ticks = us * (load / 1000 + 1) / 1000;
	 *   while (((t0 - SysTick->VAL) & 0xFFFFFF) < ticks);
	 */

	/* 示例：NOP 循环（24MHz CPU，每循环约 4 周期） */
	volatile uint32_t n = us * (CPU_FREQ_MHZ / 4U);
	while (n--) {
		__NOP();
	}
}

/* ---- 关全局中断 ---- */
unsigned int ambic1010_hw_irq_lock(void)
{
	/*
	 * ARM Cortex-M:
	 *   uint32_t primask = __get_PRIMASK();
	 *   __disable_irq();
	 *   return primask;
	 *
	 * 通用 MCU（无 OS）：
	 *   EA = 0;  // 51 系列
	 *   return 0;
	 *
	 * FreeRTOS:
	 *   taskENTER_CRITICAL();
	 *   return 0;
	 */
	__disable_irq();
	return 0;
}

/* ---- 恢复中断 ---- */
void ambic1010_hw_irq_unlock(unsigned int key)
{
	(void)key;
	/*
	 * ARM Cortex-M:
	 *   if (!key) __enable_irq();
	 *
	 * 通用 MCU：
	 *   EA = 1;  // 51 系列
	 *
	 * FreeRTOS:
	 *   taskEXIT_CRITICAL();
	 */
	__enable_irq();
}
