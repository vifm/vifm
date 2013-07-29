#include "builtin_keys.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

#ifdef TEST
#define printf(...)
#endif

int last; /* 1 = k, 2 = j */
int last_command_count; /* for ctrl+w <, dd and d + selector*/
int last_selector_count; /* for k */

static int* mode;

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
static void keys_v(key_info_t key_info, keys_info_t *keys_info);
static void keys_quit(key_info_t key_info, keys_info_t *keys_info);
static void keys_norm(key_info_t key_info, keys_info_t *keys_info);

void
init_builtin_keys(int *key_mode)
{
	key_conf_t *curr;

	assert(key_mode != NULL);

	mode = key_mode;

	curr = add_cmd(L":", NORMAL_MODE);
	curr->data.handler = keys_colon;

	curr = add_cmd(L"m", NORMAL_MODE);
	curr->type = BUILTIN_WAIT_POINT;
	curr->data.handler = keys_m;
	curr->followed = FOLLOWED_BY_MULTIKEY;

	curr = add_cmd(L"'", NORMAL_MODE);
	curr->type = BUILTIN_WAIT_POINT;
	curr->data.handler = keys_quote;
	curr->followed = FOLLOWED_BY_MULTIKEY;

	curr = add_selector(L"gg", NORMAL_MODE);
	curr->type = BUILTIN_WAIT_POINT;
	curr->data.handler = keys_gg;
	curr->followed = FOLLOWED_BY_NONE;

	curr = add_selector(L"'", NORMAL_MODE);
	curr->type = BUILTIN_WAIT_POINT;
	curr->data.handler = keys_quote;
	curr->followed = FOLLOWED_BY_MULTIKEY;

	curr = add_cmd(L"H", NORMAL_MODE);
	curr->data.handler = keys_H;

	curr = add_cmd(L"gu", NORMAL_MODE);
	curr->data.handler = keys_gu;
	curr->followed = FOLLOWED_BY_SELECTOR;
	curr->type = BUILTIN_WAIT_POINT;

	curr = add_cmd(L"guu", NORMAL_MODE);
	curr->data.handler = keys_gu;

	curr = add_cmd(L"gugu", NORMAL_MODE);
	curr->data.handler = keys_gu;

	curr = add_cmd(L"j", NORMAL_MODE);
	curr->data.handler = keys_j;

	curr = add_selector(L"j", NORMAL_MODE);
	curr->data.handler = keys_j;

	curr = add_cmd(L"j", VISUAL_MODE);
	curr->data.handler = keys_j;

	curr = add_selector(L"j", VISUAL_MODE);
	curr->data.handler = keys_j;

	curr = add_cmd(L"k", NORMAL_MODE);
	curr->data.handler = keys_k;

	curr = add_selector(L"k", NORMAL_MODE);
	curr->data.handler = keys_k;

	curr = add_cmd(L"k", VISUAL_MODE);
	curr->data.handler = keys_k;

	curr = add_selector(L"k", VISUAL_MODE);
	curr->data.handler = keys_k;

	curr = add_selector(L"s", NORMAL_MODE);
	curr->data.handler = keys_s;

	curr = add_selector(L"s", VISUAL_MODE);
	curr->data.handler = keys_s;

	curr = add_cmd(L"i", NORMAL_MODE);
	curr->data.handler = keys_i;

	curr = add_selector(L"if", NORMAL_MODE);
	curr->data.handler = keys_if;

	curr = add_selector(L"if", VISUAL_MODE);
	curr->data.handler = keys_if;

	curr = add_cmd(L"o", NORMAL_MODE);
	curr->type = BUILTIN_CMD;
	curr->data.cmd = wcsdup(L":only");

	curr = add_cmd(L"v", NORMAL_MODE);
	curr->type = BUILTIN_CMD;
	curr->data.cmd = wcsdup(L":vsplit");

	curr = add_cmd(L"<", NORMAL_MODE);
	curr->type = BUILTIN_NIM_KEYS;
	curr->data.handler = keys_ctrl_w_less_than;

	curr = add_cmd(L"d", NORMAL_MODE);
	curr->type = BUILTIN_WAIT_POINT;
	curr->data.handler = keys_delete_selector;
	curr->followed = FOLLOWED_BY_SELECTOR;

	curr = add_cmd(L"dd", NORMAL_MODE);
	curr->data.handler = keys_delete;
	curr->type = BUILTIN_NIM_KEYS;

	curr = add_cmd(L"v", NORMAL_MODE);
	curr->data.handler = keys_v;

	curr = add_cmd(L"v", VISUAL_MODE);
	curr->data.handler = keys_v;

	curr = add_cmd(L"ZQ", NORMAL_MODE);
	curr->data.handler = keys_quit;

	curr = add_cmd(L"ZZ", NORMAL_MODE);
	curr->data.handler = keys_quit;

	curr = add_cmd(L"ZZ", VISUAL_MODE);
	curr->data.handler = keys_quit;

	curr = add_cmd(L"norm", NORMAL_MODE);
	curr->data.handler = keys_norm;
}

static void
keys_colon(key_info_t key_info, keys_info_t *keys_info)
{
	*mode = CMDLINE_MODE;
}

static void
keys_m(key_info_t key_info, keys_info_t *keys_info)
{
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
	wchar_t **list, **p;
	list = list_cmds(NORMAL_MODE);

	if(list == NULL)
	{
		printf("%s\n", "error");
		return;
	}
	
	p = list;
	while(*p != NULL)
	{
		printf("%ls\n", *p);
		free(*p++);
	}
	free(list);
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
	last_command_count = key_info.count;
}

static void
keys_delete_selector(key_info_t key_info, keys_info_t *keys_info)
{
	last_command_count = key_info.count;
	printf("(%d)delete with selector in register %c\n", key_info.count,
			key_info.reg);
}

static void
keys_v(key_info_t key_info, keys_info_t *keys_info)
{
	*mode = (*mode == NORMAL_MODE) ? VISUAL_MODE : NORMAL_MODE;
	printf("v visual mode toggle\n");
}

static void
keys_quit(key_info_t key_info, keys_info_t *keys_info)
{
	printf("(%d)quit in register %c\n", key_info.count, key_info.reg);
}

static void
keys_norm(key_info_t key_info, keys_info_t *keys_info)
{
	execute_keys_timed_out(L"ZZ");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
