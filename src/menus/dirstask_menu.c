/* vifm
 * Copyright (C) 2001 Ken Steen.
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

#include <string.h> /* strdup() */

#include "../modes/menu.h"
#include "../dir_stack.h"
#include "../ui.h"
#include "menus.h"

#include "dirstask_menu.h"

int
show_dirstack_menu(FileView *view)
{
	int i;

	static menu_info m;
	init_menu_info(&m, DIRSTACK);
	m.title = strdup(" Directory Stack ");

	m.items = dir_stack_list();

	i = -1;
	while(m.items[++i] != NULL);
	if(i != 0)
	{
		m.len = i;
	}
	else
	{
		m.items[0] = strdup("Directory stack is empty");
		m.len = 1;
	}

	setup_menu();
	draw_menu(&m);
	move_to_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
	return 0;
}

void
execute_dirstack_cb(FileView *view, menu_info *m)
{
	int pos = 0;
	int i;

	if(m->items[m->pos][0] == '-')
		return;

	for(i = 0; i < m->pos; i++)
		if(m->items[i][0] == '-')
			pos++;
	rotate_stack(pos);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
