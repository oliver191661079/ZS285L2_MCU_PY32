/*******************************************************************************
 *                                      US283C
 *                            Module: usp Driver
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>    <time>           <version >             <desc>
 *       wuyufan    2023-4-27  11:39     1.0             build this file
*******************************************************************************/
/*!
 * \file     at_parser.c
 * \brief    at parser
 * \author   wuyufan
 * \par      GENERAL DESCRIPTION:
 *               function related to at parser
 * \par      EXTERNALIZED FUNCTIONS:
 *
 * \version 1.0
 * \date  2023-4-27
*******************************************************************************/
#include <ctype.h>
#include "at_parser_inner.h"

static inline bool is_terminated(char chr)
{
	if (chr == '\0') {
		return true;
	}

	return false;
}


static inline int is_lfcr(uint8_t byte)
{
	if ((byte == AT_PARSER_CR_C ) || (byte == AT_PARSER_LF_C)) {
		return true;
	}

	return false;
}

static inline int is_command(uint8_t *str, uint32_t str_len)
{
    if(str_len < 2){
        return false;
    }

    if((str[0] != AT_PARSER_CHAR_A && str[0] != AT_PARSER_char_a) || \
        (str[1] != AT_PARSER_CHAR_T && str[1] != AT_PARSER_char_t)){
        return false;
    }

	if ((str[2] == AT_PARSER_CHAR_PLUS) ||
	    (str[2] == AT_PARSER_CHAR_PERCENT) ||
	    (str[2] == AT_PARSER_CHAR_POUND) ||
	    is_lfcr(str[2]) || \
        is_terminated(str[2])) {
		return true;
	}

    return false;
}


#ifdef CONFIG_AT_PARSER_SUPPORT_URC
static const struct at_parser_urc_item *find_urc_item_in_table(struct at_parser_ctx *ctx)
{
    int i;
    const struct at_parser_urc_item *tbl = ctx->urc_table;

    for (i = 0; i < ctx->urc_table_size && tbl; i++, tbl++) {
       if (strstr(ctx->data_ptr, tbl->prefix)){
            return tbl;
       }
    }
    return NULL;
}

static int at_parser_check_urc_data(struct at_parser_ctx *ctx)
{
    struct at_parser_urc_info info;
    const struct at_parser_urc_item *item = find_urc_item_in_table(ctx);

    if(item){
        info.urc_buf = ctx->data_ptr;
        info.urc_buf_size = ctx->data_len;
        return item->handler(&info);
    }else{
        return -EINVAL;
    }
}

void at_parser_set_urc_table(struct at_parser_ctx *ctx, const struct at_parser_urc_item *urc_table, uint32_t urc_table_size)
{
	at_mutex_lock(&ctx->at_mutex, AT_TIMEOUT_FOREVER);

    ctx->urc_table = urc_table;
    ctx->urc_table_size = urc_table_size;

	at_mutex_unlock(&ctx->at_mutex);
}

#endif

static const struct at_parser_cmd_item *find_cmd_item_in_table(struct at_parser_ctx *ctx)
{
    int i;
	uint8_t name_buf[AT_PARSER_CMD_NAME_MAX_LEN];

    const struct at_parser_cmd_item *tbl = ctx->cmd_table;

	if  (ctx->cmd_name_len > AT_PARSER_CMD_NAME_MAX_LEN){
		return NULL;
	}

	for (i = 0; i < ctx->cmd_name_len; i++){
		name_buf[i] = toupper(ctx->data_ptr[i]);
	}

	name_buf[i] = '\0';

    for (i = 0; i < ctx->cmd_table_size && tbl; i++, tbl++) {
       if (strncmp(name_buf, tbl->prefix, ctx->cmd_name_len - 1) == 0){
            return tbl;
       }
    }
    return NULL;
}


static int at_parser_filter_at_mode(struct at_parser_ctx *ctx)
{
    int index;
    char *data_ptr = ctx->data_ptr;

    /* filter char "AT" */
    index = 0;
    while(index < ctx->data_len - 1) {
        if (AT_PARSER_CHAR_A == data_ptr[index] || AT_PARSER_char_a == data_ptr[index]) {
            if (AT_PARSER_CHAR_T == data_ptr[index+1] || AT_PARSER_char_t == data_ptr[index+1]) {
                break;
            }
        }
        index++;
    }
    if (index >= ctx->data_len-1) {
        return -EINVAL;
    }
    index += 2;

    /* check extend AT command: AT+, AT#, AT%, AT*, AT^, AT$ */
    if (AT_PARSER_CHAR_PLUS == data_ptr[index] || AT_PARSER_CHAR_POUND == data_ptr[index] || AT_PARSER_CHAR_PERCENT == data_ptr[index] ||
        AT_PARSER_CHAR_STAR == data_ptr[index] || AT_PARSER_HAT == data_ptr[index] || AT_PARSER_MONEY == data_ptr[index]) {
        //search = ?
        while(data_ptr[index] != AT_PARSER_EQUAL && data_ptr[index] != AT_PARSER_QUESTION_MARK \
            && !is_lfcr(data_ptr[index]) && !is_terminated(data_ptr[index])){
            index++;
        }

        ctx->cmd_name_len = index;

        if(data_ptr[index] == AT_PARSER_EQUAL){
            index++;
            if(data_ptr[index] == AT_PARSER_QUESTION_MARK){
                index++;
                ctx->cmd_mode = AT_PARSER_CMD_MODE_TEST;
            }else{
                ctx->cmd_mode = AT_PARSER_CMD_MODE_SETUP;
            }
        }else if(data_ptr[index] == AT_PARSER_QUESTION_MARK){
            index++;
            ctx->cmd_mode = AT_PARSER_CMD_MODE_QUERY;
        }else{
            ctx->cmd_mode = AT_PARSER_CMD_MODE_EXEC;
        }
    } else {
        ctx->cmd_name_len = index;
		ctx->cmd_mode = AT_PARSER_CMD_MODE_EXEC;
    }

	return 0;

}

static int at_parser_cmd_process(struct at_parser_ctx *ctx, const struct at_parser_cmd_item *item)
{
    int result;

	printk("cmd %s mode %x item %p %s %p\n", ctx->data_ptr, ctx->cmd_mode, item, item->prefix, item->test);

    if (ctx->cmd_mode == AT_PARSER_CMD_MODE_TEST){
        if (!item->test){
            at_parser_send_resp(ctx, AT_PARSER_RESP_ERR_SYNTAX);
            return -ENOEXEC;
        }
        result = item->test(ctx);

    }else if (ctx->cmd_mode == AT_PARSER_CMD_MODE_QUERY){
        if (!item->query){
            at_parser_send_resp(ctx, AT_PARSER_RESP_ERR_SYNTAX);
            return -ENOEXEC;
        }

        result = item->query(ctx);
    }else if (ctx->cmd_mode == AT_PARSER_CMD_MODE_SETUP){
        if (!item->setup){
            at_parser_send_resp(ctx, AT_PARSER_RESP_ERR_SYNTAX);
            return -ENOEXEC;
        }

        result = item->setup(ctx, (const uint8_t *)(ctx->data_ptr + ctx->cmd_name_len + 1));
    }else if (ctx->cmd_mode == AT_PARSER_CMD_MODE_EXEC){
        if (!item->exec){
            at_parser_send_resp(ctx, AT_PARSER_RESP_ERR_SYNTAX);
            return -ENOEXEC;
        }
        result = item->exec(ctx);
    }else{
        return -EINVAL;
    }

    return result;
}


int at_parser_check_cmd_data(struct at_parser_ctx *ctx)
{
    const struct at_parser_cmd_item *item;

    if(is_lfcr(ctx->data_ptr[2]) || is_terminated(ctx->data_ptr[2])){
        //only AT command
        return at_parser_send_resp(ctx, AT_PARSER_RESP_OK);
    }

    if(ctx->data_len < 4){
		at_parser_print_result(ctx, __LINE__, AT_PARSER_CMD_ERR);
        return -EINVAL;
    }

	if(at_parser_filter_at_mode(ctx) != 0){
		at_parser_print_result(ctx, __LINE__, AT_PARSER_CMD_ERR);
		return -EINVAL;
	}

	item = find_cmd_item_in_table(ctx);

	if(!item){
		at_parser_print_result(ctx, __LINE__, AT_PARSER_CMD_ERR);
		return -EINVAL;
	}

    return at_parser_cmd_process(ctx, item);
}

static void at_parser_copy_response(struct at_parser_ctx *ctx, struct at_parser_response *response)
{
	if(response->resp_buf){
	    memcpy(response->resp_buf + ctx->cur_resp_recv_len, ctx->data_ptr, ctx->data_len);
	}

	ctx->cur_resp_recv_len += ctx->data_len;
	ctx->cur_resp_line_count++;

    return;
}

static int at_parser_exact_match_response(struct at_parser_ctx *ctx, struct at_parser_response *response)
{
    char *match_str = NULL;
    uint32_t match_mask = 0;

	if(ctx->data_len > response->resp_max_len){
		response->code = AT_RESP_FULL;
		return -1;
	}

    //匹配前缀
    if(response->resp_prefix && strlen(response->resp_prefix)) {
        match_str = strstr(ctx->data_ptr, response->resp_prefix);
        if(match_str){
            match_mask |= 0x01;
        }
    }

    if(match_mask & 0x01){
        //匹配后缀
        if(response->resp_suffix && strlen(response->resp_suffix)){
            match_str = strstr(ctx->data_ptr, response->resp_suffix);
            if(match_str){
                match_mask |= 0x02;
            }
        }else{
            match_mask |= 0x02;
        }
    }

	printk("check %s match mask %x\n", ctx->data_ptr, match_mask);

    if(match_mask & 0x02){
        at_parser_copy_response(ctx, response);
		response->code = AT_RESP_OK;
        ctx->response = NULL;
		at_sem_give(&ctx->resp_sem);
        return 0;
    }

    return -1;
}
int at_parser_check_cmd_response(struct at_parser_ctx *ctx, struct at_parser_response *response)
{
    char *match_str = NULL;
    uint32_t match_mask = 0;
    printk("check resp %p %d %d\n", response, ctx->data_len, response->resp_max_len);

	if(ctx->data_len > response->resp_max_len){
		response->code = AT_RESP_FULL;
		return 0;
	}

    //匹配前缀
    if(response->resp_prefix && strlen(response->resp_prefix)) {
        match_str = strstr(ctx->data_ptr, response->resp_prefix);
        if(match_str){
            match_mask |= 0x01;
        }
    }else{
        match_mask |= 0x01;
    }

    if(match_mask & 0x01){
        //匹配后缀
        if(response->resp_suffix && strlen(response->resp_suffix)){
            match_str = strstr(ctx->data_ptr, response->resp_suffix);
            if(match_str){
                match_mask |= 0x02;
            }
        }else{
            match_mask |= 0x02;
        }

        //查看是否匹配了ERROR字符串
        match_str = strstr(ctx->data_ptr, AT_PARSER_RESP_ERR);
        if(match_str){
            match_mask |= 0x04;
        }
    }

	printk("check %s match mask %x\n", ctx->data_ptr, match_mask);

    if(match_mask & 0x04){
        at_parser_copy_response(ctx, response);
		response->code = AT_RESP_ERROR;
        ctx->response = NULL;
		at_sem_give(&ctx->resp_sem);
        return 0;
    }else if(match_mask & 0x02){
        at_parser_copy_response(ctx, response);
		response->code = AT_RESP_OK;
        ctx->response = NULL;
		at_sem_give(&ctx->resp_sem);
        return 0;
    }else{
		at_parser_copy_response(ctx, response);
		if(response->resp_line_count && ctx->cur_resp_line_count == response->resp_line_count){
			response->code = AT_RESP_OK;
            ctx->response = NULL;
			at_sem_give(&ctx->resp_sem);
		}
	}

    return 0;
}

int at_parser_process_response(struct at_parser_ctx *ctx, int exact_match)
{
    int ret = -1;
	//at_mutex_lock(&ctx->at_mutex, AT_TIMEOUT_FOREVER);

	if(ctx->response){
        if(exact_match)
            ret = at_parser_exact_match_response(ctx, ctx->response);
        else
    		ret = at_parser_check_cmd_response(ctx, ctx->response);
	}

	//at_mutex_unlock(&ctx->at_mutex);

    return ret;
}


int at_parser_check_frame_type(struct at_parser_ctx *ctx)
{
 	//1 strip first CRLF char
	while (is_lfcr(*ctx->data_ptr)) {
		ctx->data_ptr++;
        ctx->data_len--;
	}

    //2 check if is cmd frame
    if(is_command(ctx->data_ptr, ctx->data_len)){
        return at_parser_check_cmd_data(ctx);
    }

    if(at_parser_process_response(ctx, 1) == 0) {
        return 0;
    }

#ifdef CONFIG_AT_PARSER_SUPPORT_URC
    //3 check if urc frame
    if(at_parser_check_urc_data(ctx) == 0){
		return 0;
	}
#endif

    //4 check if cmd response
    return at_parser_process_response(ctx, 0);
}


int at_parser_receive_process(struct at_parser_ctx *ctx)
{
    while(1){
        ctx->data_len = at_parser_read_line(ctx, ctx->data_ptr, AT_PARSER_EXEC_CMD_MAX_LEN);

        if(!ctx->data_len){
            break;
        }

    #ifdef AT_PARSER_DUMP_RAW_DATA
        at_parser_dump_data("rawrecv:", ctx->data_ptr, ctx->data_len);
    #endif

        at_parser_check_frame_type(ctx);
    }

    return 0;
}

