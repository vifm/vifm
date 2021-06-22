#include <stic.h>

#include <unistd.h> /* chdir() */

#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */
#include <wchar.h> /* wcsdup() wcslen() */

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/cmds.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/macros.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/status.h"

static int builtin_cmd(const cmd_info_t *cmd_info);

static const cmd_add_t commands[] = {
	{ .name = "builtin",       .abbr = NULL,  .id = -1,      .descr = "descr",
	  .flags = HAS_EMARK,
	  .handler = &builtin_cmd, .min_args = 0, .max_args = 0, },
};

static int called;

SETUP_ONCE()
{
	undo_setup();
	conf_setup();
}

TEARDOWN_ONCE()
{
	undo_teardown();
	conf_teardown();
}

SETUP()
{
	lwin.list_rows = 0;

	curr_view = &lwin;
	other_view = &rwin;

	init_commands();

	vle_cmds_add(commands, ARRAY_LEN(commands));

	assert_success(stats_init(&cfg));
}

TEARDOWN()
{
	assert_success(stats_reset(&cfg));

	vle_cmds_reset();
}

static int
builtin_cmd(const cmd_info_t* cmd_info)
{
	called = 1;
	return 0;
}

TEST(repeat_of_no_command_prints_a_message)
{
	called = 0;
	(void)exec_commands("builtin", &lwin, CIT_COMMAND);
	assert_int_equal(1, called);

	update_string(&curr_stats.last_cmdline_command, NULL);

	called = 0;
	assert_int_equal(1, exec_commands("!!", &lwin, CIT_COMMAND));
	assert_int_equal(0, called);
}

TEST(double_emark_repeats_last_command)
{
	called = 0;
	(void)exec_commands("builtin", &lwin, CIT_COMMAND);
	assert_int_equal(1, called);

	update_string(&curr_stats.last_cmdline_command, "builtin");

	called = 0;
	assert_int_equal(0, exec_commands("!!", &lwin, CIT_COMMAND));
	assert_int_equal(1, called);
}

TEST(single_emark_without_args_fails)
{
	update_string(&curr_stats.last_cmdline_command, "builtin");

	called = 0;
	assert_false(exec_commands("!", &lwin, CIT_COMMAND) == 0);
	assert_int_equal(0, called);
}

TEST(provide_input_to_fg_process, IF(have_cat))
{
	assert_success(chdir(SANDBOX_PATH));

	view_setup(&lwin);
	setup_grid(&lwin, 20, 2, /*init=*/1);
	replace_string(&lwin.dir_entry[0].name, "a");
	replace_string(&lwin.dir_entry[1].name, "b");

	lwin.dir_entry[0].marked = 1;
	lwin.dir_entry[1].marked = 1;
	lwin.pending_marking = 1;

	assert_int_equal(0, exec_commands("!cat > file %Pl", &lwin, CIT_COMMAND));

	const char *lines[] = { "/path/a", "/path/b" };
	file_is("file", lines, ARRAY_LEN(lines));

	remove_file("file");

	view_teardown(&lwin);
}

TEST(provide_input_to_bg_process, IF(have_cat))
{
	assert_success(chdir(SANDBOX_PATH));

	view_setup(&lwin);
	setup_grid(&lwin, 20, 2, /*init=*/1);
	replace_string(&lwin.dir_entry[0].name, "a");
	replace_string(&lwin.dir_entry[1].name, "b");

	lwin.dir_entry[0].marked = 1;
	lwin.dir_entry[1].marked = 1;
	lwin.pending_marking = 1;

	assert_int_equal(0, exec_commands("!cat > file %Pl &", &lwin, CIT_COMMAND));

	wait_for_all_bg();

	const char *lines[] = { "/path/a", "/path/b" };
	file_is("file", lines, ARRAY_LEN(lines));

	remove_file("file");

	view_teardown(&lwin);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
