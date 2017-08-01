obj-m += scull.o
SRC_PATH:=/home/awe/beaglebone/kernel-stock-4.9
PWD=$(shell pwd)
DEBUG_FLAGS:=

ifeq ($(DEBUG),1)
 #DEBUG_FLAGS += -DSCULL_DEBUG
else
 DEBUG_FLAGS +=
endif

ifeq ($(VEXP), 1)
 SRC_PATH:= /home/awe/kernel/vexpress-out/moduels/lib/modules/4.9.11+/build
endif

EXTRA_CFLAGS += $(DEBUG_FLAGS)

bone:
	make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C $(SRC_PATH) M=$(PWD) $(DEBUG_FLAGS) modules

clean:
	rm -rf *.o *.order *.symvers .tmp_versions *.ko .*.cmd *.mod.*
