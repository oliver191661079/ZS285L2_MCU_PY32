#include <os_common_api.h>
#include <zephyr.h>
#include <string.h>
#include <bt_manager.h>
#include <logging/sys_log.h>
#include "nsm_sppble_test.h"
#include <flash.h>
#include <device.h>
#include <img_load.h>
#include <mem_manager.h>
#include <media_player.h>
#include <property_manager.h>
#include <soc.h>

typedef struct{
	u8_t cmd;
	u8_t len;
}nsm_sppble_cmd_t;

const nsm_sppble_cmd_t nsm_sppble_cmd_tbl[]=
{
	{0x02,	1},
	{0x0c,	1},
	{0x09,	1},
	{0x19,	2},//tx excute
	{0x1A,	0},
	{0x17,	1},
	{0x32,	0},

	{0x37,	4},
	{0x01,	1},
	{0x33,		2},//rx excute
	//{0x34,	0},
	//{0x35,	0},
	//{0x44,	0},
	{0x31,	1},
	{0x19,	2},
	{0x18,	4},
};

static nsm_data_t *nsm_data_p = NULL;

nsm_data_t * nsm_get_data_p(void)
{
	return nsm_data_p;
}


static int nsm_result_reset(void)
{

	nsm_data_t * data_p =  nsm_get_data_p();

	if(!data_p || !data_p->dev)
		return -1;

	nsm_res_t res ;
	memset(&res, 0xff, sizeof(nsm_res_t));
	memcpy(res.magic, NSM_MAGIC, sizeof(NSM_MAGIC));
	res.magic[5] = 0;

	flash_write_protection_set(data_p->dev, false);

	//int ret = flash_erase(data_p->dev, NSM_RES_ADDR_START, sizeof(nsm_res_t));
	int ret = flash_erase(data_p->dev, NSM_RES_ADDR_START, 4096);

	if(!ret)
	{
		flash_write_protection_set(data_p->dev, false);
		ret = flash_write(data_p->dev, NSM_RES_ADDR_START, &res, sizeof(res.magic));
		if(!ret )
		{
			ret = flash_read(data_p->dev, NSM_RES_ADDR_START, &res, sizeof(nsm_res_t));

#if 0
			printk("%s magic %s \n",__func__, res.magic);
			printk("%s writen_bitmap 0x%02x\n",__func__, res.writen_bitmap);
			printk("%s read_bitmap 0x%02x\n",__func__, res.read_bitmap);

			for(u8_t i = 0; i < 8; i ++)
			{
				printk("result byte[%d]= 0x%08x\n", i, res.res[i].rx_result[0]);
				printk("result byte[%d]= 0x%08x\n", i, res.res[i].rx_result[1]);
				printk("result byte[%d]= 0x%08x\n", i, res.res[i].rx_result[2]);
				printk("result byte[%d]= 0x%08x\n", i, res.res[i].rx_result[3]);
				printk("\n");
			}

#endif
			SYS_LOG_INF("succ");
			return 0;
		}
	}
	SYS_LOG_INF("err %d\n", ret);
	return -2;
}

void nsm_check_result(void)
{
	nsm_data_t * data_p =  nsm_get_data_p();
	if(!data_p)
		return;

	nsm_res_t * res = &data_p->res;

	struct device *dev = device_get_binding(CONFIG_XSPI_NOR_ACTS_DEV_NAME);
	if(!dev)
	{
		SYS_LOG_ERR("get flash dev err\n");
		return;
	}

	data_p->dev = dev;

	int ret = flash_read(data_p->dev, NSM_RES_ADDR_START, res, sizeof(nsm_res_t));
	if(ret < 0)
	{
		SYS_LOG_ERR("read res err %d\n", ret);
		return;
	}

	res->magic[5] = 0;

	if(strcmp(NSM_MAGIC, res->magic) || (res->writen_bitmap == res->read_bitmap &&
			res->writen_bitmap == 0) )
	{
		if(!nsm_result_reset())
		{
			printk("%s reset succ\n", __func__);
			SYS_LOG_INF("succ");
			return;
		}
	}

#if 0
	printk("%s magic %s \n",__func__, res->magic);
	printk("%s writen_bitmap 0x%02x\n",__func__, res->writen_bitmap);
	printk("%s read_bitmap 0x%02x\n",__func__, res->read_bitmap);

	for(u8_t i = 0; i < 8; i ++)
	{
		printk("result byte[%d]= 0x%08x\n", i, res->res[i].rx_result[0]);
		printk("result byte[%d]= 0x%08x\n", i, res->res[i].rx_result[1]);
		printk("result byte[%d]= 0x%08x\n", i, res->res[i].rx_result[2]);
		printk("result byte[%d]= 0x%08x\n", i, res->res[i].rx_result[3]);
		printk("\n");
	}
#endif

	SYS_LOG_INF("succ\n");
}



/*"NSM"={0x6e, 0x73, 0x6d} */

/* SPP  */
/* UUID: "00001101-0000-1000-8000-00805F6E736D" */
static const uint8_t sppble_nsm_spp_uuid[16] = {0x6d, 0x73, 0x6e, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x01, 0x11, 0x00, 0x00};

/* BLE */
/* UUID: "e49a25f8-f69a-11e8-8eb2-f2801f16E736D" */
#define BLE_NSM_SERVICE_UUID BT_UUID_DECLARE_128( \
				0x6d, 0x73, 0x6e, 0x1f, 0x80, 0xf2, 0xb2, 0x8e, \
				0xe8, 0x11, 0x9a, 0xf6, 0xf8, 0x25, 0x9a, 0xe4)

/* UUID: "e49a25e0-f69a-11e8-8eb2-f2801f6E736D" */
#define BLE_NSM_CHA_RX_UUID BT_UUID_DECLARE_128( \
				0x6d, 0x73, 0x6e, 0x1f, 0x80, 0xf2, 0xb2, 0x8e, \
				0xe8, 0x11, 0x9a, 0xf6, 0xe0, 0x25, 0x9a, 0xe4)

/* UUID: "e49a28e1-f69a-11e8-8eb2-f2801f6E736D" */
#define BLE_NSM_CHA_TX_UUID BT_UUID_DECLARE_128( \
				0x6d, 0x73, 0x6e, 0x1f, 0x80, 0xf2, 0xb2, 0x8e, \
				0xe8, 0x11, 0x9a, 0xf6, 0xe1, 0x28, 0x9a, 0xe4)

static struct bt_gatt_attr nsm_gatt_attr[] = {
	BT_GATT_PRIMARY_SERVICE(BLE_NSM_SERVICE_UUID),
	BT_GATT_CHARACTERISTIC(BLE_NSM_CHA_RX_UUID, BT_GATT_CHRC_WRITE_WITHOUT_RESP|BT_GATT_CHRC_READ,
							BT_GATT_PERM_WRITE, NULL, NULL, NULL),
	BT_GATT_CHARACTERISTIC(BLE_NSM_CHA_TX_UUID, BT_GATT_CHRC_NOTIFY|BT_GATT_CHRC_READ,
							BT_GATT_PERM_READ, NULL, NULL, NULL),
	BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
};


void nsm_upload_data(void)
{
	nsm_data_t * data_p =  nsm_get_data_p();

	if(!data_p || !data_p->dev || data_p->uploaded_res)
		return;

	nsm_res_t * res = &data_p->res;

	printk("wmap 0x%x, rmap 0x%02x\n", res->writen_bitmap, res->read_bitmap);

	//flash is 0xff after erased, clear to 0 when used
	u8_t map = (~res->writen_bitmap) & (res->read_bitmap);


	if(map)
	{
		printk("left map 0x%02x\n", map);

		u8_t i = 0;
		for( ; i < ARRAY_SIZE(res->res); i++)
		{
			if((map) & (1 <<i))
			{
				printk("send map %d, size %d\n", i, sizeof(rx_res_t));
				printk("rx type %d\n", res->res[i].rx_type);
				printk("channel %d\n", res->res[i].channel);
				printk("packet_type 0x%x\n", res->res[i].packet_type);
				printk("\n\n");
				res->read_bitmap |=  1<<i;
				stream_write(data_p->sppble_stream, (void *)&res->res[i], sizeof(rx_res_t));
			}
		}

		if(i == (ARRAY_SIZE(res->res)))
		{
			nsm_result_reset();
		}
	}
	data_p->uploaded_res = 1;
}

static void nsm_delay_work(struct k_work *work)
{
	nsm_data_t * data_p =  nsm_get_data_p();

	if(!data_p)
		return;

	if(data_p->connected)
	{
		nsm_upload_data();

		int len = stream_tell(data_p->sppble_stream);

		printk("cmd len %d\n", len);

		if(len > 0)
		{
			nsm_cmd_t * cmd_buf = (nsm_cmd_t *)NSM_CMD_STREAM_START;
			memset(cmd_buf, 0, NSM_CMD_STREAM_LEN);

			if(len < NSM_CMD_STREAM_LEN)
			{
				media_player_t * player = media_player_get_current_main_player();

				if(player)
				{
					media_player_stop(player);
					media_player_close(player);
					goto out;
				}


				cmd_buf->len = len;
				stream_read(data_p->sppble_stream, cmd_buf->cmd, len);

				if(len == 1 && cmd_buf->cmd[0] == 0x80)
				{
#ifndef CONFIG_ACTIONS_IMG_TEST_ALWAYS
					u8_t mode = '0';
					SYS_LOG_INF("reset bin test id\n");
					int ret = property_set(CFG_BIN_TEST_ID, &mode, 1);
					if (ret < 0) {
						return;
					}

					property_flush(CFG_BIN_TEST_ID);
					sys_pm_reboot(REBOOT_TYPE_GOTO_WIFISYS);
					return;
#endif
				}

				u32_t p = 0;
				p = img_code_load_nsm();
				if(!p)
				{
					SYS_LOG_ERR("load code error\n");
					goto out;
				}


				SYS_LOG_INF("entery %x\n", p);
				((void (*)(void))(p))();
				//go into bin test, never return;
			}else{
				SYS_LOG_ERR();
			}
		}
	}

	out:
	os_delayed_work_submit(&data_p->nsm_delay_work, 50);
}


static void nsm_test_sppble_connect(bool connected)
{

	SYS_LOG_INF("%s\n", (connected) ? "connected" : "disconnected");

	nsm_data_t * data_p =  nsm_get_data_p();

	if(!data_p)
		return;

	if (connected) {
		data_p->connected = 1;
		os_delayed_work_init(&data_p->nsm_delay_work, nsm_delay_work);
		os_delayed_work_submit(&data_p->nsm_delay_work, 50);
	} else {
		data_p->connected = 0;
		if(data_p->sppble_stream)
		{
			stream_flush(data_p->sppble_stream);
		}
		os_delayed_work_cancel(&data_p->nsm_delay_work);
	}
}

void nsm_sppble_create(void)
{
	nsm_data_t * data_p =  nsm_get_data_p();

	if(!data_p)
		return;

	struct sppble_stream_init_param init_param;
	memset(&init_param, 0, sizeof(struct sppble_stream_init_param));
	init_param.spp_uuid = (uint8_t *)sppble_nsm_spp_uuid;
	init_param.gatt_attr = nsm_gatt_attr;
	init_param.attr_size = ARRAY_SIZE(nsm_gatt_attr);
	init_param.tx_chrc_attr = &nsm_gatt_attr[3];
	init_param.tx_attr = &nsm_gatt_attr[4];
	init_param.tx_ccc_attr = &nsm_gatt_attr[5];
	init_param.rx_attr = &nsm_gatt_attr[2];
	init_param.connect_cb = nsm_test_sppble_connect;
	init_param.read_timeout = OS_FOREVER;/* K_FOREVER, K_NO_WAIT,  K_MSEC(ms) */
	init_param.write_timeout = OS_FOREVER;
	init_param.read_buf_size = 250;

	/* Just call stream_create once, for register spp/ble service
	 * not need call stream_destroy
	 */
	data_p->sppble_stream = sppble_stream_create(&init_param);
	if (!data_p->sppble_stream) {
		SYS_LOG_ERR("stream_create failed");
		return;
	}

	int ret = stream_open(data_p->sppble_stream, MODE_IN_OUT);

	if (ret) {
		SYS_LOG_ERR("stream_open failed");
		return;
	}

	SYS_LOG_INF("create succ\n");
	return;
}


void nsm_sppble_test_init(void)
{
	if(!img_code_is_nsm_sppble_test())
		return;

	nsm_data_p = (nsm_data_t *)mem_malloc(sizeof(nsm_data_t));

	if(!nsm_data_p)
	{
		SYS_LOG_ERR("malloc err\n");
		return;
	}

	nsm_check_result();

#if 0
	int ret = flash_read(nsm_data_p->dev, NSM_RES_ADDR_START, &nsm_data_p->res, sizeof(nsm_res_t));
	printk("%s,%d ,%s \n", __func__,ret, nsm_data_p->res.magic);
	printk("%s,wbm 0x%x , rbm 0x%x \n", __func__,nsm_data_p->res.read_bitmap, nsm_data_p->res.writen_bitmap);
#endif

	nsm_sppble_create();
}


