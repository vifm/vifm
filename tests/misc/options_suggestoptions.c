#include <stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/cmd_core.h"
#include "../../src/ui/ui.h"

SETUP()
{
	init_commands();
	curr_view = &lwin;
	opt_handlers_setup();
}

TEARDOWN()
{
	opt_handlers_teardown();
	curr_view = NULL;
	vle_cmds_reset();
}

TEST(suggestoptions_all_values)
{
	cfg.sug.flags = 0;
	cfg.sug.maxregfiles = 0;
	cfg.sug.delay = 0;

	assert_success(exec_commands("set suggestoptions=normal,visual,view,otherpane"
				",delay,keys,marks,registers,foldsubkeys", &lwin, CIT_COMMAND));

	assert_int_equal(SF_NORMAL | SF_VISUAL | SF_VIEW | SF_OTHERPANE | SF_DELAY |
			SF_KEYS | SF_MARKS | SF_REGISTERS | SF_FOLDSUBKEYS, cfg.sug.flags);
	assert_int_equal(5, cfg.sug.maxregfiles);
	assert_int_equal(500, cfg.sug.delay);
}

TEST(suggestoptions_wrong_value)
{
	cfg.sug.flags = 0;
	cfg.sug.maxregfiles = 0;
	cfg.sug.delay = 0;

	assert_failure(exec_commands("set suggestoptions=asdf", &lwin, CIT_COMMAND));

	assert_int_equal(0, cfg.sug.flags);
	assert_int_equal(0, cfg.sug.maxregfiles);
	assert_int_equal(0, cfg.sug.delay);
}

TEST(suggestoptions_empty_value)
{
	assert_success(exec_commands("set suggestoptions=normal", &lwin,
				CIT_COMMAND));

	cfg.sug.flags = SF_NORMAL;
	cfg.sug.maxregfiles = 0;
	cfg.sug.delay = 0;

	assert_success(exec_commands("set suggestoptions=", &lwin, CIT_COMMAND));

	assert_int_equal(0, cfg.sug.flags);
	assert_int_equal(5, cfg.sug.maxregfiles);
	assert_int_equal(500, cfg.sug.delay);
}

TEST(suggestoptions_registers_number)
{
	cfg.sug.flags = SF_NORMAL | SF_VISUAL | SF_VIEW | SF_OTHERPANE | SF_DELAY |
					SF_KEYS | SF_MARKS | SF_REGISTERS | SF_FOLDSUBKEYS;
	cfg.sug.maxregfiles = 4;

	assert_failure(exec_commands("set suggestoptions=registers:-4", &lwin,
				CIT_COMMAND));
	assert_int_equal(4, cfg.sug.maxregfiles);

	assert_failure(exec_commands("set suggestoptions=registers:0", &lwin,
				CIT_COMMAND));
	assert_int_equal(4, cfg.sug.maxregfiles);

	assert_success(exec_commands("set suggestoptions=registers:1", &lwin,
				CIT_COMMAND));

	assert_int_equal(1, cfg.sug.maxregfiles);
}

TEST(suggestoptions_delay_number)
{
	cfg.sug.delay = 4;

	assert_failure(exec_commands("set suggestoptions=delay:-4", &lwin,
				CIT_COMMAND));
	assert_int_equal(4, cfg.sug.delay);

	assert_success(exec_commands("set suggestoptions=delay:0", &lwin,
				CIT_COMMAND));
	assert_int_equal(0, cfg.sug.delay);

	assert_success(exec_commands("set suggestoptions=delay:100", &lwin,
				CIT_COMMAND));

	assert_int_equal(100, cfg.sug.delay);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
