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

#ifndef __DIR_STACK_H__
#define __DIR_STACK_H__

struct stack_entry {
	char *lpane_dir;
	char *lpane_file;
	char *rpane_dir;
	char *rpane_file;
};

extern struct stack_entry * stack;
extern unsigned int stack_top;

/*
 * Returns 0 on success and -1 when not enough memory
 */
int pushd(void);

/*
 * Returns 0 on success and -1 when not enough memory
 */
int push_to_dirstack(const char *ld, const char *lf, const char *rd,
		const char *rf);

/*
 * Returns 0 on success and -1 on underflow
 */
int popd(void);

/*
 * Returns non-zero if directory stack contains given directory pair
 */
int is_in_dir_list(const char *ldir, const char *rdir);

/*
 * Last element of list returned is NULL.
 * Returns NULL on error.
 */
char ** dir_stack_list(void);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
