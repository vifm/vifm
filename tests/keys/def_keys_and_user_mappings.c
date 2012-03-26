#include "seatest.h"

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

extern int mode;

static int counter;

static int
handler(wchar_t key)
{
	static const wchar_t line[] = L"shelltoto";

	char exp[] = {line[counter++], '\0'};
	char act[] = {key, '\0'};

	assert_string_equal(exp, act);
	return 0;
}

static void
add_custom_keys(void)
{
	set_def_handler(CMDLINE_MODE, &handler);
	add_user_keys(L"s", L":shell", NORMAL_MODE, 0);
	add_user_keys(L"q", L"toto", CMDLINE_MODE, 0);
}

static void
teardown(void)
{
	set_def_handler(CMDLINE_MODE, NULL);
	mode = NORMAL_MODE;
}

static void
cmd_line_tst(void)
{
	counter = 0;

	assert_false(IS_KEYS_RET_CODE(execute_keys(L"s")));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"q")));

	assert_true(counter == 9);
}

void
def_keys_and_user_mappings(void)
{
	test_fixture_start();

	fixture_setup(add_custom_keys);
	fixture_teardown(teardown);

	run_test(cmd_line_tst);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
