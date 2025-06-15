#include <stic.h>

#include <test-utils.h>

#include "../../src/cfg/config.h"
#include "../../src/engine/cmds.h"
#include "../../src/engine/completion.h"
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
static void X_key(key_info_t key_info, keys_info_t *keys_info);
static void set_pending_key(key_info_t key_info, keys_info_t *keys_info);
static void check_pending_key(key_info_t key_info, keys_info_t *keys_info);

static int quit;
static char last_key;

SETUP()
{
	curr_view = &lwin;
	other_view = &rwin;

	view_setup(&lwin);
	view_setup(&rwin);

	modes_init();
	cmds_init();

	cfg.timeout_len = 1;

	last_key = '\0';
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

TEST(escape_cancels_waiting_of_key_sequence)
{
	keys_add_info_t keys[] = {
		{ WK_x, { {&x_key} } },
		{ WK_X, { {&X_key} } },
	};
	vle_keys_add(keys, 2U, NORMAL_MODE);

	key_conf_t x_key_info = { { &x_key } };
	assert_success(vle_keys_foreign_add(WK_x WK_X, &x_key_info, /*is_selector=*/0,
				NORMAL_MODE));

	feed_keys(WK_x WK_ESC WK_X);

	quit = 0;
	event_loop(&quit, /*manage_marking=*/1);

	assert_int_equal('X', last_key);
}

/* Verify that Escape doesn't cancel input if it completes a mapping. */
TEST(can_have_escape_key_at_the_end)
{
	keys_add_info_t keys = { WK_x, { {&x_key} } };
	vle_keys_add(&keys, 1U, NORMAL_MODE);

	key_conf_t X_key_info = { { &X_key } };
	assert_success(vle_keys_foreign_add(WK_x WK_ESC, &X_key_info,
				/*is_selector=*/0, NORMAL_MODE));

	feed_keys(WK_x WK_ESC);

	quit = 0;
	event_loop(&quit, /*manage_marking=*/1);

	assert_int_equal('X', last_key);
}

/* Verify that Escape doesn't cancel input if there is a mapping which includes
 * it. */
TEST(can_have_escape_key_in_the_middle)
{
	keys_add_info_t keys = { WK_x, { {&x_key} } };
	vle_keys_add(&keys, 1U, NORMAL_MODE);

	key_conf_t X_key_info = { { &X_key } };
	assert_success(vle_keys_foreign_add(WK_x WK_ESC WK_X, &X_key_info,
				/*is_selector=*/0, NORMAL_MODE));

	feed_keys(WK_x WK_ESC WK_X);

	quit = 0;
	event_loop(&quit, /*manage_marking=*/1);

	assert_int_equal('X', last_key);
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

TEST(key_suggestions)
{
	cfg.sug.flags = SF_NORMAL | SF_KEYS;
	cfg.min_timeout_len = 1;

	keys_add_info_t x_key = { WK_X, { {&X_key} } };
	vle_keys_add(&x_key, 1U, NORMAL_MODE);

	assert_success(vle_keys_user_add(L"Xj", L"j", "Xj help", NORMAL_MODE,
				KEYS_FLAG_NONE));
	assert_success(vle_keys_user_add(L"Xk", L"k", "Xk help", NORMAL_MODE,
				KEYS_FLAG_NONE));

	feed_keys(L"X");

	quit = 0;
	event_loop(&quit, /*manage_marking=*/1);

	const vle_compl_t *items = vle_compl_get_items();
	assert_int_equal(2, vle_compl_get_count());
	assert_string_equal("key: j", items[0].text);
	assert_string_equal("Xj help", items[0].descr);
	assert_string_equal("key: k", items[1].text);
	assert_string_equal("Xk help", items[1].descr);

	cfg.sug.flags = 0;
	cfg.min_timeout_len = 0;
}

static void
x_key(key_info_t key_info, keys_info_t *keys_info)
{
	last_key = 'x';
	quit = 1;
}

static void
X_key(key_info_t key_info, keys_info_t *keys_info)
{
	last_key = 'X';
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
