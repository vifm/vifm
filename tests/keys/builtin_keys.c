#include "builtin_keys.h"

#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/utils/macros.h"

int last;
int last_command_count;
int last_command_register;
int last_selector_count;
int last_indexes_count;
int key_is_mapped;
int mapping_state;

static void keys_colon(key_info_t key_info, keys_info_t *keys_info);
static void keys_m(key_info_t key_info, keys_info_t *keys_info);
static void m_suggest(vle_keys_list_cb cb);
static void keys_quote(key_info_t key_info, keys_info_t *keys_info);
static void keys_gg(key_info_t key_info, keys_info_t *keys_info);
static void keys_H(key_info_t key_info, keys_info_t *keys_info);
static void keys_gu(key_info_t key_info, keys_info_t *keys_info);
static void keys_j(key_info_t key_info, keys_info_t *keys_info);
static void keys_k(key_info_t key_info, keys_info_t *keys_info);
static void keys_s(key_info_t key_info, keys_info_t *keys_info);
static void keys_i(key_info_t key_info, keys_info_t *keys_info);
static void keys_if(key_info_t key_info, keys_info_t *keys_info);
static void keys_ctrl_w_less_than(key_info_t key_info, keys_info_t *keys_info);
static void keys_delete(key_info_t key_info, keys_info_t *keys_info);
static void keys_delete_selector(key_info_t key_info, keys_info_t *keys_info);
static void keys_yank_selector(key_info_t key_info, keys_info_t *keys_info);
static void keys_v(key_info_t key_info, keys_info_t *keys_info);
static void keys_quit(key_info_t key_info, keys_info_t *keys_info);
static void keys_norm(key_info_t key_info, keys_info_t *keys_info);

static keys_add_info_t normal_cmds[] = {
	{WK_COLON,            {{&keys_colon}}},
	{WK_m,                {{&keys_m}, FOLLOWED_BY_MULTIKEY, .suggest = &m_suggest}},
	{WK_QUOTE,            {{&keys_quote}, FOLLOWED_BY_MULTIKEY}},
	{WK_H,                {{&keys_H}}},
	{WK_g WK_u,           {{&keys_gu}, FOLLOWED_BY_SELECTOR}},
	{WK_g WK_u WK_u,      {{&keys_gu}}},
	{WK_g WK_u WK_g WK_u, {{&keys_gu}}},
	{WK_g WK_u WK_g WK_g, {{&keys_gu}, .skip_suggestion = 1}},
	{WK_j,                {{&keys_j}}},
	{WK_k,                {{&keys_k}}},
	{WK_i,                {{&keys_i}}},
	{WK_C_w WK_LT,        {{&keys_ctrl_w_less_than}, .nim = 1}},
	{WK_d,                {{&keys_delete_selector}, FOLLOWED_BY_SELECTOR}},
	{WK_d WK_d,           {{&keys_delete}, .nim = 1}},
	{WK_v,                {{&keys_v}}},
	{WK_y,                {{&keys_yank_selector}, FOLLOWED_BY_SELECTOR}},
	{WK_Z WK_Q,           {{&keys_quit}}},
	{WK_Z WK_Z,           {{&keys_quit}}},
	{WK_n WK_o WK_r WK_m, {{&keys_norm}}},
};

static keys_add_info_t normal_selectors[] = {
	{WK_g WK_g,           {{&keys_gg}}},
	{WK_QUOTE,            {{&keys_quote}, FOLLOWED_BY_MULTIKEY}},
};

static keys_add_info_t visual_cmds[] = {
	{WK_j,                {{&keys_j}}},
	{WK_k,                {{&keys_k}}},
	{WK_v,                {{&keys_v}}},
	{WK_Z WK_Z,           {{&keys_quit}}},
};

static keys_add_info_t common_selectors[] = {
	{WK_j,                {{&keys_j}}},
	{WK_k,                {{&keys_k}}},
	{WK_s,                {{&keys_s}}},
	{WK_i WK_f,           {{&keys_if}}},
};

void
init_builtin_keys(void)
{
	assert_success(vle_keys_add(normal_cmds, ARRAY_LEN(normal_cmds),
				NORMAL_MODE));
	assert_success(vle_keys_add(visual_cmds, ARRAY_LEN(visual_cmds),
				VISUAL_MODE));

	assert_success(vle_keys_add_selectors(normal_selectors,
				ARRAY_LEN(normal_selectors), NORMAL_MODE));
	assert_success(vle_keys_add_selectors(common_selectors,
				ARRAY_LEN(common_selectors), NORMAL_MODE));
	assert_success(vle_keys_add_selectors(common_selectors,
				ARRAY_LEN(common_selectors), VISUAL_MODE));
}

static void
keys_colon(key_info_t key_info, keys_info_t *keys_info)
{
	vle_mode_set(CMDLINE_MODE, VMT_SECONDARY);
	key_is_mapped = keys_info->mapped;
	mapping_state = vle_keys_mapping_state();
}

static void
keys_m(key_info_t key_info, keys_info_t *keys_info)
{
	key_is_mapped = keys_info->mapped;
	mapping_state = vle_keys_mapping_state();
}

static void
m_suggest(vle_keys_list_cb cb)
{
	cb(L"a", L"this dir", "");
	cb(L"z", L"that dir", "");
}

static void
keys_quote(key_info_t key_info, keys_info_t *keys_info)
{
}

static void
keys_gg(key_info_t key_info, keys_info_t *keys_info)
{
}

static void
keys_H(key_info_t key_info, keys_info_t *keys_info)
{
}

static void
keys_gu(key_info_t key_info, keys_info_t *keys_info)
{
	last = 2;
}

static void
keys_j(key_info_t key_info, keys_info_t *keys_info)
{
	last = 2;
}

static void
keys_k(key_info_t key_info, keys_info_t *keys_info)
{
	last = 1;
	if(keys_info->selector)
	{
		last_selector_count = key_info.count;
	}
}

static void
keys_s(key_info_t key_info, keys_info_t *keys_info)
{
}

static void
keys_i(key_info_t key_info, keys_info_t *keys_info)
{
}

static void
keys_if(key_info_t key_info, keys_info_t *keys_info)
{
}

static void
keys_ctrl_w_less_than(key_info_t key_info, keys_info_t *keys_info)
{
	last_command_count = key_info.count;
}

static void
keys_delete(key_info_t key_info, keys_info_t *keys_info)
{
	last = 4;
	last_command_count = key_info.count;
	last_command_register = key_info.reg;
}

static void
keys_delete_selector(key_info_t key_info, keys_info_t *keys_info)
{
	last = 4;
	last_command_count = key_info.count;
	last_indexes_count = keys_info->count;
}

static void
keys_v(key_info_t key_info, keys_info_t *keys_info)
{
	vle_mode_set(vle_mode_is(NORMAL_MODE) ? VISUAL_MODE : NORMAL_MODE,
			VMT_PRIMARY);
}

static void
keys_yank_selector(key_info_t key_info, keys_info_t *keys_info)
{
	last = 3;
}

static void
keys_quit(key_info_t key_info, keys_info_t *keys_info)
{
}

static void
keys_norm(key_info_t key_info, keys_info_t *keys_info)
{
	vle_keys_exec_timed_out(L"ZZ");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
