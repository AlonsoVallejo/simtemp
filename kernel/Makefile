KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

.PHONY: all drv_simtemp dto_simtemp clean

all: drv_simtemp dto_simtemp

drv_simtemp:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

dto_simtemp: dts/nxp-simtemp.dtsi
	dtc -@ -I dts -O dtb -o nxp-simtemp.dtbo dts/nxp-simtemp.dtsi

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f nxp-simtemp.dtbo
