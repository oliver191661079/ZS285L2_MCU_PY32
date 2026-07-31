/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** @file sys_log.h
 *  @brief Logging macros.
 */
#ifndef __SYS_LOG_H
#define __SYS_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_ACTION_LOG_FRAME
#include <kernel.h>
#include <log_str_id.h>
#include <log_model_id.h>

#define ALF_MAX_PARAM 9

void alf_print_str(uint8_t level, uint16_t model, uint32_t line, const char* str);
void alf_print_id(uint8_t level, uint16_t model, uint32_t line, uint32_t id, uint8_t num, ...);

#define ACT_LOG_STR_ERR(str) alf_print_str(1, ACT_LOG_MODEL_ID, __LINE__, str);
#define ACT_LOG_STR_WRN(str) alf_print_str(2, ACT_LOG_MODEL_ID, __LINE__, str);
#define ACT_LOG_STR_INF(str) alf_print_str(3, ACT_LOG_MODEL_ID, __LINE__, str);
#define ACT_LOG_STR_DBG(str) alf_print_str(4, ACT_LOG_MODEL_ID, __LINE__, str);

#define ACT_LOG_ID_ERR(strid, num, ...) alf_print_id(1, ACT_LOG_MODEL_ID, __LINE__, strid | (ALF_STR_SPACE<<16), num, ##__VA_ARGS__);
#define ACT_LOG_ID_WRN(strid, num, ...) alf_print_id(2, ACT_LOG_MODEL_ID, __LINE__, strid | (ALF_STR_SPACE<<16), num, ##__VA_ARGS__);
#define ACT_LOG_ID_INF(strid, num, ...) alf_print_id(3, ACT_LOG_MODEL_ID, __LINE__, strid | (ALF_STR_SPACE<<16), num, ##__VA_ARGS__);
#define ACT_LOG_ID_DBG(strid, num, ...) alf_print_id(4, ACT_LOG_MODEL_ID, __LINE__, strid | (ALF_STR_SPACE<<16), num, ##__VA_ARGS__);

#else

#define ACT_LOG_STR_ERR(str) do{}while(0)
#define ACT_LOG_STR_WRN(str) do{}while(0)
#define ACT_LOG_STR_INF(str) do{}while(0)
#define ACT_LOG_STR_DBG(str) do{}while(0)

#define ACT_LOG_ID_ERR(...) do{}while(0)
#define ACT_LOG_ID_WRN(...) do{}while(0)
#define ACT_LOG_ID_INF(...) do{}while(0)
#define ACT_LOG_ID_DBG(...) do{}while(0)
#endif


#define SYS_LOG_LEVEL_OFF 0
#define SYS_LOG_LEVEL_ERROR 1
#define SYS_LOG_LEVEL_WARNING 2
#define SYS_LOG_LEVEL_INFO 3
#define SYS_LOG_LEVEL_DEBUG 4

/* Determine this compile unit log level */
#if !defined(SYS_LOG_LEVEL)
/* Use default */
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_DEFAULT_LEVEL
#elif (SYS_LOG_LEVEL < CONFIG_SYS_LOG_OVERRIDE_LEVEL)
/* Use override */
#undef SYS_LOG_LEVEL
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_OVERRIDE_LEVEL
#endif

#if defined(CONFIG_SYS_LOG)
void syslog_set_log_level(int log_level);
#endif

/**
 * @brief System Log
 * @defgroup system_log System Log
 * @{
 */
#if defined(CONFIG_SYS_LOG) && (SYS_LOG_LEVEL > SYS_LOG_LEVEL_OFF)

#define IS_SYS_LOG_ACTIVE 1

extern void (*syslog_hook)(const char *fmt, ...);
void syslog_hook_install(void (*hook)(const char *, ...));

extern int syslog_log_level;

/* decide print func */
#if defined(CONFIG_SYS_LOG_EXT_HOOK)
#define SYS_LOG_BACKEND_FN syslog_hook
#else
#include <misc/printk.h>
#define SYS_LOG_BACKEND_FN printk
#endif

/* Should use color? */
#if defined(CONFIG_SYS_LOG_SHOW_COLOR)
#define SYS_LOG_COLOR_OFF     "\x1B[0m"
#define SYS_LOG_COLOR_RED     "\x1B[0;31m"
#define SYS_LOG_COLOR_YELLOW  "\x1B[0;33m"
#else
#define SYS_LOG_COLOR_OFF     ""
#define SYS_LOG_COLOR_RED     ""
#define SYS_LOG_COLOR_YELLOW  ""
#endif /* CONFIG_SYS_LOG_SHOW_COLOR */

/* Should use log lv tags? */
#if defined(CONFIG_SYS_LOG_SHOW_TAGS)
#define SYS_LOG_TAG_ERR " [ERR]"
#define SYS_LOG_TAG_WRN " [WRN]"
#define SYS_LOG_TAG_INF " [INF]"
#define SYS_LOG_TAG_DBG " [DBG]"
#else
#define SYS_LOG_TAG_ERR ""
#define SYS_LOG_TAG_WRN ""
#define SYS_LOG_TAG_INF ""
#define SYS_LOG_TAG_DBG ""
#endif /* CONFIG_SYS_LOG_SHOW_TAGS */

/* Log domain name */
#if !defined(SYS_LOG_DOMAIN)
#define SYS_LOG_DOMAIN "general"
#endif /* SYS_LOG_DOMAIN */

/**
 * @def SYS_LOG_NO_NEWLINE
 *
 * @brief Specifies whether SYS_LOG should add newline at the end of line
 * or not.
 *
 * @details User can define SYS_LOG_NO_NEWLINE to prevent the header file
 * from adding newline if the debug print already has a newline character.
 */
#if !defined(SYS_LOG_NO_NEWLINE)
#define SYS_LOG_NL "\n"
#else
#define SYS_LOG_NL ""
#endif

/* [domain] [level] function: */
#define LOG_LAYOUT "[%s]%s %s: %s"
#define LOG_BACKEND_CALL(level, log_lv, log_color, log_format, color_off, ...)	\
	do {	\
		if(syslog_log_level >= level) \
			SYS_LOG_BACKEND_FN(LOG_LAYOUT log_format "%s" SYS_LOG_NL,	\
			SYS_LOG_DOMAIN, log_lv, __func__, log_color, ##__VA_ARGS__, color_off); \
	} while (0)

#ifdef CONFIG_USE_ROM_LOG
#define LOG_NANO_BACKEND_CALL(module, level, log_lv, func, str, num, ...) \
	do {	\
		if(syslog_log_level >= level) \
			log_nano(module, log_lv, func, str, num, ##__VA_ARGS__); \
	} while (0)
#else
#define LOG_NANO_BACKEND_CALL(module, level, log_lv, func, str, num, ...)
#endif

#define LOG_NO_COLOR(level, log_lv, log_format, ...)				\
	LOG_BACKEND_CALL(level, log_lv, "", log_format, "", ##__VA_ARGS__)
#define LOG_COLOR(level, log_lv, log_color, log_format, ...)			\
	LOG_BACKEND_CALL(level, log_lv, log_color, log_format,			\
	SYS_LOG_COLOR_OFF, ##__VA_ARGS__)

#define SYS_LOG_ERR(...) LOG_COLOR(SYS_LOG_LEVEL_ERROR,SYS_LOG_TAG_ERR, SYS_LOG_COLOR_RED,	\
	##__VA_ARGS__)

#if (SYS_LOG_LEVEL >= SYS_LOG_LEVEL_WARNING)
#define SYS_LOG_WRN(...) LOG_COLOR(SYS_LOG_LEVEL_WARNING,SYS_LOG_TAG_WRN,			\
	SYS_LOG_COLOR_YELLOW, ##__VA_ARGS__)

#define LOGN_HEX_WRN(module, str, num, ...)					  \
	LOG_NANO_BACKEND_CALL(module, SYS_LOG_LEVEL_WARNING, SYS_LOG_TAG_WRN, NULL, str, num, "x", ##__VA_ARGS__);		       \

#define LOGN_DEC_WRN(module, str, num, ...)					  \
	LOG_NANO_BACKEND_CALL(module, SYS_LOG_LEVEL_WARNING, SYS_LOG_TAG_WRN, NULL, str, num, "d", ##__VA_ARGS__);			   \

#define LOGN_UDEC_WRN(module, str, num, ...)					  \
	LOG_NANO_BACKEND_CALL(module, SYS_LOG_LEVEL_WARNING, SYS_LOG_TAG_WRN, NULL, str, num, "u", ##__VA_ARGS__);			   \

#endif

#if (SYS_LOG_LEVEL >= SYS_LOG_LEVEL_INFO)
#define SYS_LOG_INF(...) LOG_NO_COLOR(SYS_LOG_LEVEL_INFO,SYS_LOG_TAG_INF, ##__VA_ARGS__)

#define LOGN_HEX_INF(module, str, num, ...)					  \
	LOG_NANO_BACKEND_CALL(module, SYS_LOG_LEVEL_INFO, SYS_LOG_TAG_INF, NULL, str, num, "x", ##__VA_ARGS__);		       \

#define LOGN_DEC_INF(module, str, num, ...)					  \
	LOG_NANO_BACKEND_CALL(module, SYS_LOG_LEVEL_INFO, SYS_LOG_TAG_INF, NULL, str, num, "d", ##__VA_ARGS__);			   \

#define LOGN_UDEC_INF(module, str, num, ...)					  \
	LOG_NANO_BACKEND_CALL(module, SYS_LOG_LEVEL_INFO, SYS_LOG_TAG_INF, NULL, str, num, "u", ##__VA_ARGS__);			   \

#endif

#if (SYS_LOG_LEVEL == SYS_LOG_LEVEL_DEBUG)
#define SYS_LOG_DBG(...) LOG_NO_COLOR(SYS_LOG_LEVEL_DEBUG,SYS_LOG_TAG_DBG, ##__VA_ARGS__)

#define LOGN_HEX_DBG(module, str, num, ...)					  \
	LOG_NANO_BACKEND_CALL(module, SYS_LOG_LEVEL_DEBUG, SYS_LOG_TAG_DBG, NULL, str, num, "x", ##__VA_ARGS__);		       \

#define LOGN_DEC_DBG(module, str, num, ...)					  \
	LOG_NANO_BACKEND_CALL(module, SYS_LOG_LEVEL_DEBUG, SYS_LOG_TAG_DBG, NULL, str, num, "d", ##__VA_ARGS__);			   \

#define LOGN_UDEC_DBG(module, str, num, ...)					  \
	LOG_NANO_BACKEND_CALL(module, SYS_LOG_LEVEL_DEBUG, SYS_LOG_TAG_DBG, NULL, str, num, "u", ##__VA_ARGS__);			   \

#endif

#define LOGN_HEX(module, str, num, ...) \
	LOG_NANO_BACKEND_CALL(module, SYS_LOG_LEVEL_INFO, NULL, NULL, str, num, "x", ##__VA_ARGS__);			   \

#else
/**
 * @def IS_SYS_LOG_ACTIVE
 *
 * @brief Specifies whether SYS_LOG is in use or not.
 *
 * @details This macro expands to 1 if SYS_LOG was activated for current .c
 * file, 0 otherwise.
 */
#define IS_SYS_LOG_ACTIVE 0
/**
 * @def SYS_LOG_ERR
 *
 * @brief Writes an ERROR level message to the log.
 *
 * @details Lowest logging level, these messages are logged whenever sys log is
 * active. it's meant to report severe errors, such as those from which it's
 * not possible to recover.
 *
 * @param ... A string optionally containing printk valid conversion specifier,
 * followed by as many values as specifiers.
 */
#define SYS_LOG_ERR(...) { ; }
#endif /* CONFIG_SYS_LOG */

/* create dummy macros */
#if !defined(SYS_LOG_WRN)
/**
 * @def SYS_LOG_WRN
 *
 * @brief Writes a WARNING level message to the log.
 *
 * @details available if SYS_LOG_LEVEL is SYS_LOG_LEVEL_WARNING or higher.
 * It's meant to register messages related to unusual situations that are
 * not necessarily errors.
 *
 * @param ... A string optionally containing printk valid conversion specifier,
 * followed by as many values as specifiers.
 */
#define SYS_LOG_WRN(...) { ; }
#endif

#if !defined(SYS_LOG_INF)
/**
 * @def SYS_LOG_INF
 *
 * @brief Writes an INFO level message to the log.
 *
 * @details available if SYS_LOG_LEVEL is SYS_LOG_LEVEL_INFO or higher.
 * It's meant to write generic user oriented messages.
 *
 * @param ... A string optionally containing printk valid conversion specifier,
 * followed by as many values as specifiers.
 */
#define SYS_LOG_INF(...) { ; }
#endif

#if !defined(SYS_LOG_DBG)
/**
 * @def SYS_LOG_DBG
 *
 * @brief Writes a DEBUG level message to the log.
 *
 * @details highest logging level, available if SYS_LOG_LEVEL is
 * SYS_LOG_LEVEL_DEBUG. It's meant to write developer oriented information.
 *
 * @param ... A string optionally containing printk valid conversion specifier,
 * followed by as many values as specifiers.
 */
#define SYS_LOG_DBG(...) { ; }
#endif

#include <logging/log_core.h>

#ifdef CONFIG_ACT_EVENT
#include <act_event.h>
#endif
/**
 * @brief Writes a ERROR level event to the event log buffer.
 *
 */
#ifndef CONFIG_ACT_EVENT
#define SYS_EVENT_ERR(...)
#else
#define SYS_EVENT_ERR(event, ...) ACT_EVENT_SEND(ACT_EVENT_LEVEL_ERROR, event, ##__VA_ARGS__)
#endif


/**
 * @brief Writes a WARNING level event to the event log buffer.
 *
 */
#ifndef CONFIG_ACT_EVENT
#define SYS_EVENT_WRN(...)
#else
#define SYS_EVENT_WRN(event, ...) ACT_EVENT_SEND(ACT_EVENT_LEVEL_WARN, event, ##__VA_ARGS__)
#endif

/**
 * @brief Writes a INFO level event to the event log buffer.
 *
 */
#ifndef CONFIG_ACT_EVENT
#define SYS_EVENT_INF(...)
#else
#define SYS_EVENT_INF(event, ...) ACT_EVENT_SEND(ACT_EVENT_LEVEL_INFO, event, ##__VA_ARGS__)
#endif


/**
 * @brief Writes a DEBUG level event to the event log buffer.
 *
 */
#ifndef CONFIG_ACT_EVENT
#define SYS_EVENT_DBG(...)
#else
#define SYS_EVENT_DBG(event, ...) ACT_EVENT_SEND(ACT_EVENT_LEVEL_DEBUG, event, ##__VA_ARGS__)
#endif

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __SYS_LOG_H */
