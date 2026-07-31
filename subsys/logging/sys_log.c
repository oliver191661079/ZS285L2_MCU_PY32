/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <misc/printk.h>
#include <logging/sys_log.h>

int syslog_log_level = CONFIG_SYS_USER_LOG_DEFAULT_LEVEL;

void syslog_set_log_level(int log_level)
{
	syslog_log_level = log_level;
}

void syslog_hook_default(const char *fmt, ...)
{
	(void)(fmt);  /* Prevent warning about unused argument */
}

void (*syslog_hook)(const char *fmt, ...) = (void(*)(const char *fmt, ...))printk;

void syslog_hook_install(void (*hook)(const char *, ...))
{
	syslog_hook = hook;
}

#ifdef CONFIG_ACTION_LOG_FRAME
extern void trace_output_data(const uint8_t *buf, size_t len);

typedef struct
{
	uint8_t tag;
	uint8_t version;
	uint8_t len;
	uint32_t seq;
	uint32_t time_stamp;
	uint8_t level;
	uint16_t model;
	uint16_t line;
	uint8_t type;
	uint8_t checksum;
}__packed ALF_HEADER;

static uint8_t chksum8(const unsigned char *buff, size_t len)
{
    unsigned int sum = 0;

    while(len != 0){
        sum += *(buff++);
	len--;
    }
    return (uint8_t)sum;
}

static void alf_pack(uint8_t level, uint16_t model, uint32_t line, uint8_t type, const uint8_t* data, size_t len)
{
	static uint32_t seq=0;

	//buffer on thread stack to support multi-thread calling.
	uint8_t log_buffer[sizeof(ALF_HEADER) + CONFIG_ACTION_LOG_FRAME_BODY_SIZE];
	ALF_HEADER h;

	if (len > CONFIG_ACTION_LOG_FRAME_BODY_SIZE ){
		len = CONFIG_ACTION_LOG_FRAME_BODY_SIZE;
	}

	h.tag = 0x10; //DATA LINK ESCAPE
	h.version = 0x01;
	h.len = len;
	h.seq = seq++;
	h.time_stamp = k_uptime_get_32();
	h.level = level;
	h.model = model;
	h.line = line;
	h.type = type;
	if(len > 0) {
		h.checksum = (uint8_t)(chksum8((const unsigned char*)&h, sizeof(ALF_HEADER) - 1) + chksum8(data, len));
	} else {
		h.checksum = (uint8_t)(chksum8((const unsigned char*)&h, sizeof(ALF_HEADER) - 1) );
	}


	log_buffer[0] = h.tag;
	log_buffer[1] = h.version;
	log_buffer[2] = h.len;
	log_buffer[3 + 0] = (h.seq >> 24) & 0xFF;
	log_buffer[3 + 1] = (h.seq >> 16) & 0xFF;
	log_buffer[3 + 2] = (h.seq >> 8) & 0xFF;
	log_buffer[3 + 3] = (h.seq ) & 0xFF;
	log_buffer[7 + 0] = (h.time_stamp >> 24) & 0xFF;
	log_buffer[7 + 1] = (h.time_stamp >> 16) & 0xFF;
	log_buffer[7 + 2] = (h.time_stamp >> 8) & 0xFF;
	log_buffer[7 + 3] = (h.time_stamp) & 0xFF;
	log_buffer[11] = h.level;
	log_buffer[12 + 0] = (h.model>> 8) & 0xFF;
	log_buffer[12 + 1] = (h.model) & 0xFF;
	log_buffer[14 + 0] = (h.line >> 8) & 0xFF;
	log_buffer[14 + 1] = (h.line) & 0xFF;
	log_buffer[16] = h.type;
	log_buffer[17] = h.checksum;

	memcpy(log_buffer+sizeof(ALF_HEADER), data, len);
	trace_output_data(log_buffer, sizeof(ALF_HEADER) + len);
}

void alf_print_id(uint8_t level, uint16_t model, uint32_t line, uint32_t id, uint8_t num, ...)
{
	va_list valist;
	int i;
	uint8_t body[4+ALF_MAX_PARAM*4];
	uint32_t val;

	if (level > syslog_log_level) {
		return;
	}

	/* initialize valist for num number of arguments */
	va_start(valist, num);

	body[0] = (id >> 24) & 0xFF;
	body[1] = (id >> 16) & 0xFF;
	body[2] = (id >> 8) & 0xFF;
	body[3] = id & 0xFF;

	if(num > ALF_MAX_PARAM ){
		num = ALF_MAX_PARAM ;
	}

	for (i=0; i<num; i++) {
		val = va_arg(valist, int);
		body[4 + i*4] = (val >> 24) & 0xFF;
		body[4 + i*4+1] = (val >> 16) & 0xFF;
		body[4 + i*4+2] = (val >> 8) & 0xFF;
		body[4 + i*4+3] = val & 0xFF;
	}

	/* clean memory reserved for valist */
	va_end(valist);

	alf_pack(level, model, line, 1, body, 4 + num*4);
}

void alf_print_str(uint8_t level, uint16_t model, uint32_t line, const char* str)
{
	if (level > syslog_log_level) {
		return;
	}
	alf_pack(level, model, line, 0, str, strlen(str));
}
#endif
