#include <stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/compat/fs_limits.h"
#include "../../src/engine/cmds.h"
#include "../../src/ui/statusbar.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/path.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/registers.h"

SETUP()
{
	view_setup(&lwin);
	curr_view = &lwin;

	conf_setup();
	cmds_init();
	regs_init();
}

TEARDOWN()
{
	view_teardown(&lwin);
	curr_view = NULL;

	conf_teardown();
	vle_cmds_reset();
	regs_reset();
}

TEST(regedit, IF(not_windows))
{
	create_executable(SANDBOX_PATH "/script");
	make_file(SANDBOX_PATH "/script",
			"#!/bin/sh\n"
			"sed 's/from/to/' < \"$3\" > \"$3_out\"\n"
			"mv \"$3_out\" \"$3\"\n");

	char vi_cmd[PATH_MAX + 1];
	make_abs_path(vi_cmd, sizeof(vi_cmd), SANDBOX_PATH, "script", NULL);
	update_string(&cfg.vi_command, vi_cmd);

	ui_sb_msg("");
	assert_failure(cmds_dispatch("regedit abc", &lwin, CIT_COMMAND));
	assert_string_equal("Invalid argument: abc", ui_sb_last());
	assert_failure(cmds_dispatch("regedit _", &lwin, CIT_COMMAND));
	assert_string_equal("Cannot modify blackhole register.", ui_sb_last());
	assert_failure(cmds_dispatch("regedit %", &lwin, CIT_COMMAND));
	assert_string_equal("Register with given name does not exist.", ui_sb_last());

	regs_append('a', "/path/before");
	regs_append('a', "/path/from-1");
	regs_append('a', "/path/from-2");
	regs_append('a', "/path/to-2");
	regs_append('a', "/path/this-was-after");

	assert_success(cmds_dispatch("regedit a", &lwin, CIT_COMMAND));

	/* Result should be sorted and without duplicates. */
	const reg_t *reg = regs_find('a');
	assert_int_equal(4, reg->nfiles);
	assert_string_equal("/path/before", reg->files[0]);
	assert_string_equal("/path/this-was-after", reg->files[1]);
	assert_string_equal("/path/to-1", reg->files[2]);
	assert_string_equal("/path/to-2", reg->files[3]);

	remove_file(SANDBOX_PATH "/script");
}

TEST(regedit_normalizes_paths, IF(not_windows))
{
	create_executable(SANDBOX_PATH "/script");
	make_file(SANDBOX_PATH "/script",
			"#!/bin/sh\n"
			"sed 's/from/to/' < \"$3\" > \"$3_out\"\n"
			"mv \"$3_out\" \"$3\"\n");

	char vi_cmd[PATH_MAX + 1];
	make_abs_path(vi_cmd, sizeof(vi_cmd), SANDBOX_PATH, "script", NULL);
	update_string(&cfg.vi_command, vi_cmd);

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), SANDBOX_PATH, "", NULL);

	regs_append(DEFAULT_REG_NAME, "/abs/path/from-1");
	regs_append(DEFAULT_REG_NAME, "from-2");

	assert_success(cmds_dispatch("regedit", &lwin, CIT_COMMAND));

	/* Result should contain only absolute paths. */
	const reg_t *reg = regs_find(DEFAULT_REG_NAME);
	assert_int_equal(2, reg->nfiles);
	int rel_idx = (ends_with(reg->files[0], "/to-2") ? 0 : 1);
	assert_string_equal("/abs/path/to-1", reg->files[1 - rel_idx]);
	assert_string_ends_with("/to-2", reg->files[rel_idx]);
	assert_true(is_path_absolute(reg->files[0]));
	assert_true(is_path_absolute(reg->files[1]));

	remove_file(SANDBOX_PATH "/script");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
