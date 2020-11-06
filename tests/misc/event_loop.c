#include <stic.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/cmds.h"
#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/cmd_core.h"
#include "../../src/event_loop.h"

static void x_key(key_info_t key_info, keys_info_t *keys_info);

static int quit;

SETUP()
{
	curr_view = &lwin;
	other_view = &rwin;

	init_modes();
	init_commands();

	cfg.timeout_len = 1;
}

TEARDOWN()
{
	vle_cmds_reset();
	vle_keys_reset();
}

TEST(quit_immediately)
{
	int quit = 1;
	event_loop(&quit);
}

TEST(quit_on_key_press)
{
	keys_add_info_t keys = { WK_x, { {&x_key} } };
	vle_keys_add(&keys, 1U, NORMAL_MODE);

	feed_keys(L"x");

	quit = 0;
	event_loop(&quit);
}

static void
x_key(key_info_t key_info, keys_info_t *keys_info)
{
	quit = 1;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
