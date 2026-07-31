
#ifndef __OTA_BACKEND_UART_H_
#define __OTA_BACKEND_UART_H_

enum {
    SHAKEHAND_INIT,
    SHAKEHAND_STEP1,
    SHAKEHAND_STEP2,
    SHAKEHAND_STEP3,
    SHAKEHAND_END,
};


#ifdef CONFIG_OTA_BACKEND_UART_CDC
#define OTA_BACKEND_UART_NAME          CONFIG_CDC_ACM_PORT_NAME
#else
#define OTA_BACKEND_UART_NAME          CONFIG_UART_ACTS_PORT_0_NAME
#endif
#define OTA_BACKEND_UART_BAUD          CONFIG_UART_ACTS_PORT_0_BAUD_RATE

/*
    1bit start  + 8bit data + 1bit stop = 10bit
    @_BYTES bytes
    @return ms
    Read timeout is 20 times of max speed.
*/
#define BACKEND_READ_TIMEOUT(_BYTES)      ((_BYTES / (OTA_BACKEND_UART_BAUD/10/1000) + 1)*20)

#define SHAKEHAND_WORD1          "OK?"
#define SHAKEHAND_WORD2          "YES,YOU?"
#define SHAKEHAND_WORD3          "METOO"
#define SHAKEHAND_WORD1_LEN      (sizeof(SHAKEHAND_WORD1) - 1)
#define SHAKEHAND_WORD2_LEN      (sizeof(SHAKEHAND_WORD2) - 1)
#define SHAKEHAND_WORD3_LEN      (sizeof(SHAKEHAND_WORD3) - 1)
#define SHAKEHAND_WORD_MAX       8

#define SHAKEHAD_PERIOD                 1000

struct ota_backend_uart {
    struct ota_backend backend;
    io_stream_t uio;
    struct thread_timer shakehand_looper;
    u8_t shakehand_state;
    u8_t counter;
    u8_t shakehanded;
    u8_t uio_opened;
    u32_t comm_seq;
};

struct obu_request{
    u8_t *command;
    u8_t *param;
    u16_t param_size;
    u16_t recv_size;
    u8_t *recv_buf;
    u8_t with_resp;
};

extern uint32_t utils_crc32(uint32_t crc, const uint8_t *ptr, unsigned long buf_len);

extern struct ota_backend *ota_backend_uart_init(ota_backend_notify_cb_t cb, void *param);

#endif

