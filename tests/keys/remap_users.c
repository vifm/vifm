#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

static int counter;

TEST(allow_user_key_remap)
{
	assert_success(vle_keys_user_add(L"jo", L":do movement", NORMAL_MODE,
				KEYS_FLAG_NONE));
	assert_success(vle_keys_user_add(L"jo", L":leave insert mode", NORMAL_MODE,
				KEYS_FLAG_NONE));
}

static int
handler(wchar_t key)
{
	static const wchar_t line[] = L"toto";

	char exp[] = {line[counter++], '\0'};
	char act[] = {key, '\0'};

	assert_string_equal(exp, act);
	return 0;
}

TEST(prevent_stack_overflow)
{
	assert_success(vle_keys_user_add(L"j", L"j", NORMAL_MODE, KEYS_FLAG_NONE));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"j")));

	assert_success(vle_keys_user_add(L"q", L"q", NORMAL_MODE, KEYS_FLAG_NONE));
	assert_int_equal(KEYS_UNKNOWN, vle_keys_exec(L"q"));

	vle_keys_set_def_handler(NORMAL_MODE, handler);

	assert_success(vle_keys_user_add(L"t", L"toto", NORMAL_MODE, KEYS_FLAG_NONE));
	assert_success(vle_keys_exec(L"t"));

	assert_int_equal(4, counter);

	vle_keys_set_def_handler(NORMAL_MODE, NULL);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
