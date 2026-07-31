# SPDX-License-Identifier: Apache-2.0

ZEPHYRINCLUDE += -I$(srctree)/ext/actions/bluetooth/bt_stack
ZEPHYRINCLUDE += -I$(srctree)/ext/actions/bluetooth/include
ZEPHYRINCLUDE += -I$(srctree)/ext/actions/bluetooth/bt_stack/src/inc

obj-$(CONFIG_BT_HCI_RAW) += hci_raw.o hci_common.o
obj-$(CONFIG_ATCS_BT_DEBUG_MONITOR) += monitor.o
obj-$(CONFIG_BT_TINYCRYPT_ECC) += hci_ecc.o
obj-$(CONFIG_BT_TESTING) += testing.o
obj-$(CONFIG_BT_HOST_CCM) += aes_ccm.o
obj-$(CONFIG_BT_PROPERTY) += property.o

obj-$(CONFIG_BT_BREDR) += sdp.o ssp.o


ifdef CONFIG_BT_HCI_HOST
  obj-y += uuid.o addr.o buf.o hci_core.o hci_common.o register_sdp.o id.o
  obj-$(CONFIG_BT_BROADCASTER) += adv.o
  obj-$(CONFIG_BT_OBSERVER) += scan.o
  obj-$(CONFIG_BT_HOST_CRYPTO) += crypto.o
  obj-$(CONFIG_BT_DF) += direction.o
  obj-$(CONFIG_BT_CONN) += conn.o l2cap.o att.o gatt.o
  ifdef CONFIG_BT_CONN
    ifdef CONFIG_BT_SMP
      obj-y += smp.o keys.o
    else
      obj-y += smp_null.o
    endif
  endif
endif

obj-$(CONFIG_BT_ISO) += iso.o
