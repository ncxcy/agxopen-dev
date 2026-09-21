obj-m += agxrtbuddy.o
agxrtbuddy-y += src/mailbox.o src/power.o src/display.o src/rtkit.o
agxrtbuddy-y += src/rtbuddy.o src/probe.o src/debugfs.o src/uat.o src/gfxhandoff.o

ccflags-y += -I$(src)/include

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(MAKE) -C tools clean
	$(MAKE) -C tests clean

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install

tools:
	$(MAKE) -C tools

test:
	$(MAKE) -C tests

.PHONY: all clean install tools test
