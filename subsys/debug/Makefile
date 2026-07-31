ccflags-y +=-I$(srctree)/include/debug

obj-$(CONFIG_OPENOCD_SUPPORT) += openocd.o
obj-y += kallsyms.o
obj-$(CONFIG_MAGIC_SYSRQ) += sysrq.o

obj-$(CONFIG_DEBUG_COREDUMP) += coredump/
obj-$(CONFIG_STREAM_FLASH) += stream_flash/
obj-$(CONFIG_DEBUG_RAMDUMP) += ramdump/
obj-$(CONFIG_TASK_WDT) += task_watchdog/
obj-$(CONFIG_POWEROFF_SIMULATOR) += poweroff_simulator/
obj-y += data_export/
