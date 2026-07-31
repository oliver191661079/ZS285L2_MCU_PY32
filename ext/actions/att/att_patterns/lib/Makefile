ATT_ROOT := $(CURDIR)/../..

IC_TYPE ?= andes
TARGET ?= libatt.a

all:
	cd $(IC_TYPE) && make || exit "$$?"

clean:
	cd $(IC_TYPE) && make clean || exit "$$?"
	rm -rf *.d
