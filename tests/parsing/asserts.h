#ifndef VIFM_TESTS__PARSING__ASSERTS_H__
#define VIFM_TESTS__PARSING__ASSERTS_H__

#include <stdlib.h> /* free() */

/* This should be a macro to see what test has failed. */
#define ASSERT_OK(str, result) \
	do \
	{ \
		char *str_res; \
		var_t res_var = var_false(); \
		assert_int_equal(PE_NO_ERROR, parse((str), 0, &res_var)); \
		str_res = var_to_str(res_var); \
		assert_string_equal((result), str_res); \
		free(str_res); \
		var_free(res_var); \
	} \
	while(0)

/* This should be a macro to see what test has failed. */
#define ASSERT_INT_OK(str, result) \
	do \
	{ \
		int int_res; \
		var_t res_var = var_false(); \
		assert_int_equal(PE_NO_ERROR, parse((str), 0, &res_var)); \
		int_res = var_to_int(res_var); \
		assert_int_equal((result), int_res); \
		var_free(res_var); \
	} \
	while(0)

/* This should be a macro to see what test has failed. */
#define ASSERT_FAIL(str, error) \
	do \
	{ \
		var_t res_var = var_false(); \
		assert_int_equal((error), parse((str), 0, &res_var)); \
		var_free(res_var); \
	} \
	while(0)

#endif /* VIFM_TESTS__PARSING__ASSERTS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
