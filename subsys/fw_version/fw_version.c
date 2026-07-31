#include <kernel.h>
#include <soc.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "fw_version.h"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_FW_VERSION

const struct fw_version *fw_version_get_current(void)
{
	const struct fw_version *ver =
		(struct fw_version *)soc_boot_get_fw_ver_addr();

	return ver;
}

void fw_version_dump(const struct fw_version *ver)
{
	printk("Firmware Version: 0x%08x\n", ver->version_code);
	printk("  System Version: 0x%08x\n", ver->system_version_code);
#ifdef CONFIG_FIRMWARE_THIRDPARTY_VERSION
	printk("Thirdparty mcu Version: 0x%08x\n", ver->thirdparty_mcu_version_code);
	printk("Thirdparty dsp Version: 0x%08x\n", ver->thirdparty_dsp_version_code);
#endif
	printk("    Version Name: %s\n", ver->version_name);
	printk("      Board Name: %s\n", ver->board_name);
	printk("Full version: %d\n", fw_version_get_full_version());
}

int fw_version_check(const struct fw_version *ver)
{
	u32_t checksum;

	if (ver->magic != FIRMWARE_VERSION_MAGIC)
		return -1;

	checksum = utils_crc32(0, (const u8_t *)ver, sizeof(struct fw_version) - 4);

	if (ver->checksum != checksum)
		return -1;

	return 0;
}

#ifdef CONFIG_FIRMWARE_THIRDPARTY_VERSION

u32_t fw_version_get_thirdparty_mcu_version(void)
{
	const struct fw_version *ver =
		(struct fw_version *)soc_boot_get_fw_ver_addr();

	return ver->thirdparty_mcu_version_code;
}

u32_t fw_version_get_thirdparty_dsp_version(void)
{
	const struct fw_version *ver =
		(struct fw_version *)soc_boot_get_fw_ver_addr();

	return ver->thirdparty_dsp_version_code;
}

#endif

int fw_version_get_full_version(void)
{
	char *vPos;
    char version[16];

	const struct fw_version *ver =
		(struct fw_version *)soc_boot_get_fw_ver_addr();

    vPos = strchr(ver->version_name, 'v');
    if (vPos == NULL){
		return -1;
	}

    int i = 0;
    vPos++;
    while (isdigit(*vPos) || *vPos == '.') {
		if(*vPos != '.'){
	        version[i++] = *vPos;
		}
        vPos++;
		if(i == 15){
			break;
		}
    }
    version[i] = '\0';

    return atoi(version);

}

u32_t fw_version_get_name_version(void)
{
	u32_t namever = 0;
	char *vPos;
	u8_t  i, temp[16];
	const struct fw_version *ver =(struct fw_version *)soc_boot_get_fw_ver_addr();

	vPos = strchr(ver->version_name, 'v');
	if (vPos == NULL){
		return -1;
	}
	vPos++;
	i = 0;
	memset(temp, 0, sizeof(temp));
	while (isdigit(*vPos) || *vPos == '.') {
		if (*vPos != '.') {
	        temp[i++] = *vPos;
		}
		if (*vPos == '.' || i == 15) {
			namever = (namever << 8) + (atoi(temp) & 0xFF);
			memset(temp, 0, sizeof(temp));
			i = 0;
		}
		vPos++;
	}
	if (i > 0) {
		namever = (namever << 8) + (atoi(temp) & 0xFF);
	}
	return namever;
}
