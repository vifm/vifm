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

#ifndef __KEYS_H__
#define __KEYS_H__

#include <wchar.h>

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
	KEYS_UNKNOWN = -1024,
	KEYS_WAIT = -2048,
	KEYS_WAIT_SHORT = -4096,
};

#define IS_KEYS_RET_CODE(c) \
		({ \
			int tmp = (c); \
			tmp == KEYS_UNKNOWN || tmp == KEYS_WAIT || tmp == KEYS_WAIT_SHORT; \
		})

enum FOLLOWED_BY
{
	FOLLOWED_BY_NONE,
	FOLLOWED_BY_SELECTOR,
	FOLLOWED_BY_MULTIKEY,
};

enum KEYS_TYPE
{
	BUILDIN_WAIT_POINT, /* infinite wait of next key press */
	BUILDIN_KEYS,
	BUILDIN_CMD,
	USER_CMD,
};

enum KEYS_SELECTOR
{
	KS_NOT_A_SELECTOR,
	KS_SELECTOR_AND_CMD,
	KS_ONLY_SELECTOR,
};

struct key_info
{
	int count; /* repeat count, maybe equal NO_COUNT_GIVEN */
	int reg;   /* number of selected register */
	int multi; /* multi key */
};

struct keys_info
{
	int selector;   /* selector passed */
	int count;      /* count of selected items */
	int *indexes;   /* item indexes */
	int after_wait; /* after short timeout */
	int mapped;     /* not users input */
};

typedef void (*keys_handler)(struct key_info key_info,
		struct keys_info *keys_info);

struct key_t
{
	enum KEYS_TYPE type;
	enum FOLLOWED_BY followed;   /* what type of key should we wait for */
	union
	{
		keys_handler handler;
		wchar_t *cmd;
	}data;
};

struct keys_add_info {
	const wchar_t keys[4];
	struct key_t key_t;
};

/*
 * Return value:
 *  - 0 - success
 *  - something else - an error
 */
typedef int (*default_handler)(wchar_t key);

/*
 * Assumed that key_mode_flags is an array of at least modes_count items
 */
void init_keys(int modes_count, int *key_mode, int *key_mode_flags);

/*
 * Frees all allocated memory
 */
void clear_keys(void);

/*
 * handler could be NULL to remove default handler
 */
void set_def_handler(int mode, default_handler handler);

/*
 * Return value:
 *  - 0 - success
 *  - KEYS_*
 *  - something else from the default key handler
 */
int execute_keys(const wchar_t *keys);

/*
 * Return value:
 *  - 0 - success
 *  - KEYS_*
 *  - something else from the default key handler
 */
int execute_keys_timed_out(const wchar_t *keys);

/*
 * Returns not zero on error
 */
int add_cmds(struct keys_add_info *cmds, size_t len, int mode);

/*
 * Returns not zero on error
 */
int add_selectors(struct keys_add_info *cmds, size_t len, int mode);

/*
 * Returns not zero when can't find user keys
 */
int add_user_keys(const wchar_t *keys, const wchar_t *cmd, int mode);

/*
 * Returns not zero on error
 */
int remove_user_keys(const wchar_t *keys, int mode);

/*
 * Lists all commands of the given mode with description.
 *
 * Every line is like L"command\0description\0".
 *
 * End of list could be determined by the NULL element.
 * Caller should free array and all its elements using free().
 * Returns NULL on error.
 */
wchar_t ** list_cmds(int mode);

/*
 * Returns count of processed keys
 */
size_t get_key_counter(void);

#ifdef TEST
/*
 * Returns NULL on error
 */
struct key_t * add_cmd(const wchar_t *keys, int mode);

/*
 * Returns NULL on error
 */
struct key_t* add_selector(const wchar_t *keys, int mode);
#endif

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
/* vim: set cinoptions+=t0 : */
