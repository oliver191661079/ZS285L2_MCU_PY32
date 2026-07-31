ccflags-y += -I$(srctree)/kernel/include
asflags-y := ${ccflags-y}

obj-y = crt0.o entry.o exc_exit.o prep_c.o irq_init.o swap.o irq_manage.o thread.o \
	cpu_idle.o fatal.o sys_fatal_error_handler.o swap_c.o

obj-$(CONFIG_IRQ_OFFLOAD) += irq_offload.o
obj-$(CONFIG_STACK_BACKTRACE) += backtrace.o
obj-$(CONFIG_KALLSYMS) += csky_backtrace.o
