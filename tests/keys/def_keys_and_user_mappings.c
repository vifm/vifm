#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/modes/modes.h"

static int handler(wchar_t key);

static int counter;

SETUP()
{
	vle_keys_set_def_handler(CMDLINE_MODE, &handler);
	vle_keys_user_add(L"s", L":shell", NORMAL_MODE, KEYS_FLAG_NONE);
	vle_keys_user_add(L"q", L"toto", CMDLINE_MODE, KEYS_FLAG_NONE);
}

TEARDOWN()
{
	vle_keys_set_def_handler(CMDLINE_MODE, NULL);
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

	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"s")));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"q")));

	assert_true(counter == 9);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
