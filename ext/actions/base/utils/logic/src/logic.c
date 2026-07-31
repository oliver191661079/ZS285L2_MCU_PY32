
#ifdef CONFIG_LOGIC_ANALYZER
#include <kernel.h>
#include <gpio.h>
#include <logic.h>

#define LOGIC_NUM 9
static struct device *logic_analysis_dev = NULL;
static bool status[LOGIC_NUM];
static unsigned char gpios[LOGIC_NUM] = {0, 1, 5, 8, 9, 10, 16, 21, 38};

void logic_set_1(unsigned char index)
{
	if (index >= LOGIC_NUM) {
		return;
	}
	gpio_pin_write(logic_analysis_dev, gpios[index], 1);
	status[index] = true;
}
void logic_set_0(unsigned char index)
{
	if (index >= LOGIC_NUM) {
		return;
	}
	gpio_pin_write(logic_analysis_dev, gpios[index], 0);
	status[index] = false;
}

void logic_set(unsigned char index, bool high)
{
	if (high) {
		logic_set_1(index);
	} else {
		logic_set_0(index);
	}
}

void logic_init(void)
{
	int i;

	logic_analysis_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);

	if(logic_analysis_dev == NULL) {
		printk("Can not get gpio device!\n");
		return;
	} else {
		printk("Got gpio device.\n");
	}

	for(i = 0; i<LOGIC_NUM; i++){
		gpio_pin_configure(logic_analysis_dev, gpios[i], GPIO_DIR_OUT);
		logic_set_0(0);
	}

}

void logic_switch(unsigned char index)
{
	bool revert;

	if (index >= LOGIC_NUM) {
		return;
	}

	revert = status[index];
	if (revert) {
		logic_set_0(index);
	} else {
		logic_set_1(index);
	}
}
#endif
