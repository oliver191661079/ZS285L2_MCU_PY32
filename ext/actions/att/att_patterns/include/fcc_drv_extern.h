
#ifndef _FCC_DRV_EXTERN_H_
#define _FCC_DRV_EXTERN_H_

#define FCC_WORD_MODE_SHIFT   (0) /* 0 - fcc, 1 - att, 2 - ft */
#define FCC_UART_PORT_SHIFT   (2) /* 0 - uart0, 1 - uart1 */
#define FCC_UART_BAUD_SHIFT   (16) /* baud / 100, 1152 = 115200 */

#define FCC_UART_PORT         (1)
#define FCC_UART_BAUD         (115200)
#define FCC_UART_TX           (15)
#define FCC_UART_RX           (14)

#define FCC_DRV_INSTALL_ENTRY_ADDR                     (*(unsigned int *)(0xbfc3fa00))
#define FCC_DRV_UNINSTALL_ENTRY_ADDR                   (*(unsigned int *)(0xbfc3fa04))
#define BT_PACKET_RECEIVE_PROCESS_ENTRY_ADDR           (*(unsigned int *)(0xbfc3fa08))
#define BT_RX_REPORT_ENTRY_ADDR                        (*(unsigned int *)(0xbfc3fa0c))
#define SOC_ATP_SET_HOSC_CALIB_ENTRY_ADDR              (*(unsigned int *)(0xbfc3fa10))
#define BT_PACKET_SEND_PROCESS_ENTRY_ADDR              (*(unsigned int *)(0xbfc3fa14))
#define FCC_DRV_SEND_DATA_TO_TRANSMITTER_ENTRY_ADDR    (*(unsigned int *)(0xbfc3fa18))
#define FCC_DRV_RECV_DATA_FROM_TRANSMITTER_ENTRY_ADDR  (*(unsigned int *)(0xbfc3fa1c))

/*************************************************/
typedef struct
{
    unsigned int rx_packet_cnts;
    unsigned int total_error_bits;
    unsigned int packet_len;
    signed short rx_rssi;          // -127 ~ 127 (0  =  0dBm  unit is 0.5 dBm)
    signed short cfo_index;        // -127 ~ 127 ( unit .tbd)
} mp_rx_report_t;

typedef int (*install_fcc_drv)(unsigned int para);
typedef int (*uninstall_fcc_drv)(void);
typedef int (*bt_packet_receive_process)(unsigned int channel, unsigned int packet_counter);
typedef int (*bt_rx_report)(mp_rx_report_t *rx_report);
typedef int (*bt_packet_send_process)(unsigned int channel, unsigned int power, unsigned int packet_counter);
typedef int (*fcc_drv_send_data_to_transmitter)(unsigned char *buf, unsigned int len);
typedef int (*fcc_drv_recv_data_from_transmitter)(unsigned char *buf, unsigned int max_len);

#define act_test_install_fcc_drv(a)                ((int(*)(unsigned int))FCC_DRV_INSTALL_ENTRY_ADDR)((a))
#define act_test_uninstall_fcc_drv()               ((int(*)(void))FCC_DRV_UNINSTALL_ENTRY_ADDR)()
#define act_test_bt_packet_receive_process(a,b)    ((int(*)(unsigned int , unsigned int))BT_PACKET_RECEIVE_PROCESS_ENTRY_ADDR)((a),(b))
#define act_test_bt_rx_report(a)                   ((int(*)(mp_rx_report_t *))BT_RX_REPORT_ENTRY_ADDR)((a))
#define soc_atp_set_hosc_calib(a,b)                ((int(*)(int , unsigned char))SOC_ATP_SET_HOSC_CALIB_ENTRY_ADDR)((a),(b))
#define act_test_bt_packet_send_process(a,b,c)     ((int(*)(unsigned int, unsigned int, unsigned int))BT_PACKET_SEND_PROCESS_ENTRY_ADDR)((a),(b),(c))
#define act_test_fcc_drv_send_data_to_transmitter(a,b)    ((int(*)(unsigned char *, unsigned int))FCC_DRV_SEND_DATA_TO_TRANSMITTER_ENTRY_ADDR)((a),(b))
#define act_test_fcc_drv_recv_data_from_transmitter(a,b)  ((int(*)(unsigned char *, unsigned int))FCC_DRV_RECV_DATA_FROM_TRANSMITTER_ENTRY_ADDR)((a),(b))

#endif
