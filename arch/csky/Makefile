KBUILD_CFLAGS += $(call cc-option,-ffunction-sections,) $(call cc-option,-fdata-sections,)

ifeq ($(CONFIG_FLOAT), y)
HAVE_FLOAT = f -mhard-float
endif

include $(srctree)/arch/$(ARCH)/soc/$(SOC_PATH)/Makefile

cflags-$(CONFIG_LTO) += $(call cc-option,-flto,)

aflags-$(CONFIG_CPU_CK802)   += -mcpu=ck802 -Os -fshort-enums -fsigned-char -fno-common -fno-delete-null-pointer-checks
cflags-$(CONFIG_CPU_CK802)   += -mcpu=ck802 -Os -fshort-enums -fsigned-char -fno-common -fno-delete-null-pointer-checks
cxxflags-$(CONFIG_CPU_CK802) += -mcpu=ck802 -Os -fshort-enums -fsigned-char -fno-common -fno-delete-null-pointer-checks

aflags-$(CONFIG_CPU_CK803S)   += -mcpu=ck803s$(HAVE_FLOAT)
cflags-$(CONFIG_CPU_CK803S)   += -mcpu=ck803s$(HAVE_FLOAT)
cxxflags-$(CONFIG_CPU_CK803S) += -mcpu=ck803s$(HAVE_FLOAT)

KBUILD_AFLAGS += $(aflags-y)
KBUILD_CFLAGS += $(cflags-y)
KBUILD_CXXFLAGS += $(cflags-y)

soc-cxxflags ?= $(soc-cflags)
soc-aflags ?= $(soc-cflags)
KBUILD_CFLAGS += $(soc-cflags)
KBUILD_CXXFLAGS += $(soc-cxxflags)
KBUILD_AFLAGS += $(soc-aflags)

