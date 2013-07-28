#ifndef VIFM_TESTS__MISC__ASSERTS_H__
#define VIFM_TESTS__MISC__ASSERTS_H__

/* This should be a macro to see what test has failed. */
#define ASSERT_NEXT_MATCH(str) \
	{ \
		char *buf = next_completion(); \
		assert_string_equal((str), buf); \
		free(buf); \
	}

#endif /* VIFM_TESTS__MISC__ASSERTS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
