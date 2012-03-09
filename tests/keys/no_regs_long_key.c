#include "seatest.h"

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

static int
def_handler(const wchar_t keys)
{
	return 0;
}

static void
setup(void)
{
	static int mode = CMDLINE_MODE;
	static int mode_flags[]= {
		MF_USES_REGS | MF_USES_COUNT,
		MF_USES_INPUT,
		MF_USES_COUNT
	};

	clear_keys();
	init_keys(MODES_COUNT, &mode, mode_flags);
	set_def_handler(CMDLINE_MODE, def_handler);
	add_user_keys(L"asdf", L"ddd", CMDLINE_MODE, 0);
}

static void
test_asdf(void)
{
	assert_int_equal(KEYS_WAIT_SHORT, execute_keys(L"a"));
	assert_int_equal(KEYS_WAIT_SHORT, execute_keys(L"as"));
	assert_int_equal(KEYS_WAIT_SHORT, execute_keys(L"asd"));
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"asdf")));
}

void
no_regs_long_key(void)
{
	test_fixture_start();

	fixture_setup(setup);

	run_test(test_asdf);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
