#include <stic.h>

#include "../../src/engine/keys.h"

#include "builtin_keys.h"

#define check(wait, full, cmd_count) \
	{ \
		assert_int_equal(KEYS_WAIT, vle_keys_exec(wait)); \
		assert_int_equal(0, vle_keys_exec(full)); \
		assert_int_equal((cmd_count), last_command_count); \
	}

TEST(no_number_ok)
{
	check(L"", L"<", NO_COUNT_GIVEN);
}

TEST(with_number_ok)
{
	check(L"1",   L"1<",   1*1);
	check(L"12",  L"12<",  1*12);
	check(L"123", L"123<", 1*123);
}

TEST(with_zero_number_fail)
{
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L"0"));
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L"0<"));
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L"01"));
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L"01<"));
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L"012"));
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L"012<"));
}

TEST(with_number_before_and_in_the_middle_ok)
{
	check(L"21",   L"21<",   2*1);
	check(L"312",  L"312<",  3*12);
	check(L"2123", L"2123<", 2*123);
}

TEST(dd_nim_ok)
{
	check(L"2d1",   L"2d1d",   2*1);
	check(L"3d12",  L"3d12d",  3*12);
	check(L"2d123", L"2d123d", 2*123);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
