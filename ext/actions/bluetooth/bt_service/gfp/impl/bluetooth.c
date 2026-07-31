#include <stdio.h>
#include <string.h>
#include <fast_pair.h>
#include "bluetooth.h"
#include <helper.h>

#include <bt_manager.h>
#include "bt_manager_inner.h"
#include <sys_comm.h>
#include <gfp_ble_stream.h>
#include <gfp_ble_stream_ctrl.h>


#define BT_NAME_LEN		(32+1)	/* 32(len) + 1(NULL) */


/////////////////////////////////////////////////////////////////
// Bluetooth Provider.
//
// Provides stubs for Bluetooth functionality, this
// implementation will vary significatly based on chipset.
/////////////////////////////////////////////////////////////////

void static bytes_reverse(uint8_t *dst, uint8_t *src, uint8_t len)       
{
    uint8_t i;
    if ( ((dst < src) && (dst + len <= src)) 
        || ((dst > src) && (dst - len >= src)) )
    {
        for (i = 0; i < len; i++)
        {
            dst[i] = src[len - 1 - i];
        }
    }
    else if (dst == src)
    {
        for (i = 0; i < len/2; i++)
        {
            dst[i] ^= src[len - 1 - i];
            src[len - 1 - i]  ^= dst[i];
            dst[i] ^= src[len - 1 - i];
        }
    }
    else
    {
        //QA
    }
}

void get_ble_address(uint8_t* address)
{
    bt_addr_le_t addr;
    btif_ble_get_local_mac(&addr);
    bytes_reverse(addr.a.val, addr.a.val, MAC_ADDRESS_LENGTH);
	memcpy(address,addr.a.val,MAC_ADDRESS_LENGTH);
}

void get_public_address(uint8_t* address) 
{
    //get_nvram_mac_info(address,CFG_BT_MAC,6);
    bd_address_t addr;
    btif_br_get_local_mac(&addr);
    bytes_reverse(addr.val, addr.val, MAC_ADDRESS_LENGTH);
	memcpy(address,addr.val,MAC_ADDRESS_LENGTH);
}

uint16 get_personalized_name(uint8_t * name)
{
    uint16 name_len = 0;
    int ret_val;

    ret_val = property_get(VM_FAST_PERSONALIZED_NAME, name, PERSONALIZED_NAME_SIZE);
    if ((ret_val < PERSONALIZED_NAME_SIZE) || (strlen(name) < 1)) {
        SYS_LOG_WRN("no name!");
        return 0;
    }

    name_len = strlen(name);
    if (name_len > PERSONALIZED_NAME_SIZE)
        name_len = PERSONALIZED_NAME_SIZE;

    SYS_LOG_INF("name_len %d.", name_len);
    return name_len;
}

void update_personalized_name(uint8_t * name,uint8_t length, bool additional)
{
    int ret_val;

    if(name == NULL){
        SYS_LOG_INF("ERROR");
        return;
    }

#ifdef CONFIG_PROPERTY
    SYS_LOG_INF("name:%s",name);
    u8_t tmp_name[64];

    memset(tmp_name, 0, PERSONALIZED_NAME_SIZE);
    if (length > PERSONALIZED_NAME_SIZE)
        length = PERSONALIZED_NAME_SIZE;

    memcpy(tmp_name, name, length);
    ret_val = property_set(VM_FAST_PERSONALIZED_NAME,tmp_name,PERSONALIZED_NAME_SIZE);
    if (ret_val < 0) {
        SYS_LOG_ERR("failed to upgade gfp bt name,ret %d\n", ret_val);
    }
    else{
        if(additional){
            property_set(CFG_BT_NAME, tmp_name, BT_NAME_LEN);
            property_set(CFG_BLE_NAME, tmp_name, BT_NAME_LEN);
            property_set(CFG_BT_LOCAL_NAME, tmp_name, BT_NAME_LEN);
            bt_manager_gfp_personalized_name_update(true);
        }
    }
#endif
}

int is_paired_device(uint8_t* address)
{
    struct bt_mgr_dev_info *p_dev = NULL;
    bd_address_t addr;

    bytes_reverse(addr.val, address, MAC_ADDRESS_LENGTH);
    p_dev = bt_mgr_find_dev_info(&addr);

    if(p_dev != NULL){
        return 1;
    }
    else{
        return 0;
    }
}

int is_pairing(void)
{
    return bt_manager_is_pair_mode();
}

int is_lea_open(void)
{
    return bt_manager_audio_is_lea_open();
}

void set_capabilities_display_yes_no(bool ble_devices)
{
  //  TODO: Set device capabilities to Display/YesNo.
    SYS_LOG_INF("ble:%d",ble_devices);

    if(ble_devices){
        btsrv_gfp_le_cap_io_set(true);
    }
    else{
        btsrv_gfp_cap_io_set(true);
    }
}

/* 协议栈默认的是noinputnooutput，这里做恢复操作 */
void set_capabilities_noinputnooutput(bool ble_devices)
{
    SYS_LOG_INF("ble %d",ble_devices);
    if(ble_devices){
        btsrv_gfp_le_cap_io_set(false);
    }
    else{
        btsrv_gfp_cap_io_set(false);
    }
}

void initiate_bonding(uint8_t* address)
{
    //TODO: Initiate bonding with the provided address.
    print_hex_comm("BONDING ADDR:",address,6);  
}

void update_scan(bool valid)
{
    SYS_LOG_INF("gfp scan %d",valid);

    btif_br_set_gfp_scan_mode(valid);
}

void confirm_pairing(uint8_t* address, bool success,bool ble_devices)
{
	btsrv_gfp_confirm_pairing_reply(success,ble_devices);
}

const BluetoothProvider bluetooth = {
  .get_ble_address                   = get_ble_address,
  .get_public_address                = get_public_address,
  .notify                            = gfp_send_pkg_to_stream,
  .is_pairing                        = is_pairing,
  .set_capabilities_display_yes_no   = set_capabilities_display_yes_no,
  .set_capabilities_noinputnooutput  = set_capabilities_noinputnooutput,
  .initiate_bonding                  = initiate_bonding,
  .update_scan                       = update_scan,
  .set_pairing_request_callback      = btsrv_gfp_pairing_request_reg,
  .confirm_pairing                   = confirm_pairing,
  .is_paired_device                  = is_paired_device,
  .get_personalized_name             = get_personalized_name,
  .update_personalized_name          = update_personalized_name,
  .is_lea_open                       = is_lea_open,
};

void init_bluetooth(BluetoothProvider** bluetooth_p)
{
    *bluetooth_p = (BluetoothProvider*)&bluetooth;
}



