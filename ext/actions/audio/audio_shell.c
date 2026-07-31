/*
 * Copyright (c) 2024 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief audio shell
*/
#if defined(CONFIG_SYS_LOG)
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "audio"
#endif

#include <shell/shell.h>
#include <init.h>
#include <stdlib.h>
#include <string.h>

#include <audio_system.h>

#define AUDIO_MODEL_VERSION "1.0.0"

static int shell_cmd_version(int argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	printk("Audio module version: %s\n", AUDIO_MODEL_VERSION);
	return 0;
}

static int shell_cmd_dump_records(int argc, char *argv[])
{
	extern void audio_system_dump_record(struct audio_record_t * record);
	struct audio_record_t *record;

	record = audio_system_get_record();
	audio_system_dump_record(record);

	return 0;
}

static int shell_cmd_dump_tracks(int argc, char *argv[])
{
	void audio_system_dump_track(struct audio_track_t * track);
	struct audio_track_t *track;

	track = audio_system_get_track();
	audio_system_dump_track(track);
	return 0;
}

static int shell_cmd_dump_system(int argc, char *argv[])
{
	extern void audio_system_dump_data(void);
	audio_system_dump_data();

	return 0;
}

const struct shell_cmd audio_shell_commands[] = {
	{"version", shell_cmd_version, "show version of audio module"},
	{"dumprecord", shell_cmd_dump_records, "dump audio records"},
	{"dumptrack", shell_cmd_dump_tracks, "dump audio tracks"},
	{"dumpsystem", shell_cmd_dump_system, "dump audio system"},
	{NULL, NULL, NULL}
};

SHELL_REGISTER("audio", audio_shell_commands);
