#include <kernel.h>
#include <string.h>
#include <stdio.h>
#include <nvram_config.h>
#include <property_manager.h>
#include <logging/sys_log.h>

#define MAX_BT_KEYS 8
#define BT_ADDR_STR_LEN 13  // 12位地址 + 结束符
#define MAX_KEY_NAME_LEN 32

//struct bt_keys
struct bt_keys_data {
	uint8_t id;
	uint8_t type;
    uint8_t addr[6];  // 蓝牙地址
};

struct valid_addr_list {
    char addrs[MAX_BT_KEYS][BT_ADDR_STR_LEN];
	uint8_t addrs_index[MAX_BT_KEYS];
    int count;
};

typedef struct {
	u8_t  val[6];
} le_bt_addr_t;

typedef struct {
	u8_t      type;
	le_bt_addr_t a;
} le_bt_addr_le_t;

#define BT_ADDR_LE_ANY  (&(le_bt_addr_le_t) { 0, { { 0, 0, 0, 0, 0, 0 } } })

static void bt_addr_to_str(const uint8_t *addr, char *str)
{
    snprintf(str, BT_ADDR_STR_LEN, "%02x%02x%02x%02x%02x%02x",
             addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
}

static bool is_addr_valid(const char *addr_str, const struct valid_addr_list *addr_list)
{
    for (int i = 0; i < addr_list->count; i++) {
		printk("check addr %s %s\n", addr_list->addrs[i], addr_str);
        if (strncmp(addr_list->addrs[i], addr_str, 12) == 0) {
            return true;
        }
    }
    return false;
}

static int system_le_cleanup_load_bt_keys(struct valid_addr_list *addr_list)
{
    char key_name[MAX_KEY_NAME_LEN];
    struct bt_keys_data keys_data;
    int ret;

    addr_list->count = 0;

    for (int i = 0; i < MAX_BT_KEYS; i++) {
        snprintf(key_name, sizeof(key_name), "%s_%d", CFG_BT_KEYS, i);

        ret = nvram_config_get(key_name, &keys_data, sizeof(keys_data));
        if (ret > 0) {
            bt_addr_to_str(keys_data.addr, addr_list->addrs[addr_list->count]);
			addr_list->addrs_index[addr_list->count] = i;
            SYS_LOG_INF("Found valid BT_KEYS_%d with addr: %s",
                       i, addr_list->addrs[addr_list->count]);
            addr_list->count++;
        }
    }

    return addr_list->count;
}

static const char *extract_addr_from_key(const char *key_name, const char *prefix)
{
    size_t prefix_len = strlen(prefix);

    if (strncmp(key_name, prefix, prefix_len) != 0) {
        return NULL;
    }

    const char *addr_part = key_name + prefix_len;

    if (strlen(addr_part) < 12) {
        return NULL;
    }

    return addr_part;
}


static void check_cleanup_ble_nvram_on_boot(uint8_t *key_name, uint32_t key_len, void *priv_data)
{

	const struct valid_addr_list *addr_list = (const struct valid_addr_list *)priv_data;

    char ccc_prefix[32], sc_prefix[32], cf_prefix[32];

    snprintf(ccc_prefix, sizeof(ccc_prefix), "%s_", CFG_BT_CCC);
    snprintf(sc_prefix, sizeof(sc_prefix), "%s_", CFG_BT_SC);
    snprintf(cf_prefix, sizeof(cf_prefix), "%s_", CFG_BT_CF);

    const char *prefixes[] = {ccc_prefix, sc_prefix, cf_prefix};
    const int prefix_count = sizeof(prefixes) / sizeof(prefixes[0]);

    for (int i = 0; i < prefix_count; i++) {
        const char *addr_part = extract_addr_from_key(key_name, prefixes[i]);

		if(addr_part){

			printk("extrace %s from %s\n", addr_part, key_name);

			//如果不存在BT_KEYS_xxx,则所有的BT_CCC/BT_SC/BT_CF都需要删除
			if (addr_list->count == 0) {
				printk("[WRN]:delete unused nvram key %s\n", key_name);
				nvram_config_set(key_name, NULL, 0);
			}else{

				//找到对应的蓝牙记录但是不在BT_KEYS里面需要删除
				if(!is_addr_valid(addr_part, addr_list)){
					printk("[WRN]:delete unused nvram key %s\n", key_name);
					nvram_config_set(key_name, NULL, 0);
				}
			}
		}

    }
}


int system_le_cleanup_nvram_on_boot(void)
{
    struct valid_addr_list addr_list;
    int ret;
	uint8_t data_buf[128];

	uint32_t start_time = k_uptime_get_32();

    ret = system_le_cleanup_load_bt_keys(&addr_list);
    if (ret < 0) {
        SYS_LOG_WRN("Failed to load bt keys");
        return ret;
    }

	nvram_config_traverse(data_buf, sizeof(data_buf), (void *)&addr_list, check_cleanup_ble_nvram_on_boot, NULL);

	printk("le cleanup time:%d ms\n", (k_uptime_get_32() - start_time));

    return 0;
}

uint8_t hostif_bt_le_get_devices(le_bt_addr_le_t dev_buf[], uint8_t buf_count);

static int system_le_cleanup_load_bt_keys_poweroff(struct valid_addr_list *addr_list)
{

	le_bt_addr_le_t le_dev_buf[MAX_BT_KEYS];

    addr_list->count = 0;

	memset(le_dev_buf, 0, sizeof(le_bt_addr_le_t) * MAX_BT_KEYS);

	hostif_bt_le_get_devices(le_dev_buf, MAX_BT_KEYS);

    for (int i = 0; i < MAX_BT_KEYS; i++) {
		if (memcmp(&le_dev_buf[i], BT_ADDR_LE_ANY, sizeof(le_bt_addr_le_t))) {
            bt_addr_to_str((const uint8_t *)&le_dev_buf[i].a, addr_list->addrs[addr_list->count]);
			addr_list->addrs_index[addr_list->count] = i;
            SYS_LOG_INF("Found valid BT_KEYS_%d with addr: %s",
                       i, addr_list->addrs[addr_list->count]);
            addr_list->count++;
		}
    }

	printk("load keys %d\n", addr_list->count);

    return addr_list->count;
}

static int check_if_bt_keys_valid(uint8_t *key_name, const struct valid_addr_list *addr_list)
{
	uint8_t key_index;

	int i, valid_flag;

	key_index = key_name[8] - '0';

	valid_flag = false;
	for(i = 0; i < addr_list->count; i++){
		if(addr_list->addrs_index[i] == key_index){
			valid_flag = true;
			break;
		}
	}

	return valid_flag;
}


static void check_cleanup_ble_nvram_on_poweroff(uint8_t *key_name, uint32_t key_len, void *priv_data)
{

	const struct valid_addr_list *addr_list = (const struct valid_addr_list *)priv_data;

    char ccc_prefix[32], sc_prefix[32], cf_prefix[32];

    snprintf(ccc_prefix, sizeof(ccc_prefix), "%s_", CFG_BT_CCC);
    snprintf(sc_prefix, sizeof(sc_prefix), "%s_", CFG_BT_SC);
    snprintf(cf_prefix, sizeof(cf_prefix), "%s_", CFG_BT_CF);

    const char *prefixes[] = {ccc_prefix, sc_prefix, cf_prefix};
    const int prefix_count = sizeof(prefixes) / sizeof(prefixes[0]);

    for (int i = 0; i < prefix_count; i++) {
        const char *addr_part = extract_addr_from_key(key_name, prefixes[i]);

		if(addr_part){
			printk("extrace %s from %s\n", addr_part, key_name);

			if (addr_list->count == 0) {
				printk("[WRN]:delete unused nvram key %s\n", key_name);
				nvram_config_set(key_name, NULL, 0);
			}else{
				//找到对应的蓝牙记录但是不在BT_KEYS里面需要删除
		        if (!is_addr_valid(addr_part, addr_list)) {
					 printk("[WRN]:delete unused nvram key %s\n", key_name);
		             nvram_config_set(key_name, NULL, 0);
		        }
			}
		}
    }

	//查找BT_KEYS是否有不在addr_list里面的，如果有说明也需要删除
	//运行过程中有部分BT_KEYS可能已经删除
	if(strncmp(key_name, CFG_BT_KEYS, 7) == 0){
		if(!check_if_bt_keys_valid(key_name, addr_list)){
			printk("[WRN]:delete unused nvram key %s\n", key_name);
			nvram_config_set(key_name, NULL, 0);
		}
	}
}


int system_le_cleanup_nvram_on_poweroff(void)
{
    struct valid_addr_list addr_list;
    int ret;
	uint8_t data_buf[128];

	uint32_t start_time = k_uptime_get_32();

    ret = system_le_cleanup_load_bt_keys_poweroff(&addr_list);
    if (ret < 0) {
        SYS_LOG_WRN("Failed to load bt keys");
        return ret;
    }

	nvram_config_traverse(data_buf, sizeof(data_buf), (void *)&addr_list, check_cleanup_ble_nvram_on_poweroff, NULL);

	printk("poweroff le cleanup time:%d ms\n", (k_uptime_get_32() - start_time));

    return 0;
}

