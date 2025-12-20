#include <stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/options.h"
#include "../../src/engine/text_buffer.h"
#include "../../src/utils/str.h"
#include "../../src/cmd_core.h"
#include "../../src/ui/ui.h"

SETUP()
{
	cmds_init();

	curr_view = &lwin;
	other_view = &rwin;

	opt_handlers_setup();
}

TEARDOWN()
{
	opt_handlers_teardown();

	curr_view = NULL;
	other_view = NULL;

	vle_cmds_reset();
}

TEST(fillchars_is_set_on_correct_input)
{
	(void)replace_string(&cfg.vborder_filler, "x");
	(void)replace_string(&cfg.hborder_filler, "y");
	(void)replace_string(&cfg.millersep_filler, "z");
	assert_success(cmds_dispatch("set fillchars=vborder:a,hborder:b,millersep:c",
				&lwin, CIT_COMMAND));
	assert_string_equal("a", cfg.vborder_filler);
	assert_string_equal("b", cfg.hborder_filler);
	assert_string_equal("c", cfg.millersep_filler);
	update_string(&cfg.vborder_filler, NULL);
	update_string(&cfg.hborder_filler, NULL);
	update_string(&cfg.millersep_filler, NULL);
}

TEST(fillchars_not_changed_on_wrong_input)
{
	(void)replace_string(&cfg.vborder_filler, "x");
	assert_failure(cmds_dispatch("set fillchars=vorder:a", &lwin, CIT_COMMAND));
	assert_string_equal("x", cfg.vborder_filler);
	update_string(&cfg.vborder_filler, NULL);
}

TEST(values_in_fillchars_are_deduplicated)
{
	(void)replace_string(&cfg.vborder_filler, "x");

	assert_success(cmds_dispatch("set fillchars=vborder:a", &lwin, CIT_COMMAND));
	assert_success(cmds_dispatch("set fillchars+=vborder:b", &lwin, CIT_COMMAND));
	assert_string_equal("b", cfg.vborder_filler);
	update_string(&cfg.vborder_filler, NULL);

	vle_tb_clear(vle_err);
	assert_success(vle_opts_set("fillchars?", OPT_GLOBAL));
	assert_string_equal("  fillchars=hborder:,millersep:,vborder:b",
			vle_tb_get_data(vle_err));

	update_string(&cfg.vborder_filler, NULL);
}

TEST(fillchars_can_be_reset)
{
	assert_success(cmds_dispatch("set fillchars=vborder:v,hborder:h,millersep:m",
				&lwin, CIT_COMMAND));

	assert_success(cmds_dispatch("set fillchars=vborder:v", &lwin, CIT_COMMAND));
	assert_string_equal("v", cfg.vborder_filler);
	assert_string_equal("", cfg.hborder_filler);
	assert_string_equal("", cfg.millersep_filler);

	assert_success(cmds_dispatch("set fillchars=hborder:h", &lwin, CIT_COMMAND));
	assert_string_equal(" ", cfg.vborder_filler);
	assert_string_equal("h", cfg.hborder_filler);
	assert_string_equal("", cfg.millersep_filler);

	assert_success(cmds_dispatch("set fillchars=millersep:m", &lwin,
				CIT_COMMAND));
	assert_string_equal(" ", cfg.vborder_filler);
	assert_string_equal("", cfg.hborder_filler);
	assert_string_equal("m", cfg.millersep_filler);

	assert_success(cmds_dispatch("set fillchars=", &lwin, CIT_COMMAND));
	assert_string_equal(" ", cfg.vborder_filler);
	assert_string_equal("", cfg.hborder_filler);
	assert_string_equal("", cfg.millersep_filler);

	update_string(&cfg.vborder_filler, NULL);
	update_string(&cfg.hborder_filler, NULL);
	update_string(&cfg.millersep_filler, NULL);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
