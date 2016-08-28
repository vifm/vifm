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

/* Single entry of the stack. */
typedef struct
{
	char *lpane_dir;  /* Path of the left pane. */
	char *lpane_file; /* File under cursor in the left pane. */
	char *rpane_dir;  /* Path of the right pane. */
	char *rpane_file; /* File under cursor in the right pane. */
}
dir_stack_entry_t;

/* The stack itself.  NULL, when empty. */
extern dir_stack_entry_t *dir_stack;
/* Current size of the stack. */
extern unsigned int dir_stack_top;

/* Pushes current locations of views onto the stack.  Returns 0 on success and
 * -1 when not enough memory. */
int dir_stack_push_current(void);

/* Pushes specified locations onto the stack.  Returns 0 on success and -1 when
 * not enough memory. */
int dir_stack_push(const char ld[], const char lf[], const char rd[],
		const char rf[]);

/* Pops top of the stack and navigates views to those locations.  Returns 0 on
 * success and -1 on underflow. */
int dir_stack_pop(void);

/* Swaps current locations with those at the top of the stack.  Returns 0 on
 * success and -1 on error. */
int dir_stack_swap(void);

/* Rotates stack entries by n items.  Returns 0 on success and -1 on error. */
int dir_stack_rotate(int n);

/* Empties the stack.  Always successful. */
void dir_stack_clear(void);

/* Lists contents of the stack as lines of text.  Last element of list returned
 * is NULL.  Returns NULL on error, otherwise caller should free the memory. */
char ** dir_stack_list(void);

/* Freezes change in the directory stack.  In combination with
 * dir_stack_changed() function might be used to detect changes in the stack. */
void dir_stack_freeze(void);

/* Checks that directory stack was ever changed or changed after last freezing.
 * Returns non-zero if it was changed, otherwise zero is returned. */
int dir_stack_changed(void);

#endif /* VIFM__DIR_STACK_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
