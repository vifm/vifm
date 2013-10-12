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

#ifndef VIFM__ENGINE__COMPLETION_H__
#define VIFM__ENGINE__COMPLETION_H__

/* Returns zero on success. */
int add_completion(const char *completion);

void completion_group_end(void);

/* Squashes all existing completion groups into one.  Performs resorting and
 * de-duplication of resulting single group. */
void completion_groups_unite(void);

void reset_completion(void);

/* Returns copy of the string or NULL. */
char * next_completion(void);

int get_completion_count(void);

void set_completion_order(int reversed);

const char ** get_completion_list(void);

int get_completion_pos(void);

/* Go to the last item (probably to user input). */
void rewind_completion(void);

#endif /* VIFM__ENGINE__COMPLETION_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
