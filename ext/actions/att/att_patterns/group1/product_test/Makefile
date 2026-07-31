ATT_ROOT := $(CURDIR)/../../..

IC_TYPE ?= andes
TARGET ?= p21_mpd.bin

all:
	rm -rf link.ld
	cd $(IC_TYPE) && make || exit "$$?"
	cp -rf $(IC_TYPE)/$(TARGET) $(ATT_ROOT)/att_bin/outbin

clean:
	cd $(IC_TYPE) && make clean || exit "$$?"
	rm -rf link.ld
	rm -rf $(ATT_ROOT)/att_bin/outbin/$(TARGET)
	rm -rf *.d
