/********************************************************************************
 *                            USDK(US283A_master)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018年2月13日-下午4:55:31             1.0             build this file
 ********************************************************************************/
/*!
 * \file     image.h
 * \brief
 * \author
 * \version  1.0
 * \date  2018年2月13日-下午4:55:31
 *******************************************************************************/

#ifndef PLATFORM_INCLUDE_BOOT_IMAGE_H_
#define PLATFORM_INCLUDE_BOOT_IMAGE_H_

//image头部maigc (ACTH)
#define IMAGE_MAGIC0      0x48544341

#define IMAGE_MAGIC1      0x41435448

#define IMAGE_NAME_MBREC            "MBREC"
#define IMAGE_NAME_LOADER           "LOADER"
#define IMAGE_NAME_APP              "APP"

#ifdef BOOT_SECURITY
#define BOOT_HASH_TYPE_MD5          1
#define BOOT_HASH_TYPE_SHA256       2
#define BOOT_KEY_TYPE_RSA2048       1
#define BOOT_KEY_TYPE_ECDSA192      2
#define BOOT_KEY_TYPE_ECDSA256      3

#define PREFIX_CHAR                 0xBE

#define KEY_LEN                     (4 + 256 + 256)
#define SIG_LEN                     256

typedef struct security_data {
	struct boot_hdr {
		unsigned short security;		//是否启用非对称加密
		unsigned char hash_type;//哈希类型
		unsigned char key_type;//key类型
		unsigned short key_len;//key长度
		unsigned short sig_len;//签名类型
		unsigned int reserved;
	}boot_hdr_t;
	unsigned char key[KEY_LEN];		//key内容
	unsigned char sig[SIG_LEN];//签名内容
}security_data_t;
#endif

#define MAX_VERSION_LEN            64

//校验和规则: 头部段所有数据相加，checksum必须为0xffff
//数据段所有数据相加，checksum必须为0xffff
typedef struct image_head {
	uint32_t magic0;         //两组魔数
	uint32_t magic1;
	uint32_t load_addr;      //加载地址
	uint8_t  name[4];        //映像名称
	uint16_t version;
	uint16_t header_size;
	uint16_t header_chksum;  //头部校验和
	uint16_t data_chksum;    //数据校验和
	uint32_t body_size;      //数据区大小
	uint32_t tail_size;      //尾部数据区大小
	void (*entry)(void *api, void *arg);
	uint32_t attribute;
	uint32_t priv[2];        //私有数据
} image_head_t;

//image尾部数据是非CRC的，因为需要运行过程中改写
typedef struct{
    //由下载时写入,标识当前分区的新旧关系
    uint16_t image_count;
#ifdef BOOT_SECURITY
    /* image签名信息*/
    security_data_t security;
#endif
} image_tail_t;


#endif /* PLATFORM_INCLUDE_BOOT_IMAGE_H_ */
