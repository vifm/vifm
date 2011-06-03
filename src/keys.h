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
		((c) == KEYS_UNKNOWN || (c) == KEYS_WAIT || (c) == KEYS_WAIT_SHORT)

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
	; /* can this key be used as a selector? */
	union
	{
		keys_handler handler;
		const wchar_t *cmd;
	}data;
};

typedef int (*default_handler)(wchar_t keys);

/*
 * assumed that key_mode_flags is an array of at least modes_count items
 */
void init_keys(int modes_count, int *key_mode, int *key_mode_flags);
void set_def_handler(int mode, default_handler handler);
int execute_keys(const wchar_t *keys);
int execute_keys_timed_out(const wchar_t *keys);
struct key_t* add_cmd(const wchar_t *keys, int mode);
struct key_t* add_selector(const wchar_t *keys, int mode);
/* Returns:
 * -1 - can't remap buildin keys
 */
int add_user_keys(const wchar_t *keys, const wchar_t *cmd, int mode);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
