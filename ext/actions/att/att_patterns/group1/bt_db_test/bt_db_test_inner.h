typedef struct
{
    uint32 magicl;
    uint32 magich;
    uint8 reserved;
    uint8 addr[3];
    uint32 record_cnt;
    uint32 succeed_cnt;
    uint32 failed_cnt;
    bt_paired_dev_info2_t bt_paired_dev_info;
    uint32 checksum;
}btaddr_log_file_t;