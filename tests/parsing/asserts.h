#ifndef VIFM_TESTS__PARSING__ASSERTS_H__
#define VIFM_TESTS__PARSING__ASSERTS_H__

/* This should be a macro to see what test has failed. */
#define ASSERT_OK(str, result) \
	{ \
		char *str_res; \
		var_t res_var = var_false(); \
		assert_int_equal(PE_NO_ERROR, parse((str), &res_var)); \
		str_res = var_to_string(res_var); \
		assert_string_equal((result), str_res); \
		free(str_res); \
		var_free(res_var); \
	}

/* This should be a macro to see what test has failed. */
#define ASSERT_FAIL(str, error) \
	{ \
		var_t res_var = var_false(); \
		assert_int_equal((error), parse((str), &res_var)); \
		var_free(res_var); \
	}

#endif /* VIFM_TESTS__PARSING__ASSERTS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
