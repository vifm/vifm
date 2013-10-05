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

#ifndef VIFM__DIR_STACK_H__
#define VIFM__DIR_STACK_H__

typedef struct
{
	char *lpane_dir;
	char *lpane_file;
	char *rpane_dir;
	char *rpane_file;
}
stack_entry_t;

extern stack_entry_t * stack;
extern unsigned int stack_top;

/* Returns 0 on success and -1 when not enough memory. */
int pushd(void);

/* Returns 0 on success and -1 when not enough memory. */
int push_to_dirstack(const char *ld, const char *lf, const char *rd,
		const char *rf);

/* Returns 0 on success and -1 on underflow. */
int popd(void);

/* Returns 0 on success and -1 on error. */
int swap_dirs(void);

/* Returns 0 on success and -1 on error. */
int rotate_stack(int n);

/* Always successful. */
void clean_stack(void);

/* Last element of list returned is NULL.  Returns NULL on error. */
char ** dir_stack_list(void);

/* Freezes change in the directory stack.  In combination with
 * dir_stack_changed() function might be used to detect changes in the stack. */
void dir_stack_freeze(void);

/* Checks that directory stack was ever changed or changed after last freezing.
 * Returns non-zero if it was changed, otherwise zero is returned. */
int dir_stack_changed(void);

#endif /* VIFM__DIR_STACK_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
