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

#include "dirstask_menu.h"

#include <string.h> /* strdup() */

#include "../modes/menu.h"
#include "../dir_stack.h"
#include "../ui.h"
#include "menus.h"

int
show_dirstack_menu(FileView *view)
{
	static menu_info m;
	/* Directory stack always contains at least one item (current directories). */
	init_menu_info(&m, DIRSTACK, NULL);
	m.title = strdup(" Directory Stack ");

	m.items = dir_stack_list();

	m.len = -1;
	while(m.items[++m.len] != NULL);

	return display_menu(&m, view);
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
