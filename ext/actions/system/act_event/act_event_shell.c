#include <misc/printk.h>
#include <shell/shell.h>
#include <stdlib.h>
#include "act_event_inner.h"

#define SHELL_ACT_EVENT  "act_event"

#define ACT_EVENT_PREFIX_STR	"#LOG:"
#define ACT_EVENT_BEGIN_STR	"BEGIN#"
#define ACT_EVENT_END_STR	    "END#"
#define ACT_EVENT_ERROR_STR	"ERROR CANNOT DUMP#"

#define MAX_LINE_LENGTH_BYTES (64)
#define DEFAULT_LINE_LENGTH_BYTES (16)

extern void rom_log_output(const char *s, unsigned int len);

int act_event_dump_callback(uint8_t *data, uint32_t len)
{
    uint32_t read_len = 0;
    uint32_t str_len;

    while(read_len < len) {
        if(*data) {
            str_len = act_event_strnlen(data, len - read_len);
            //flow output,do not use printk
            rom_log_output(data, str_len);
            read_len += str_len;
            data += str_len;
        } else {
            data++;
            read_len++;
        }
    }

    k_busy_wait(10);

    return 0;
}

int cmd_print_act_event(void)
{
    int traverse_len;

    /* Print buffer */
    char print_buf[CONFIG_ACT_EVENT_LINEBUF_SIZE];

	act_event_output_flush();

    traverse_len = act_event_output_traverse(act_event_dump_callback, print_buf, sizeof(print_buf));

    return traverse_len;
}

static int cmd_print_stored_act_event(int argc, char **argv)
{
    int traverse_len;

    int file_len;

	if(!act_event_ctrl.enable){
		printk("act_event disabled\n");
		return -EIO;
	}

    printk("%s%s", ACT_EVENT_PREFIX_STR, ACT_EVENT_BEGIN_STR);

    traverse_len = cmd_print_act_event();

    printk("%s%s", ACT_EVENT_PREFIX_STR, ACT_EVENT_END_STR);

    file_len = act_event_output_get_used_size();

    if (traverse_len) {
        printk("Stored act_event printed origin len %d print len %d.", file_len, traverse_len);
    } else {
        printk("Stored act_event verification failed "
                "or there is no stored act_event.");
    }

    return 0;
}

static int cmd_erase_act_event_erase(int argc, char **argv)
{
    int ret = 0;

	if(!act_event_ctrl.enable){
		printk("act_event disabled\n");
		return -EIO;
	}

    ret = act_event_output_clear();

    if (ret == 0) {
        printk("Stored act_event erased.");
    } else {
        printk("Failed to perform erase command: %d", ret);
    }

    return 0;
}

static int cmd_query_stored_act_event(int argc, char **argv)
{
    int ret = 0;

	if(!act_event_ctrl.enable){
		printk("act_event disabled\n");
		return -EIO;
	}

	act_event_output_flush();

    ret = act_event_output_get_used_size();

    printk("Stored act_event found %d bytes.\n", ret);

    return 0;
}

static int cmd_set_level_act_event(int argc, char **argv)
{
    int ret = 0;

    if (argc < 3) {
        printk("argc err %d\n", argc);
        return -EINVAL;
    }

    uint32_t level = strtoul(argv[2], NULL, 16);

    ret = act_event_set_level_filter(argv[1], level);

    printk("Set act_event module %s level %d ret %d", argv[1], level, ret);

    return 0;
}

static int cmd_get_info_act_event(int argc, char **argv)
{
    act_event_dump_runtime_info();

    return 0;

}



#if 0
static int cmd_test_act_event(const struct shell *shell,
        int argc, char **argv)
{
    log_test();

    return 0;

}
#endif


const struct shell_cmd act_event_commands[] = {
	{ "find", cmd_query_stored_act_event, "Query if there is a stored act_event" },
	{ "erase", cmd_erase_act_event_erase, "Erase stored act_event" },
	{ "print", cmd_print_stored_act_event, "Print stored act_event to shell" },
	{ "level", cmd_set_level_act_event, "Set level filter act_event to shell" },
	//{ "num", cmd_set_num_act_event, "Set num filter act_event to shell" },
	{ "info", cmd_get_info_act_event, "Print stored act_event to shell" },
	{ NULL, NULL, NULL }
};


SHELL_REGISTER(SHELL_ACT_EVENT, act_event_commands);
