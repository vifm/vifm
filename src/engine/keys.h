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

#ifndef VIFM__ENGINE__KEYS_H__
#define VIFM__ENGINE__KEYS_H__

#include <stddef.h> /* size_t wchar_t */

enum
{
	MAX_LHS_LEN = 4
};

enum
{
	NO_COUNT_GIVEN = -1,
	NO_REG_GIVEN = -1,
};

enum
{
	MF_USES_REGS = 1,
	MF_USES_COUNT = 2,
	MF_USES_INPUT = 4,
};

enum
{
	KEYS_UNKNOWN    = -1024,
	KEYS_WAIT       = -2048,
	KEYS_WAIT_SHORT = -4096,
};

/* Flags for a user key mapping. */
enum
{
	KEYS_FLAG_NONE = 0,    /* No flags. */
	KEYS_FLAG_NOREMAP = 1, /* Ignore user mappings in RHS. */
	KEYS_FLAG_SILENT = 2,  /* Postpone screen update until mapping's end. */
	KEYS_FLAG_WAIT = 4,    /* Do not use short wait to resolve conflict against
	                          builtin mapping. */
};

/* Checks passed in value for being among list of error codes this unit can
 * return. */
#define IS_KEYS_RET_CODE(c) \
		({ \
			const int tmp = (c); \
			tmp == KEYS_UNKNOWN || tmp == KEYS_WAIT || tmp == KEYS_WAIT_SHORT; \
		})

typedef enum
{
	FOLLOWED_BY_NONE,
	FOLLOWED_BY_SELECTOR,
	FOLLOWED_BY_MULTIKEY,
}
FollowedBy;

/* Describes single key (command or selector) on its own. */
typedef struct
{
	int count;       /* Repeat count, may be equal NO_COUNT_GIVEN. */
	int reg;         /* Number of selected register. */
	int multi;       /* Multikey. */
	void *user_data; /* User data for the key (can be NULL). */
}
key_info_t;

/* Describes sequence of keys (it can consist of a single key).  This structure
 * is shared among elements composite constructs like command+selector. */
typedef struct
{
	int selector;   /* Selector passed. */
	int count;      /* Count of selected items. */
	int *indexes;   /* Item indexes. */
	int after_wait; /* After short timeout. */
	int mapped;     /* Not users input. */
	int recursive;  /* The key is from recursive call of execute_keys_*(...). */
}
keys_info_t;

/* Handler for builtin keys. */
typedef void (*vle_keys_handler)(key_info_t key_info, keys_info_t *keys_info);
/* Type of function invoked by vle_keys_list() and vle_keys_suggest().  rhs is
 * provided for user-defined keys and is empty otherwise.  Description is empty
 * for user-defined keys or when not set.  Extra messages have empty lhs and
 * rhs, but can have non-empty description. */
typedef void (*vle_keys_list_cb)(const wchar_t lhs[], const wchar_t rhs[],
		const char descr[]);
/* User-provided suggestion callback for multikeys. */
typedef void (*vle_suggest_func)(vle_keys_list_cb cb);
/* User-provided callback for silencing UI.  Non-zero argument makes UI more
 * silent, zero argument makes it less silent. */
typedef void (*vle_silence_func)(int more);

/* Type of callback that handles all keys uncaught by shortcuts.  Should return
 * zero on success and non-zero on error. */
typedef int (*default_handler)(wchar_t key);

typedef struct
{
	union
	{
		vle_keys_handler handler; /* Handler for builtin commands. */
		wchar_t *cmd;             /* Mapped value for user-defined keys. */
	}
	data;

	FollowedBy followed : 2;          /* What type of key should we wait for. */
	unsigned int nim : 1;             /* Whether additional count in the middle is
	                                     allowed. */
	unsigned int skip_suggestion : 1; /* Do not print this among suggestions. */

	vle_suggest_func suggest; /* Suggestion function (can be NULL).  Invoked for
	                             multikeys. */
	const char *descr;        /* Brief description of the key (can be NULL). */
	void *user_data;          /* User data for the key (can be NULL). */
}
key_conf_t;

typedef struct
{
	const wchar_t keys[MAX_LHS_LEN + 1];
	key_conf_t info;
}
keys_add_info_t;

/* Initializes the unit.  Assumed that key_mode_flags is an array of at least
 * modes_count items. */
void vle_keys_init(int modes_count, int *key_mode_flags,
		vle_silence_func silence);

/* Frees all memory allocated by the unit and returns it to initial state. */
void vle_keys_reset(void);

/* Removes just user-defined keys (leaving builtin keys intact). */
void vle_keys_user_clear(void);

/* Set handler for unregistered keys.  handler can be NULL to remove it. */
void vle_keys_set_def_handler(int mode, default_handler handler);

/* Process sequence of keys.  Return value:
 *  - 0 - success
 *  - KEYS_*
 *  - something else from the default key handler */
int vle_keys_exec(const wchar_t keys[]);

/* Same as vle_keys_exec(), but disallows map processing in RHS of maps. */
int vle_keys_exec_no_remap(const wchar_t keys[]);

/* Same as vle_keys_exec(), but assumes that key wait timeout has expired. */
int vle_keys_exec_timed_out(const wchar_t keys[]);

/* Same as vle_keys_exec_no_remap(), but assumes that key wait timeout has
 * expired. */
int vle_keys_exec_timed_out_no_remap(const wchar_t keys[]);

/* Registers cmds[0 .. len-1] commands for the mode.  Returns non-zero on error,
 * otherwise zero is returned. */
int vle_keys_add(keys_add_info_t cmds[], size_t len, int mode);

/* Registers cmds[0 .. len-1] selectors for the mode.  Returns non-zero on
 * error, otherwise zero is returned. */
int vle_keys_add_selectors(keys_add_info_t cmds[], size_t len, int mode);

/* Registers a foreign builtin-like key (but among user's keys) or a selector
 * (among builtin selectors, so they can't clash).  Returns non-zero on error,
 * otherwise zero is returned. */
int vle_keys_foreign_add(const wchar_t lhs[], const key_conf_t *info,
		int is_selector, int mode);

/* Registers user key mapping.  The flags parameter accepts combinations of
 * KEYS_FLAG_*.  Returns non-zero or error, otherwise zero is returned. */
int vle_keys_user_add(const wchar_t keys[], const wchar_t rhs[], int mode,
		int flags);

/* Checks whether given user mapping exists.  Returns non-zero if so, otherwise
 * zero is returned. */
int vle_keys_user_exists(const wchar_t keys[], int mode);

/* Removes user mapping from the mode.  Returns non-zero if given key sequence
 * wasn't found. */
int vle_keys_user_remove(const wchar_t keys[], int mode);

/* Lists all or just user keys of the given mode with description. */
void vle_keys_list(int mode, vle_keys_list_cb cb, int user_only);

/* Retrieves number of keys processed so far.  Clients are expected to use
 * difference of returned values.  Returns the number. */
size_t vle_keys_counter(void);

/* Retrieves current mapping state.  Entering a mapping at the top level (so not
 * a nested mapping) increases state number.  Returns the state number or zero
 * if no mapping is currently active. */
int vle_keys_mapping_state(void);

/* Invokes cb for each possible keys continuation.  Intended to be used on
 * KEYS_WAIT and KEYS_WAIT_SHORT returns.  The custom_only flag limits
 * suggestions to those generated by invoking key_conf_t::suggest.  The
 * fold_subkeys enables folding of multiple keys with common prefix. */
void vle_keys_suggest(const wchar_t keys[], vle_keys_list_cb cb,
		int custom_only, int fold_subkeys);

#endif /* VIFM__ENGINE__KEYS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
