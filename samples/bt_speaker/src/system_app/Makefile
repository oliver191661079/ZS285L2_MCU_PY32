obj-y += system_app_init.o
obj-y += system_app_init_input.o
obj-y += system_le_audio.o
obj-y += system_app_shell.o
obj-y += system_audio_policy.o
obj-y += system_tts_policy.o
obj-y += system_event_map.o
obj-y += system_btmgr_config.o
obj-y += system_app.o
obj-y += system_app_bt_callback.o
# SPP bridge and/or AVRCP ID3 UART hook (see system_app_spp_uart_bridge.c)
ifeq ($(CONFIG_BT_SPP),y)
obj-y += system_app_spp_uart_bridge.o
else ifeq ($(CONFIG_BT_AVRCP_ID3_UART_DUMP),y)
obj-y += system_app_spp_uart_bridge.o
endif
obj-$(CONFIG_ACTIONS_ATT) += system_app_att.o
obj-$(CONFIG_BT_ADV_MANAGER) += system_ble_advertise.o
ifeq ($(CONFIG_BT_BLE),y)
obj-$(CONFIG_SYSTEM_APP_BLE_REMOTE_CMD) += system_app_ble_remote_cmd.o
endif
obj-$(CONFIG_SYSTEM_APP_PY32_UART) += system_app_py32_uart.o
obj-$(CONFIG_LOGSRV_SELF_APP) += system_app_log_send.o
obj-$(CONFIG_APP_TWS_SNOOP) += system_event_tws.o