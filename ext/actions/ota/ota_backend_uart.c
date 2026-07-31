#define SYS_LOG_LEVEL 3
#define SYS_LOG_DOMAIN "otauart"
#include <logging/sys_log.h>

#include <kernel.h>
#include <fs.h>
#include <file_stream.h>
#include <string.h>
#include <mem_manager.h>
#include <fs_manager.h>
#include <thread_timer.h>
#include <device.h>
#include <stream.h>
#include <uart_stream.h>
#include <ota_backend.h>
#include <ota_backend_uart.h>
#include <init.h>

static struct ota_backend_uart* g_ota_backend_uart;

struct ota_backend *ota_backend_uart_init(ota_backend_notify_cb_t cb, void *param);

void dump_buffer(const void * buffer, int width, int count, int linelen, unsigned long disp_addr)
{
    #define DUMP_BLOCK_SIZE       80
    #define DELAY                 1

    int i = 0, remain;
    int offset = 0;

    for(i = 0; i < count/DUMP_BLOCK_SIZE; i++){
        print_buffer(buffer+offset, 1, DUMP_BLOCK_SIZE, 16, offset);
        offset += DUMP_BLOCK_SIZE;
        os_sleep(DELAY);
    }

    remain = i > 1 ? count % DUMP_BLOCK_SIZE : 0;
    if(remain){
        print_buffer(buffer+offset, 1, remain, 16, offset);
        offset += remain;
        os_sleep(DELAY);
    }
}

static int stream_read_untill(io_stream_t handle, unsigned char *buf, int num, u32_t timeout_ms)
{
    int rx_num = 0, remain = num;
    u64_t start_time = k_uptime_get();

    do{
        /* for TYPE_UART_STREAM,  stream_read return 0 or requested size*/
        rx_num = stream_read(handle, &buf[num-remain], remain);
        remain -= rx_num;

        u64_t cur_time = k_uptime_get();
        if (timeout_ms != -1 && (cur_time - start_time) > timeout_ms){
            break;
        }
        k_sleep(1);
    } while(remain > 0);

    SYS_LOG_INF("got %d/%d in %d/%dms", num-remain, num, (u32_t)(k_uptime_get() - start_time), timeout_ms);
    return num-remain;
}

static int ota_backend_uart_command_wait_resp(struct ota_backend_uart *backend_uart, struct obu_request *req)
{
    enum {
        OTU_COMM_WAIT_RESP_CRC32,
        OTU_COMM_WAIT_RESP_SEQ,
        OTU_COMM_WAIT_RESP_DATA,
        OTU_COMM_WAIT_RESP_SUCC,
        OTU_COMM_WAIT_RESP_FAIL,
    };

    u8_t obu_cwr_state = OTU_COMM_WAIT_RESP_CRC32;
    u32_t crc_peer, crc, seq, readone_len;

    while (1) {
        SYS_LOG_INF("state %d\n", obu_cwr_state);
        switch (obu_cwr_state){
            case OTU_COMM_WAIT_RESP_CRC32:
                if (stream_read_untill(backend_uart->uio, (u8_t*)&crc_peer, 4, 100+BACKEND_READ_TIMEOUT(4)) < 4){
                    SYS_LOG_ERR("peer crc read fail, %d", stream_get_length(backend_uart->uio));
                    obu_cwr_state = OTU_COMM_WAIT_RESP_FAIL;
                }else{
                    obu_cwr_state = OTU_COMM_WAIT_RESP_SEQ;
                }
            break;

            case OTU_COMM_WAIT_RESP_SEQ:
                if (stream_read_untill(backend_uart->uio, (u8_t*)&seq, 4, 100+BACKEND_READ_TIMEOUT(4)) < 4){
                    SYS_LOG_ERR("seq read fail, %d", stream_get_length(backend_uart->uio));
                    obu_cwr_state = OTU_COMM_WAIT_RESP_FAIL;
                } else {
                    SYS_LOG_INF("%d, seq=%d", backend_uart->comm_seq, seq);
                    if (backend_uart->comm_seq == seq){
                        obu_cwr_state = OTU_COMM_WAIT_RESP_DATA;
                    }else{
                        SYS_LOG_ERR("seq not match");
                        obu_cwr_state = OTU_COMM_WAIT_RESP_FAIL;
                    }
                }
            break;

            case OTU_COMM_WAIT_RESP_DATA:
                readone_len = stream_read_untill(backend_uart->uio, req->recv_buf , req->recv_size,
                                    100+BACKEND_READ_TIMEOUT(req->recv_size));
                if (readone_len ==  req->recv_size){
                    crc = utils_crc32(0, req->recv_buf, req->recv_size);
                    if (crc != crc_peer) {
                        SYS_LOG_ERR("crc 0x%x != 0x%x", crc_peer, crc);
                        obu_cwr_state = OTU_COMM_WAIT_RESP_FAIL;
                    }else{
                        obu_cwr_state = OTU_COMM_WAIT_RESP_SUCC;;
                    }
                }else{
                    SYS_LOG_INF("data insufficient 0x%x < 0x%x", stream_get_length(backend_uart->uio), req->recv_size);
                    obu_cwr_state = OTU_COMM_WAIT_RESP_FAIL;
                }
            break;
        }

        if(obu_cwr_state == OTU_COMM_WAIT_RESP_SUCC
            || obu_cwr_state == OTU_COMM_WAIT_RESP_FAIL){
            break;
        }
    }

    return (obu_cwr_state == OTU_COMM_WAIT_RESP_SUCC);
}

#define COMMAND_SIZE             8
#define COMMAND_SEQ_SIZE         4

static u32_t ota_backend_uart_command_pack(struct obu_request *req, u32_t seq, u8_t *pack)
{
    memcpy(pack, req->command, COMMAND_SIZE);

    pack[COMMAND_SIZE + 0] = seq & 0xff;
    pack[COMMAND_SIZE + 1] = (seq >> 8) & 0xff;
    pack[COMMAND_SIZE + 2] = (seq >> 16) & 0xff;
    pack[COMMAND_SIZE + 3] = (seq >> 24) & 0xff;

    memcpy(pack + COMMAND_SIZE + COMMAND_SEQ_SIZE, req->param, req->param_size);

    return COMMAND_SIZE+COMMAND_SEQ_SIZE+req->param_size;
}

static int ota_backend_uart_command_request(struct ota_backend_uart *backend_uart, struct obu_request *req,
        s32_t retry_times)
{
    enum {
        OTU_COMM_STATE_INIT,
        OTU_COMM_STATE_WAIT_RESP,
        OTU_COMM_STATE_SUCCESS,
        OTU_COMM_STATE_FAIL,
    };

    u8_t *comm_pack;
    u8_t otu_comm_state;
    u32_t obu_comm_size;

    comm_pack = malloc(COMMAND_SIZE + COMMAND_SEQ_SIZE + req->param_size);
    if(comm_pack == NULL) {
        SYS_LOG_ERR("malloc fails");
        return 0;
    }
    otu_comm_state = OTU_COMM_STATE_INIT;

    while(1){
        SYS_LOG_INF("state %d\n", otu_comm_state);
        k_sleep(1);
        switch (otu_comm_state) {
        case OTU_COMM_STATE_INIT:
            stream_flush(backend_uart->uio);
            SYS_LOG_INF("seq=%d", backend_uart->comm_seq);
            obu_comm_size = ota_backend_uart_command_pack(req, backend_uart->comm_seq, comm_pack);
            stream_write(backend_uart->uio, comm_pack, obu_comm_size);
            if (req->with_resp) {
                otu_comm_state = OTU_COMM_STATE_WAIT_RESP;
            } else {
                otu_comm_state = OTU_COMM_STATE_SUCCESS;
            }
            break;

        case OTU_COMM_STATE_WAIT_RESP:
            if (ota_backend_uart_command_wait_resp(backend_uart, req) == true) {
                otu_comm_state = OTU_COMM_STATE_SUCCESS;
            } else {
                SYS_LOG_INF("retry %d", retry_times);
                if (retry_times <= 0) {
                    otu_comm_state = OTU_COMM_STATE_FAIL;
                } else {
                    retry_times--;
                    otu_comm_state = OTU_COMM_STATE_INIT;
                }
                //Wait sometime to flush some data.
                u8_t i = 5;
                while(i--){
                    k_sleep(100);
                    stream_flush(backend_uart->uio);
                }
            }
            //Always go to next sequence number to avoid wrong feedback.
            backend_uart->comm_seq++;
            break;
        }

        if (otu_comm_state == OTU_COMM_STATE_FAIL || otu_comm_state == OTU_COMM_STATE_SUCCESS) {
            break;
        }
    }

    free(comm_pack);

    return (otu_comm_state == OTU_COMM_STATE_SUCCESS);
}

static void ota_backend_uart_exit(struct ota_backend *backend)
{
    struct ota_backend_uart *backend_uart = CONTAINER_OF(backend,
                                                        struct ota_backend_uart, backend);

    mem_free(backend_uart);
}

static int ota_backend_uart_open(struct ota_backend *backend)
{
    struct ota_backend_uart *backend_uart = CONTAINER_OF(backend,
                                                        struct ota_backend_uart, backend);

    if(!backend_uart->uio_opened){
        stream_open(backend_uart->uio, MODE_IN_OUT);
    }

    if(backend_uart->shakehanded == false){
        backend_uart->counter = 0;
        backend_uart->shakehand_state = SHAKEHAND_INIT;
        thread_timer_start(&backend_uart->shakehand_looper, 0, SHAKEHAD_PERIOD);
    }

    /* 此时应该已经shakehanded了 */
    while (backend_uart->shakehanded == false)
        ;

    return 0;
}

/* command:
 * *------------* -----* -----*
 * |8bytes      |4bytes|4bytes|
 * *------------* -----* -----*
 * |PULLDATA    |OFFSET|LEN   |
 * *------------* -----* -----*
 *
 * response:
 * *----------------------------------*
 * |4bytes| command中的LEN bytes      |
 * *----------------------------------*
 * |crc32 | data                      |
 * *----------------------------------*
 *
 * 小端字节序
 */
static int ota_backend_uart_read(struct ota_backend *backend, int offset, void *buf, int size)
{
    struct ota_backend_uart *backend_uart = CONTAINER_OF(backend,
                                                        struct ota_backend_uart, backend);
    struct obu_request req;
    u8_t read_cmd_para[8];

    SYS_LOG_INF("size %d", size);
    read_cmd_para[0]  = offset & 0xff;
    read_cmd_para[1]  = (offset >> 8) & 0xff;
    read_cmd_para[2] = (offset >> 16) & 0xff;
    read_cmd_para[3] = (offset >> 24) & 0xff;
    read_cmd_para[4] = size & 0xff;
    read_cmd_para[5] = (size >> 8) & 0xff;
    read_cmd_para[6] = (size >> 16) & 0xff;
    read_cmd_para[7] = (size >> 24) & 0xff;

    req.command = "PULLDATA";
    req.param = read_cmd_para;
    req.param_size = 8;
    req.recv_buf = buf;
    req.recv_size = size;
    req.with_resp = true;

    if(ota_backend_uart_command_request(backend_uart, &req, 100) == false){
        return -EIO;
    }

    return 0;
}

/* command:
 * *------------* -----* -----*
 * |8bytes      |4bytes|4bytes|
 * *------------* -----* -----*
 * |REPORTxx    |PARA1 |PARA2 |
 * *------------* -----* -----*
 * response:
 * *--------------------*
 * |4bytes| 8bytes      |
 * *--------------------*
 * |crc32 | REPORTxx    |
 * *--------------------*
 * 小端字节序
 */

static int ota_backend_uart_ioctl(struct ota_backend *backend, int cmd, unsigned int param)
{
    struct ota_backend_uart *backend_uart = CONTAINER_OF(backend,
                                                        struct ota_backend_uart, backend);

    SYS_LOG_INF("cmd %d param %d", cmd, param);
    switch(cmd){
        case OTA_BACKEND_IOCTL_REPORT_IMAGE_VALID:
        {
            struct obu_request req;
            u8_t cmd_para[8];
            u8_t reture_data[8];

            cmd_para[0]  = param & 0xff;
            cmd_para[1]  = (param >> 8) & 0xff;
            cmd_para[2] = (param >> 16) & 0xff;
            cmd_para[3] = (param >> 24) & 0xff;
            cmd_para[4] = 0;
            cmd_para[5] = 0;
            cmd_para[6] = 0;
            cmd_para[7] = 0;

            req.command = "REPORT01";
            req.param = cmd_para;
            req.param_size = 8;
            req.recv_buf = reture_data;
            req.recv_size = 8;
            req.with_resp = false;

            ota_backend_uart_command_request(backend_uart, &req, 100);
        }
        break;

        case OTA_BACKEND_IOCTL_REPORT_PROCESS:
            backend->cb(backend, OTA_BACKEND_UPGRADE_PROGRESS, param);
            break;        

	    case OTA_BACKEND_IOCTL_GET_MAX_SIZE:
			*(unsigned int*)param = 0x400;
		    break;

        default:
            SYS_LOG_WRN("unspport");
			break;
    }

    return 0;
}

static int ota_backend_uart_close(struct ota_backend *backend)
{
    struct ota_backend_uart *backend_uart = CONTAINER_OF(backend,
                                                        struct ota_backend_uart, backend);

    backend_uart->counter = 0;
    backend_uart->shakehanded = false;
    backend_uart->shakehand_state = SHAKEHAND_END;
    backend_uart->comm_seq = 0;
    if(backend_uart->uio){
        stream_close(backend_uart->uio);
        backend_uart->uio_opened = false;
    }

    return 0;
}

static struct ota_backend_api ota_backend_api_uart = {
    .exit = ota_backend_uart_exit,
    .open = ota_backend_uart_open,
    .close = ota_backend_uart_close,
    .read = ota_backend_uart_read,
    .ioctl = ota_backend_uart_ioctl,
};

static void shakehand_request(struct thread_timer *ttimer, void *para)
{
    struct ota_backend_uart *backend_uart = para;
    u8_t skhd_buf[SHAKEHAND_WORD_MAX+1];

    if(!backend_uart->uio_opened){
        return;
    }

    SYS_LOG_INF("%d", backend_uart->shakehand_state);

    switch(backend_uart->shakehand_state){
        case SHAKEHAND_INIT:
            backend_uart->counter = 0;
            //copy string from nor flash to ram to support USB DMA transfer.
            strncpy(skhd_buf, SHAKEHAND_WORD1, SHAKEHAND_WORD_MAX);
            stream_write(backend_uart->uio, skhd_buf, SHAKEHAND_WORD1_LEN);
            backend_uart->shakehand_state = SHAKEHAND_STEP1;
            break;

        case SHAKEHAND_STEP1:
            backend_uart->counter++;
            if (SHAKEHAND_WORD2_LEN == stream_read_untill(backend_uart->uio, skhd_buf,
                    SHAKEHAND_WORD2_LEN, BACKEND_READ_TIMEOUT(SHAKEHAND_WORD2_LEN))){
                if (0 == memcmp(skhd_buf, SHAKEHAND_WORD2, SHAKEHAND_WORD2_LEN)){
                    backend_uart->shakehand_state = SHAKEHAND_STEP2;
                }
            }else{
                //SYS_LOG_WRN("read fail, %d", stream_get_length(backend_uart->uio));
            }
            if(backend_uart->shakehand_state == SHAKEHAND_STEP1
                && backend_uart->counter >= 3){
                backend_uart->shakehand_state = SHAKEHAND_INIT;
                SYS_LOG_WRN("read fail, %d", stream_get_length(backend_uart->uio));
            }
            break;

        case SHAKEHAND_STEP2:
            //copy string from nor flash to ram to support USB DMA transfer.
            strncpy(skhd_buf, SHAKEHAND_WORD3, SHAKEHAND_WORD_MAX);
            stream_write(backend_uart->uio, skhd_buf, SHAKEHAND_WORD3_LEN);
            backend_uart->shakehand_state = SHAKEHAND_STEP3;
            break;

        case SHAKEHAND_STEP3:
            backend_uart->shakehanded = true;
            if(backend_uart->backend.cb)
                backend_uart->backend.cb(&backend_uart->backend, OTA_BACKEND_UPGRADE_STATE, 1);
            backend_uart->shakehand_state = SHAKEHAND_END;
            break;

        case SHAKEHAND_END:
            thread_timer_stop(ttimer);
            break;
    }
}


extern struct ota_backend *ota_backend_uart_init(ota_backend_notify_cb_t cb, void *param)
{
    struct ota_backend_uart *backend_uart = g_ota_backend_uart;
    struct uart_stream_param uparam;

    if(backend_uart == NULL){
        SYS_LOG_ERR("g_ota_backend_uart NULL");
        return NULL;
    }

    ota_backend_init(&backend_uart->backend, OTA_BACKEND_TYPE_UART,
            &ota_backend_api_uart, cb);

    backend_uart->shakehand_state = SHAKEHAND_INIT;

	uparam.uart_dev_name = OTA_BACKEND_UART_NAME;
	uparam.uart_baud_rate = OTA_BACKEND_UART_BAUD;
#ifdef CONFIG_OTA_BACKEND_UART_CDC
	backend_uart->uio = cdc_uart_stream_create(&uparam.uart_dev_name);
#else
	backend_uart->uio = uart_stream_create( &uparam.uart_dev_name);
#endif
    if(!backend_uart->uio){
       SYS_LOG_ERR("stream create fail");
       return NULL;
    }
    stream_open(backend_uart->uio, MODE_IN_OUT);
    backend_uart->uio_opened = true;

    return &backend_uart->backend;
}

static int ota_backend_uart_init_early(struct device *device)
{
    struct ota_backend_uart *backend_uart;

    SYS_LOG_INF("init");

    backend_uart = mem_malloc(sizeof(struct ota_backend_uart));
    if (!backend_uart) {
        SYS_LOG_ERR("malloc failed");
        return -1;
    }
    memset(backend_uart, 0x0, sizeof(struct ota_backend_uart));

    thread_timer_init(&backend_uart->shakehand_looper, shakehand_request, backend_uart);
    thread_timer_start(&backend_uart->shakehand_looper, SHAKEHAD_PERIOD, SHAKEHAD_PERIOD);

    g_ota_backend_uart = backend_uart;

    return 0;
}


SYS_INIT(ota_backend_uart_init_early, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

