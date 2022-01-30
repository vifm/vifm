#include <stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/cmds.h"
#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/cmd_core.h"
#include "../../src/event_loop.h"

static void x_key(key_info_t key_info, keys_info_t *keys_info);
static void set_pending_key(key_info_t key_info, keys_info_t *keys_info);
static void check_pending_key(key_info_t key_info, keys_info_t *keys_info);

static int quit;

SETUP()
{
	curr_view = &lwin;
	other_view = &rwin;

	view_setup(&lwin);
	view_setup(&rwin);

	init_modes();
	init_commands();

	cfg.timeout_len = 1;
}

TEARDOWN()
{
	view_teardown(&lwin);
	view_teardown(&rwin);
	vle_cmds_reset();
	vle_keys_reset();
}

TEST(quit_immediately)
{
	int quit = 1;
	event_loop(&quit, /*manage_marking=*/1);
}

TEST(quit_on_key_press)
{
	keys_add_info_t keys = { WK_x, { {&x_key} } };
	vle_keys_add(&keys, 1U, NORMAL_MODE);

	feed_keys(L"x");

	quit = 0;
	event_loop(&quit, /*manage_marking=*/1);
}

TEST(pending_flags_are_reset)
{
	keys_add_info_t x_key = { WK_x, { {&set_pending_key} } };
	vle_keys_add(&x_key, 1U, NORMAL_MODE);

	keys_add_info_t w_key = { WK_w, { {&check_pending_key} } };
	vle_keys_add(&w_key, 1U, NORMAL_MODE);

	feed_keys(L"xw");

	quit = 0;
	event_loop(&quit, /*manage_marking=*/1);

	lwin.pending_marking = 0;
	rwin.pending_marking = 0;
}

static void
x_key(key_info_t key_info, keys_info_t *keys_info)
{
	quit = 1;
}

static void
set_pending_key(key_info_t key_info, keys_info_t *keys_info)
{
	lwin.pending_marking = 1;
	rwin.pending_marking = 1;
}

static void
check_pending_key(key_info_t key_info, keys_info_t *keys_info)
{
	assert_false(lwin.pending_marking);
	assert_false(rwin.pending_marking);
	quit = 1;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
