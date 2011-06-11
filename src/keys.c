/* vifm
 * Copyright (C) 2011 xaizek.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

#include "keys.h"

struct key_chunk_t
{
	wchar_t key;
	struct key_t conf;
	struct key_chunk_t *child;
	struct key_chunk_t *next;
};

static struct key_chunk_t *cmds_root;
static struct key_chunk_t *selectors_root;
static int *mode;
static int *mode_flags;
static default_handler *def_handlers;

static int execute_keys_general(const wchar_t *keys, int timed_out, int mapped);
static int execute_keys_inner(const wchar_t *keys, struct keys_info *keys_info);
static int execute_next_keys(struct key_chunk_t *curr, const wchar_t *keys,
		struct key_info *key_info, struct keys_info *keys_info);
static int run_cmd(struct key_info key_info, struct keys_info *keys_info,
		struct key_t *key_t);
static void init_keys_info(struct keys_info *keys_info, int mapped);
static const wchar_t* get_reg(const wchar_t *keys, int *reg);
static const wchar_t* get_count(const wchar_t *keys, int *count);
static struct key_t* add_keys_inner(struct key_chunk_t *root,
		const wchar_t *keys);

void
init_keys(int modes_count, int *key_mode, int *key_mode_flags)
{
	assert(key_mode != NULL);
	assert(key_mode_flags != NULL);
	assert(modes_count > 0);

	mode = key_mode;
	mode_flags = key_mode_flags;

	cmds_root = calloc(modes_count, sizeof(*cmds_root));
	assert(cmds_root != NULL);
	selectors_root = calloc(modes_count, sizeof(*selectors_root));
	assert(selectors_root != NULL);

	def_handlers = calloc(modes_count, sizeof(*def_handlers));
	assert(def_handlers != NULL);
}

void
set_def_handler(int mode, default_handler handler)
{
	def_handlers[mode] = handler;
}

int
execute_keys(const wchar_t *keys)
{
	return execute_keys_general(keys, 0, 0);
}

int execute_keys_timed_out(const wchar_t *keys)
{
	return execute_keys_general(keys, 1, 0);
}

static int
execute_keys_general(const wchar_t *keys, int timed_out, int mapped)
{
	int result;
	struct keys_info keys_info;

	if(keys[0] == L'\0')
		return KEYS_UNKNOWN;

	init_keys_info(&keys_info, mapped);
	keys_info.after_wait = timed_out;
	result = execute_keys_inner(keys, &keys_info);
	if(result == KEYS_UNKNOWN && def_handlers[*mode] != NULL)
	{
		result = def_handlers[*mode](*keys);
		execute_keys_general(keys + 1, 0, mapped);
	}
	return result;
}

static int
execute_keys_inner(const wchar_t *keys, struct keys_info *keys_info)
{
	struct key_chunk_t *root, *curr;
	struct key_info key_info;

	keys = get_reg(keys, &key_info.reg);
	if(keys == NULL)
	{
		return KEYS_WAIT;
	}
	keys = get_count(keys, &key_info.count);
	root = keys_info->selector ? &selectors_root[*mode] : &cmds_root[*mode];
	curr = root;
	while(*keys != L'\0')
	{
		struct key_chunk_t *p;
		p = curr->child;
		while(p != NULL && p->key < *keys)
		{
			p = p->next;
		}
		if(p == NULL || p->key != *keys)
		{
			int result;
			if(curr == root)
			{
				return KEYS_UNKNOWN;
			}
			if(curr->conf.followed != FOLLOWED_BY_NONE)
			{
				break;
			}
			if(curr->conf.type == BUILDIN_WAIT_POINT)
			{
				return KEYS_UNKNOWN;
			}
			result = execute_next_keys(curr, L"", &key_info, keys_info);
			if(IS_KEYS_RET_CODE(result))
			{
				if(result == KEYS_WAIT_SHORT)
				{
					return KEYS_UNKNOWN;
				}
				return result;
			}
			return execute_keys_general(keys, 0, keys_info->mapped);
		}
		keys++;
		curr = p;
	}
	return execute_next_keys(curr, keys, &key_info, keys_info);
}

static int
execute_next_keys(struct key_chunk_t *curr, const wchar_t *keys,
		struct key_info *key_info, struct keys_info *keys_info)
{
	if(*keys == L'\0')
	{
		int wait_point = (curr->conf.type == BUILDIN_WAIT_POINT);
		if(wait_point)
		{
			int with_input = (mode_flags[*mode] & MF_USES_INPUT);
			if(!keys_info->after_wait)
			{
				return with_input ? KEYS_WAIT_SHORT : KEYS_WAIT;
			}
		}
		else if(curr->conf.data.handler == NULL
				|| curr->conf.followed != FOLLOWED_BY_NONE)
		{
			return KEYS_UNKNOWN;
		}
	}
	else
	{
		int result;
		if(keys[1] == L'\0' && curr->conf.followed == FOLLOWED_BY_MULTIKEY)
		{
			key_info->multi = keys[0];
			return run_cmd(*key_info, keys_info, &curr->conf);
		}
		keys_info->selector = 1;
		result = execute_keys_inner(keys, keys_info);
		keys_info->selector = 0;
		if(IS_KEYS_RET_CODE(result))
		{
			return result;
		}
	}
	return run_cmd(*key_info, keys_info, &curr->conf);
}

static int
run_cmd(struct key_info key_info, struct keys_info *keys_info,
		struct key_t *key_t)
{
	if(key_t->type != USER_CMD && key_t->type != BUILDIN_CMD)
	{
		if(key_t->data.handler == NULL)
			return KEYS_UNKNOWN;
		key_t->data.handler(key_info, keys_info);
		return 0;
	}
	else
	{
		int result;
		struct keys_info keys_info;
		init_keys_info(&keys_info, 1);

		result = execute_keys_inner(key_t->data.cmd, &keys_info);
		if(result == KEYS_UNKNOWN && def_handlers[*mode] != NULL)
		{
			result = def_handlers[*mode](*key_t->data.cmd);
			init_keys_info(&keys_info, 1);
			execute_keys_inner(key_t->data.cmd + 1, &keys_info);
		}
		return result;
	}
}

static void
init_keys_info(struct keys_info *keys_info, int mapped)
{
	keys_info->selector = 0;
	keys_info->count = 0;
	keys_info->indexes = NULL;
	keys_info->after_wait = 0;
	keys_info->mapped = mapped;
}

static const wchar_t*
get_reg(const wchar_t *keys, int *reg)
{
	if((mode_flags[*mode] & MF_USES_REGS) == 0)
	{
		return keys;
	}
	if(keys[0] == L'"')
	{
		if(keys[1] == L'\0')
		{
			return NULL;
		}
		*reg = keys[1];
		keys += 2;
	}
	else
	{
		*reg = NO_REG_GIVEN;
	}

	return keys;
}

static const wchar_t*
get_count(const wchar_t *keys, int *count)
{
	if((mode_flags[*mode] & MF_USES_COUNT) == 0)
	{
		return keys;
	}
	if(keys[0] != L'0' && isdigit(keys[0]))
	{
		*count = wcstof(keys, (wchar_t**)&keys);
	}
	else
	{
		*count = NO_COUNT_GIVEN;
	}

	return keys;
}

/* Returns:
 * -1 - can't remap buildin keys
 */
int
add_user_keys(const wchar_t *keys, const wchar_t *cmd, int mode)
{
	struct key_t *curr;
	curr = add_cmd(keys, mode);
	if(curr->type != USER_CMD && curr->data.handler != NULL)
	{
		return -1;
	}
	else if(curr->type == USER_CMD)
	{
		free((void*)curr->data.cmd);
	}
	curr->type = USER_CMD;
	curr->data.cmd = my_wcsdup(cmd);
	return 0;
}

struct key_t*
add_cmd(const wchar_t *keys, int mode)
{
	return add_keys_inner(&cmds_root[mode], keys);
}

struct key_t*
add_selector(const wchar_t *keys, int mode)
{
	return add_keys_inner(&selectors_root[mode], keys);
}

static struct key_t*
add_keys_inner(struct key_chunk_t *root, const wchar_t *keys)
{
	struct key_chunk_t *curr = root;
	while(*keys != L'\0')
	{
		struct key_chunk_t *prev, *p;
		prev = NULL;
		p = curr->child;
		while(p != NULL && p->key < *keys)
		{
			prev = p;
			p = p->next;
		}
		if(p == NULL || p->key != *keys)
		{
			struct key_chunk_t *c = malloc(sizeof(*c));
			if(c == NULL)
			{
				return NULL;
			}
			c->key = *keys;
			c->conf.type = (keys[1] == L'\0') ? BUILDIN_KEYS : BUILDIN_WAIT_POINT;
			c->conf.data.handler = NULL;
			c->conf.followed = FOLLOWED_BY_NONE;
			c->next = p;
			c->child = NULL;
			if(prev == NULL)
			{
				curr->child = c;
			}
			else
			{
				prev->next = c;
			}
			p = c;
		}
		keys++;
		curr = p;
	}
	return &curr->conf;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
