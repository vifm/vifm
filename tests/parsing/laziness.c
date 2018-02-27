#include <stic.h>

#include "../../src/engine/functions.h"
#include "../../src/engine/parsing.h"
#include "../../src/engine/var.h"

#include "asserts.h"

static var_t dummy(const call_info_t *call_info);

static int called;

SETUP_ONCE()
{
	static const function_t function_a = { "a", "descr", {0,0}, &dummy };

	assert_success(function_register(&function_a));
}

TEARDOWN_ONCE()
{
	function_reset_all();
}

TEARDOWN()
{
	called = 0;
}

static var_t
dummy(const call_info_t *call_info)
{
	called = 1;
	return var_from_str("");
}

TEST(or_is_lazy)
{
	ASSERT_OK("1 || a()", "1");
	assert_false(called);
}

TEST(and_is_lazy)
{
	ASSERT_OK("0 && a()", "0");
	assert_false(called);
}

TEST(compares_are_lazy)
{
	ASSERT_OK("0 && 1 < a()", "0");
	assert_false(called);
}

TEST(negation_is_lazy)
{
	ASSERT_OK("0 && -a()", "0");
	assert_false(called);
}

TEST(not_is_lazy)
{
	ASSERT_OK("0 && !a()", "0");
	assert_false(called);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
