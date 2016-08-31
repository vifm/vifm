#ifndef VIFM_TESTS__MISC__ASSERTS_H__
#define VIFM_TESTS__MISC__ASSERTS_H__

/* This should be a macro to see what test and where has failed. */
#define ASSERT_NEXT_MATCH(str) \
	do \
	{ \
		char *const buf = vle_compl_next(); \
		assert_string_equal((str), buf); \
		free(buf); \
	} \
	while (0)

#endif /* VIFM_TESTS__MISC__ASSERTS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
