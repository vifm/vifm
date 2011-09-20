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
#include <wchar.h> /* swprintf */

#include "utils.h"

#include "keys.h"

struct key_chunk_t
{
	wchar_t key;
	int no_remap;
	size_t children_count;
	int enters; /* to prevent stack overflow */
	struct key_t conf;
	struct key_chunk_t *child;
	struct key_chunk_t *parent;
	struct key_chunk_t *prev, *next;
};

static struct key_chunk_t *builtin_cmds_root;
static struct key_chunk_t *selectors_root;
static struct key_chunk_t *user_cmds_root;
static int max_modes;
static int *mode;
static int *mode_flags;
static default_handler *def_handlers;
static size_t counter;

static void free_tree(struct key_chunk_t *root);
static int execute_keys_general(const wchar_t *keys, int timed_out, int mapped,
		int no_remap);
static int execute_keys_inner(const wchar_t *keys, struct keys_info *keys_info,
		int no_remap);
static int execute_keys_loop(const wchar_t *keys, struct keys_info *keys_info,
		struct key_chunk_t *root, struct key_info key_info, int no_remap);
static int contains_chain(struct key_chunk_t *root, const wchar_t *begin,
		const wchar_t *end);
static int execute_next_keys(struct key_chunk_t *curr, const wchar_t *keys,
		struct key_info *key_info, struct keys_info *keys_info, int has_duplicate,
		int no_remap);
static int run_cmd(struct key_info key_info, struct keys_info *keys_info,
		struct key_chunk_t *curr, const wchar_t *keys);
static void init_keys_info(struct keys_info *keys_info, int mapped);
static const wchar_t* get_reg(const wchar_t *keys, int *reg);
static const wchar_t* get_count(const wchar_t *keys, int *count);
static struct key_chunk_t * find_user_keys(const wchar_t *keys, int mode);
static struct key_chunk_t* add_keys_inner(struct key_chunk_t *root,
		const wchar_t *keys);
static int fill_list(struct key_chunk_t *curr, size_t len, wchar_t **list);

void
init_keys(int modes_count, int *key_mode, int *key_mode_flags)
{
	assert(key_mode != NULL);
	assert(key_mode_flags != NULL);
	assert(modes_count > 0);

	max_modes = modes_count;
	mode = key_mode;
	mode_flags = key_mode_flags;

	builtin_cmds_root = calloc(modes_count, sizeof(*builtin_cmds_root));
	assert(builtin_cmds_root != NULL);

	user_cmds_root = calloc(modes_count, sizeof(*user_cmds_root));
	assert(user_cmds_root != NULL);

	selectors_root = calloc(modes_count, sizeof(*selectors_root));
	assert(selectors_root != NULL);

	def_handlers = calloc(modes_count, sizeof(*def_handlers));
	assert(def_handlers != NULL);
}

void
clear_keys(void)
{
	int i;

	for(i = 0; i < max_modes; i++)
		free_tree(&builtin_cmds_root[i]);
	free(builtin_cmds_root);

	for(i = 0; i < max_modes; i++)
		free_tree(&user_cmds_root[i]);
	free(user_cmds_root);

	for(i = 0; i < max_modes; i++)
		free_tree(&selectors_root[i]);
	free(selectors_root);

	free(def_handlers);
}

void
clear_user_keys(void)
{
	int i;

	for(i = 0; i < max_modes; i++)
		free_tree(&user_cmds_root[i]);
	free(user_cmds_root);

	user_cmds_root = calloc(max_modes, sizeof(*user_cmds_root));
	assert(user_cmds_root != NULL);
}

static void
free_tree(struct key_chunk_t *root)
{
	if(root->child != NULL)
	{
		free_tree(root->child);
		free(root->child);
	}

	if(root->next != NULL)
	{
		free_tree(root->next);
		free(root->next);
	}

	if(root->conf.type == USER_CMD || root->conf.type == BUILDIN_CMD)
	{
		free(root->conf.data.cmd);
	}
}

void
set_def_handler(int mode, default_handler handler)
{
	def_handlers[mode] = handler;
}

int
execute_keys(const wchar_t *keys)
{
	return execute_keys_general(keys, 0, 0, 0);
}

int execute_keys_timed_out(const wchar_t *keys)
{
	return execute_keys_general(keys, 1, 0, 0);
}

static int
execute_keys_general(const wchar_t *keys, int timed_out, int mapped,
		int no_remap)
{
	int result;
	struct keys_info keys_info;

	if(keys[0] == L'\0')
		return KEYS_UNKNOWN;

	init_keys_info(&keys_info, mapped);
	keys_info.after_wait = timed_out;
	result = execute_keys_inner(keys, &keys_info, no_remap);
	if(result == KEYS_UNKNOWN && def_handlers[*mode] != NULL)
	{
		result = def_handlers[*mode](keys[0]);
		execute_keys_general(keys + 1, 0, mapped, no_remap);
	}
	return result;
}

static int
execute_keys_inner(const wchar_t *keys, struct keys_info *keys_info,
		int no_remap)
{
	struct key_info key_info;
	struct key_chunk_t *root;
	int result;

	keys = get_reg(keys, &key_info.reg);
	if(keys == NULL)
		return KEYS_WAIT;
	if(key_info.reg == L'\x1b' || key_info.reg == L'\x03')
		return 0;
	keys = get_count(keys, &key_info.count);
	root = keys_info->selector ?
		&selectors_root[*mode] : &user_cmds_root[*mode];

	if(!no_remap)
		result = execute_keys_loop(keys, keys_info, root, key_info, no_remap);
	else
		result = KEYS_UNKNOWN;
	if(result == KEYS_UNKNOWN && !keys_info->selector)
		result = execute_keys_loop(keys, keys_info, &builtin_cmds_root[*mode],
				key_info, no_remap);

	return result;
}

static int
execute_keys_loop(const wchar_t *keys, struct keys_info *keys_info,
		struct key_chunk_t *root, struct key_info key_info, int no_remap)
{
	struct key_chunk_t *curr;
	const wchar_t *keys_start = keys;
	int has_duplicate;
	int result;

	curr = root;
	while(*keys != L'\0')
	{
		struct key_chunk_t *p;
		p = curr->child;
		while(p != NULL && p->key < *keys)
			p = p->next;
		if(p == NULL || p->key != *keys)
		{
			if(curr == root)
				return KEYS_UNKNOWN;

			if(curr->conf.followed != FOLLOWED_BY_NONE)
				break;

			if(curr->conf.type == BUILDIN_WAIT_POINT)
				return KEYS_UNKNOWN;

			has_duplicate = root == &user_cmds_root[*mode] &&
					contains_chain(&builtin_cmds_root[*mode], keys_start, keys);
			result = execute_next_keys(curr, curr->conf.type == USER_CMD ? keys : L"",
					&key_info, keys_info, has_duplicate, no_remap);
			if(curr->conf.type == USER_CMD)
				return result;
			if(IS_KEYS_RET_CODE(result))
			{
				if(result == KEYS_WAIT_SHORT)
					return KEYS_UNKNOWN;

				return result;
			}
			counter += keys_info->mapped ? 0 : (keys - keys_start);
			return execute_keys_general(keys, 0, keys_info->mapped, no_remap);
		}
		keys++;
		curr = p;
	}

	if(*keys == '\0' && curr->conf.type != BUILDIN_WAIT_POINT &&
			curr->children_count > 0 && curr->conf.data.handler != NULL &&
			!keys_info->after_wait)
	{
		return KEYS_WAIT_SHORT;
	}

	has_duplicate = root == &user_cmds_root[*mode] &&
			contains_chain(&builtin_cmds_root[*mode], keys_start, keys);
	result = execute_next_keys(curr, keys, &key_info, keys_info, has_duplicate,
			no_remap);
	if(!IS_KEYS_RET_CODE(result))
	{
		counter += keys_info->mapped ? 0 : (keys - keys_start);
	}
	else if(*keys == '\0' && result == KEYS_UNKNOWN && curr->children_count > 0)
	{
		return keys_info->after_wait ? KEYS_UNKNOWN : KEYS_WAIT_SHORT;
	}
	return result;
}

static int
contains_chain(struct key_chunk_t *root, const wchar_t *begin,
		const wchar_t *end)
{
	struct key_chunk_t *curr;

	if(begin == end)
		return 0;

	curr = root;
	while(begin != end)
	{
		struct key_chunk_t *p;

		p = curr->child;
		while(p != NULL && p->key < *begin)
			p = p->next;

		if(p == NULL || p->key != *begin)
			return 0;

		begin++;
		curr = p;
	}
	return (curr->conf.followed == FOLLOWED_BY_NONE &&
			curr->conf.type != BUILDIN_WAIT_POINT);
}

static int
execute_next_keys(struct key_chunk_t *curr, const wchar_t *keys,
		struct key_info *key_info, struct keys_info *keys_info, int has_duplicate,
		int no_remap)
{
	if(*keys == L'\0')
	{
		int wait_point = (curr->conf.type == BUILDIN_WAIT_POINT);
		wait_point = wait_point || (curr->conf.type == USER_CMD &&
				curr->conf.followed != FOLLOWED_BY_NONE);
		if(wait_point)
		{
			int with_input = (mode_flags[*mode] & MF_USES_INPUT);
			if(!keys_info->after_wait)
			{
				return (with_input || has_duplicate) ? KEYS_WAIT_SHORT : KEYS_WAIT;
			}
		}
		else if(curr->conf.data.handler == NULL
				|| curr->conf.followed != FOLLOWED_BY_NONE)
		{
			return KEYS_UNKNOWN;
		}
	}
	else if(curr->conf.type != USER_CMD)
	{
		int result;
		if(keys[1] == L'\0' && curr->conf.followed == FOLLOWED_BY_MULTIKEY)
		{
			key_info->multi = keys[0];
			return run_cmd(*key_info, keys_info, curr, L"");
		}
		keys_info->selector = 1;
		result = execute_keys_inner(keys, keys_info, no_remap);
		keys_info->selector = 0;
		if(IS_KEYS_RET_CODE(result))
			return result;
	}
	return run_cmd(*key_info, keys_info, curr, keys);
}

static int
run_cmd(struct key_info key_info, struct keys_info *keys_info,
		struct key_chunk_t *curr, const wchar_t *keys)
{
	struct key_t *key_t = &curr->conf;

	if(key_t->type != USER_CMD && key_t->type != BUILDIN_CMD)
	{
		if(key_t->data.handler == NULL)
			return KEYS_UNKNOWN;
		key_t->data.handler(key_info, keys_info);
		return 0;
	}
	else
	{
		int result = (def_handlers[*mode] == NULL) ? KEYS_UNKNOWN : 0;
		struct keys_info ki;
		if(curr->conf.followed == FOLLOWED_BY_SELECTOR)
			ki = *keys_info;
		else
			init_keys_info(&ki, 1);

		if(curr->enters == 0)
		{
			wchar_t buf[16 + wcslen(key_t->data.cmd) + 1 + wcslen(keys) + 1];

			buf[0] = '\0';
			if(key_info.reg != NO_REG_GIVEN)
#ifndef _WIN32
				swprintf(buf, ARRAY_LEN(buf), L"\"%c", key_info.reg);
#else
				swprintf(buf, L"\"%c", key_info.reg);
#endif
			if(key_info.count != NO_COUNT_GIVEN)
#ifndef _WIN32
				swprintf(buf + wcslen(buf), ARRAY_LEN(buf) - wcslen(buf), L"%d",
						key_info.count);
#else
				swprintf(buf + wcslen(buf), L"%d", key_info.count);
#endif
			wcscat(buf, key_t->data.cmd);
			wcscat(buf, keys);

			curr->enters = 1;
			result = execute_keys_inner(buf, &ki, curr->no_remap);
			curr->enters = 0;
		}
		else if(def_handlers[*mode] != NULL)
		{
			result = def_handlers[*mode](curr->key);

			if(result == 0)
				result = execute_keys_general(keys, keys_info->after_wait, 0,
						curr->no_remap);
		}
		if(result == KEYS_UNKNOWN && def_handlers[*mode] != NULL)
		{
			if(curr->enters == 0)
			{
				result = def_handlers[*mode](key_t->data.cmd[0]);
				curr->enters = 1;
				execute_keys_general(key_t->data.cmd + 1, 0, 1, curr->no_remap);
				curr->enters = 0;
			}
			else
			{
				int i;
				for(i = 0; key_t->data.cmd[i] != '\0'; i++)
					result = def_handlers[*mode](key_t->data.cmd[i]);
			}
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

static const wchar_t *
get_reg(const wchar_t *keys, int *reg)
{
	*reg = NO_REG_GIVEN;
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

	return keys;
}

static const wchar_t *
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

#ifndef TEST
static
#endif
struct key_t*
add_cmd(const wchar_t *keys, int mode)
{
	struct key_chunk_t *curr = add_keys_inner(&builtin_cmds_root[mode], keys);
	return &curr->conf;
}

int
add_user_keys(const wchar_t *keys, const wchar_t *cmd, int mode, int no_r)
{
	struct key_chunk_t *curr;

	curr = add_keys_inner(&user_cmds_root[mode], keys);
	if(curr->conf.type == USER_CMD)
		free((void*)curr->conf.data.cmd);

	curr->conf.type = USER_CMD;
	curr->conf.data.cmd = my_wcsdup(cmd);
	curr->no_remap = no_r;
	return 0;
}

int
has_user_keys(const wchar_t *keys, int mode)
{
	return find_user_keys(keys, mode) != NULL;
}

int
remove_user_keys(const wchar_t *keys, int mode)
{
	struct key_chunk_t *curr, *p;

	if((curr = find_user_keys(keys, mode)) == NULL)
		return -1;

	free(curr->conf.data.cmd);
	curr->conf.type = BUILDIN_WAIT_POINT;
	curr->conf.data.handler = NULL;

	p = curr;
	while(p->parent != NULL)
	{
		p->parent->children_count--;
		p = p->parent;
	}

	if(curr->children_count > 0)
		return 0;

	do
	{
		struct key_chunk_t *parent = curr->parent;
		if(curr->prev != NULL)
			curr->prev->next = curr->next;
		else
			parent->child = curr->next;
		free(curr);
		curr = parent;
	} while(curr->parent != NULL && curr->parent->conf.data.handler == NULL &&
			curr->parent->conf.type == BUILDIN_WAIT_POINT &&
			curr->parent->children_count == 0);

	return 0;
}

static struct key_chunk_t *
find_user_keys(const wchar_t *keys, int mode)
{
	struct key_chunk_t *curr = &user_cmds_root[mode], *p;
	while(*keys != L'\0')
	{
		p = curr->child;
		while(p != NULL && p->key < *keys)
			p = p->next;
		if(p == NULL || p->key != *keys)
			return NULL;
		curr = p;
		keys++;
	}

	if(curr->conf.type != USER_CMD)
		return NULL;
	return curr;
}

#ifndef TEST
static
#endif
struct key_t *
add_selector(const wchar_t *keys, int mode)
{
	struct key_chunk_t *curr = add_keys_inner(&selectors_root[mode], keys);
	return &curr->conf;
}

int
add_cmds(struct keys_add_info *cmds, size_t len, int mode)
{
	int result = 0;
	size_t i;

	for(i = 0; i < len; i++)
	{
		struct key_t *curr;

		curr = add_cmd(cmds[i].keys, mode);
		if(curr == NULL)
			result = -1;
		else
			*curr = cmds[i].key_t;
	}

	return result;
}

int
add_selectors(struct keys_add_info *cmds, size_t len, int mode)
{
	int result = 0;
	size_t i;

	for(i = 0; i < len; i++)
	{
		struct key_t *curr;

		curr = add_selector(cmds[i].keys, mode);
		if(curr == NULL)
			result = -1;
		else
			*curr = cmds[i].key_t;
	}

	return result;
}

static struct key_chunk_t*
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
			c->prev = prev;
			c->next = p;
			c->child = NULL;
			c->parent = curr;
			c->children_count = 0;
			c->enters = 0;
			if(prev == NULL)
				curr->child = c;
			else
				prev->next = c;

			if(keys[1] == L'\0')
			{
				while(curr != NULL)
				{
					curr->children_count++;
					curr = curr->parent;
				}
			}

			p = c;
		}
		keys++;
		curr = p;
	}
	return curr;
}

wchar_t **
list_cmds(int mode)
{
	int i, j;
	int count;
	wchar_t **result;

	count = user_cmds_root[mode].children_count;
	count += 1;
	count += builtin_cmds_root[mode].children_count;
	result = calloc(count + 1, sizeof(*result));
	if(result == NULL)
	{
		return NULL;
	}

	if(fill_list(&user_cmds_root[mode], 0, result) < 0)
	{
		free_wstring_array(result, count);
		return NULL;
	}

	j = -1;
	for(i = 0; i < count && result[i] != NULL; i++)
	{
		if(result[i][0] == L'\0')
		{
			free(result[i]);
			result[i] = NULL;
			if(j == -1)
				j = i;
		}
	}
	if(j == -1)
		j = i;

	result[j++] = my_wcsdup(L"");

	if(fill_list(&builtin_cmds_root[mode], 0, result + j) < 0)
	{
		free_wstring_array(result, count);
		return NULL;
	}

	for(i = j; i < count && result[i] != NULL; i++)
	{
		if(result[i][0] == L'\0')
		{
			free(result[i]);
			result[i] = NULL;
		}
	}

	return result;
}

static int
fill_list(struct key_chunk_t *curr, size_t len, wchar_t **list)
{
	size_t i;
	struct key_chunk_t *child;
	size_t count;

	count = curr->children_count;
	if(curr->conf.type == USER_CMD)
		count++;
	if(count == 0)
		count = 1;

	for(i = 0; i < count; i++)
	{
		wchar_t *t;

		t = realloc(list[i], sizeof(wchar_t)*(len + 1));
		if(t == NULL)
			return -1;

		list[i] = t;
		if(len > 0)
			t[len - 1] = curr->key;
		t[len] = L'\0';
	}

	i = 0;
	if(curr->children_count == 0 || curr->conf.type == USER_CMD)
	{
		const wchar_t *s;
		wchar_t *t;

		s = (curr->conf.type == USER_CMD) ? curr->conf.data.cmd : L"<built in>";
		t = realloc(list[0], sizeof(wchar_t)*(len + 1 + wcslen(s) + 1));
		if(t == NULL)
			return -1;

		list[0] = t;
		t[wcslen(t) + 1] = L'\0';
		wcscpy(t + wcslen(t) + 1, s);
		if(curr->children_count == 0)
			return 1;

		list++;
		i = 1;
	}

	child = curr->child;
	while(child != NULL)
	{
		int filled = fill_list(child, len + 1, list);
		if(filled < 0)
			return -1;

		list += filled;
		i += filled;

		child = child->next;
	}

	while(i < count--)
	{
		free(*list);
		*list++ = NULL;
	}

	return i;
}

size_t
get_key_counter(void)
{
	return counter;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 : */
