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
#include "../ui/ui.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../background.h"
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
			char info_buf[24];
			char item_buf[sizeof(info_buf) + strlen(p->cmd)];

			if(p->type == BJT_COMMAND)
			{
				snprintf(info_buf, sizeof(info_buf), "%" PRINTF_ULL,
						(unsigned long long)p->pid);
			}
			else if(p->bg_op.total == BG_UNDEFINED_TOTAL)
			{
				snprintf(info_buf, sizeof(info_buf), "n/a");
			}
			else
			{
				snprintf(info_buf, sizeof(info_buf), "%d/%d", p->bg_op.done + 1,
						p->bg_op.total);
			}

			snprintf(item_buf, sizeof(item_buf), "%-8s  %s", info_buf, p->cmd);
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
