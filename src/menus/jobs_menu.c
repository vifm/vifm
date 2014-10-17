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

#include "jobs_menu.h"

#include <stdio.h> /* snprintf() */
#include <string.h> /* strlen() strdup() */

#include "../modes/menu.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../background.h"
#include "../ui.h"
#include "menus.h"

static int execute_jobs_cb(FileView *view, menu_info *m);

int
show_jobs_menu(FileView *view)
{
	job_t *p;
	int i;

	static menu_info m;
	init_menu_info(&m, JOBS_MENU, strdup("No jobs currently running"));
	m.title = strdup(" Pid --- Command ");
	m.execute_handler = &execute_jobs_cb;

	check_background_jobs();

	bg_jobs_freeze();

	p = jobs;

	i = 0;
	while(p != NULL)
	{
		if(p->running)
		{
			char item_buf[strlen(p->cmd) + 24];
			if(p->type == BJT_COMMAND)
			{
				snprintf(item_buf, sizeof(item_buf), " " PRINTF_PID_T " %s ", p->pid,
						p->cmd);
			}
			else
			{
				if(p->total == BG_UNDEFINED_TOTAL)
				{
					snprintf(item_buf, sizeof(item_buf), " N/A %s ", p->cmd);
				}
				else
				{
					snprintf(item_buf, sizeof(item_buf), " %d/%d %s ", p->done + 1,
							p->total, p->cmd);
				}
			}
			i = add_to_string_array(&m.items, i, 1, item_buf);
		}

		p = p->next;
	}

	bg_jobs_unfreeze();

	m.len = i;

	return display_menu(&m, view);
}

/* Callback that is called when menu item is selected.  Should return non-zero
 * to stay in menu mode. */
static int
execute_jobs_cb(FileView *view, menu_info *m)
{
	/* TODO write code for job control */
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
