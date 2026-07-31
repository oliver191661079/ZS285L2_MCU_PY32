
#include <misc/util.h>
#include <misc/byteorder.h>
#include <dsp_hal.h>
#include <stdint.h>
#include <stdbool.h>
#include <media_player.h>
#include <audio_policy.h>
#include "tool_app_inner.h"
#include "audio_system.h"
#include "app_defines.h"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_TOOL_COMMON_DAE

#define KEEP_DAE_DATA_ACTIVE

#define ASET_DATA_BUFFER_LEN (1030)
#define DAE_PARAM_SIZE   (2048)

extern void *audio_effect_get_param_addr(void);
extern int audio_effect_get_param_size(void);


static u8_t *p_dae_rw_buffer = NULL;
static u8_t *p_dae_debug_buffer = NULL;
static u8_t  dae_has_getted = 0;
static u8_t  dae_player_updated = 0;

typedef struct {
    short pkt_total;
    short pkt_index;
    short data_len;
    short reserved;
}dae_pkt_t;

#ifdef KEEP_DAE_DATA_ACTIVE
static int dae_config_effect_mem(void *effect_mem, u32_t size)
{
	u8_t stream_type = AUDIO_STREAM_MUSIC; //TODO: get correct stream type.

	SYS_LOG_INF("%p 0x%x", effect_mem, size);
	media_effect_set_user_param(stream_type, 0, effect_mem, size);

	return 0;
}
#endif

s32_t dae_write_dae_data(u16_t cmd, void *data_buffer, u32_t data_len)
{
    s32_t ret_val;
    s32_t timeout;

    stub_ext_param_t ext_param;

try_again:
    timeout = 0;
    ext_param.opcode = cmd;
    ext_param.payload_len = data_len;
    ext_param.rw_buffer = p_dae_rw_buffer;

    memcpy(&(ext_param.rw_buffer[6]), data_buffer, data_len);

    ret_val = stub_ext_write(tool_stub_dev_get(), &ext_param);

    if (!ret_val) {
        //Wait for an ACK 100 ms
        while (1) {
            //Load ACK data, wait up to 100ms
            ext_param.payload_len = 0;
            ret_val = stub_ext_read(tool_stub_dev_get(), &ext_param);

            if (!ret_val) {
                if ((ext_param.rw_buffer[1] == 0x80)
                    && (ext_param.rw_buffer[2] == 0xfe)) {
                    break;
                }
            } else {
                k_busy_wait(10);
                timeout++;
                if (timeout == 10) {
                    //If the data reception fails, the data is resended
                    goto try_again;
                }
            }
        }
    }
    return 0;
}

s32_t dae_read_data(u16_t cmd, void *data_buffer, u32_t data_len, u32_t read_len, u32_t new_cmd)
{
    s32_t ret_val;
    static u8_t *last_data = NULL;
    static u32_t last_data_len = 0;

    if(new_cmd) {
        stub_ext_param_t ext_param;

        ext_param.opcode = cmd;
        ext_param.payload_len = 0;
        ext_param.rw_buffer = p_dae_rw_buffer;

        last_data = NULL;
        last_data_len = 0;
        if(data_len > ASET_DATA_BUFFER_LEN - 6){
            printk("sssssss line=%d, func=%s\n", __LINE__, __func__);
            return -1;
        }

        ret_val = stub_ext_write(tool_stub_dev_get(), &ext_param);
        //SYS_LOG_INF("cmd=0x%x, len=%d, ret: %d", cmd, data_len, ret_val);
        if (ret_val != 0) {
            SYS_LOG_INF("cmd=0x%x, len=%d, ret: %d", cmd, data_len, ret_val);
            return ret_val;
        }

        ext_param.payload_len = (u16_t)data_len;
        ret_val = stub_ext_read(tool_stub_dev_get(), &ext_param);
        if (ret_val) {
            ACT_LOG_ID_INF(ALF_STR_dae_read_data__DAE_READ_ERRLC0XXN, 1,(data_len <<16) + cmd);
            return ret_val;
        }
        //printk("sssssss line=%d, func=%s\n", __LINE__, __func__);

        last_data = &(ext_param.rw_buffer[6]);
        last_data_len = data_len;

        //stub_ext_write will write 2 bytes crc at rw_buffer[6/7], so copy here
        memcpy((u8_t *)data_buffer, last_data, read_len);
        last_data += read_len;
        last_data_len -= read_len;

        ext_param.opcode = STUB_CMD_COMMON_ACK;
        ext_param.payload_len = 0;
        ret_val = stub_ext_write(tool_stub_dev_get(), &ext_param);
        return 0;
    }

    //printk("sssssss line=%d, func=%s, %d, %d\n", __LINE__, __func__, read_len, last_data_len);
    if(read_len > last_data_len) {
        printk("sssssss line=%d, func=%s, %d, %d\n", __LINE__, __func__, read_len, last_data_len);
        return -1;
    }

    memcpy((u8_t *)data_buffer, last_data, read_len);
    last_data += read_len;
    last_data_len -= read_len;
    return 0;
}

s32_t dae_write_data(u16_t cmd, void *data_buffer, u32_t data_len)
{
    s32_t ret_val;
    s32_t timeout;

    stub_ext_param_t ext_param;

    ext_param.rw_buffer = malloc(256);
try_again:
    timeout = 0;
    ext_param.opcode = cmd;
    ext_param.payload_len = (u16_t)data_len;

    memcpy(&(ext_param.rw_buffer[6]), data_buffer, data_len);

    ret_val = stub_ext_write(tool_stub_dev_get(), &ext_param);

    if (!ret_val) {
        //Wait for an ACK 100 ms
        while (1) {
            //Load ACK data, wait up to 100ms
            ext_param.payload_len = 0;
            ret_val = stub_ext_read(tool_stub_dev_get(), &ext_param);

            if (!ret_val) {
                if ((ext_param.rw_buffer[1] == 0x80)
                    && (ext_param.rw_buffer[2] == 0xfe)) {
                    break;
                }
            } else {
                k_busy_wait(10);
                timeout++;
                if (timeout == 10) {
                    //If the data reception fails, the data is resended
                    goto try_again;
                }
            }
        }
    }

    free(ext_param.rw_buffer);

    return ret_val;
}

static s32_t dae_dae_deal(media_player_t *dump_player)
{
    int ret_val = 0;
    int pkt_index = 1;
    int offset = 0;
    dae_pkt_t dae_pkt;
    int loop_count = 5;
    int wait_time;
    int dae_getted = 0;

    while(loop_count > 0) {
        wait_time = 250;
        while(wait_time > 0) {
            common_dae_status_t dae_status;

            ret_val = dae_read_data(STUB_CMD_COMMON_READ_STATUS, &dae_status, sizeof(dae_status), sizeof(dae_status), 1);
            if(ret_val != 0) {
                SYS_LOG_ERR("read fail: %d\n", ret_val);
                return -1;
            }
            if (dae_status.state == 1) {
                break;
            }
            k_sleep(10);
            wait_time -= 10;
        }

        loop_count--;
        ret_val = dae_read_data(STUB_CMD_COMMON_READ_DAE_PARAM,
            &dae_pkt, 1024, sizeof(dae_pkt_t), 1);
        SYS_LOG_INF("ret %d, loop_count %d, pkt_index %d\n", ret_val, loop_count, pkt_index);
        SYS_LOG_INF("dae_pkt: %d, %d, %d, %d", dae_pkt.pkt_total, dae_pkt.pkt_index, dae_pkt.data_len, dae_pkt.reserved);
        if(ret_val != 0) {
            SYS_LOG_ERR("read fail: %d\n", ret_val);
            break;
        }
        if(dae_pkt.pkt_index != pkt_index) {
            SYS_LOG_INF("index err: %d, %d, %d\n", pkt_index, dae_pkt.pkt_index, dae_pkt.pkt_total);
            continue;
        }

        ret_val = dae_read_data(STUB_CMD_COMMON_READ_DAE_PARAM,
            &p_dae_debug_buffer[offset], 1024, dae_pkt.data_len, 0);
        if(ret_val != 0) {
            SYS_LOG_ERR("read fail: %d\n", ret_val);
            break;
        }

        if(dae_pkt.pkt_total == pkt_index) {
            dae_getted = 1;
            break;
        }

        offset += dae_pkt.data_len;
        pkt_index++;
    }

    if (dae_getted && dump_player) {
//      audio_effect_music_param_print(p_dae_debug_buffer);
        //Audio Parameters Setting
        SYS_LOG_INF("dae gotten\n");
#ifdef ATS283F_SDK
        media_player_set_effect_param_update(dump_player, NULL, 0);
#else
#ifdef KEEP_DAE_DATA_ACTIVE
        dae_config_effect_mem(p_dae_debug_buffer, DAE_PARAM_SIZE);
#endif
        media_player_update_effect_param(dump_player, p_dae_debug_buffer, DAE_PARAM_SIZE);
#endif
        dae_has_getted = 1;
        dae_player_updated = 0;
    }

    return ret_val;
}

s32_t dae_cmd_deal(common_dae_status_t *dae_status,  media_player_t *dump_player)
{
    if (dae_status->state == 1) {
        dae_dae_deal(dump_player);
    } else {
        if (dae_has_getted && dump_player && dae_player_updated){
            media_player_update_effect_param(dump_player, p_dae_debug_buffer, DAE_PARAM_SIZE);
            dae_player_updated = 0;
        }
    }

    return 0;
}


void tool_common_dae_loop(void)
{
    common_dae_status_t dae_status;
    media_player_t *dump_player = NULL;
    uint8_t app_id;
    media_player_t *last_dump_player = NULL;
    uint8_t last_fg_app = DESKTOP_PLUGIN_ID_NONE;
    int ret_val = 0;
    u8_t err_cnt = 0;
    dae_has_getted = 0;

    if (NULL == p_dae_rw_buffer) {
	    p_dae_rw_buffer = malloc(ASET_DATA_BUFFER_LEN);
	    if (p_dae_rw_buffer == NULL) {
	        ACT_LOG_ID_INF(ALF_STR_tool_common_dae_loop__MALLOC_DAE_RW_ERR, 0);
	        return;
	    }
    }

    if (NULL == p_dae_debug_buffer ) {
        p_dae_debug_buffer = malloc(DAE_PARAM_SIZE);
        if (p_dae_debug_buffer == NULL) {
            ACT_LOG_ID_INF(ALF_STR_tool_common_dae_loop__MALLOC_DAE_DEBUG_ERR, 0);
            return;
        }
    }
    memset(p_dae_debug_buffer, 0, DAE_PARAM_SIZE);
    SYS_LOG_INF("init");

    while (1)
    {
        app_id = desktop_manager_get_plugin_id();

        if (app_id != last_fg_app) {
            ACT_LOG_ID_INF(ALF_STR_tool_common_dae_loop__AGAIN5, 0);
            last_dump_player = NULL;
            last_fg_app = app_id;
        }

        dump_player = media_player_get_current_dumpable_player();
        if (!dump_player) {
            ACT_LOG_ID_INF(ALF_STR_tool_common_dae_loop__AGAIN2, 0);
        } else if(last_dump_player != dump_player){
            #ifdef ATS283F_SDK
            ret_val = media_player_set_effect_addr(dump_player, p_dae_debug_buffer);
            if (ret_val) {
                ACT_LOG_ID_INF(ALF_STR_tool_common_dae_loop__AGAIN3, 0);
            }
            #endif
        }

        if (last_dump_player == NULL && dump_player) {
            dae_player_updated = 1;
        }

        last_dump_player = dump_player;

        ret_val = dae_read_data(STUB_CMD_COMMON_READ_STATUS, &dae_status, sizeof(dae_status), sizeof(dae_status), 1);

        if (!ret_val) {
            err_cnt = 0;
            ret_val = dae_cmd_deal(&dae_status, last_dump_player);
            if (ret_val) {
                SYS_LOG_INF("Deal fails");
            }
        } else {
            SYS_LOG_INF("reading status fails %d, %d", ret_val, err_cnt++);
            if(err_cnt > 1) {
                SYS_LOG_INF("timeout");
                break;
            }
        }

        k_sleep(10);
    }
#ifdef ATS283F_SDK
    media_player_set_effect_addr(last_dump_player, NULL);
#endif

    SYS_LOG_INF("deinit");

    free(p_dae_rw_buffer);
    p_dae_rw_buffer = NULL;

#ifndef KEEP_DAE_DATA_ACTIVE
    //Do not break to keep dae data for long time use.
    free(p_dae_debug_buffer);
    p_dae_debug_buffer = NULL;
#endif
}

