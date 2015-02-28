#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/modes/modes.h"

static int def_handler(const wchar_t keys);

SETUP()
{
	static int mode_flags[]= {
		MF_USES_REGS | MF_USES_COUNT,
		MF_USES_INPUT,
		MF_USES_COUNT
	};

	clear_keys();
	init_keys(MODES_COUNT, mode_flags);
	vle_mode_set(CMDLINE_MODE, VMT_SECONDARY);
	set_def_handler(CMDLINE_MODE, def_handler);
	add_user_keys(L"asdf", L"ddd", CMDLINE_MODE, 0);
}

static int
def_handler(const wchar_t keys)
{
	return 0;
}

TEST(asdf)
{
	assert_int_equal(KEYS_WAIT_SHORT, execute_keys(L"a"));
	assert_int_equal(KEYS_WAIT_SHORT, execute_keys(L"as"));
	assert_int_equal(KEYS_WAIT_SHORT, execute_keys(L"asd"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"asdf")));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 : */
