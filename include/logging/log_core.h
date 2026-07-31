/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_LOGGING_LOG_CORE_H_
#define ZEPHYR_INCLUDE_LOGGING_LOG_CORE_H_

#include <zephyr/types.h>
#include <logging/util_loops.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONFIG_LOG
#define CONFIG_LOG_DEFAULT_LEVEL 2U
#endif

/** @brief Constant data associated with the source of log messages. */
struct log_source_const_data {
	const char *name;
	uint8_t level;
};

#define UTIL_CAT(a, ...) UTIL_PRIMITIVE_CAT(a, __VA_ARGS__)
#define UTIL_PRIMITIVE_CAT(a, ...) a##__VA_ARGS__

/** @brief Creates name of variable and section for constant log data.
 *
 *  @param _name Name.
 */
#define LOG_ITEM_CONST_DATA(_name) UTIL_CAT(log_const_, _name)

	/* Used internally by COND_CODE_1 and COND_CODE_0. */
#define Z_COND_CODE_1(_flag, _if_1_code, _else_code) \
		__COND_CODE(_XXXX##_flag, _if_1_code, _else_code)
#define Z_COND_CODE_0(_flag, _if_0_code, _else_code) \
		__COND_CODE(_ZZZZ##_flag, _if_0_code, _else_code)
#define _ZZZZ0 _YYYY,

#define __COND_CODE(one_or_two_args, _if_code, _else_code) \
	__GET_ARG2_DEBRACKET(one_or_two_args _if_code, _else_code)

/* Gets second argument and removes brackets around that argument. It
 * is expected that the parameter is provided in brackets/parentheses.
 */
#define __GET_ARG2_DEBRACKET(ignore_this, val, ...) __DEBRACKET val

/* Used to remove brackets from around a single argument. */
#define __DEBRACKET(...) __VA_ARGS__


/**
 * @brief Macro for conditional code generation if provided log level allows.
 *
 * Macro behaves similarly to standard \#if \#else \#endif clause. The
 * difference is that it is evaluated when used and not when header file is
 * included.
 *
 * @param _eval_level Evaluated level. If level evaluates to one of existing log
 *		      log level (1-4) then macro evaluates to _iftrue.
 * @param _iftrue     Code that should be inserted when evaluated to true. Note,
 *		      that parameter must be provided in brackets.
 * @param _iffalse    Code that should be inserted when evaluated to false.
 *		      Note, that parameter must be provided in brackets.
 */
#define Z_LOG_EVAL(_eval_level, _iftrue, _iffalse) \
	Z_LOG_EVAL1(_eval_level, _iftrue, _iffalse)

#define Z_LOG_EVAL1(_eval_level, _iftrue, _iffalse) \
	__COND_CODE(_LOG_ZZZZ##_eval_level, _iftrue, _iffalse)

#define _LOG_ZZZZ1  _LOG_YYYY,
#define _LOG_ZZZZ1U _LOG_YYYY,
#define _LOG_ZZZZ2  _LOG_YYYY,
#define _LOG_ZZZZ2U _LOG_YYYY,
#define _LOG_ZZZZ3  _LOG_YYYY,
#define _LOG_ZZZZ3U _LOG_YYYY,
#define _LOG_ZZZZ4  _LOG_YYYY,
#define _LOG_ZZZZ4U _LOG_YYYY,

	/**
	 * @brief Get nth argument from argument list.
	 *
	 * @param N Argument index to fetch. Counter from 1.
	 * @param ... Variable list of argments from which one argument is returned.
	 *
	 * @return Nth argument.
	 */
#define GET_ARG_N(N, ...) Z_GET_ARG_##N(__VA_ARGS__)

#define _LOG_LEVEL_RESOLVE(...) \
	Z_LOG_EVAL(LOG_LEVEL, \
		  (GET_ARG_N(2, __VA_ARGS__, LOG_LEVEL)), \
		  (GET_ARG_N(2, __VA_ARGS__, CONFIG_LOG_DEFAULT_LEVEL)))

/* Implementation details for NUM_VA_ARGS_LESS_1 */
#define NUM_VA_ARGS_LESS_1_IMPL(				\
				_ignored,						\
				_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10,		\
				_11, _12, _13, _14, _15, _16, _17, _18, _19, _20,	\
				_21, _22, _23, _24, _25, _26, _27, _28, _29, _30,	\
				_31, _32, _33, _34, _35, _36, _37, _38, _39, _40,	\
				_41, _42, _43, _44, _45, _46, _47, _48, _49, _50,	\
				_51, _52, _53, _54, _55, _56, _57, _58, _59, _60,	\
				_61, _62, N, ...) N

/**
 * @brief Number of arguments in the variable arguments list minus one.
 *
 * @param ... List of arguments
 * @return	Number of variadic arguments in the argument list, minus one
 */
#define NUM_VA_ARGS_LESS_1(...) \
				NUM_VA_ARGS_LESS_1_IMPL(__VA_ARGS__, 63, 62, 61, \
				60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 	 \
				50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 	 \
				40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 	 \
				30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 	 \
				20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 	 \
				10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, ~)

/**
 * @brief Insert code depending on whether @p _flag expands to 1 or not.
 *
 * This relies on similar tricks as IS_ENABLED(), but as the result of
 * @p _flag expansion, results in either @p _if_1_code or @p
 * _else_code is expanded.
 *
 * To prevent the preprocessor from treating commas as argument
 * separators, the @p _if_1_code and @p _else_code expressions must be
 * inside brackets/parentheses: <tt>()</tt>. These are stripped away
 * during macro expansion.
 *
 * Example:
 *
 *     COND_CODE_1(CONFIG_FLAG, (uint32_t x;), (there_is_no_flag();))
 *
 * If @p CONFIG_FLAG is defined to 1, this expands to:
 *
 *     uint32_t x;
 *
 * It expands to <tt>there_is_no_flag();</tt> otherwise.
 *
 * This could be used as an alternative to:
 *
 *     #if defined(CONFIG_FLAG) && (CONFIG_FLAG == 1)
 *     #define MAYBE_DECLARE(x) uint32_t x
 *     #else
 *     #define MAYBE_DECLARE(x) there_is_no_flag()
 *     #endif
 *
 *     MAYBE_DECLARE(x);
 *
 * However, the advantage of COND_CODE_1() is that code is resolved in
 * place where it is used, while the @p \#if method defines @p
 * MAYBE_DECLARE on two lines and requires it to be invoked again on a
 * separate line. This makes COND_CODE_1() more concise and also
 * sometimes more useful when used within another macro's expansion.
 *
 * @note @p _flag can be the result of preprocessor expansion, e.g.
 *	 an expression involving <tt>NUM_VA_ARGS_LESS_1(...)</tt>.
 *	 However, @p _if_1_code is only expanded if @p _flag expands
 *	 to the integer literal 1. Integer expressions that evaluate
 *	 to 1, e.g. after doing some arithmetic, will not work.
 *
 * @param _flag evaluated flag
 * @param _if_1_code result if @p _flag expands to 1; must be in parentheses
 * @param _else_code result otherwise; must be in parentheses
 */
#define COND_CODE_1(_flag, _if_1_code, _else_code) \
	Z_COND_CODE_1(_flag, _if_1_code, _else_code)

/**
 *  @def LOG_CONST_ID_GET
 *  @brief Macro for getting ID of the element of the section.
 *
 *  @param _addr Address of the element.
 */
#define LOG_CONST_ID_GET(_addr) \
	COND_CODE_1(CONFIG_ACT_EVENT, ((__log_level ? log_const_source_id(_addr) : 0)), (0))

/**
 * @brief Macro for declaring a log module (not registering it).
 *
 * Modules which are split up over multiple files must have exactly
 * one file use LOG_MODULE_REGISTER() to create module-specific state
 * and register the module with the logger core.
 *
 * The other files in the module should use this macro instead to
 * declare that same state. (Otherwise, LOG_INF() etc. will not be
 * able to refer to module-specific state variables.)
 *
 * Macro accepts one or two parameters:
 * - module name
 * - optional log level. If not provided then default log level is used in
 *  the file.
 *
 * Example usage:
 * - LOG_MODULE_DECLARE(foo, CONFIG_FOO_LOG_LEVEL)
 * - LOG_MODULE_DECLARE(foo)
 *
 * @note The module's state is declared only if LOG_LEVEL for the
 *       current source file is non-zero or it is not defined and
 *       CONFIG_LOG_DEFAULT_LEVEL is non-zero.  In other cases,
 *       this macro has no effect.
 * @see LOG_MODULE_REGISTER
 */
#define LOG_MODULE_DECLARE(...)						      \
	extern const struct log_source_const_data			      \
			LOG_ITEM_CONST_DATA(GET_ARG_N(1, __VA_ARGS__));	      \
									      \
	static const struct log_source_const_data *			      \
		__log_current_const_data __unused =			      \
			_LOG_LEVEL_RESOLVE(__VA_ARGS__) ?		      \
			&LOG_ITEM_CONST_DATA(GET_ARG_N(1, __VA_ARGS__)) :     \
			NULL;						      \
									      									      \
	static const uint32_t __log_level __unused =			      \
					_LOG_LEVEL_RESOLVE(__VA_ARGS__)

#define _LOG_MODULE_CONST_DATA_CREATE(_name, _level)			     \
	const struct log_source_const_data LOG_ITEM_CONST_DATA(_name)	     \
	__attribute__ ((section("." STRINGIFY(LOG_ITEM_CONST_DATA(_name))))) \
	__attribute__((used)) = {					     \
		.name = STRINGIFY(_name),				     \
		.level = _level						     \
	}


#define _LOG_MODULE_DATA_CREATE(_name, _level)			\
	_LOG_MODULE_CONST_DATA_CREATE(_name, _level);
/**
 * @brief Create module-specific state and register the module with Logger.
 *
 * This macro normally must be used after including <logging/log.h> to
 * complete the initialization of the module.
 *
 * Module registration can be skipped in two cases:
 *
 * - The module consists of more than one file, and another file
 *   invokes this macro. (LOG_MODULE_DECLARE() should be used instead
 *   in all of the module's other files.)
 * - Instance logging is used and there is no need to create module entry. In
 *   that case LOG_LEVEL_SET() should be used to set log level used within the
 *   file.
 *
 * Macro accepts one or two parameters:
 * - module name
 * - optional log level. If not provided then default log level is used in
 *  the file.
 *
 * Example usage:
 * - LOG_MODULE_REGISTER(foo, CONFIG_FOO_LOG_LEVEL)
 * - LOG_MODULE_REGISTER(foo)
 *
 *
 * @note The module's state is defined, and the module is registered,
 *       only if LOG_LEVEL for the current source file is non-zero or
 *       it is not defined and CONFIG_LOG_DEFAULT_LEVEL is non-zero.
 *       In other cases, this macro has no effect.
 * @see LOG_MODULE_DECLARE
 */

#define LOG_MODULE_REGISTER(...)					\
	Z_LOG_EVAL(							\
		_LOG_LEVEL_RESOLVE(__VA_ARGS__),			\
		(_LOG_MODULE_DATA_CREATE(GET_ARG_N(1, __VA_ARGS__),	\
				      _LOG_LEVEL_RESOLVE(__VA_ARGS__))),\
		()/*Empty*/						\
	)								\
	LOG_MODULE_DECLARE(__VA_ARGS__)


extern struct log_source_const_data __log_const_start[];
extern struct log_source_const_data __log_const_end[];

/** @brief Get name of the log source.
 *
 * @param source_id Source ID.
 * @return Name.
 */
static inline const char *log_name_get(uint32_t source_id)
{
	return __log_const_start[source_id].name;
}

/** @brief Get compiled level of the log source.
 *
 * @param source_id Source ID.
 * @return Level.
 */
static inline uint8_t log_compiled_level_get(uint32_t source_id)
{
	return __log_const_start[source_id].level;
}

/** @brief Get index of the log source based on the address of the constant data
 *         associated with the source.
 *
 * @param data Address of the constant data.
 *
 * @return Source ID.
 */
static inline uint32_t log_const_source_id(
				const struct log_source_const_data *data)
{
	return ((uint8_t *)data - (uint8_t *)__log_const_start)/
			sizeof(struct log_source_const_data);
}

/** @brief Get number of registered sources. */
static inline uint32_t log_sources_count(void)
{
	return log_const_source_id(__log_const_end);
}

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_LOGGING_LOG_CORE_H_ */

