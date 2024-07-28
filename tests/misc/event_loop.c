#include <stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/cmds.h"
#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/lua/vlua.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/cmd_core.h"
#include "../../src/event_loop.h"
#include "../../src/status.h"

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

	modes_init();
	cmds_init();

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

TEST(quit_on_key_press_user_defined_ctrl_z)
{
	key_conf_t key = { { &x_key} };
	assert_success(vle_keys_foreign_add(WK_C_z, &key, /*is_selector=*/0,
				NORMAL_MODE));
	assert_true(vle_mode_get() == NORMAL_MODE);
	assert_true(vle_keys_user_exists(WK_C_z, NORMAL_MODE));

	feed_keys(WK_C_z);

	quit = 0;
	event_loop(&quit, /*manage_marking=*/1);
}

TEST(pending_flags_are_reset)
{
	/* This is just to test more parts of the loop. */
	curr_stats.vlua = vlua_init();

	keys_add_info_t x_key = { WK_x, { {&set_pending_key} } };
	vle_keys_add(&x_key, 1U, NORMAL_MODE);

	keys_add_info_t w_key = { WK_w, { {&check_pending_key} } };
	vle_keys_add(&w_key, 1U, NORMAL_MODE);

	feed_keys(L"xw");

	quit = 0;
	event_loop(&quit, /*manage_marking=*/1);

	lwin.pending_marking = 0;
	rwin.pending_marking = 0;

	vlua_finish(curr_stats.vlua);
	curr_stats.vlua = NULL;
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
