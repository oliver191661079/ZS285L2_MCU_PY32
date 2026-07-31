#!/bin/bash
###################################################################
# Thu Sep 11 14:50:07 CST 2025
# hanchenfeng@actionstech.com

#
# A script to build multiple projects in one time.
#
###################################################################

#set -x

readonly projects=('multi_ch' 'spdif' '285l')

#default parameters
DEST='out'

function usage {
	echo "Usage: $(basename $0) <-p PROJECT> <-s DIR> <-t TAG> [-o DIR] [-dvh]" 2>&1
	echo 'build multiple projects locally.'
	echo '   -p PROJECT         Specify the project name.'
	for prj in ${projects[@]}; do
		echo "      $prj"
	done
	echo '   -s DIR             set the Source directory.'
	echo '   -o DIR             set the Output directory.'
	echo '   -t TAG             set the Tag name.'
	echo '   -d                 enable Debug mode.'
	echo '   -v                 enable Verbose mode.'
	echo '   -h                 print Help message.'
	exit 1
}

function print_out {
	local MESSAGE="${@}"
	if [[ "${VERBOSE}" == true ]];then
		echo "${MESSAGE}"
	fi
}

function check_env {

	#Config information feedback
	print_out "Project: ${PROJECT}"
	print_out "Source:  ${SOURCE_PATH}"
	print_out "Dest:    $DEST_PATH"

	#check project
	local correct_project=0
	for prj in ${projects[@]}; do
		if [ $PROJECT == $prj ]; then
			correct_project=1
			break
		fi
	done

	if [ 0 == $correct_project ]; then
		echo "Wrong project!"
		usage
		exit -1
	fi

	#check source path
	if [ ! -d $SOURCE_PATH ]; then
		echo "Wrong path setting!"
		usage	
		exit -2
	fi

	#check dest path
	mkdir -p $DEST_PATH
	if [ ! -d $DEST_PATH ]; then
		echo "No dest path!"
		usage
		exit -3
	fi

}


fix_prj_conf() {
	CONFIG_FILE=$1
	if [ -f ${CONFIG_FILE} ]; then
		print_out "change to release mode in ${CONFIG_FILE}"
		sed -i 's/CONFIG_CPU0_EJTAG_ENABLE=y/CONFIG_CPU0_EJTAG_ENABLE=n/g' ${CONFIG_FILE}
		sed -i 's/CONFIG_WDT_MODE_RESET=n/CONFIG_WDT_MODE_RESET=y/g' ${CONFIG_FILE}
		sed -i 's/#CONFIG_BT_CONTROLER_DEBUG_GPIO=n/CONFIG_BT_CONTROLER_DEBUG_GPIO=n/g' ${CONFIG_FILE}
		#sed -i 's/CONFIG_BT_CONTROLER_DEBUG_PRINT=y/CONFIG_BT_CONTROLER_DEBUG_PRINT=n/g' ${CONFIG_FILE}
	fi
}

fix_code() {
	if [[ "${DEBUG_MODE}" == true ]]; then
		print_out "Debug mode."
	else
		print_out "Release mode."
		# to disable debug configration in project config file.
		for prj_conf in $(find "${SOURCE_PATH}/samples/bt_speaker/app_conf" -name "prj.conf"); do
			print_out "$prj_conf"
			fix_prj_conf "$prj_conf"
		done
	fi
}

fix_code_multi_ch() {
	CH_MODE=$1
	CONFIG_FILE=${SOURCE_PATH}/samples/bt_speaker/app_conf/multi_ch/prj.conf

	if [ -f ${CONFIG_FILE} ]; then
		print_out "change channel mode in ${CONFIG_FILE} to ${CH_MODE}"

		if [ ${CH_MODE} -eq 0 ]; then
			sed -i 's/CONFIG_AUDIO_MULTI_CHANNEL_MODE=.*/CONFIG_AUDIO_MULTI_CHANNEL_MODE=0/g' ${CONFIG_FILE}
			sed -i 's/CONFIG_AUDIO_SUBWOOFER=y/CONFIG_AUDIO_SUBWOOFER=n/g' ${CONFIG_FILE}
		elif [ ${CH_MODE} -eq 1 ]; then
			sed -i 's/CONFIG_AUDIO_MULTI_CHANNEL_MODE=.*/CONFIG_AUDIO_MULTI_CHANNEL_MODE=1/g' ${CONFIG_FILE}
			sed -i 's/CONFIG_AUDIO_SUBWOOFER=n/CONFIG_AUDIO_SUBWOOFER=y/g' ${CONFIG_FILE}
		elif [ ${CH_MODE} -eq 2 ]; then
			sed -i 's/CONFIG_AUDIO_MULTI_CHANNEL_MODE=.*/CONFIG_AUDIO_MULTI_CHANNEL_MODE=2/g' ${CONFIG_FILE}
			sed -i 's/CONFIG_AUDIO_SUBWOOFER=n/CONFIG_AUDIO_SUBWOOFER=y/g' ${CONFIG_FILE}
		elif [ ${CH_MODE} -eq 3 ]; then
			sed -i 's/CONFIG_AUDIO_MULTI_CHANNEL_MODE=.*/CONFIG_AUDIO_MULTI_CHANNEL_MODE=3/g' ${CONFIG_FILE}
			sed -i 's/CONFIG_AUDIO_SUBWOOFER=n/CONFIG_AUDIO_SUBWOOFER=y/g' ${CONFIG_FILE}
		elif [ ${CH_MODE} -eq 4 ]; then
			sed -i 's/CONFIG_AUDIO_MULTI_CHANNEL_MODE=.*/CONFIG_AUDIO_MULTI_CHANNEL_MODE=4/g' ${CONFIG_FILE}
			sed -i 's/CONFIG_AUDIO_SUBWOOFER=y/CONFIG_AUDIO_SUBWOOFER=n/g' ${CONFIG_FILE}
		fi
	else
		print_out "no file ${CONFIG_FILE}"
	fi

}

make_build_config() {
	cd ${SDK_PATH}

	#remove old config
	if [ -f .build_config ]
	then
		rm .build_config
	fi

	# write varibles to config file
	echo "# Build config" > .build_config
	echo "BOARD=${target_board}" >> .build_config
	echo "APPLICATION=${target_sample}" >> .build_config
	echo "CONFIG=${target_prj}" >> .build_config

	cat .build_config

	cd -
}

build_sample() {
	cd ${SDK_PATH}

	target_board=$1
	target_sample=$2
	target_prj=$3
	prj_modifier=$4
	if [ -z "$prj_modifier" ]; then
		FIRMWARE_PATH=${DEST_PATH}/${target_sample}/${target_board}_${target_prj}
	else
		FIRMWARE_PATH=${DEST_PATH}/${target_sample}/${target_board}_${target_prj}_${prj_modifier}
	fi

	echo "Building Firmware ${FIRMWARE_PATH}"
	release_out_dir=${SDK_PATH}/samples/${target_sample}/outdir/${target_board}_${target_prj}
	if [ -d ${release_out_dir}/_firmware ]; then
		rm -rf ${release_out_dir}/_firmware
	fi

	make_build_config
	./build.sh

	#Copy result
	if [ -d ${release_out_dir}/_firmware ]; then
		mkdir -p ${FIRMWARE_PATH}
		mkdir -p ${FIRMWARE_PATH}/elf
		cp -a ${release_out_dir}/_firmware/*.fw ${FIRMWARE_PATH}/
		if [ -d ${release_out_dir}/_firmware/test_ota_fws ]; then
			cp -a ${release_out_dir}/_firmware/test_ota_fws/*.bin ${FIRMWARE_PATH}/
		fi
		if [ -f ${release_out_dir}/_firmware/ota.bin ]; then
			cp -a ${release_out_dir}/_firmware/ota.bin ${FIRMWARE_PATH}/
		fi
		cp -a ${release_out_dir}/zephyr.* ${FIRMWARE_PATH}/elf
		cp -a ${release_out_dir}/.config ${FIRMWARE_PATH}/elf
		cp -a ${release_out_dir}/include/generated/sdk_version.h ${FIRMWARE_PATH}/elf/
	fi

	cd -
}

build_firmware_spdif() {
	build_sample ats2835p2_evb bt_speaker full
}

build_firmware_multi_ch() {
	fix_code_multi_ch 0
	build_sample ats2835p2_evb bt_speaker multi_ch m0

	fix_code_multi_ch 1
	build_sample ats2835p2_evb bt_speaker multi_ch m1

	fix_code_multi_ch 2
	build_sample ats2835p2_evb bt_speaker multi_ch m2

	fix_code_multi_ch 3
	build_sample ats2835p2_evb bt_speaker multi_ch m3

	fix_code_multi_ch 4
	build_sample ats2835p2_evb bt_speaker multi_ch m4
}

build_firmware_285l() {
	build_sample ats2853p2_evb bt_speaker 285l2
	build_sample ats2853p2_evb bt_speaker 285l2_mc
	build_sample ats2853p2_evb bt_speaker 285l2_mc0
	build_sample ats2853p2_evb bt_speaker 285l2_mc_bt
	build_sample ats2853p2_evb bt_speaker 285l2_mc_sb
	build_sample ats2853p2_evb bt_speaker 285l2_mc_ota
	build_sample ats2853p2_evb bt_speaker 285l2_mc_ota_sb
	build_sample ats2853p2_evb bt_speaker 285l2_mc_ota_dae_bypass
}

build_firmware() {
	if [ ${PROJECT} == "spdif" ]; then
		build_firmware_spdif
	elif [ ${PROJECT} == "multi_ch" ]; then
		build_firmware_multi_ch
	elif [ ${PROJECT} == "285l" ]; then
		build_firmware_285l
	else
		echo "Wrong project."
		exit -10
	fi
}

main() {
	check_env

	echo "------------------------------------------------------------"
	echo "Build and make firmware..."
	build_firmware

	echo "------------------------------------------------------------"
	echo "All done ${DEST_PATH}"
}

# if not enough input argument found, exit the script with usage
if [[ ${#} -lt 2 ]]; then
	echo "Too less parameters. $*"
	usage
fi

optstring=":p:s:o:t:dv"

while getopts ${optstring} arg; do
	case ${arg} in
	p)
		PROJECT="${OPTARG}"
		;;
	s)
		SOURCE="${OPTARG}"
		;;
	o)
		DEST="${OPTARG}"
		;;
	t)
		TAG="${OPTARG}"
		;;
	d)
		DEBUG_MODE='true'
		print_out "Debug mode is ON"
		;;
	v)
		VERBOSE='true'
		;;
	h)
		usage
		;;
	?)
		echo "Invalid option: -${OPTARG}."
		echo ""
		usage
		;;
	esac
done

#update variables after updating input parameters
WORK_DIR=$(pwd)
SOURCE_PATH=${WORK_DIR}/${SOURCE}
DEST_PATH=${WORK_DIR}/${DEST}/${PROJECT}/${TAG}/
SDK_PATH=${SOURCE_PATH}

main

