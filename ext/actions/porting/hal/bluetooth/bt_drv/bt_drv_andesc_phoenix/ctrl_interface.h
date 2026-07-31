/*
 * Copyright(c) 2017-2027 Actions (Zhuhai) Technology Co., Limited,
 *                            All Rights Reserved.
 *
 * Johnny           2017-5-4        create initial version
 */

#ifndef __CTRL_INTERFACE_H
#define __CTRL_INTERFACE_H

/*******************************
 * type definitions
 *******************************/
struct bt_cis_recv_report {
	uint16_t no_pkt_err;		  /* every bit record one packet recv no pkt err flag, bit[0] for the first packet */
	uint16_t crc_err;		   /* every bit record one packet recv no pkt err flag, bit[0] for the first packet */
	uint16_t iso_interval_delay : 3; /* FT > 1 valid, 0 for no delay, 1 for delay 1 iso interval */
	uint16_t pkt_bn : 4;			 /* pkt_bn is 0,1,2 when BN = 3 */
	uint16_t pkt_num : 4;			 /* BN > 1	pkt num maybe 1 or BN */
	uint16_t reserved[2];
}; /* align to 8Bytes */

typedef struct {
    /* local bluetooth address */
    unsigned char bd_addr[6];
    /* callback for controller sending data to host */
    signed int (*deliver_data_from_c2h)(unsigned char type, const unsigned char *buf);
    /* RF maxinum tx power (unit: 0.1db) */
    signed short tx_max_pwr;
    /* Page\Inquire\Scan\LE default tx power  (unit: 0.1db) */
    signed short def_pwr;
    /* controller parameters buffer address */
    unsigned int param_buf;

    /* callback for controller sending data via CIS */
	int (*cis_send)(uint16_t handle, uint8_t *buf, uint16_t *len);
    /* callback for controller receiving data via CIS */
	int (*cis_recv)(uint16_t handle, uint8_t *buf, uint16_t len,
			struct bt_cis_recv_report *rx_rpt);

//    unsigned char log_level;
//    unsigned char reserved[3];
//    unsigned int log_module_mask;
//    unsigned int trace_module_mask;

    /* default 4.   suggest more than 1. */
    unsigned char le_cis_real_trans_max_num;
    /* 2-- back off 2.5db;  1-- no back off;  0-- back off 1db.  */
    unsigned char modem_flag;
    /* default 3.   suggest more than 3. */
    unsigned char le_bis_real_trans_min_num;
    /* default 0.  0 -- fixed start bis;
      1-- dynamically start bis for BR.  bis irc is not less than 5.
      2-- when br is as slave; bis interval is 1.25ms; br rx is between two bis tx. 
      0x80 --- for statistic, tx and rx all bis data.  suggest irc is not more than 3. */
    unsigned char le_bis_trans_mode;
    /* default 0.  0 -- legacy channel (all in band);
      0x01-- bis data of the last irc use out band.
      0x11-- all bis data use out band.
      0x12 -- change channel from 0 to 36 every some seconds. */
    unsigned char big_channel_config;
    /* RF LE maxinum tx power level */
    signed short le_tx_max_pwr;
    /* default 0.  0 -- LE use low-power table in normal mode;  
      0x01 -- LE use BR power table in normal mode. if le max power is more than 10dbm, maybe set 1. */
    unsigned char le_tx_power_config;
    /* default 0 -- not bypass;    1 -- bypass; */
    unsigned char rf_bypass : 1;

    /* reserve fot future used */
    void *rsv;
} ctrl_cfg_t;

#define CTRL_PARAM_RSV_MASK                 0xBFCE03E0
#define CTRL_PARAM_RSV_GPIO_DEBUG           0x00000010
#define CTRL_PARAM_RSV_BQB_MODE             0x00000020
#define CTRL_PARAM_RSV_QC_BQB_MODE          0x00000040
#define CTRL_PARAM_RSV_SRRC_TEST            0x00000080
#define CTRL_PARAM_RSV_PRINT_DEBUG          0x00000100
#define CTRL_PARAM_RSV_UART_TRANS           0x00001000
#define CTRL_PARAM_RSV_ENABLE_BIG_CHANNEL_ASSESS 0x00004000
#define CTRL_PARAM_RSV_EDR3M_DISABLE        0x00010000
#define CTRL_PARAM_RSV_FIX_MAX_PWR          0x00100000
#define CTRL_PARAM_RSV_DBG_CIS_MAX_LATENCY  0x00200000
#define CTRL_PARAM_RSV_DISABLE_BLE_CHN_ASSESS  0x00400000
#define CTRL_PARAM_RSV_DISABLE_ACTIVE_CHN_ASSESS  0x00800000
#define CTRL_PARAM_RSV_ATTACH_PA            0x01000000
#define CTRL_PARAM_RSV_CIS_NOACK_MODE       0x04000000
#define CTRL_PARAM_RSV_DISABLE_REMOTE_ROLE_SWITCH 0x08000000
#define CTRL_PARAM_RSV_PTS_MODE             0x10000000

typedef struct bt_clock
{
    unsigned int bt_clk;  /* by 312.5us. the bit0 and bit1 are 0.  range is 0 ~ 0x0FFFFFFF.  */
    unsigned short bt_intraslot;  /* by 1us.  range is 0 ~ 1249.  */
} t_devicelink_clock;

struct le_link_time {
    uint64_t ce_cnt:39;
    uint32_t ce_interval_us;
    int32_t  ce_offs_us;
};

enum {
    CTRL_TIMER_TYPE_NATIVE = 0,
    CTRL_TIMER_TYPE_PICONET = 1,
    CTRL_TIMER_TYPE_CIG = 2,
    CTRL_TIMER_TYPE_CIS = 3,
    CTRL_TIMER_TYPE_BIG = 4,
    CTRL_TIMER_TYPE_BIS = 5,
};

typedef void (*ctrl_timer_cbk_t)(uint32_t cbk_param);
struct ctrl_timer_param {
	uint8_t type;
	uint8_t is_period:1;
	uint16_t handle;

	union {
		t_devicelink_clock bt_abs_clk;
		int32_t ce_offs_us;
	};

	ctrl_timer_cbk_t cbk;
	uint32_t cbk_param;
	uint32_t period_us;
};

/*******************************
 * function declaration
 *******************************/
signed int ctrl_init(ctrl_cfg_t *p_param);
signed int ctrl_deinit(void);
signed int ctrl_deliver_data_from_h2c(unsigned char type, const unsigned char *buf);
/* controller thread loop, return 0 means idle and 1 means busy */
signed int ctrl_loop(void);

/* get current bt clock, accuracy is 1us.  */
unsigned int ctrl_get_bt_clk(unsigned short acl_handle, t_devicelink_clock *bt_clock);
/* set bt clock to be informed.  */
unsigned int ctrl_set_bt_clk(unsigned short acl_handle, t_devicelink_clock bt_clock);
/* adjust link time.  adjust_val---range -36 ~ 96,  by 2.  */
void ctrl_adjust_link_time(unsigned short acl_handle, signed short adjust_val);
/* get null package count by controller for sync link after last call */
unsigned short ctrl_read_add_null_cnt(unsigned short sco_handle);
/* set flow-control switcher for acl link 2cap */
void ctrl_set_user_data_rx_flow(unsigned short acl_handle, unsigned char stop);

/* print log level */
#define BT_LOG_LEVEL_NONE		(0)
#define BT_LOG_LEVEL_ERROR		(1)
#define BT_LOG_LEVEL_WARNING		(2)
#define BT_LOG_LEVEL_INFO		(3)
#define BT_LOG_LEVEL_DEBUG		(4)

unsigned int ctrl_get_log_level(void);
void ctrl_set_log_level(unsigned int log_level);

/* log module for dump data */
#define BT_LOG_MODULE_LMP_TX		(1u << 0)
#define BT_LOG_MODULE_LMP_RX		(1u << 1)
#define BT_LOG_MODULE_LL_TX		(1u << 2)
#define BT_LOG_MODULE_LL_RX		(1u << 3)
#define BT_LOG_MODULE_HCI_TX		(1u << 4)
#define BT_LOG_MODULE_HCI_RX		(1u << 5)
#define BT_LOG_MODULE_ALL		(0xffffffff)

unsigned int ctrl_get_log_module_mask(void);
void ctrl_set_log_module_mask(unsigned int log_module_mask);

unsigned int ctrl_get_trace_module_mask(void);
void ctrl_set_trace_module_mask(unsigned int trace_module_mask);

void ctrl_mem_dump_info(void);
void ctrl_mem_dump_info_all(void);

unsigned char ctrl_get_le_link_time(unsigned short handle, struct le_link_time *link_time);
/* dynamically set minimum le bis real trans number. */
void ctrl_set_le_bis_trans(uint16_t handle, uint16_t real_trans_num);

/* set scan private policy. 
 * type ----  2 for page scan;
 * mode ----  0   is default, every 1.28 s a different frequency is selected;
              1 ~ 8 is every 640ms ~ 5ms a different frequency is selected;
              0x10 is every time internal scan window start a different frequency is selected;
              0x11 is every time internal scan window start every other frequency is selected;
 *
 * example : if pixel 7 has connected device over CIS link and is playing, 
 *           suggest set page scan policy mode 3 or 0x11.
*/
void ctrl_set_scan_policy(uint8_t type, uint8_t mode);

/* set host customized channel map (for anker). 
 * host_chn_map_flag ----  bit0 --- for LE channel; 0 is normal all channel; 1 is customized channel map;
 *                         bit4 --- for BR channel; 0 is normal all channel; 1 is customized channel map;
*/
void ctrl_set_chn_map_flag(uint8_t chn_map_flag);

/* set host customized channel map (for anker). 
 * leorbr ----  bit0 --- for LE channel;
 *              bit1 --- for BR channel;
 *
 *              all default {0x1f, 0x7c, 0xf0, 0xc1, 0x07}
*/
void ctrl_set_custom_chn_map(uint8_t *chn_map, uint8_t leorbr);

uint32_t ctrl_timer_request(struct ctrl_timer_param *ctimer_param);
void ctrl_timer_free(uint32_t ctimer_id);
void ctrl_timer_start(uint32_t ctimer_id);
void ctrl_timer_restart(uint32_t ctimer_id, struct ctrl_timer_param *ctimer_param);
void ctrl_timer_stop(uint32_t ctimer_id);

void ctrl_set_le_legacy_adv_tx_pwr(int16_t tx_power);

uint8_t ctrl_cis_set_active_device(uint16_t acl_handle);
uint8_t ctrl_cis_set_device_policy(uint16_t acl_handle, uint32_t policy);

void ctrl_set_le_conn_rx_max_num(uint8_t rx_max_num);

/* 0 - 异步校验   1 - 实时校验 */
void ctrl_set_br_aesccm_mode(int mode);

#endif

