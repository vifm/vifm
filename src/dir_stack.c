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

#include "filelist.h"
#include "ui.h"

stack_entry_t *stack;
unsigned int stack_top;
static unsigned int stack_size;

/* Whether directory stack was changed since its creation or last freezing. */
static int changed;

static void free_entry(const stack_entry_t *entry);

int
pushd(void)
{
	return push_to_dirstack(lwin.curr_dir, lwin.dir_entry[lwin.list_pos].name,
			rwin.curr_dir, rwin.dir_entry[rwin.list_pos].name);
}

int
push_to_dirstack(const char *ld, const char *lf, const char *rd, const char *rf)
{
	if(stack_top == stack_size)
	{
		stack_entry_t *s = realloc(stack, (stack_size + 1)*sizeof(*stack));
		if(s == NULL)
		{
			return -1;
		}

		stack = s;
		stack_size++;
	}

	stack[stack_top].lpane_dir = strdup(ld);
	stack[stack_top].lpane_file = strdup(lf);
	stack[stack_top].rpane_dir = strdup(rd);
	stack[stack_top].rpane_file = strdup(rf);

	if(stack[stack_top].lpane_dir == NULL ||
			stack[stack_top].lpane_file == NULL ||
			stack[stack_top].rpane_dir == NULL || stack[stack_top].rpane_file == NULL)
	{
		free_entry(&stack[stack_top]);
		return -1;
	}

	stack_top++;
	changed = 1;

	return 0;
}

int
popd(void)
{
	if(stack_top == 0)
		return -1;

	stack_top--;

	if(change_directory(&lwin, stack[stack_top].lpane_dir) >= 0)
		load_dir_list(&lwin, 0);

	if(change_directory(&rwin, stack[stack_top].rpane_dir) >= 0)
		load_dir_list(&rwin, 0);

	move_to_list_pos(curr_view, curr_view->list_pos);
	refresh_view_win(other_view);

	free_entry(&stack[stack_top]);

	return 0;
}

int
swap_dirs(void)
{
	stack_entry_t item;

	if(stack_top == 0)
		return -1;

	item = stack[--stack_top];

	if(pushd() != 0)
	{
		free_entry(&item);
		return -1;
	}

	if(change_directory(&lwin, item.lpane_dir) >= 0)
		load_dir_list(&lwin, 0);

	if(change_directory(&rwin, item.rpane_dir) >= 0)
		load_dir_list(&rwin, 0);

	move_to_list_pos(curr_view, curr_view->list_pos);
	refresh_view_win(other_view);

	free_entry(&item);
	return 0;
}

int
rotate_stack(int n)
{
	stack_entry_t *new_stack;
	int i;

	if(n == 0)
		return 0;

	if(pushd() != 0)
		return -1;

	new_stack = malloc(stack_size*sizeof(*stack));
	if(new_stack == NULL)
		return -1;

	for(i = 0; i < stack_top; i++)
		new_stack[(i + n)%stack_top] = stack[i];

	free(stack);
	stack = new_stack;
	return popd();
}

void
clean_stack(void)
{
	while(stack_top > 0)
	{
		free_entry(&stack[--stack_top]);
	}
}

static void
free_entry(const stack_entry_t *entry)
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
	int i;
	int len;
	char **list, **p;

	if(stack_top == 0)
		len = 2 + 1 + 1;
	else
		len = 2 + 1 + stack_top*2 + stack_top - 1 + 1;
	list = malloc(sizeof(char *)*len);

	if(list == NULL)
		return NULL;

	p = list;
	if((*p++ = strdup(lwin.curr_dir)) == NULL)
		return list;
	if((*p++ = strdup(rwin.curr_dir)) == NULL)
		return list;
	if(stack_top != 0)
	{
		if((*p++ = strdup("-----")) == NULL)
			return list;
	}

	for(i = 0; i < stack_top; i++)
	{
		if((*p++ = strdup(stack[stack_top - 1 - i].lpane_dir)) == NULL)
			return list;

		if((*p++ = strdup(stack[stack_top - 1 - i].rpane_dir)) == NULL)
			return list;

		if(i == stack_top - 1)
			continue;

		if((*p++ = strdup("-----")) == NULL)
			return list;
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
/* vim: set cinoptions+=t0 : */
