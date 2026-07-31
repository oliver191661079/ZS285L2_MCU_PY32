ZEPHYRINCLUDE += -I$(srctree)/lib/utils/include
obj-$(CONFIG_AUDIO_OUT_ACTS_ANDES) += audio_out_acts_andes.o audio_out_acts_inner.o \
				phy/phy_audio_pa.o \
				phy/phy_audio_misc.o \
				phy/phy_audio_dac.o \
				phy/phy_audio_asrcout.o \
				phy/phy_audio_coefficients.o \
				phy/phy_audio_aa.o \
				phy/phy_audio_i2stx.o \
				phy/phy_audio_spdiftx.o \
				phy/phy_audio_antipop.o
				
obj-$(CONFIG_AUDIO_IN_ACTS_ANDES) += audio_in_acts_andes.o audio_in_acts_inner.o \
				phy/phy_audio_adc.o \
				phy/phy_audio_asrcin.o \
				phy/phy_audio_i2srx.o	\
				phy/phy_audio_spdifrx.o	
				
obj-$(CONFIG_AUDIODRV_SHELL) += shell/audio_driver_shell.o

