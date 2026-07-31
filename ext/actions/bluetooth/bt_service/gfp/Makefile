obj-y += impl/actions_impl.o
obj-y += impl/bluetooth.o
obj-y += impl/crypto.o
obj-y += impl/storage.o
obj-y += impl/btsrv_gfp.o
obj-y += sys_comm/sys_comm.o
obj-y += spp_connect/gfp_spp_stream_ctrl.o
obj-y += ble_l2cap/gfp_ble_l2cap_ctrl.o
obj-y += spp_connect/gfp_rfcomm.o
obj-y += le_connect/gfp_ble_stream.o
obj-y += le_connect/gfp_ble_stream_ctrl.o
obj-y += libbt_gfp/

ccflags-y += -I${ZEPHYR_BASE}/ext/actions/bluetooth/include/gfp
ccflags-y += -I${ZEPHYR_BASE}/ext/actions/bluetooth/bt_manager
ccflags-y += -I${ZEPHYR_BASE}/ext/actions/bluetooth/bt_service/gfp/le_connect
ccflags-y += -I${ZEPHYR_BASE}/ext/lib/crypto/tinycrypt/include
