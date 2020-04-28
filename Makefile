obj-m:=SystemCall.o mmap_test_kernel.o
CURRENT_PATH:=$(shell pwd)
LINUX_KERNEL_PATH:=/lib/modules/$(shell uname -r)/build
all: mod Spectre
mod:
	make -C  $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) modules
Spectre: 
	gcc -o Spectre Spectre.c
clean:
	make -C  $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) clean
	rm Spectre
