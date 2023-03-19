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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

/* WARNING: code implementing execution of keys deserves a prize in nomination
 *          "insanely perplexed control flow".  You have been warned.
 * TODO: seriously, indirect recursion and jumps between routines is a horrible
 *       mess, even not touching tree representation this should be possible to
 *       rewrite with master loop that simply goes through the string in
 *       left-to-right order. */

#include "keys.h"

#include <assert.h> /* assert() */
#include <ctype.h>
#include <limits.h> /* INT_MAX */
#include <stddef.h> /* NULL wchar_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h>
#include <string.h>
#include <wctype.h> /* iswdigit() */
#include <wchar.h> /* wcscat() wcslen() */

#include "../utils/macros.h"
#include "../utils/str.h"
#include "mode.h"

/* Type of key chunk. */
typedef enum
{
	BUILTIN_WAIT_POINT, /* Infinite wait of next key press. */
	BUILTIN_KEYS,       /* Normal builtin key or a foreign if in user's tree. */
	BUILTIN_NIM_KEYS,   /* NIM - number in the middle. */
	USER_CMD,           /* User mapping. */
}
KeyType;

typedef struct key_chunk_t
{
	wchar_t key;
	int children_count;
	/* Number of current uses.  To prevent stack overflow and manager lifetime. */
	int enters;
	/* General key type. */
	KeyType type : 2;
	/* This is a builtin-like user-defined key. */
	unsigned int foreign : 1;
	/* Whether RHS should be treated as if there are no user mappings. */
	unsigned int no_remap : 1;
	/* Postpone free() call for proper lazy deletion. */
	unsigned int deleted : 1;
	/* Postpone UI updates until RHS is done executing. */
	unsigned int silent : 1;
	/* Do not use short wait to resolve conflict against builtin mapping, do long
	 * wait instead. */
	unsigned int wait : 1;
	key_conf_t conf;
	struct key_chunk_t *child;
	struct key_chunk_t *parent;
	struct key_chunk_t *prev, *next;
}
key_chunk_t;

/* Callback of traverse_children(). */
typedef void (*traverse_func)(const key_chunk_t *chunk, const wchar_t lhs[],
		void *arg);

static key_chunk_t *builtin_cmds_root;
static key_chunk_t *selectors_root;
static key_chunk_t *user_cmds_root;
static int max_modes;
static int *mode_flags;
static default_handler *def_handlers;
static size_t counter;
/* Main external functions enter recursion level. */
static size_t enters_counter;
/* Current (when inside) or previous (when outside) sequence number of entering
 * input resolution machinery. */
static size_t enter_seq;
/* Shows whether a mapping handler is being executed at the moment. */
static int inside_mapping;
/* Current (when inside mapping) or previous (when outside) sequence number of a
 * mapping state.  It gets updated when a mapping handling is started within
 * current input resolution request (basically unit was entered from outside
 * and hasn't left yet). */
static int mapping_state;
/* Value of enter_seq variable last seen on processing a mapping. */
static size_t mapping_enter_seq;
/* User-provided callback for silencing UI. */
static vle_silence_func silence_ui;

static void remove_foreign_in_tree(key_chunk_t *chunk);
static void remove_foreign_in_forest(key_chunk_t *forest, size_t size);
static void free_forest(key_chunk_t *forest, size_t size);
static void free_tree(key_chunk_t *root);
static void free_chunk(key_chunk_t *chunk);
static int execute_keys_general_wrapper(const wchar_t keys[], int timed_out,
		int mapped, int no_remap);
static int execute_keys_general(const wchar_t keys[], int timed_out, int mapped,
		int no_remap);
static int dispatch_keys(const wchar_t keys[], keys_info_t *keys_info,
		int no_remap, int prev_count);
static int dispatch_keys_at_root(const wchar_t keys[], keys_info_t *keys_info,
		key_chunk_t *root, key_info_t key_info, int no_remap);
static int dispatch_selector(const wchar_t keys[], keys_info_t *keys_info,
		key_info_t master_key_info, key_chunk_t *master_curr, int no_remap);
static int fill_key_info(const wchar_t **keys, key_info_t *key_info,
		int prev_count, int *result);
static int contains_chain(key_chunk_t *root, const wchar_t *begin,
		const wchar_t *end);
static int execute_next_keys(key_chunk_t *curr, const wchar_t keys[],
		key_info_t *key_info, keys_info_t *keys_info, int has_duplicate,
		int no_remap);
static int needs_waiting(const key_chunk_t *curr);
static int dispatch_key(key_info_t key_info, keys_info_t *keys_info,
		key_chunk_t *curr, const wchar_t keys[]);
static int has_def_handler(void);
static default_handler def_handler(void);
static int execute_after_remapping(const wchar_t rhs[],
		const wchar_t left_keys[], keys_info_t keys_info, key_info_t key_info,
		key_chunk_t *curr);
static void enter_chunk(key_chunk_t *chunk);
static void leave_chunk(key_chunk_t *chunk);
static void init_keys_info(keys_info_t *keys_info, int mapped);
static const wchar_t * get_reg(const wchar_t *keys, int *reg);
static const wchar_t * get_count(const wchar_t keys[], int *count);
static int is_at_count(const wchar_t keys[]);
static int combine_counts(int count_a, int count_b);
static key_chunk_t * find_keys(key_chunk_t *root, const wchar_t keys[]);
static void remove_chunk(key_chunk_t *chunk);
static int add_list_of_keys(key_chunk_t *root, keys_add_info_t cmds[],
		size_t len);
static key_chunk_t * add_keys_inner(key_chunk_t *root, const wchar_t *keys);
static void init_chunk_data(key_chunk_t *chunk, wchar_t key, KeyType type);
static void list_chunk(const key_chunk_t *chunk, const wchar_t lhs[],
		void *arg);
static void inc_counter(const keys_info_t *keys_info, size_t by);
static int is_recursive(void);
static int execute_mapping_handler(const key_conf_t *info,
		key_info_t key_info, keys_info_t *keys_info);
static void pre_execute_mapping_handler(const keys_info_t *keys_info);
static void post_execute_mapping_handler(const keys_info_t *keys_info);
static void keys_suggest(const key_chunk_t *root, const wchar_t keys[],
		const wchar_t prefix[], vle_keys_list_cb cb, int custom_only,
		int fold_subkeys);
static void suggest_children(const key_chunk_t *chunk, const wchar_t prefix[],
		vle_keys_list_cb cb, int fold_subkeys);
static void traverse_children(const key_chunk_t *chunk, const wchar_t prefix[],
		traverse_func cb, void *param);
static void suggest_chunk(const key_chunk_t *chunk, const wchar_t lhs[],
		void *arg);

void
vle_keys_init(int modes_count, int *key_mode_flags, vle_silence_func silence)
{
	assert(key_mode_flags != NULL);
	assert(modes_count > 0);
	assert(silence != NULL);

	max_modes = modes_count;
	mode_flags = key_mode_flags;
	silence_ui = silence;

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
vle_keys_reset(void)
{
	free_forest(builtin_cmds_root, max_modes);
	free_forest(user_cmds_root, max_modes);
	free_forest(selectors_root, max_modes);

	free(def_handlers);

	builtin_cmds_root = NULL;
	selectors_root = NULL;
	user_cmds_root = NULL;
	max_modes = 0;
	mode_flags = NULL;
	def_handlers = NULL;
}

void
vle_keys_user_clear(void)
{
	remove_foreign_in_forest(selectors_root, max_modes);
	free_forest(user_cmds_root, max_modes);

	user_cmds_root = calloc(max_modes, sizeof(*user_cmds_root));
	assert(user_cmds_root != NULL);
}

/* Removes foreign keys in trees from array of length size. */
static void
remove_foreign_in_forest(key_chunk_t *forest, size_t size)
{
	size_t i;
	for(i = 0; i < size; ++i)
	{
		remove_foreign_in_tree(&forest[i]);
	}
}

/* Removes foreign keys from a tree. */
static void
remove_foreign_in_tree(key_chunk_t *chunk)
{
	/* To postpone deletion until we're done with the chunk. */
	enter_chunk(chunk);

	if(chunk->child != NULL)
	{
		remove_foreign_in_tree(chunk->child);
	}

	if(chunk->next != NULL)
	{
		remove_foreign_in_tree(chunk->next);
	}

	if(chunk->foreign)
	{
		remove_chunk(chunk);
	}

	leave_chunk(chunk);
}

/* Releases array of trees of length size including trees. */
static void
free_forest(key_chunk_t *forest, size_t size)
{
	size_t i;
	for(i = 0; i < size; i++)
	{
		free_tree(&forest[i]);
	}
	free(forest);
}

static void
free_tree(key_chunk_t *root)
{
	if(root->child != NULL)
	{
		free_tree(root->child);
		free_chunk(root->child);
	}

	if(root->next != NULL)
	{
		free_tree(root->next);
		free_chunk(root->next);
	}

	if(root->type == USER_CMD)
	{
		free(root->conf.data.cmd);
	}
}

static void
free_chunk(key_chunk_t *chunk)
{
	if(chunk->enters == 0)
	{
		free(chunk);
	}
	else
	{
		chunk->deleted = 1;
	}
}

void
vle_keys_set_def_handler(int mode, default_handler handler)
{
	def_handlers[mode] = handler;
}

/* This function should never be called from this module, only externally. */
int
vle_keys_exec(const wchar_t keys[])
{
	return execute_keys_general_wrapper(keys, 0, 0, 0);
}

/* This function should never be called from this module, only externally. */
int
vle_keys_exec_no_remap(const wchar_t keys[])
{
	return execute_keys_general_wrapper(keys, 0, 0, 1);
}

/* This function should never be called from this module, only externally. */
int
vle_keys_exec_timed_out(const wchar_t keys[])
{
	return execute_keys_general_wrapper(keys, 1, 0, 0);
}

/* This function should never be called from this module, only externally. */
int
vle_keys_exec_timed_out_no_remap(const wchar_t keys[])
{
	return execute_keys_general_wrapper(keys, 1, 0, 1);
}

static int
execute_keys_general_wrapper(const wchar_t keys[], int timed_out, int mapped,
		int no_remap)
{
	if(++enters_counter == 1)
	{
		enter_seq = (enter_seq == INT_MAX ? 1 : enter_seq + 1);
	}
	int result = execute_keys_general(keys, timed_out, mapped, no_remap);
	--enters_counter;

	return result;
}

static int
execute_keys_general(const wchar_t keys[], int timed_out, int mapped,
		int no_remap)
{
	int result;
	keys_info_t keys_info;

	if(keys[0] == L'\0')
	{
		return KEYS_UNKNOWN;
	}

	init_keys_info(&keys_info, mapped);
	keys_info.after_wait = timed_out;
	result = dispatch_keys(keys, &keys_info, no_remap, NO_COUNT_GIVEN);
	if(result == KEYS_UNKNOWN && def_handlers[vle_mode_get()] != NULL)
	{
		result = def_handlers[vle_mode_get()](keys[0]);
		execute_keys_general(keys + 1, 0, mapped, no_remap);
	}
	return result;
}

/* Executes keys from the start of key sequence or at some offset in it.
 * Returns error code. */
static int
dispatch_keys(const wchar_t keys[], keys_info_t *keys_info, int no_remap,
		int prev_count)
{
	const wchar_t *keys_start = keys;
	key_info_t key_info;
	int result;

	if(fill_key_info(&keys, &key_info, prev_count, &result) != 0)
	{
		return result;
	}

	result = KEYS_UNKNOWN;
	if(!no_remap)
	{
		result = dispatch_keys_at_root(keys, keys_info,
				&user_cmds_root[vle_mode_get()], key_info, no_remap);
	}

	if(result == KEYS_UNKNOWN)
	{
		result = dispatch_keys_at_root(keys, keys_info,
				&builtin_cmds_root[vle_mode_get()], key_info, no_remap);
	}

	if(!IS_KEYS_RET_CODE(result))
	{
		/* Take characters spent on count and register specification into
		 * account. */
		inc_counter(keys_info, keys - keys_start);
	}

	return result;
}

/* Dispatches keys passed in using a tree of shortcuts registered in the root.
 * Returns error code. */
static int
dispatch_keys_at_root(const wchar_t keys[], keys_info_t *keys_info,
		key_chunk_t *root, key_info_t key_info, int no_remap)
{
	key_chunk_t *curr;
	const wchar_t *keys_start = keys;
	int has_duplicate;
	int result;

	/* The loop finds longest match of the input (keys) among registered
	 * shortcuts. */
	curr = root;
	while(*keys != L'\0')
	{
		key_chunk_t *p;
		int number_in_the_middle = 0;

		for(p = curr->child; p != NULL && p->key < *keys; p = p->next)
		{
			if(p->type == BUILTIN_NIM_KEYS)
			{
				number_in_the_middle = 1;
			}
		}

		if(p == NULL || p->key != *keys)
		{
			if(curr == root)
				return KEYS_UNKNOWN;

			for(; p != NULL; p = p->next)
			{
				if(p->type == BUILTIN_NIM_KEYS)
				{
					number_in_the_middle = 1;
				}
			}

			if(curr->conf.followed != FOLLOWED_BY_NONE &&
					(!number_in_the_middle || !is_at_count(keys)))
			{
				break;
			}

			if(number_in_the_middle)
			{
				int count;
				const wchar_t *new_keys = get_count(keys, &count);
				if(new_keys != keys)
				{
					key_info.count = combine_counts(key_info.count, count);
					keys = new_keys;
					continue;
				}
			}

			if(curr->type == BUILTIN_WAIT_POINT)
			{
				return KEYS_UNKNOWN;
			}

			key_info.user_data = curr->conf.user_data;

			has_duplicate = root == &user_cmds_root[vle_mode_get()] &&
					contains_chain(&builtin_cmds_root[vle_mode_get()], keys_start, keys);
			result = execute_next_keys(curr, curr->type == USER_CMD ? keys : L"",
					&key_info, keys_info, has_duplicate, no_remap);

			if(curr->type == USER_CMD)
			{
				/* We've at least attempted to execute a user mapping.  Maybe it did
				 * nothing, but it's also possible that it executed something and failed
				 * afterwards.  Either way, finishing processing successfully is the
				 * best course of action.  We either really did succeed or we need to
				 * avoid any further processing anyway (trying to interpret LHS of the
				 * mapping in a different way would be a mistake and waiting of any kind
				 * within a mapping can only be ignored). */
				return 0;
			}

			if(IS_KEYS_RET_CODE(result))
			{
				return (result == KEYS_WAIT_SHORT) ? KEYS_UNKNOWN : result;
			}
			inc_counter(keys_info, keys - keys_start);
			return execute_keys_general(keys, 0, keys_info->mapped, no_remap);
		}
		++keys;
		curr = p;
	}

	if(*keys == '\0' && curr->type != BUILTIN_WAIT_POINT &&
			curr->children_count > 0 && curr->conf.data.handler != NULL &&
			!keys_info->after_wait)
	{
		return KEYS_WAIT_SHORT;
	}

	key_info.user_data = curr->conf.user_data;

	has_duplicate = root == &user_cmds_root[vle_mode_get()] &&
			contains_chain(&builtin_cmds_root[vle_mode_get()], keys_start, keys);
	result = execute_next_keys(curr, keys, &key_info, keys_info, has_duplicate,
			no_remap);
	if(!IS_KEYS_RET_CODE(result))
	{
		inc_counter(keys_info, keys - keys_start);
	}
	else if(*keys == '\0' && result == KEYS_UNKNOWN && curr->children_count > 0)
	{
		return keys_info->after_wait ? KEYS_UNKNOWN : KEYS_WAIT_SHORT;
	}
	return result;
}

/* Dispatches keys passed in as a selector followed by arbitrary other keys.
 * Returns error code. */
static int
dispatch_selector(const wchar_t keys[], keys_info_t *keys_info,
		key_info_t master_key_info, key_chunk_t *master_curr, int no_remap)
{
	const wchar_t *keys_start = keys;
	key_chunk_t *curr = &selectors_root[vle_mode_get()];
	key_info_t key_info;
	int result;

	if(fill_key_info(&keys, &key_info, master_key_info.count, &result) != 0)
	{
		return result;
	}

	/* The loop finds longest match of the input (keys) among registered
	 * shortcuts. */
	while(*keys != L'\0')
	{
		key_chunk_t *p;

		for(p = curr->child; p != NULL && p->key < *keys; p = p->next)
		{
			/* Advance. */
		}
		if(p == NULL || p->key != *keys)
		{
			break;
		}
		++keys;
		curr = p;
	}

	/* Handle ambiguous selector. */
	if(*keys == '\0' && curr->type != BUILTIN_WAIT_POINT &&
			curr->children_count > 0 && curr->conf.data.handler != NULL &&
			!keys_info->after_wait)
	{
		return KEYS_WAIT_SHORT;
	}

	key_info.user_data = curr->conf.user_data;

	/* Execute the selector. */
	if(curr->conf.followed == FOLLOWED_BY_MULTIKEY && keys[0] != L'\0')
	{
		const wchar_t mk[] = { keys[0], L'\0' };
		result = execute_next_keys(curr, mk, &key_info, keys_info, 0, no_remap);
		++keys;
	}
	else
	{
		result = keys[0] == L'\0'
		       ? execute_next_keys(curr, L"", &key_info, keys_info, 0, no_remap)
		       : dispatch_key(key_info, keys_info, curr, L"");
	}
	if(IS_KEYS_RET_CODE(result))
	{
		return result;
	}

	/* We used this count in selector, so don't pass it to command. */
	master_key_info.count = NO_COUNT_GIVEN;

	/* Execute command that requested the selector. */
	result = execute_mapping_handler(&master_curr->conf, master_key_info,
			keys_info);
	if(IS_KEYS_RET_CODE(result))
	{
		return result;
	}

	inc_counter(keys_info, keys - keys_start);

	/* execute_keys_general() treats empty input as an error. */
	if(keys[0] == L'\0')
	{
		return 0;
	}
	/* Process the rest of the line. */
	return execute_keys_general(keys, keys_info->after_wait, keys_info->mapped,
			no_remap);
}

/* Fills *key_info advancing input if necessary.  Returns zero on success,
 * otherwise non-zero is returned and *result is set. */
static int
fill_key_info(const wchar_t **keys, key_info_t *key_info, int prev_count,
		int *result)
{
	*keys = get_reg(*keys, &key_info->reg);
	if(*keys == NULL)
	{
		*result = KEYS_WAIT;
		return 1;
	}
	if(key_info->reg == L'\x1b' || key_info->reg == L'\x03')
	{
		*result = 0;
		return 1;
	}

	*keys = get_count(*keys, &key_info->count);
	key_info->count = combine_counts(key_info->count, prev_count);
	key_info->multi = L'\0';
	return 0;
}

static int
contains_chain(key_chunk_t *root, const wchar_t *begin, const wchar_t *end)
{
	key_chunk_t *curr;

	if(begin == end)
		return 0;

	curr = root;
	while(begin != end)
	{
		key_chunk_t *p;

		p = curr->child;
		while(p != NULL && p->key < *begin)
			p = p->next;

		if(p == NULL || p->key != *begin)
			return 0;

		begin++;
		curr = p;
	}
	return (curr->conf.followed == FOLLOWED_BY_NONE &&
			curr->type != BUILTIN_WAIT_POINT);
}

/* Handles the rest of the keys after first one has been determined (in curr).
 * These can be: <nothing> (empty line), selector, multikey argument.  Returns
 * error code. */
static int
execute_next_keys(key_chunk_t *curr, const wchar_t keys[], key_info_t *key_info,
		keys_info_t *keys_info, int has_duplicate, int no_remap)
{
	const key_conf_t *const conf = &curr->conf;

	if(*keys == L'\0')
	{
		int wait_point = (curr->type == BUILTIN_WAIT_POINT);
		wait_point = wait_point || (curr->type == USER_CMD &&
				conf->followed != FOLLOWED_BY_NONE);

		if(wait_point)
		{
			if(!keys_info->after_wait)
			{
				if(needs_waiting(curr))
				{
					/* Wait flag on a user mapping should turn short wait into indefinite
					 * wait when there is a conflict with builtin mapping, pretending
					 * that there is no such conflict is enough to get the effect. */
					has_duplicate = 0;
				}
				const int with_input = (mode_flags[vle_mode_get()] & MF_USES_INPUT);
				return (with_input || has_duplicate) ? KEYS_WAIT_SHORT : KEYS_WAIT;
			}
		}
		else if(conf->data.handler == NULL || conf->followed != FOLLOWED_BY_NONE)
		{
			return KEYS_UNKNOWN;
		}
	}
	else if(curr->type != USER_CMD)
	{
		if(conf->followed == FOLLOWED_BY_MULTIKEY)
		{
			key_info->multi = keys[0];
			return dispatch_key(*key_info, keys_info, curr, keys + 1);
		}

		keys_info->selector = 1;
		return dispatch_selector(keys, keys_info, *key_info, curr, no_remap);
	}

	return dispatch_key(*key_info, keys_info, curr, keys);
}

/* Checks whether any child of the node has wait flag set.  Returns non-zero if
 * so, otherwise zero is returned. */
static int
needs_waiting(const key_chunk_t *curr)
{
	if(curr->wait)
	{
		return 1;
	}

	for(curr = curr->child; curr != NULL; curr = curr->next)
	{
		if(needs_waiting(curr))
		{
			return 1;
		}
	}

	return 0;
}

/* Performs action associated with the key (in curr), if any.  Returns error
 * code. */
static int
dispatch_key(key_info_t key_info, keys_info_t *keys_info, key_chunk_t *curr,
		const wchar_t keys[])
{
	const key_conf_t *const conf = &curr->conf;

	if(curr->type != USER_CMD)
	{
		const int result = execute_mapping_handler(conf, key_info, keys_info);
		const int finish_dispatching = result != 0
		                            || *keys == L'\0'
		                            || conf->followed != FOLLOWED_BY_MULTIKEY;
		if(finish_dispatching)
		{
			return result;
		}

		/* Process the rest of the input after a command followed by multikey. */
		return execute_keys_general_wrapper(keys, keys_info->after_wait, 0,
				curr->no_remap);
	}
	else
	{
		if(curr->silent)
		{
			silence_ui(1);
		}

		int result = has_def_handler() ? 0 : KEYS_UNKNOWN;

		/* Protect chunk from deletion while it's in use. */
		enter_chunk(curr);

		if(curr->enters == 1)
		{
			result = execute_after_remapping(conf->data.cmd, keys, *keys_info,
					key_info, curr);
		}
		else if(has_def_handler())
		{
			result = def_handler()(curr->key);

			if(result == 0)
			{
				result = execute_keys_general(keys, keys_info->after_wait, 0,
						curr->no_remap);
			}
		}

		if(result == KEYS_UNKNOWN && has_def_handler())
		{
			if(curr->enters == 1)
			{
				result = def_handler()(conf->data.cmd[0]);
				enter_chunk(curr);
				execute_keys_general(conf->data.cmd + 1, 0, 1, curr->no_remap);
				leave_chunk(curr);
			}
			else
			{
				int i;
				for(i = 0; conf->data.cmd[i] != '\0'; i++)
				{
					result = def_handler()(conf->data.cmd[i]);
				}
			}
		}

		if(curr->silent)
		{
			silence_ui(0);
		}

		/* Release the chunk, this will free it if deletion was attempted. */
		leave_chunk(curr);

		return result;
	}
}

/* Checks that default handler exists for active mode.  Returns non-zero if so,
 * otherwise zero is returned. */
static int
has_def_handler(void)
{
	return (def_handler() != NULL);
}

/* Gets default handler of active mode.  Returns the handler, which is NULL when
 * not set for active mode. */
static default_handler
def_handler(void)
{
	return def_handlers[vle_mode_get()];
}

/* Processes remapping of a key.  Returns error code. */
static int
execute_after_remapping(const wchar_t rhs[], const wchar_t left_keys[],
		keys_info_t keys_info, key_info_t key_info, key_chunk_t *curr)
{
	int result;
	if(rhs[0] == L'\0' && left_keys[0] == L'\0')
	{
		/* No-operation command "executed" correctly. */
		result = 0;
	}
	else if(rhs[0] == L'\0')
	{
		keys_info_t keys_info;
		init_keys_info(&keys_info, 1);
		enter_chunk(curr);
		result = dispatch_keys(left_keys, &keys_info, curr->no_remap,
				NO_COUNT_GIVEN);
		leave_chunk(curr);
	}
	else
	{
		wchar_t buf[16 + wcslen(rhs) + 1 + wcslen(left_keys) + 1];

		buf[0] = '\0';
		if(key_info.reg != NO_REG_GIVEN)
		{
			vifm_swprintf(buf, ARRAY_LEN(buf), L"\"%c", key_info.reg);
		}
		if(key_info.count != NO_COUNT_GIVEN)
		{
			/* XXX: this can insert thousand separators in some locales... */
			vifm_swprintf(buf + wcslen(buf), ARRAY_LEN(buf) - wcslen(buf), L"%d",
					key_info.count);
		}
		wcscat(buf, rhs);
		wcscat(buf, left_keys);

		if(curr->conf.followed != FOLLOWED_BY_SELECTOR)
		{
			init_keys_info(&keys_info, 1);
		}

		/* XXX: keys_info.mapped now covers both RHS of a mapping and the rest of
		 *      the keys (`left_keys`)!  This is bad. */

		enter_chunk(curr);
		result = dispatch_keys(buf, &keys_info, curr->no_remap, NO_COUNT_GIVEN);
		leave_chunk(curr);
	}
	return result;
}

/* Handles entering a chunk.  Counterpart of leave_chunk(). */
static void
enter_chunk(key_chunk_t *chunk)
{
	++chunk->enters;
}

/* Handles leaving a chunk performing postponed chunk removal if needed.
 * Counterpart of enter_chunk(). */
static void
leave_chunk(key_chunk_t *chunk)
{
	--chunk->enters;

	if(chunk->enters == 0 && chunk->deleted)
	{
		/* Removal of the chunk was postponed because it was in use, proceed with
		 * this now. */
		free(chunk);
	}
}

static void
init_keys_info(keys_info_t *keys_info, int mapped)
{
	keys_info->selector = 0;
	keys_info->count = 0;
	keys_info->indexes = NULL;
	keys_info->after_wait = 0;
	keys_info->mapped = mapped;
	keys_info->recursive = is_recursive();
}

static const wchar_t *
get_reg(const wchar_t *keys, int *reg)
{
	*reg = NO_REG_GIVEN;
	if((mode_flags[vle_mode_get()] & MF_USES_REGS) == 0)
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

/* Reads count of the command.  Sets *count to NO_COUNT_GIVEN if there is no
 * count in the current position.  Returns pointer to a character right next to
 * the count. */
static const wchar_t *
get_count(const wchar_t keys[], int *count)
{
	if(is_at_count(keys))
	{
		wchar_t *ptr;
		*count = wcstol(keys, &ptr, 10);
		/* Handle overflow correctly. */
		if(*count <= 0)
		{
			*count = INT_MAX;
		}
		keys = ptr;
	}
	else
	{
		*count = NO_COUNT_GIVEN;
	}

	return keys;
}

/* Checks keys for a count.  Returns non-zero if there is count in the current
 * position. */
static int
is_at_count(const wchar_t keys[])
{
	if((mode_flags[vle_mode_get()] & MF_USES_COUNT) != 0)
	{
		if(keys[0] != L'0' && iswdigit(keys[0]))
		{
			return 1;
		}
	}
	return 0;
}

/* Combines two counts: before command and in the middle of it. */
static int
combine_counts(int count_a, int count_b)
{
	if(count_a == NO_COUNT_GIVEN)
	{
		return count_b;
	}
	else if(count_b == NO_COUNT_GIVEN)
	{
		return count_a;
	}
	else
	{
		return count_a*count_b;
	}
}

int
vle_keys_foreign_add(const wchar_t lhs[], const key_conf_t *info,
		int is_selector, int mode)
{
	key_chunk_t *root = is_selector ? &selectors_root[mode]
	                                : &user_cmds_root[mode];

	if(is_selector && find_keys(root, lhs) != NULL)
	{
		return -1;
	}

	key_chunk_t *curr = add_keys_inner(root, lhs);
	if(curr == NULL)
	{
		return -1;
	}

	curr->type = (info->followed == FOLLOWED_BY_NONE) ? BUILTIN_KEYS
	                                                  : BUILTIN_WAIT_POINT;
	curr->foreign = 1;
	curr->conf = *info;
	return 0;
}

int
vle_keys_user_add(const wchar_t lhs[], const wchar_t rhs[], int mode,
		int flags)
{
	key_chunk_t *curr = add_keys_inner(&user_cmds_root[mode], lhs);
	if(curr == NULL)
	{
		return -1;
	}

	curr->type = USER_CMD;
	curr->conf.data.cmd = vifm_wcsdup(rhs);
	curr->no_remap = ((flags & KEYS_FLAG_NOREMAP) != 0);
	curr->silent = ((flags & KEYS_FLAG_SILENT) != 0);
	curr->wait = ((flags & KEYS_FLAG_WAIT) != 0);
	return 0;
}

int
vle_keys_user_exists(const wchar_t keys[], int mode)
{
	return find_keys(&user_cmds_root[mode], keys) != NULL;
}

int
vle_keys_user_remove(const wchar_t keys[], int mode)
{
	key_chunk_t *chunk = find_keys(&user_cmds_root[mode], keys);
	if(chunk == NULL)
		return -1;

	remove_chunk(chunk);
	return 0;
}

/* Finds chunk that corresponds to key sequence.  Returns the chunk or NULL. */
static key_chunk_t *
find_keys(key_chunk_t *root, const wchar_t keys[])
{
	if(*keys == L'\0')
	{
		return NULL;
	}

	key_chunk_t *curr = root;
	while(*keys != L'\0')
	{
		key_chunk_t *p = curr->child;
		while(p != NULL && p->key < *keys)
			p = p->next;
		if(p == NULL || p->key != *keys)
			return NULL;
		curr = p;
		keys++;
	}
	return curr;
}

/* Removes information within the chunk and removes it from the tree if it has
 * no child chunks.  Updates all parent nodes. */
static void
remove_chunk(key_chunk_t *chunk)
{
	if(chunk->type == USER_CMD)
	{
		free(chunk->conf.data.cmd);
	}

	chunk->type = BUILTIN_WAIT_POINT;
	chunk->conf.data.handler = NULL;

	key_chunk_t *p = chunk;
	while(p->parent != NULL)
	{
		p->parent->children_count--;
		p = p->parent;
	}

	if(chunk->children_count > 0)
		return;

	do
	{
		key_chunk_t *const parent = chunk->parent;
		if(chunk->prev != NULL)
			chunk->prev->next = chunk->next;
		else
			parent->child = chunk->next;
		if(chunk->next != NULL)
			chunk->next->prev = chunk->prev;
		free_chunk(chunk);
		chunk = parent;
	}
	while(chunk->parent != NULL && chunk->conf.data.handler == NULL &&
			chunk->type == BUILTIN_WAIT_POINT && chunk->children_count == 0);
}

int
vle_keys_add(keys_add_info_t cmds[], size_t len, int mode)
{
	return add_list_of_keys(&builtin_cmds_root[mode], cmds, len);
}

int
vle_keys_add_selectors(keys_add_info_t cmds[], size_t len, int mode)
{
	return add_list_of_keys(&selectors_root[mode], cmds, len);
}

/* Registers cmds[0 .. len-1] keys specified tree.  Returns non-zero on error,
 * otherwise zero is returned. */
static int
add_list_of_keys(key_chunk_t *root, keys_add_info_t cmds[], size_t len)
{
	int result = 0;
	size_t i;

	for(i = 0U; i < len; ++i)
	{
		key_chunk_t *const curr = add_keys_inner(root, cmds[i].keys);
		if(curr == NULL)
		{
			result = -1;
			continue;
		}

		curr->conf = cmds[i].info;
		if(curr->conf.nim)
		{
			curr->type = BUILTIN_NIM_KEYS;
		}
		else
		{
			curr->type = (curr->conf.followed == FOLLOWED_BY_NONE)
			           ? BUILTIN_KEYS
			           : BUILTIN_WAIT_POINT;
		}
	}

	return result;
}

static key_chunk_t *
add_keys_inner(key_chunk_t *root, const wchar_t *keys)
{
	key_chunk_t *curr = root;
	while(*keys != L'\0')
	{
		key_chunk_t *prev, *p;
		prev = NULL;
		p = curr->child;
		while(p != NULL && p->key < *keys)
		{
			prev = p;
			p = p->next;
		}
		if(p == NULL || p->key != *keys)
		{
			key_chunk_t *c = malloc(sizeof(*c));
			if(c == NULL)
			{
				return NULL;
			}

			KeyType type = (keys[1] == L'\0' ? BUILTIN_KEYS : BUILTIN_WAIT_POINT);
			init_chunk_data(c, *keys, type);

			c->prev = prev;
			c->next = p;
			c->child = NULL;
			c->parent = curr;
			c->children_count = 0;
			c->enters = 0;
			c->deleted = 0;
			if(prev == NULL)
				curr->child = c;
			else
				prev->next = c;
			if(p != NULL)
				p->prev = c;

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

	/* Reset most of the fields of a previously existing key before returning
	 * it. */
	if(curr->type == USER_CMD)
	{
		free(curr->conf.data.cmd);
	}
	init_chunk_data(curr, curr->key, BUILTIN_KEYS);

	return curr;
}

/* Initializes key metadata and its configuration fields, but not lifetime/tree
 * structure management parts of the structure. */
static void
init_chunk_data(key_chunk_t *chunk, wchar_t key, KeyType type)
{
	chunk->key = key;
	chunk->type = type;
	chunk->foreign = 0;
	chunk->no_remap = 1;
	chunk->silent = 0;
	chunk->wait = 0;

	chunk->conf.data.cmd = NULL;
	chunk->conf.followed = FOLLOWED_BY_NONE;
	chunk->conf.nim = 0;
	chunk->conf.skip_suggestion = 0;
	chunk->conf.suggest = NULL;
	chunk->conf.descr = NULL;
	chunk->conf.user_data = NULL;
}

void
vle_keys_list(int mode, vle_keys_list_cb cb, int user_only)
{
	const key_chunk_t *user = &user_cmds_root[mode];
	const key_chunk_t *builtin = &builtin_cmds_root[mode];

	cb(L"", L"", "User mappings:");

	/* Don't traverse empty tries. */
	if(user->children_count != 0)
	{
		traverse_children(user, L"", &list_chunk, cb);
	}

	cb(L"", L"", "");
	cb(L"", L"", "Builtin mappings:");

	if(!user_only && builtin->children_count != 0)
	{
		traverse_children(builtin, L"", &list_chunk, cb);
	}
}

/* Invokes list callback (in arg) for leaf chunks. */
static void
list_chunk(const key_chunk_t *chunk, const wchar_t lhs[], void *arg)
{
	if(chunk->children_count == 0 || chunk->type == USER_CMD)
	{
		const wchar_t *rhs = (chunk->type == USER_CMD) ? chunk->conf.data.cmd : L"";
		const char *descr = (chunk->conf.descr == NULL) ? "" : chunk->conf.descr;
		vle_keys_list_cb cb = arg;
		cb(lhs, rhs, descr);
	}
}

size_t
vle_keys_counter(void)
{
	return counter;
}

/* Increments counter if we are at the first level of key parsing recursion and
 * the key sequence isn't mapped. */
static void
inc_counter(const keys_info_t *keys_info, size_t by)
{
	assert(enters_counter > 0);

	if(!is_recursive() && !keys_info->mapped)
	{
		counter += by;
	}
}

/* Returns non-zero if current level of recursion is deeper than 1. */
static int
is_recursive(void)
{
	return (enters_counter > 1);
}

int
vle_keys_mapping_state(void)
{
	return (inside_mapping ? mapping_state : 0);
}

/* Executes handler for a mapping, if any.  Error or success code is
 * returned. */
static int
execute_mapping_handler(const key_conf_t *info, key_info_t key_info,
		keys_info_t *keys_info)
{
	if(info->data.handler != NULL)
	{
		pre_execute_mapping_handler(keys_info);
		info->data.handler(key_info, keys_info);
		post_execute_mapping_handler(keys_info);
		return 0;
	}
	return KEYS_UNKNOWN;
}

/* Pre-execution of a mapping handler callback. */
static void
pre_execute_mapping_handler(const keys_info_t *keys_info)
{
	if(keys_info->mapped)
	{
		++inside_mapping;
		assert(inside_mapping >= 0 && "Calls to pre/post funcs should be balanced");

		if(inside_mapping == 1 && mapping_enter_seq != enter_seq)
		{
			mapping_state = (mapping_state == INT_MAX ? 1 : mapping_state + 1);
			mapping_enter_seq = enter_seq;
		}
	}
}

/* Post-execution of a mapping handler callback. */
static void
post_execute_mapping_handler(const keys_info_t *keys_info)
{
	inside_mapping -= keys_info->mapped != 0;
	assert(inside_mapping >= 0 && "Calls to pre/post funcs should be balanced");
}

void
vle_keys_suggest(const wchar_t keys[], vle_keys_list_cb cb, int custom_only,
		int fold_subkeys)
{
	keys_suggest(&user_cmds_root[vle_mode_get()], keys, L"key: ", cb,
			custom_only, fold_subkeys);
	keys_suggest(&builtin_cmds_root[vle_mode_get()], keys, L"key: ", cb,
			custom_only, fold_subkeys);
}

/* Looks up possible continuations of keys for the given root and calls cb on
 * them. */
static void
keys_suggest(const key_chunk_t *root, const wchar_t keys[],
		const wchar_t prefix[], vle_keys_list_cb cb, int custom_only,
		int fold_subkeys)
{
	const key_chunk_t *curr = root;

	while(*keys != L'\0')
	{
		const key_chunk_t *p;
		int number_in_the_middle = 0;

		/* Look up current key among children of current node (might be root), while
		 * inspecting NIM as well. */
		for(p = curr->child; p != NULL && p->key < *keys; p = p->next)
		{
			if(p->type == BUILTIN_NIM_KEYS)
			{
				number_in_the_middle = 1;
			}
		}

		/* Go to the next character if a match is found. */
		if(p != NULL && p->key == *keys)
		{
			++keys;
			curr = p;

			/* Branch into matching selectors at every step to handle situation when
			 * multichar selector and multichar command continuation are available at
			 * the same time.. */
			if(curr->type == BUILTIN_WAIT_POINT &&
					curr->conf.followed == FOLLOWED_BY_SELECTOR)
			{
				/* Suggest selectors. */
				keys_suggest(&selectors_root[vle_mode_get()], keys, L"sel: ", cb,
						custom_only, fold_subkeys);
			}

			continue;
		}

		/* No match for the first character is fatal for the lookup. */
		if(curr == root)
		{
			return;
		}

		/* Need to inspect all children for NIM. */
		for(; p != NULL && !number_in_the_middle; p = p->next)
		{
			if(p->type == BUILTIN_NIM_KEYS)
			{
				number_in_the_middle = 1;
			}
		}

		/* Give up if this isn't one of cases where next character is not presented
		 * in the tree by design. */
		if(curr->conf.followed != FOLLOWED_BY_NONE &&
				(!number_in_the_middle || !is_at_count(keys)))
		{
			break;
		}

		/* Skip over number in the middle, if any. */
		if(number_in_the_middle)
		{
			int count;
			const wchar_t *new_keys = get_count(keys, &count);
			if(new_keys != keys)
			{
				keys = new_keys;
				continue;
			}
		}

		break;
	}

	if(!custom_only && *keys == L'\0')
	{
		if(curr->type == USER_CMD)
		{
			if(!curr->no_remap)
			{
				keys_suggest(&user_cmds_root[vle_mode_get()], curr->conf.data.cmd,
						prefix, cb, custom_only, fold_subkeys);
			}
			keys_suggest(&builtin_cmds_root[vle_mode_get()], curr->conf.data.cmd,
					prefix, cb, custom_only, fold_subkeys);
			return;
		}

		suggest_children(curr, prefix, cb, fold_subkeys);
	}

	if(curr->type == BUILTIN_WAIT_POINT &&
			curr->conf.followed == FOLLOWED_BY_MULTIKEY)
	{
		/* Invoke optional external function to provide suggestions. */
		if(curr->conf.suggest != NULL)
		{
			curr->conf.suggest(cb);
		}
	}
}

/* Suggests children of the specified chunk. */
static void
suggest_children(const key_chunk_t *chunk, const wchar_t prefix[],
		vle_keys_list_cb cb, int fold_subkeys)
{
	const key_chunk_t *child;

	const size_t prefix_len = wcslen(prefix);
	wchar_t item[prefix_len + 1U + 1U];
	wcscpy(item, prefix);
	item[prefix_len + 1U] = L'\0';

	for(child = chunk->child; child != NULL; child = child->next)
	{
		if(!fold_subkeys || child->children_count <= 1)
		{
			traverse_children(child, prefix, &suggest_chunk, cb);
		}
		else
		{
			char msg[64];
			snprintf(msg, sizeof(msg), "{ %d mappings folded }",
					(int)child->children_count);
			item[prefix_len] = child->key;
			cb(item, L"", msg);
		}
	}
}

/* Visits every child of the tree and calls cb with param on it. */
static void
traverse_children(const key_chunk_t *chunk, const wchar_t prefix[],
		traverse_func cb, void *param)
{
	const key_chunk_t *child;

	const size_t prefix_len = wcslen(prefix);
	wchar_t item[prefix_len + 1U + 1U];
	wcscpy(item, prefix);
	item[prefix_len] = chunk->key;
	item[prefix_len + 1U] = L'\0';

	cb(chunk, item, param);

	for(child = chunk->child; child != NULL; child = child->next)
	{
		traverse_children(child, item, cb, param);
	}
}

/* Invokes suggestion callback (in arg) for leaf chunks. */
static void
suggest_chunk(const key_chunk_t *chunk, const wchar_t lhs[], void *arg)
{
	vle_keys_list_cb cb = arg;

	if(chunk->conf.skip_suggestion)
	{
		return;
	}

	if(chunk->type == USER_CMD)
	{
		cb(lhs, chunk->conf.data.cmd, "");
	}
	else if(chunk->children_count == 0 ||
			chunk->conf.followed != FOLLOWED_BY_NONE)
	{
		cb(lhs, L"", (chunk->conf.descr == NULL) ? "" : chunk->conf.descr);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
