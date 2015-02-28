#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/modes/modes.h"

static int handler(wchar_t key);

static int counter;

SETUP()
{
	set_def_handler(CMDLINE_MODE, &handler);
	add_user_keys(L"s", L":shell", NORMAL_MODE, 0);
	add_user_keys(L"q", L"toto", CMDLINE_MODE, 0);
}

TEARDOWN()
{
	set_def_handler(CMDLINE_MODE, NULL);
	vle_mode_set(NORMAL_MODE, VMT_PRIMARY);
}

static int
handler(wchar_t key)
{
	static const wchar_t line[] = L"shelltoto";

	char exp[] = {line[counter++], '\0'};
	char act[] = {key, '\0'};

	assert_string_equal(exp, act);
	return 0;
}

TEST(cmd_line)
{
	counter = 0;

	assert_false(IS_KEYS_RET_CODE(execute_keys(L"s")));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"q")));

	assert_true(counter == 9);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 : */
