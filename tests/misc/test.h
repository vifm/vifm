#ifndef __TEST_H__
#define __TEST_H__

/* This should be a macro to see what test have failed. */
#define ASSERT_NEXT_MATCH(str) \
	{ \
		char *buf = next_completion(); \
		assert_string_equal((str), buf); \
		free(buf); \
	}

#endif /* __TEST_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
