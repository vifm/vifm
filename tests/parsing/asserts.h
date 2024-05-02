#ifndef VIFM_TESTS__PARSING__ASSERTS_H__
#define VIFM_TESTS__PARSING__ASSERTS_H__

#include <stdlib.h> /* free() */

/* This should be a macro to see what test has failed. */
#define ASSERT_OK(str, expected) \
	do \
	{ \
		parsing_result_t result = vle_parser_eval((str), /*interactive=*/0); \
		assert_int_equal(PE_NO_ERROR, result.error); \
		\
		if(result.value.type != VTYPE_ERROR) \
		{ \
			char *str_res = var_to_str(result.value); \
			assert_string_equal((expected), str_res); \
			free(str_res); \
		} \
		\
		var_free(result.value); \
	} \
	while(0)

/* This should be a macro to see what test has failed. */
#define ASSERT_INT_OK(str, expected) \
	do \
	{ \
		parsing_result_t result = vle_parser_eval((str), /*interactive=*/0); \
		assert_int_equal(PE_NO_ERROR, result.error); \
		\
		if(result.value.type != VTYPE_ERROR) \
		{ \
			int int_res = var_to_int(result.value); \
			assert_int_equal((expected), int_res); \
		} \
		\
		var_free(result.value); \
	} \
	while(0)

/* This should be a macro to see what test has failed. */
#define ASSERT_FAIL(str, error_code) \
	do \
	{ \
		parsing_result_t result = vle_parser_eval((str), /*interactive=*/0); \
		assert_int_equal((error_code), result.error); \
		var_free(result.value); \
	} \
	while(0)

/* This should be a macro to see what test has failed. */
#define ASSERT_FAIL_AT(str, suffix, error_code) \
	do \
	{ \
		parsing_result_t result = vle_parser_eval((str), /*interactive=*/0); \
		assert_int_equal((error_code), result.error); \
		var_free(result.value); \
		\
		assert_string_equal((suffix), result.last_position); \
	} \
	while(0)

/* This should be a macro to see what test has failed. */
#define ASSERT_FAIL_GET(str, error_code) \
	({ \
		parsing_result_t result = vle_parser_eval((str), /*interactive=*/0); \
		assert_int_equal((error_code), result.error); \
		var_free(result.value); \
		\
		result; \
	})

#endif /* VIFM_TESTS__PARSING__ASSERTS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
