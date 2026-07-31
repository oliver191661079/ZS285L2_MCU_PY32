#!/bin/bash -e
#
# Copyright (c) 2017 Actions Semiconductor Co., Ltd
#
# SPDX-License-Identifier: Apache-2.0
#

unset MENU_CHOICES_ARRAY
add_item(){
	local new_item=$1
	local c
	for c in ${MENU_CHOICES_ARRAY[@]} ; do
		if [ "$new_item" = "$c" ] ; then
			return
		fi
	done
	MENU_CHOICES_ARRAY=(${MENU_CHOICES_ARRAY[@]} $new_item)
}

build_list() {
	unset MENU_CHOICES_ARRAY
	local file
	for file in ` ls -1 $1`
	do
		if [ -d $1"/"$file ]
		then
			add_item $file
		fi

	done
}

print_menu(){
	local i=1
	local choice
	for choice in ${MENU_CHOICES_ARRAY[@]}
	do
		echo "     $i. $choice"
		i=$(($i+1))
	done

	echo
}

read_choice() {
	local answer
	local result

	echo
	echo $2
	echo
	build_list $3
	print_menu
	echo -n "Which would you like? [${MENU_CHOICES_ARRAY[0]}] "
	read answer

	if [ -z "$answer" ]; then
		result=${MENU_CHOICES_ARRAY[0]}
	elif (echo -n $answer | grep -q -e "^[0-9][0-9]*$"); then
		if [ $answer -le ${#MENU_CHOICES_ARRAY[@]} ]; then
		    result=${MENU_CHOICES_ARRAY[$(($answer-1))]}
		fi
	else
		result=$answer
	fi

	if [ -z "$result" ]; then
		echo "Invalid select: $answer"
		return 1
	fi

	eval $1=$result
}

print_usage(){
	echo "Usage:"
	echo "  Build firmware using last config:"
	echo "    $0"
	echo "  Build firmware using new config:"
	echo "    $0 -n"
	echo "  Pack firmware only:"
	echo "    $0 -p"
	echo "  Build system lib:"
	echo "    $0 -lib"
	echo "  Clean build out directory:"
	echo "    $0 -c"
	echo "  Clean all build out directory:"
	echo "    $0 -ca"
	echo
}

SDK_ROOT=`pwd`
source ${SDK_ROOT}/zephyr-env.sh

#ARCH=mips
ARCH=csky
export ZEPHYR_GCC_VARIANT=gcccsky
export GCCCSKYV2_TOOLCHAIN_PATH=/opt/csky-elfabiv2

BOARD=
APPLICATION=
BUILD_CLEAN=0
BUILD_SDK_LIB=0
BUILD_CONFIG_FILE=${SDK_ROOT}/.build_config
RECOVERY_CONF=prj_recovery.conf

# import old build config
if [ -f ${BUILD_CONFIG_FILE} ]; then
	source ${BUILD_CONFIG_FILE}
fi

if [ $# = 1 ]; then
# skip old config?
if [ "$1" = "-n" ]; then
	BOARD=""
	APPLICATION=""
# clean build directory?
elif [ "$1" = "-c" ]; then
	BUILD_CLEAN=1
elif [ "$1" = "-ca" ]; then
	BUILD_CLEAN=2
elif [ "$1" = "-lib" ]; then
	BUILD_SDK_LIB=1
elif [ "$1" = "-p" ]; then
    PACK_FW_ONLY=1
else
	print_usage
	exit 1
exit 1
fi
elif [ $# = 2 ]; then
BOARD=$1
APPLICATION=$2
fi

if [ $# = 1 ] && [ "$1" = "-n" ]; then
	BOARD=""
	APPLICATION=""
	CONFIG=""
fi

# select board and application
if [ "${BOARD}" = "" ] || [ "${APPLICATION}" = "" ]; then
	read_choice BOARD "Select board:" ${SDK_ROOT}/boards/${ARCH}
	read_choice APPLICATION "Select application:" ${SDK_ROOT}/samples
fi

if [ "${CONFIG}" = "" ] && [ -d ${SDK_ROOT}/samples/${APPLICATION}/app_conf ]; then
	read_choice CONFIG "Select application conf:" ${SDK_ROOT}/samples/${APPLICATION}/app_conf
fi

# write varibles to config file
echo "# Build config" > ${BUILD_CONFIG_FILE}
echo "BOARD=${BOARD}" >> ${BUILD_CONFIG_FILE}
echo "APPLICATION=${APPLICATION}" >> ${BUILD_CONFIG_FILE}
echo "CONFIG=${CONFIG}" >> ${BUILD_CONFIG_FILE}

PRJ_BASE=${SDK_ROOT}/samples/${APPLICATION}
OUTDIR=${PRJ_BASE}/outdir/${BOARD}_${CONFIG}

if [ ! -d ${SDK_ROOT}/boards/${ARCH}/${BOARD} ]; then
	echo -e "\nNo board at ${SDK_ROOT}/boards/${ARCH}/${BOARD}\n\n"
	exit 1
fi

if [ ! -d ${PRJ_BASE} ]; then
	echo -e "\nNo application at ${PRJ_BASE}\n\n"
	exit 1
fi

if [ "${BUILD_CLEAN}" = "1" ]; then
	echo "Clean build out directory"
	rm -rf ${OUTDIR}
	rm -rf ${OUTDIR}_recovery
	rm -rf ${OUTDIR}_second
	exit 0
fi

if [ "${BUILD_CLEAN}" = "2" ]; then
	echo "Clean all build out directory"
	rm -rf ${PRJ_BASE}/outdir
	exit 0
fi

echo -e "\n--== Build application ${APPLICATION} for board ${BOARD} used config ${CONFIG} ==--\n\n"

export APPLICATION=${APPLICATION}

build_att() {
	ATT_PATTERN_DIR="${SDK_ROOT}/ext/actions/att/att_patterns"
	ATT_SRC_BIN_DIR="${SDK_ROOT}/ext/actions/att/att_bin"
	ATT_DST_BIN_DIR="${OUTDIR}/_firmware/att_pattern_bin"
	if [ "X${CONFIG_ACTIONS_ATT}" == "Xy" ] ; then
		if [ -d ${ATT_PATTERN_DIR} -a -f "${ATT_PATTERN_DIR}/build.sh" ] ; then
			echo  -e "\e[32m        try to build att pattern        \e[m"
			sleep 1
			cd $ATT_PATTERN_DIR
			./build.sh
			cd $SDK_ROOT

			if [ -d $ATT_DST_BIN_DIR ] ;then
				rm -rf ${ATT_DST_BIN_DIR}
			fi

			mkdir -p ${ATT_DST_BIN_DIR}

			cp -r ${ATT_SRC_BIN_DIR}/* ${ATT_DST_BIN_DIR}
		else
			echo  -e "\e[31m   ${ATT_PATTERN_DIR}/build.sh    not exist    build att pattern failed \e[m"
		fi
	fi
}


# build application
if [ -d ${PRJ_BASE}/app_conf/${CONFIG} ]; then

	MAKE_APP_PARAM_GEN="-C ${PRJ_BASE} BOARD=${BOARD} CONF_FILE=${PRJ_BASE}/app_conf/${CONFIG}/prj.conf APP_CONFIG=${CONFIG} O=${OUTDIR} -j4"
	#Make v4.3.1 on msys2 does not work correctly when doing make-cmd expound for '\#' (scripts/Kbuild.include).
	if uname | grep -q -E "MINGW|MSYS"; then
		MAKE_APP_PARAM_GEN+=" KBUILD_NOCMDDEP=1"
	fi

	mkdir -p ${OUTDIR}
	mkdir -p ${OUTDIR}/sdfs

	if [ -f ${PRJ_BASE}/firmware.xml ] ; then
		echo -e "FW: Override the default firmware.xml"
		cp ${PRJ_BASE}/firmware.xml ${OUTDIR}
	fi

	if [ -f ${PRJ_BASE}/app_conf/${CONFIG}/firmware.xml ] ; then
		echo -e "FW: Override the project firmware.xml"
		cp ${PRJ_BASE}/app_conf/${CONFIG}/firmware.xml ${OUTDIR}/
	fi

	if [ -d ${PRJ_BASE}/app_conf/${CONFIG}/sdfs ]; then
		echo "sdfs: copy sdfs for ${CONFIG} ..."
        rm -rf ${OUTDIR}/sdfs/*
		cp -rf  ${PRJ_BASE}/app_conf/${CONFIG}/sdfs/* ${OUTDIR}/sdfs/
	fi;

	if [ "${PACK_FW_ONLY}" != "1" ]; then
		echo -e "\n--== Build main application for board ${BOARD} ==--\n\n"
		make ${MAKE_APP_PARAM_GEN}
	fi

	CONF_FILE=${PRJ_BASE}/outdir/${BOARD}_${CONFIG}/.config
	source $CONF_FILE
	if [ "X$CONFIG_ACTIONS_IMG_LOAD" == "Xy" ] ; then

		IMG_DIR=${SDK_ROOT}/scripts/support/${CONFIG_SOC_FAMILY}/prebuilt/${CONFIG_SOC_SERIES}/${CONFIG_SOC}/test_bin

		IMG_FILE=${IMG_DIR}/pd1.bin
		#echo "cp ${IMG_FILE}  ${OUTDIR}/sdfs/"
		if [ -f ${IMG_FILE} ] ; then
			cp ${IMG_FILE}  ${OUTDIR}/sdfs/
		fi

		IMG_FILE=${IMG_DIR}/pt1.bin
		#echo "cp ${IMG_FILE}  ${OUTDIR}/sdfs/"
		if [ -f ${IMG_FILE} ] ; then
			cp ${IMG_FILE}  ${OUTDIR}/sdfs/
		fi

		IMG_FILE=${IMG_DIR}/srrc.bin
		#echo "cp ${IMG_FILE}  ${OUTDIR}/sdfs/"
		if [ -f ${IMG_FILE} ] ; then
			cp ${IMG_FILE}  ${OUTDIR}/sdfs/
		fi

	fi

	if [ "${PACK_FW_ONLY}" = "1" ]; then
		echo -e "\n--== Pack firmware for board ${BOARD} ==--\n\n"
		make ${MAKE_APP_PARAM_GEN} firmware
		exit 0;
	fi;



	#comment out build att function
	build_att

	if [ -f ${PRJ_BASE}/app_conf/${CONFIG}/prj_second.conf ]; then
		echo -e "\n--== Build second application for board ${BOARD}_${CONFIG} ==--\n\n"
		make -C ${PRJ_BASE} BOARD=${BOARD} CONF_FILE=${PRJ_BASE}/app_conf/${CONFIG}/prj_second.conf APP_CONFIG=${CONFIG} O=${OUTDIR}_second -j4
	fi

	if [ -f ${PRJ_BASE}/app_conf/${CONFIG}/${RECOVERY_CONF} ]; then
		echo -e "\n--== Build recovery application for board ${BOARD}_${CONFIG} ==--\n\n"
		# build recovery application
		make -C ${PRJ_BASE} BOARD=${BOARD} CONF_FILE=${PRJ_BASE}/app_conf/${CONFIG}/${RECOVERY_CONF} APP_CONFIG=${CONFIG} O=${OUTDIR}_recovery -j4
	fi

else
	echo -e "No application config!"
fi

if [ "${BUILD_SDK_LIB}" = "1" ]; then
	echo "build SDK library"
	make -C ${PRJ_BASE} BOARD=${BOARD}  O=${OUTDIR} update-media-library
	make -C ${PRJ_BASE} BOARD=${BOARD}  O=${OUTDIR} update-ota-library
	make -C ${PRJ_BASE} BOARD=${BOARD}  O=${OUTDIR} update-bt-service-library
	make -C ${PRJ_BASE} BOARD=${BOARD}  O=${OUTDIR} update-bt-stack-library
	make -C ${PRJ_BASE} BOARD=${BOARD}  O=${OUTDIR} update-soc-library
	make -C ${PRJ_BASE} BOARD=${BOARD}  O=${OUTDIR} update-secureboot-library
	exit 0
fi

# pack firmware
echo -e "\n--== Pack firmware for board ${BOARD} ==--\n\n"
make -C ${PRJ_BASE} BOARD=${BOARD} APP_CONFIG=${CONFIG} O=${OUTDIR} firmware
python ${OUTDIR}/_firmware/rom_ram_size_report -r -o ${OUTDIR}
