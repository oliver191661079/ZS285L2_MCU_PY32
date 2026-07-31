#!/bin/bash
###################################################################
# Tue Sep 16 11:04:08 CST 2025

# hanchenfeng@actioms-semi.com

#
# A script to release SDK
# Release tasks
# 	1. Checkout source code and remove private, create SDK package.
# 	2. Test firmware building, and generate firmware.
# 	3. Update(add, remove, delete) source code to "release repo".
#	4. Create readme, log, patchs, diff files.
#
###################################################################

#set -x

readonly projects=('btsp' 'btsp_285l2' 'btsp_285l2_ota' 'btsp_285l2_walmart')

#default parameters
JOB_FIRMWARE='false'
JOB_REPO='false'
SDK_NAME='sdk'
DEST='out'
TAG_BASE='ORIGIN'


function usage {
	echo "Usage: $(basename $0) <-p PROJECT> <-s DIR> <-t TAG> [-b TAG] [-o DIR] [-rfdvh]" 2>&1
	echo 'Release SDK.'
	echo '   -p PROJECT         Specify the project name.'
	for prj in ${projects[@]}; do
		echo "      $prj"
	done
	echo '   -s DIR             set the Source directory.'
	echo '   -o DIR             set the Output directory.'
	echo '   -t TAG             set the Tag name.'
	echo '   -b TAG             set the Base tag name.'
	echo '   -r                 enable the Repository'
	echo '   -f                 build Firmware'
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
	print_out "Repo:    ${REPO_PATH}"

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

	#check output repo
	if [[ "${JOB_REPO}" == true ]];then
		if [ ! -d $REPO_PATH ]; then
			echo "Wrong repo path setting!"
			usage
			exit -4
		fi
	fi
}


set_prj_conf_release_mode() {
	CONFIG_FILE=$1
	if [ -f ${CONFIG_FILE} ]; then
		print_out "change to release mode in ${CONFIG_FILE}"
		sed -i 's/CONFIG_CPU0_EJTAG_ENABLE=y/CONFIG_CPU0_EJTAG_ENABLE=n/g' ${CONFIG_FILE}
		sed -i 's/CONFIG_WDT_MODE_RESET=n/CONFIG_WDT_MODE_RESET=y/g' ${CONFIG_FILE}
		sed -i 's/CONFIG_CPU_REBOOT_IF_EXCEPTION=n/CONFIG_CPU_REBOOT_IF_EXCEPTION=y/g' ${CONFIG_FILE}
		sed -i 's/#CONFIG_BT_CONTROLER_DEBUG_GPIO=n/CONFIG_BT_CONTROLER_DEBUG_GPIO=n/g' ${CONFIG_FILE}
		sed -i 's/CONFIG_BT_CONTROLER_DEBUG_GPIO=y/CONFIG_BT_CONTROLER_DEBUG_GPIO=n/g' ${CONFIG_FILE}
		#sed -i 's/CONFIG_BT_CONTROLER_DEBUG_PRINT=y/CONFIG_BT_CONTROLER_DEBUG_PRINT=n/g' ${CONFIG_FILE}
	fi
}

fix_code() {
	if [[ "${DEBUG_MODE}" == true ]]; then
		print_out "Debug mode."
	else
		print_out "Release mode."
		# to disable debug configration in project config file.
		for prj_conf in $(find "${DEST_PATH}/${SDK_NAME}/samples/bt_speaker/app_conf" -name "prj.conf"); do
			print_out "$prj_conf"
			set_prj_conf_release_mode "$prj_conf"
		done
	fi
}

set_sdk_files_filter() {
	# SDK exclude and include
	EXCLUDE=""
	INCLUDE=""

	#general code
	EXCLUDE+="--exclude=.* \
		--exclude=.git/ \
		--exclude=.git* \
		--exclude=.svn \
		--exclude=.vscode \
		--exclude=.build_config \
		--exclude=exclude.txt \
		--exclude=release*.sh \
		--exclude=clean_*.sh \
		--exclude=make_release*.sh \
		--exclude=outdir/ \
		--exclude=doc/ "

	#test code
	EXCLUDE+="--exclude=tests/*/ "
	INCLUDE+="--include=tests/ztest/ "

	#scripts
	EXCLUDE+="--exclude=scripts/support/actions/prebuilt/rocky "
	EXCLUDE+="--exclude=scripts/support/actions/prebuilt/andes "
	if [ ${PROJECT} == "btsp_285l2" ] || [ ${PROJECT} == "btsp_285l2_ota" ] || [ ${PROJECT} == "btsp_285l2_walmart" ]; then
		EXCLUDE+="--exclude=scripts/support/actions/prebuilt/andesc/ats287*/ "
		EXCLUDE+="--exclude=scripts/support/actions/prebuilt/andesc/ats283*/ "
	fi
	if [ ${PROJECT} == "btsp" ]; then
		EXCLUDE+="--exclude=scripts/support/actions/prebuilt/andesc/ats287*/ "
		EXCLUDE+="--exclude=scripts/support/actions/prebuilt/andesc/ats285*/ "
		EXCLUDE+="--exclude=scripts/support/actions/prebuilt/andesc/ats2831p/ "
		EXCLUDE+="--exclude=scripts/support/actions/prebuilt/andesc/ats2835p/ "
	fi

	#arch
	INCLUDE+="--include=include/arch/csky/ "
	EXCLUDE+="--exclude=arch/*/ "
	EXCLUDE+="--exclude=arch/csky/soc/actions/andes/ "
	EXCLUDE+="--exclude=arch/csky/soc/actions/andesc/private/librom_code/ "
	INCLUDE+="--include=arch/csky/soc/actions/andesc/ "
	INCLUDE+="--include=arch/csky/ "
	INCLUDE+="--include=arch/common/ "
	if [ ${PROJECT} == "btsp_285l2" ] || [ ${PROJECT} == "btsp_285l2_ota" ] || [ ${PROJECT} == "btsp_285l2_walmart" ]; then
		EXCLUDE+="--exclude=arch/csky/soc/actions/andesc/Kconfig.defconfig.ats287* "
		EXCLUDE+="--exclude=arch/csky/soc/actions/andesc/Kconfig.defconfig.ats283* "
		EXCLUDE+="--exclude=arch/csky/soc/actions/andesc/Kconfig.soc.ats287* "
		EXCLUDE+="--exclude=arch/csky/soc/actions/andesc/Kconfig.soc.ats283* "
	fi
	if [ ${PROJECT} == "btsp" ]; then
		EXCLUDE+="--exclude=arch/csky/soc/actions/andesc/Kconfig.defconfig.ats287* "
		EXCLUDE+="--exclude=arch/csky/soc/actions/andesc/Kconfig.defconfig.ats285* "
		EXCLUDE+="--exclude=arch/csky/soc/actions/andesc/Kconfig.defconfig.ats2831p "
		EXCLUDE+="--exclude=arch/csky/soc/actions/andesc/Kconfig.defconfig.ats2835p "
		EXCLUDE+="--exclude=arch/csky/soc/actions/andesc/Kconfig.soc.ats287* "
		EXCLUDE+="--exclude=arch/csky/soc/actions/andesc/Kconfig.soc.ats285* "
		EXCLUDE+="--exclude=arch/csky/soc/actions/andesc/Kconfig.soc.ats2831p "
		EXCLUDE+="--exclude=arch/csky/soc/actions/andesc/Kconfig.soc.ats2835p "
	fi

	#boards
	EXCLUDE+="--exclude=boards/*/ "
	EXCLUDE+="--exclude=boards/csky/*/ "
	INCLUDE+="--include=boards/csky/ "

	#special boards
	if [ ${PROJECT} == "btsp" ]; then
		INCLUDE+="--include=boards/csky/ats2835p2_evb/ "
	fi

	if [ ${PROJECT} == "btsp_285l2" ] || [ ${PROJECT} == "btsp_285l2_ota" ] || [ ${PROJECT} == "btsp_285l2_walmart" ]; then
		INCLUDE+="--include=boards/csky/ats2853p2_evb/ "
	fi

	#ext code
	EXCLUDE+="--exclude=ext/hal/*/ "
	INCLUDE+="--include=ext/hal/csky/ "
	EXCLUDE+="--exclude=ext/actions/mp "

	#bluetooth code
	EXCLUDE+="--exclude=ext/actions/bluetooth/bt_stack/libbt_stack "
	EXCLUDE+="--exclude=ext/actions/bluetooth/bt_service/libbt_service "

	#libmedia code
	EXCLUDE+="--exclude=ext/actions/media/libmedia "

	#samples
	EXCLUDE+="--exclude=samples/* " 
	if [ ${PROJECT} == "btsp" ]; then
		INCLUDE+="--include=samples/bt_speaker/ "
		EXCLUDE+="--exclude=samples/bt_speaker/app_conf/* "
		INCLUDE+="--include=samples/bt_speaker/app_conf/full/ "
		INCLUDE+="--include=samples/bt_speaker/app_conf/full_nocis/ "
		INCLUDE+="--include=samples/bt_speaker/app_conf/multi_ch/ "
		INCLUDE+="--include=samples/bt_speaker/app_conf/pts/ "
		EXCLUDE+="--exclude=samples/bt_speaker/app_conf/*/sdfs/adLDAC.dsp "
		EXCLUDE+="--exclude=samples/bt_speaker/src/selfapp "
	elif [ ${PROJECT} == "btsp_285l2" ]; then
		INCLUDE+="--include=samples/bt_speaker/ "
		EXCLUDE+="--exclude=samples/bt_speaker/app_conf/* "
		INCLUDE+="--include=samples/bt_speaker/app_conf/285l2_mc/ "
		INCLUDE+="--include=samples/bt_speaker/app_conf/285l2_mc0/ "
		INCLUDE+="--include=samples/bt_speaker/app_conf/285l2_mc_bt/ "
		INCLUDE+="--include=samples/bt_speaker/app_conf/285l2_mc_sb/ "
		INCLUDE+="--include=samples/bt_speaker/app_conf/pts/ "
		EXCLUDE+="--exclude=samples/bt_speaker/app_conf/*/sdfs/adLDAC.dsp "
		EXCLUDE+="--exclude=samples/bt_speaker/src/selfapp "
	elif [ ${PROJECT} == "btsp_285l2_ota" ]; then
		INCLUDE+="--include=samples/bt_speaker/ "
		EXCLUDE+="--exclude=samples/bt_speaker/app_conf/* "
		INCLUDE+="--include=samples/bt_speaker/app_conf/285l2_mc/ "
		INCLUDE+="--include=samples/bt_speaker/app_conf/285l2_mc0/ "
		INCLUDE+="--include=samples/bt_speaker/app_conf/285l2_mc_bt/ "
		INCLUDE+="--include=samples/bt_speaker/app_conf/285l2_mc_sb/ "
		INCLUDE+="--include=samples/bt_speaker/app_conf/285l2_mc_ota/ "
		INCLUDE+="--include=samples/bt_speaker/app_conf/285l2_mc_ota_sb/ "
		INCLUDE+="--include=samples/bt_speaker/app_conf/285l2_mc_ota_dae_bypass/ "
		INCLUDE+="--include=samples/bt_speaker/app_conf/pts/ "
		EXCLUDE+="--exclude=samples/bt_speaker/app_conf/*/sdfs/adLDAC.dsp "
		EXCLUDE+="--exclude=samples/bt_speaker/src/selfapp "
	elif [ ${PROJECT} == "btsp_285l2_walmart" ]; then
		INCLUDE+="--include=samples/bt_speaker/ "
		EXCLUDE+="--exclude=samples/bt_speaker/app_conf/* "
		INCLUDE+="--include=samples/bt_speaker/app_conf/285l2/ "
	fi
}

make_sdk() {
	set_sdk_files_filter

	#sync sdk files
	cd ${SOURCE_PATH}
	git checkout .
	git clean -dfx
	rsync -a --relative ${INCLUDE} ${EXCLUDE} . ${SDK_PATH}
	cd -

	# list all sdk files
	# cd ${SDK_PATH}
	# find . | sort > ${DEST_PATH}/list.txt
	# cd -

	# Fix code for release
	fix_code

	#tar sdk files
	if [ -d ${DEST_PATH}/${SDK_NAME} ]; then
		cd ${DEST_PATH}
		tar -czf ${SDK_NAME}.tgz ${SDK_NAME}
		cd -
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

	FIRMWARE_PATH=${DEST_PATH}/${target_sample}/${target_board}_${target_prj}
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

build_firmware_btsp() {
	build_sample ats2835p2_evb bt_speaker full
	build_sample ats2835p2_evb bt_speaker full_nocis
	build_sample ats2835p2_evb bt_speaker multi_ch
	build_sample ats2835p2_evb bt_speaker pts 
}

build_firmware_btsp_285l2() {
	build_sample ats2853p2_evb bt_speaker 285l2_mc
	build_sample ats2853p2_evb bt_speaker 285l2_mc0
	build_sample ats2853p2_evb bt_speaker 285l2_mc_bt
	build_sample ats2853p2_evb bt_speaker 285l2_mc_sb
	build_sample ats2853p2_evb bt_speaker pts
}

build_firmware_btsp_285l2_ota() {
	build_sample ats2853p2_evb bt_speaker 285l2_mc
	build_sample ats2853p2_evb bt_speaker 285l2_mc0
	build_sample ats2853p2_evb bt_speaker 285l2_mc_bt
	build_sample ats2853p2_evb bt_speaker 285l2_mc_sb
	build_sample ats2853p2_evb bt_speaker 285l2_mc_ota
	build_sample ats2853p2_evb bt_speaker 285l2_mc_ota_sb
	build_sample ats2853p2_evb bt_speaker 285l2_mc_ota_dae_bypass
	build_sample ats2853p2_evb bt_speaker pts
}


build_firmware_btsp_285l2_walmart() {
	build_sample ats2853p2_evb bt_speaker 285l2
}

build_firmware() {
	if [ ${PROJECT} == "btsp" ]; then
		build_firmware_btsp
	elif [ ${PROJECT} == "btsp_285l2" ]; then
		build_firmware_btsp_285l2
	elif [ ${PROJECT} == "btsp_285l2_ota" ]; then
		build_firmware_btsp_285l2_ota
	elif [ ${PROJECT} == "btsp_285l2_walmart" ]; then
		build_firmware_btsp_285l2_walmart
	else
		echo "Wrong project."
		exit -10
	fi
}

create_log() {
	cd ${SOURCE_PATH}
	if [ ${TAG_BASE} == "ORIGIN" ]; then
		git log --name-status > ${DEST_PATH}/log.txt
 	else
		COMMIT_BASE=$(git show --oneline ${TAG_BASE}^{commit} | head -1 | awk '{print $1;}')
		COMMIT=$(git show --oneline ${TAG}^{commit} | head -1 | awk '{print $1;}')
		git log --name-status ${COMMIT_BASE}..${COMMIT} > ${DEST_PATH}/log.txt
	fi

	cd -
}

update_sdk_to_repo() {
	cd $DEST_PATH
	rm -rf ${SDK_NAME}
	tar -xf ${SDK_NAME}.tgz
	cd -

	cd ${SDK_PATH}
	rsync -a --relative --exclude=".git/" --delete . ${REPO_PATH}
	cd -

	rm -rf ${DEST_PATH}/${SDK_NAME}

	cd ${REPO_PATH}
	git add .
	git commit -q -m "Update sdk to ${TAG}"
	cd -

	if [ ! ${TAG_BASE} == "ORIGIN" ]; then
		cd ${REPO_PATH}
		#create patch
		git diff --binary HEAD^1 HEAD > ${DEST_PATH}/diff.patch

		#create diff files
		git log --name-status -n 1 >> ${DEST_PATH}/diff_files.txt
		cat ${DEST_PATH}/diff_files.txt | awk '/^A\t|^M\t|^C\t|^T\t|^R\t/' | awk {'print $2;}' > ${DEST_PATH}/diff_files_am.txt
		mkdir -p ${DEST_PATH}/diff_files/
		rsync --files-from=${DEST_PATH}/diff_files_am.txt . ${DEST_PATH}/diff_files/
		cd -

		cd ${DEST_PATH}
		tar -czf diff_files.tgz diff_files
		rm -rf diff_files diff_files.txt diff_files_am.txt
		cd -
	fi
}

create_readme() {
	touch ${DEST_PATH}/readme.txt

	#Tag
	echo "${TAG}" >> ${DEST_PATH}/readme.txt
	echo "based on ${TAG_BASE}" >> ${DEST_PATH}/readme.txt

	#Revision
	echo "" >> ${DEST_PATH}/readme.txt
	echo "Revision" >> ${DEST_PATH}/readme.txt
	echo "---------------------------------------------" >> ${DEST_PATH}/readme.txt

	cd ${SOURCE_PATH}
	git show --oneline -s HEAD >> ${DEST_PATH}/readme.txt
	cd -

	#Get md5 checksum
	echo "" >> ${DEST_PATH}/readme.txt
	echo "md5checksum" >> ${DEST_PATH}/readme.txt
	echo "---------------------------------------------" >> ${DEST_PATH}/readme.txt
	cd ${DEST_PATH}
	if [ -f sdk.tgz ]; then
		md5sum sdk.tgz >> readme.txt
	fi
	if [ -f diff.patch ]; then
		md5sum diff* >> readme.txt
	fi
	cd -
}

main() {
	check_env

	echo "------------------------------------------------------------"
	echo "Make SDK..."
	make_sdk

	if [[ "${JOB_FIRMWARE}" == true ]];then
		echo "------------------------------------------------------------"
		echo "Build and make firmware..."
		build_firmware
	fi

	cd $WORK_DIR
	rm -rf ${SDK_PATH}

	if [[ "${JOB_REPO}" == true ]];then
		echo "------------------------------------------------------------"
		echo "Update SDK to repo, create log, patchs, and diffs..."
		cd $WORK_DIR
		create_log
		update_sdk_to_repo
	fi

	create_readme

	echo "------------------------------------------------------------"
	echo "All done ${DEST_PATH}"
	if [[ "${JOB_REPO}" == true ]];then
		echo "Remember to manually modify readme.txt for md5 checksum."
	fi	
}

# if not enough input argument found, exit the script with usage
if [[ ${#} -lt 2 ]]; then
	echo "Too less parameters. $*"
	usage
fi

optstring=":p:s:o:t:b:rfdv"

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
	b)
		TAG_BASE="${OPTARG}"
		;;
	f)
		JOB_FIRMWARE='true'
		;;
	r)
		JOB_REPO='true'
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
SDK_PATH=${DEST_PATH}/${SDK_NAME}/
REPO_PATH=${WORK_DIR}/repo/${PROJECT}

main

