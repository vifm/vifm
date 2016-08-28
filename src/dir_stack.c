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

#include "dir_stack.h"

#include <stddef.h> /* NULL */
#include <stdlib.h>
#include <string.h>

#include "compat/reallocarray.h"
#include "ui/fileview.h"
#include "ui/ui.h"
#include "filelist.h"

static void free_entry(const dir_stack_entry_t *entry);

/* Actual size of the stack (allocated, but some entries might be unused). */
static unsigned int stack_size;

/* Whether directory stack was changed since its creation or last freezing. */
static int changed;

dir_stack_entry_t *dir_stack;
unsigned int dir_stack_top;

int
dir_stack_push_current(void)
{
	return dir_stack_push(flist_get_dir(&lwin), get_current_file_name(&lwin),
			flist_get_dir(&rwin), get_current_file_name(&rwin));
}

int
dir_stack_push(const char ld[], const char lf[], const char rd[],
		const char rf[])
{
	if(dir_stack_top == stack_size)
	{
		dir_stack_entry_t *s = reallocarray(dir_stack, stack_size + 1, sizeof(*s));
		if(s == NULL)
		{
			return -1;
		}

		dir_stack = s;
		++stack_size;
	}

	dir_stack[dir_stack_top].lpane_dir = strdup(ld);
	dir_stack[dir_stack_top].lpane_file = strdup(lf);
	dir_stack[dir_stack_top].rpane_dir = strdup(rd);
	dir_stack[dir_stack_top].rpane_file = strdup(rf);

	if(dir_stack[dir_stack_top].lpane_dir == NULL ||
			dir_stack[dir_stack_top].lpane_file == NULL ||
			dir_stack[dir_stack_top].rpane_dir == NULL ||
			dir_stack[dir_stack_top].rpane_file == NULL)
	{
		free_entry(&dir_stack[dir_stack_top]);
		return -1;
	}

	++dir_stack_top;
	changed = 1;

	return 0;
}

int
dir_stack_pop(void)
{
	if(dir_stack_top == 0)
	{
		return -1;
	}

	--dir_stack_top;

	if(change_directory(&lwin, dir_stack[dir_stack_top].lpane_dir) >= 0)
	{
		load_dir_list(&lwin, 0);
	}

	if(change_directory(&rwin, dir_stack[dir_stack_top].rpane_dir) >= 0)
	{
		load_dir_list(&rwin, 0);
	}

	fview_cursor_redraw(curr_view);
	refresh_view_win(other_view);

	free_entry(&dir_stack[dir_stack_top]);

	return 0;
}

int
dir_stack_swap(void)
{
	dir_stack_entry_t item;

	if(dir_stack_top == 0)
	{
		return -1;
	}

	item = dir_stack[--dir_stack_top];

	if(dir_stack_push_current() != 0)
	{
		dir_stack[dir_stack_top--] = item;
		return -1;
	}

	if(change_directory(&lwin, item.lpane_dir) >= 0)
	{
		load_dir_list(&lwin, 0);
	}

	if(change_directory(&rwin, item.rpane_dir) >= 0)
	{
		load_dir_list(&rwin, 0);
	}

	fview_cursor_redraw(curr_view);
	refresh_view_win(other_view);

	free_entry(&item);
	return 0;
}

int
dir_stack_rotate(int n)
{
	dir_stack_entry_t *new_stack;
	size_t i;

	if(n == 0)
	{
		return 0;
	}

	if(dir_stack_push_current() != 0)
	{
		return -1;
	}

	new_stack = reallocarray(NULL, stack_size, sizeof(*dir_stack));
	if(new_stack == NULL)
	{
		return -1;
	}

	for(i = 0U; i < dir_stack_top; ++i)
	{
		new_stack[(i + n)%dir_stack_top] = dir_stack[i];
	}

	free(dir_stack);
	dir_stack = new_stack;
	return dir_stack_pop();
}

void
dir_stack_clear(void)
{
	while(dir_stack_top > 0)
	{
		free_entry(&dir_stack[--dir_stack_top]);
	}
}

/* Frees memory allocated for the specified stack entry. */
static void
free_entry(const dir_stack_entry_t *entry)
{
	free(entry->lpane_dir);
	free(entry->lpane_file);
	free(entry->rpane_dir);
	free(entry->rpane_file);

	/* This function is called almost from all other public ones on successful
	 * scenario, so it's a good place to put change detection. */
	changed = 1;
}

char **
dir_stack_list(void)
{
	size_t i;
	int len;
	char **list, **p;

	if(dir_stack_top == 0)
	{
		len = 2 + 1 + 1;
	}
	else
	{
		len = 2 + 1 + dir_stack_top*2 + dir_stack_top - 1 + 1;
	}
	list = reallocarray(NULL, len, sizeof(*list));

	if(list == NULL)
	{
		return NULL;
	}

	p = list;
	if((*p++ = strdup(flist_get_dir(&lwin))) == NULL)
	{
		return list;
	}
	if((*p++ = strdup(flist_get_dir(&rwin))) == NULL)
	{
		return list;
	}

	for(i = 0U; i < dir_stack_top; ++i)
	{
		if((*p++ = strdup("-----")) == NULL)
		{
			return list;
		}

		if((*p++ = strdup(dir_stack[dir_stack_top - 1 - i].lpane_dir)) == NULL)
		{
			return list;
		}

		if((*p++ = strdup(dir_stack[dir_stack_top - 1 - i].rpane_dir)) == NULL)
		{
			return list;
		}

		if(i == dir_stack_top - 1)
		{
			continue;
		}
	}
	*p = NULL;

	return list;
}

void
dir_stack_freeze(void)
{
	changed = 0;
}

int
dir_stack_changed(void)
{
	return changed;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
