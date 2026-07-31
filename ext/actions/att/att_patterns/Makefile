.PHONY: make_bin_dir loader  gen_dir lib  group1  make_cfg make_atf

all: clean make_bin_dir gen_dir lib loader group1   #make_atf

BOARD ?= ats283x_dvb

make_bin_dir:
	mkdir -p ./../att_bin/outbin

gen_dir:
	mkdir -p ./../att_bin/outbin

lib:
	cd ./lib && make || exit "$$?"

loader:
	cd ./loader && make || exit "$$?"

group1:
	cd ./group1 && make || exit "$$?"

make_atf:
	python ./scripts/build_atf.py -c ./../att_bin/atf_make.cfg  -o ./../att_bin/actionstest_28xx.atf

clean:
	cd ./lib && make clean
	cd ./loader && make clean
	cd ./group1 && make clean
	rm -rf ./scripts/*.out
	rm -rf ../att_bin/actionstest_28xx.atf
