#include "builtin_keys.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/utils/macros.h"

#ifdef TEST
#define printf(...) do {} while(0)
#endif

int last; /* 1 = k, 2 = j, 3 = yank, 4 = delete */
int last_command_count; /* for ctrl+w <, dd and d + selector*/
int last_selector_count; /* for k */
int key_is_mapped; /* for : and m */
int is_in_maping_state; /* for : and m */

static void keys_colon(key_info_t key_info, keys_info_t *keys_info);
static void keys_m(key_info_t key_info, keys_info_t *keys_info);
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
	{WK_m,                {{&keys_m}, FOLLOWED_BY_MULTIKEY}},
	{WK_QUOTE,            {{&keys_quote}, FOLLOWED_BY_MULTIKEY}},
	{WK_H,                {{&keys_H}}},
	{WK_g WK_u,           {{&keys_gu}, FOLLOWED_BY_SELECTOR}},
	{WK_g WK_u WK_u,      {{&keys_gu}}},
	{WK_g WK_u WK_g WK_u, {{&keys_gu}}},
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
	is_in_maping_state = vle_keys_inside_mapping();
}

static void
keys_m(key_info_t key_info, keys_info_t *keys_info)
{
	key_is_mapped = keys_info->mapped;
	is_in_maping_state = vle_keys_inside_mapping();
	printf("(%d)m in register '%c' with multikey '%c'\n",
			key_info.count, key_info.reg, key_info.multi);
}

static void
keys_quote(key_info_t key_info, keys_info_t *keys_info)
{
	if(keys_info->selector)
		printf("as a selector: ");
	printf("(%d)' in register '%c' with multikey '%c'\n",
			key_info.count, key_info.reg, key_info.multi);
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
	if(keys_info->selector)
		printf("as a selector: ");
	printf("(%d)gu in register %c\n", key_info.count, key_info.reg);
}

static void
keys_j(key_info_t key_info, keys_info_t *keys_info)
{
	last = 2;
	if(keys_info->selector)
		printf("as a selector: ");
	printf("(%d)j in register %c\n", key_info.count, key_info.reg);
}

static void
keys_k(key_info_t key_info, keys_info_t *keys_info)
{
	last = 1;
	if(keys_info->selector)
	{
		last_selector_count = key_info.count;
		printf("as a selector: ");
	}
	printf("(%d)k in register %c\n", key_info.count, key_info.reg);
}

static void
keys_s(key_info_t key_info, keys_info_t *keys_info)
{
	if(keys_info->selector)
		printf("as a selector: ");
	printf("(%d)s in register %c\n", key_info.count, key_info.reg);
}

static void
keys_i(key_info_t key_info, keys_info_t *keys_info)
{
	if(keys_info->selector)
		printf("as a selector: ");
	printf("(%d)i in register %c\n", key_info.count, key_info.reg);
}

static void
keys_if(key_info_t key_info, keys_info_t *keys_info)
{
	if(keys_info->selector)
		printf("as a selector: ");
	printf("(%d)if in register %c\n", key_info.count, key_info.reg);
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
}

static void
keys_delete_selector(key_info_t key_info, keys_info_t *keys_info)
{
	last = 4;
	last_command_count = key_info.count;
	printf("(%d)delete with selector in register %c\n", key_info.count,
			key_info.reg);
}

static void
keys_v(key_info_t key_info, keys_info_t *keys_info)
{
	vle_mode_set(vle_mode_is(NORMAL_MODE) ? VISUAL_MODE : NORMAL_MODE,
			VMT_PRIMARY);
	printf("v visual mode toggle\n");
}

static void
keys_yank_selector(key_info_t key_info, keys_info_t *keys_info)
{
	last = 3;
}

static void
keys_quit(key_info_t key_info, keys_info_t *keys_info)
{
	printf("(%d)quit in register %c\n", key_info.count, key_info.reg);
}

static void
keys_norm(key_info_t key_info, keys_info_t *keys_info)
{
	vle_keys_exec_timed_out(L"ZZ");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
