
#ifndef _BROM_INTERFACE_H_
#define _BROM_INTERFACE_H_

#include "types.h"

typedef int (*out_func_t)(int c, void *ctx);
typedef int (*api_vsnprintf)(char* buf, unsigned int size, unsigned int type, const char* fmt, va_list args);

typedef void (*api_uart_init)(uint32_t io_reg, uint32_t uart_baud);

typedef void (*api_uart_puts)(char *s, unsigned int len);

typedef unsigned int (*api_brom_card_read)(unsigned int,unsigned int,unsigned char*);

typedef void (*api_adfu_launcher)(void);

typedef void (*api_uart_launcher)(void);

typedef void (*api_nor_launcher)(void);

typedef void (*api_card_launcher)(void);

typedef void (*api_launch)(unsigned int type, unsigned int addr);

typedef void (*api_brom_sysinit)(void);

typedef void (*api_brom_poweron)(void);

typedef int (*api_verify_signature)(const unsigned char *key, const unsigned char *sig, const unsigned char *data, unsigned int len);

typedef unsigned int (*api_image_data_check)(unsigned char*);

typedef unsigned int (*api_image_security_data_check)(unsigned char* buf);

typedef int (*api_sha256_init)(void* ctx);

typedef int (*api_sha256_update)(void* ctx, const void* data, int len);

typedef const unsigned char*(*api_sha256_final)(void *ctx);

typedef const unsigned char * (*api_sha256_hash)(const void* data, int len, uint8_t* digest);

//timestamp related export fuctions

typedef void (*api_timestamp_init)(unsigned int timer_num);

typedef unsigned int (*api_timestamp_get_us)(void);

typedef unsigned int (*api_timestamp_ticks_get)(void);

typedef void (*api_time_delay_ticks)(unsigned int ticks);

typedef void (*api_udelay)(unsigned int us);

typedef void (*api_mdelay)(unsigned int ms);

typedef void (*api_nor_mfp_select)(unsigned int index, unsigned int need_clear);

typedef void (*api_card_mfp_save)(void *data, unsigned int index, unsigned int data_width);

typedef void (*api_card_mfp_recovery)(void *data, unsigned int index, unsigned int data_width);

typedef void (*api_nor_reset)(unsigned int work_mode);

typedef void (*api_nor_wakeup_init)(void);

//uart protocol interfaces
typedef int (*api_uart_connect)(void);

typedef int (*api_uart_wait_connect)(unsigned int timeout_ms);

typedef int (*api_uart_disconnect)(void);

typedef int (*api_uart_open)(unsigned int protocol_type);

typedef void (*api_uart_protocol_inquiry)(void);

typedef int (*api_uart_command_parse)(unsigned char uart_cmd, unsigned int para_length);

typedef void (*api_set_uart_protocol_state)(int state);

typedef int (*api_uart_protocol_rx_fsm)(unsigned char first_data);

typedef int (*api_uart_fsm_register)(void *uart_fsm_item, unsigned int direction);

typedef int (*api_uart_fsm_unregister)(void *uart_fsm_item, unsigned int direction);

typedef int (*api_uart_protocol_write_payload)(unsigned char *user_payload, unsigned int size, unsigned short protocol);

typedef int (*api_uart_protocol_read_payload)(unsigned char *user_payload, unsigned int size, unsigned short protocol);

typedef int (*api_uart_data_packet_receive)(void *recv_head, unsigned char *payload, unsigned int size, unsigned short protocol);

typedef unsigned char (*api_crc8)(unsigned char *p, unsigned int counter);

typedef unsigned short (*api_crc16)(unsigned char* ptr, unsigned int len);

typedef void (*api_enable_irq)(unsigned int irq_num);

typedef void (*api_disable_irq)(unsigned int irq_num);

typedef void (*api_irq_set_prio)(unsigned int irq, unsigned int prio);

typedef int (*api_irq_vector_read)(void);

typedef int (*api_exception_vector_read)(void);

typedef void (*api_sys_irq_lock)(SYS_IRQ_FLAGS *flags);

typedef void (*api_sys_irq_unlock)(const SYS_IRQ_FLAGS *flags);

typedef void (*api_card_reset)(void);

typedef unsigned int (*api_card_send_cmd)(unsigned int cmd, unsigned int param,unsigned int ctl);

typedef int (*api_spimem_xfer)(void *si, unsigned char cmd, unsigned int addr,
                 int addr_len, void *buf, int length,
                 unsigned char dummy_len, unsigned int flag);

typedef void (*api_spimem_continuous_read_reset)(void *si);

typedef void (*api_sbc_decoder_ops)(void *handle, unsigned int cmd, unsigned int args);

typedef void (*api_sbc_encoder_ops)(void *handle, unsigned int cmd, unsigned int args);

typedef int (*api_printf)(const char *fmt, ...);

typedef void (*api_io_write)(unsigned int pin, unsigned int value);

typedef void  (*api_io_out)(int pin, int level, void (*delay_us_cb)(uint32_t), uint32_t delay_time_us);

typedef void (*api_print_buffer)(const void *addr, int width, int count, int linelen, unsigned long disp_addr, int (*printk)( const char *fmt, ...));

typedef int (*api_hex2bin)(uint8_t *dst, const char *src, unsigned long count);

typedef char *(*api_bin2hex)(char *dst, const void *src, unsigned long count);

typedef int (*api_get_adSBC_ops)(void **p_ops_base, uint32_t *ops_length, uint32_t *version);

typedef int (*api_get_adSBC_tbl_info)(void **tbl_info_list, uint32_t *tbl_info_nums, uint32_t *version);

typedef void (*api_vprintk)(out_func_t out, void *ctx, const char *fmt, va_list ap);

typedef int (*api_snprintk)(char *str, size_t size, const char *fmt, ...);

typedef int (*api_vsnprintk)(char *str, size_t size, const char *fmt, va_list ap);

typedef unsigned int (*api_k_cycle_get_32)(void);

typedef void (*api_k_busy_wait)(unsigned int num);

typedef void (*api_k_busy_wait_ms)(unsigned int num);

typedef unsigned int (*api_k_cycle_get_time_us)(void);

typedef unsigned int (*api_crc32)(unsigned char *buffer, unsigned int buf_len,
				unsigned int crc_initial, bool last);

typedef void (*api_null)(void);


typedef void*           (*api_memset)(void * s, int c, unsigned int count);

typedef void*           (*api_memcpy)(void *dest, const void *src, unsigned int count);

typedef int             (*api_atoi)(const char *nptr);
typedef long int        (*api_atol)(const char *nptr);
typedef int             (*api_atomic_cas)(atomic_t *target, atomic_val_t old_value, atomic_val_t new_value);
typedef atomic_val_t    (*api_atomic_add)(atomic_t *target, atomic_val_t value);
typedef atomic_val_t    (*api_atomic_sub)(atomic_t *target, atomic_val_t value);
typedef atomic_val_t    (*api_atomic_inc)(atomic_t *target);
typedef atomic_val_t    (*api_atomic_dec)(atomic_t *target);
typedef atomic_val_t    (*api_atomic_get)(const atomic_t *target);
typedef atomic_val_t    (*api_atomic_set)(atomic_t *target, atomic_val_t value);
typedef atomic_val_t    (*api_atomic_clear)(atomic_t *target);
typedef atomic_val_t    (*api_atomic_or)(atomic_t *target, atomic_val_t value);
typedef atomic_val_t    (*api_atomic_xor)(atomic_t *target, atomic_val_t value);
typedef atomic_val_t    (*api_atomic_and)(atomic_t *target, atomic_val_t value);
typedef atomic_val_t    (*api_atomic_nand)(atomic_t *target, atomic_val_t value);
typedef char *          (*api_itoa)(int value, char *string, int radix);
typedef void *          (*api_memchr)(const void *s, int c, size_t n);
typedef int             (*api_memcmp)(const void * cs, const void * ct, size_t count);
typedef void *          (*api_memmove)(void * dest, const void *src, size_t count);
typedef int             (*api_sscanf)(const char* str, const char* format, ...);
typedef int             (*api_vsscanf)(const char * buf, const char * fmt, va_list args);
typedef char *          (*api_strchr)(const char * s, int c);
typedef int             (*api_strcmp)(const char *s1, const char *s2);
typedef char *          (*api_strcpy)(char *d, const char *s);
typedef size_t          (*api_strlcpy)(char * dst, const char * src, size_t size);
typedef size_t          (*api_strlen)(const char *s);
typedef char *          (*api_strncat)(char *dest, const char *src, size_t n);
typedef int             (*api_strncmp)(const char *s1, const char *s2, size_t n);
typedef char *          (*api_strncpy)(char *d, const char *s, size_t n);
typedef char *          (*api_strpbrk)(const char * cs, const char * ct);
typedef char *          (*api_strsep)(char **s, const char *ct);
typedef size_t          (*api_strspn)(const char *s, const char *accept);
typedef char *          (*api_strstr)(const char * s1, const char * s2);
typedef long            (*api_strtol)(const char *cp, char **endp, unsigned int base);
typedef unsigned long   (*api_strtoul)(const char* str, char** endptr, unsigned int base);
typedef char *          (*api_strcat)(char *dest, const char *src);
typedef char *          (*api_strrchr)(const char *s, int c);
typedef void            (*api_qsort)(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
typedef const char *    (*api_strerror)(int errnum);

/** This structure defines interfaces of BROM
* Interfaces are located at the start of BROM, 
* and not changed when brom code is adjusted.
*/
typedef struct brom_operations
{
    const char *                                        build_info;
    api_memset                                          p_memset;
    api_memcpy                                          p_memcpy;
    api_vsnprintf                                       p_vsnprintf;
    api_uart_init                                       p_uart_init;
    api_uart_puts                                       p_uart_puts;
    api_brom_card_read                                  p_brom_card_read;
    api_adfu_launcher                                   p_adfu_launcher;
    api_uart_launcher                                   p_uart_launcher;
    api_nor_launcher                                    p_nor_launcher;
    api_card_launcher                                   p_card_launcher;
    api_launch                                          p_launch;
    api_brom_sysinit                                    p_brom_sysinit;
    api_brom_poweron                                    p_brom_poweron;
    api_verify_signature                                p_verify_signature;
    api_image_data_check                                p_image_data_check;
    api_image_security_data_check                       p_image_security_data_check;
    api_sha256_init                                     p_sha256_init;
    api_sha256_update                                   p_sha256_update;
    api_sha256_final                                    p_sha256_final;
    api_sha256_hash                                     p_sha256_hash;
    api_timestamp_init                                  p_timestamp_init;
    api_timestamp_get_us                                p_timestamp_get_us;
    api_timestamp_ticks_get                             p_timestamp_ticks_get;
    api_time_delay_ticks                                p_time_delay_ticks;
    api_udelay                                          p_udelay;
    api_mdelay                                          p_mdelay;
    api_nor_mfp_select                                  p_nor_mfp_select;
    api_card_mfp_save                                   p_card_mfp_save;
    api_card_mfp_recovery                               p_card_mfp_recovery;
    api_nor_reset                                       p_nor_reset;
    api_nor_wakeup_init                                 p_nor_wakeup_init;
    api_uart_connect                                    p_uart_connect;
    api_uart_wait_connect                               p_uart_wait_connect;
    api_uart_disconnect                                 p_uart_disconnect;
    api_uart_open                                       p_uart_open;
    api_uart_protocol_inquiry                           p_uart_protocol_inquiry;
    api_uart_command_parse                              p_uart_command_parse;
    api_set_uart_protocol_state                         p_set_uart_protocol_state;
    api_uart_protocol_rx_fsm                            p_uart_protocol_rx_fsm;
    api_uart_fsm_register                               p_uart_fsm_register;
    api_uart_fsm_unregister                             p_uart_fsm_unregister;
    api_uart_protocol_write_payload                     p_uart_protocol_write_payload;
    api_uart_protocol_read_payload                      p_uart_protocol_read_payload;
    api_uart_data_packet_receive                        p_uart_data_packet_receive;
    api_crc8                                            p_crc8;
    api_crc16                                           p_crc16;
    api_enable_irq                                      p_enable_irq;
    api_disable_irq                                     p_disable_irq;
    api_irq_set_prio                                    p_irq_set_prio;
    api_card_reset                                      p_card_reset;
    api_card_send_cmd                                   p_card_send_cmd;
    void  *                                             p_spinor_api;
    api_spimem_xfer                                     p_spimem_xfer;
    api_spimem_continuous_read_reset                    p_spimem_continuous_read_reset;

    api_irq_vector_read                                 p_irq_vector_read;
    api_exception_vector_read                           p_exception_vector_read;
    api_sys_irq_lock                                    p_sys_irq_lock;
    api_sys_irq_unlock                                  p_sys_irq_unlock;

    api_sbc_decoder_ops                                 p_sbc_decoder_ops;
    api_sbc_encoder_ops                                 p_sbc_encoder_ops;
    api_atoi                                            p_api_atoi;
    api_atol                                            p_api_atol;
    api_atomic_cas                                      p_api_atomic_cas;
    api_atomic_add                                      p_api_atomic_add;
    api_atomic_sub                                      p_api_atomic_sub;
    api_atomic_inc                                      p_api_atomic_inc;
    api_atomic_dec                                      p_api_atomic_dec;
    api_atomic_get                                      p_api_atomic_get;
    api_atomic_set                                      p_api_atomic_set;
    api_atomic_clear                                    p_api_atomic_clear;
    api_atomic_or                                       p_api_atomic_or;
    api_atomic_xor                                      p_api_atomic_xor;
    api_atomic_and                                      p_api_atomic_and;
    api_atomic_nand                                     p_api_atomic_nand;
    api_itoa                                            p_api_itoa;
    api_memchr                                          p_api_memchr;
    api_memcmp                                          p_api_memcmp;
    api_memmove                                         p_api_memmove;
    api_sscanf                                          p_api_sscanf;
    api_vsscanf                                         p_api_vsscanf;
    api_strchr                                          p_api_strchr;
    api_strcmp                                          p_api_strcmp;
    api_strcpy                                          p_api_strcpy;
    api_strlcpy                                         p_api_strlcpy;
    api_strlen                                          p_api_strlen;
    api_strncat                                         p_api_strncat;
    api_strncmp                                         p_api_strncmp;
    api_strncpy                                         p_api_strncpy;
    api_strpbrk                                         p_api_strpbrk;
    api_strsep                                          p_api_strsep;
    api_strspn                                          p_api_strspn;
    api_strstr                                          p_api_strstr;
    api_strtol                                          p_api_strtol;
    api_strtoul                                         p_api_strtoul;
    api_strcat                                          p_api_strcat;
    api_strrchr                                         p_api_strrchr;
    api_qsort                                           p_api_qsort;
    api_strerror                                        p_api_strerror;
    const unsigned char *                               p_ctype;
    api_printf                                          p_printf;
    api_print_buffer                                    p_print_buffer;
    api_hex2bin                                         p_hex2bin;
    api_bin2hex                                         p_bin2hex;
    api_io_write                                        p_io_write;
    api_io_out                                          p_io_out;
    api_vprintk                                         p_vprintk;
    api_snprintk                                        p_snprintk;
    api_vsnprintk                                       p_vsnprintk;
    api_k_cycle_get_32                                  p_k_cycle_get_32;
    api_k_busy_wait                                     p_k_busy_wait;
    api_k_busy_wait_ms                                  p_k_busy_wait_ms;
    const uint32_t                                     *p_hrtimer;
    const uint32_t                                     *p_threadtimer;
    const uint32_t                                     *p_dma;
    const uint32_t                                     *p_utils;
    const uint32_t                                     *p_csb;
    const uint32_t                                     *p_log;
    const uint32_t                                     *p_tracing;
    const uint32_t                                     *p_trace_ringbuf;
    const uint32_t                                     *p_soc;
    const uint32_t                                     *p_cbuf;
    const uint32_t                                     *p_act_ringbuf;
    const uint32_t                                     *p_csb_codec;
    void  *                                             p_spimem_api;
    api_get_adSBC_ops                                  p_get_adSBC_ops;
    api_k_cycle_get_time_us                             p_k_cycle_get_time_us;
    api_crc32                                           p_crc32;
}brom_operations_t;



int rom_vsnprintf(char* buf, size_t size, unsigned int type, const char* fmt, va_list args);

void rom_uart_init(uint32_t io_reg, uint32_t uart_baud);

void rom_uart_puts(char *s, unsigned int len);

void rom_udelay(unsigned int us);

void rom_mdelay(unsigned int ms);

unsigned int rom_timestamp_get_us(void);

void *rom_get_spinor_drv_api(void);

void *rom_get_spimem_drv_api(void);

void rom_spimem_continuous_read_reset(void *sni);

u32_t rom_get_xspi_prepare_hook(void);

#endif /* _BROM_INTERFACE_H_ */
