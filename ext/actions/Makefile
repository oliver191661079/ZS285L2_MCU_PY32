LIB_INCLUDE_DIR += -L$(ZEPHYR_BASE)/ext/actions/library/$(CONFIG_ARCH)
LIB_INCLUDE_DIR += -L$(ZEPHYR_BASE)/ext/actions/media/libal/$(CONFIG_ARCH)/al
LIB_INCLUDE_DIR += -L$(ZEPHYR_BASE)/ext/actions/library/$(CONFIG_ARCH)/$(CONFIG_SOC_SERIES)

ifdef CONFIG_BT_DRV
LIB_INCLUDE_DIR += -L$(ZEPHYR_BASE)/ext/actions/porting/hal/bluetooth/library/$(CONFIG_ARCH)
endif

ZEPHYRINCLUDE += -I$(srctree)/ext/actions/base/include/utils
ZEPHYRINCLUDE += -I$(srctree)/ext/actions/base/include/utils/iterator
ZEPHYRINCLUDE += -I$(srctree)/ext/actions/base/include/utils/stream
ZEPHYRINCLUDE += -I$(srctree)/ext/actions/base/include/core
ZEPHYRINCLUDE += -I$(srctree)/ext/actions/base/utils/cbuf/include
ZEPHYRINCLUDE += -I$(srctree)/ext/actions/system/include
ZEPHYRINCLUDE += -I$(srctree)/ext/actions/display/include
ZEPHYRINCLUDE += -I$(srctree)/ext/actions/media/include
ZEPHYRINCLUDE += -I$(srctree)/ext/actions/audio
ZEPHYRINCLUDE += -I$(srctree)/ext/actions/ota/include
ZEPHYRINCLUDE += -I$(srctree)/ext/actions/porting/include
ZEPHYRINCLUDE += -I$(srctree)/ext/actions/porting/include/al
ZEPHYRINCLUDE += -I$(srctree)/ext/actions/bluetooth/include
ZEPHYRINCLUDE += -I$(srctree)/ext/actions/bluetooth/bt_stack/include/
ZEPHYRINCLUDE += -I$(srctree)/ext/actions/usb/include
ZEPHYRINCLUDE += -I$(srctree)/ext/actions/usb/include/usb/class

ifdef CONFIG_BT_DRV_ANDESC_PHOENIX
ZEPHYRINCLUDE += -I$(srctree)/ext/actions/porting/hal/bluetooth/bt_drv/bt_drv_andesc_phoenix
endif
ifdef CONFIG_MEDIA_SERVICE
-include $(srctree)/ext/actions/media/Makefile
endif

ifdef CONFIG_OTA
-include $(srctree)/ext/actions/ota/Makefile
endif

ifdef CONFIG_BT_SERVICE
-include $(srctree)/ext/actions/bluetooth/Makefile
endif

ifdef CONFIG_BT_MP
-include $(srctree)/ext/actions/mp/Makefile
endif

ifdef CONFIG_SOC_SECURE_BOOT
-include $(srctree)/ext/actions/secure_boot/Makefile
endif

ifdef CONFIG_ACTIONS_ALGORITHMS_LIBRARY
  ifdef CONFIG_DECODER_ACT
    ALL_LIBS += _k1_act_d
  endif

  ifdef CONFIG_PARSER_MP3
     ALL_LIBS += _k1_mp3_p
  endif

  ifdef CONFIG_PARSER_APE
     ALL_LIBS += _k1_ape_p
  endif

  ifdef CONFIG_PARSER_FLAC
     ALL_LIBS += _k1_fla_p
  endif

  ifdef CONFIG_PARSER_AAC
     ALL_LIBS += _k1_a13_p
  endif

  ifdef CONFIG_PARSER_WAV
     ALL_LIBS += _k1_wav_p
  endif

  ifdef CONFIG_PARSER_WMA
     ALL_LIBS += _k1_w13_p
  endif

  ifdef CONFIG_PARSER_M4A
     ALL_LIBS += _k1_a13_p
  endif
  
  ifdef CONFIG_MUSIC_DAE_FADE
     ALL_LIBS += _k1_fad_p
  endif
endif # CONFIG_ACTIONS_ALGORITHMS_LIBRARY

ifdef CONFIG_BT_DRV
ifdef CONFIG_SOC_SERIES_ANDESC_P2

ifdef CONFIG_SOC_ATS2853P2
ALL_LIBS += btcon_phoenix_p2_nocis_285l
else

ifdef CONFIG_BT_LE_AUDIO
ALL_LIBS += btcon_phoenix_p2
else
ALL_LIBS += btcon_phoenix_p2_nocis
endif
endif
else
ALL_LIBS += btcon_phoenix
endif
endif

ROM_FILES += $(ZEPHYR_BASE)/ext/actions/libs/csky/andesc/rom.sym
