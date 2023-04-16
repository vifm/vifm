#include <stic.h>

#include <test-utils.h>

#include "../../src/engine/options.h"
#include "../../src/engine/text_buffer.h"
#include "../../src/cmd_core.h"
#include "../../src/ui/column_view.h"
#include "../../src/ui/tabs.h"
#include "../../src/ui/ui.h"

static void print_func(const char buf[], int offset, AlignType align,
		const char full_column[], const format_info_t *info);

SETUP()
{
	cmds_init();

	view_setup(&lwin);
	lwin.columns = columns_create();
	curr_view = &lwin;

	view_setup(&rwin);
	rwin.columns = columns_create();
	other_view = &rwin;

	/* Name+size matches default column view setting ("-{name},{}"). */
	columns_setup_column(SK_BY_NAME);
	columns_setup_column(SK_BY_SIZE);
	columns_set_line_print_func(&print_func);

	opt_handlers_setup();
}

TEARDOWN()
{
	opt_handlers_teardown();

	vle_cmds_reset();

	view_teardown(&lwin);
	view_teardown(&rwin);

	columns_teardown();
}

TEST(millerview)
{
	assert_success(cmds_dispatch("se millerview", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("se invmillerview", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("setl millerview", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("setl invmillerview", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("setg millerview", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("setg invmillerview", &lwin, CIT_COMMAND));
}

TEST(milleroptions_handles_wrong_input)
{
	assert_failure(cmds_dispatch("se milleroptions=msi:1", &lwin, CIT_COMMAND));

	assert_failure(cmds_dispatch("se milleroptions=lsize:a", &lwin, CIT_COMMAND));
	assert_failure(cmds_dispatch("se milleroptions=csize:a", &lwin, CIT_COMMAND));
	assert_failure(cmds_dispatch("se milleroptions=rsize:a", &lwin, CIT_COMMAND));

	assert_failure(cmds_dispatch("se milleroptions=csize:0", &lwin, CIT_COMMAND));

	assert_failure(cmds_dispatch("se milleroptions=rpreview:stuff", &lwin,
				CIT_COMMAND));
}

TEST(milleroptions_accepts_correct_input)
{
	assert_success(cmds_dispatch("set milleroptions=csize:33,rsize:12,"
				"rpreview:all", &lwin, CIT_COMMAND));

	vle_tb_clear(vle_err);
	assert_success(vle_opts_set("milleroptions?", OPT_GLOBAL));
	assert_string_equal("  milleroptions=lsize:0,csize:33,rsize:12,rpreview:all",
			vle_tb_get_data(vle_err));
}

TEST(milleroptions_recovers_after_wrong_input)
{
	assert_success(cmds_dispatch("set milleroptions=csize:33,rsize:12,"
				"rpreview:files", &lwin, CIT_COMMAND));
	assert_failure(cmds_dispatch("set milleroptions=rpreview:dirs,rsize:4,"
				"csize:a", &lwin, CIT_COMMAND));

	vle_tb_clear(vle_err);
	assert_success(vle_opts_set("milleroptions?", OPT_GLOBAL));
	assert_string_equal("  milleroptions=lsize:0,csize:33,rsize:12,"
			"rpreview:files", vle_tb_get_data(vle_err));
}

TEST(milleroptions_normalizes_input)
{
	assert_success(cmds_dispatch("set milleroptions=lsize:-10,csize:133,"
				"rpreview:dirs", &lwin, CIT_COMMAND));

	vle_tb_clear(vle_err);
	assert_success(vle_opts_set("milleroptions?", OPT_GLOBAL));
	assert_string_equal("  milleroptions=lsize:0,csize:100,rsize:0,rpreview:dirs",
			vle_tb_get_data(vle_err));
}

TEST(milleroptions_are_cloned_to_new_tabs)
{
	init_view_list(&lwin);
	init_view_list(&rwin);

	assert_success(cmds_dispatch("set milleroptions=lsize:5,rpreview:all", &lwin,
				CIT_COMMAND));

	tabs_new(NULL, NULL);

	vle_tb_clear(vle_err);
	assert_success(vle_opts_set("milleroptions?", OPT_GLOBAL));
	assert_string_equal("  milleroptions=lsize:5,csize:1,rsize:0,rpreview:all",
			vle_tb_get_data(vle_err));
}

static void
print_func(const char buf[], int offset, AlignType align,
		const char full_column[], const format_info_t *info)
{
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
