#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/modes/modes.h"

static int def_handler(wchar_t keys);
static void silence(int more);

SETUP()
{
	static int mode_flags[]= {
		MF_USES_REGS | MF_USES_COUNT,
		MF_USES_INPUT,
		MF_USES_COUNT
	};

	vle_keys_reset();
	vle_keys_init(MODES_COUNT, mode_flags, &silence);
	vle_mode_set(CMDLINE_MODE, VMT_SECONDARY);
	vle_keys_set_def_handler(CMDLINE_MODE, def_handler);
	vle_keys_user_add(L"asdf", L"ddd", CMDLINE_MODE, KEYS_FLAG_NONE);
}

static int
def_handler(wchar_t keys)
{
	return 0;
}

static void
silence(int more)
{
	/* Do nothing. */
}

TEST(asdf)
{
	assert_int_equal(KEYS_WAIT_SHORT, vle_keys_exec(L"a"));
	assert_int_equal(KEYS_WAIT_SHORT, vle_keys_exec(L"as"));
	assert_int_equal(KEYS_WAIT_SHORT, vle_keys_exec(L"asd"));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"asdf")));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
