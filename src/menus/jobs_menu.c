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

#include <signal.h> /* sig* */
#include <stdlib.h> /* malloc() realloc() */
#include <string.h> /* strlen() strdup() */

#include "../modes/menu.h"
#include "../background.h"
#include "../ui.h"
#include "menus.h"

#include "jobs_menu.h"

int
show_jobs_menu(FileView *view)
{
	job_t *p;
#ifndef _WIN32
	sigset_t new_mask;
#endif
	int x;
	static menu_info m;
	m.top = 0;
	m.current = 1;
	m.len = 0;
	m.pos = 0;
	m.hor_pos = 0;
	m.win_rows = getmaxy(menu_win);
	m.type = JOBS;
	m.matching_entries = 0;
	m.matches = NULL;
	m.match_dir = NONE;
	m.regexp = NULL;
	m.title = NULL;
	m.args = NULL;
	m.items = NULL;
	m.data = NULL;

	/*
	 * SIGCHLD needs to be blocked anytime the finished_jobs list
	 * is accessed from anywhere except the received_sigchld().
	 */
#ifndef _WIN32
	sigemptyset(&new_mask);
	sigaddset(&new_mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &new_mask, NULL);
#else
	check_background_jobs();
#endif

	p = jobs;

	x = 0;
	while(p != NULL)
	{
		if(p->running)
		{
			m.items = (char **)realloc(m.items, sizeof(char *)*(x + 1));
			m.items[x] = (char *)malloc(strlen(p->cmd) + 24);
			if(p->pid == -1)
				snprintf(m.items[x], strlen(p->cmd) + 22, " %d/%d %s ", p->done + 1,
						p->total, p->cmd);
			else
				snprintf(m.items[x], strlen(p->cmd) + 22, " %d %s ", p->pid, p->cmd);

			x++;
		}

		p = p->next;
	}

#ifndef _WIN32
	/* Unblock SIGCHLD signal */
	sigprocmask(SIG_UNBLOCK, &new_mask, NULL);
#endif

	m.len = x;

	if(m.len < 1)
	{
		char buf[256];

		m.items = (char **)realloc(m.items, sizeof(char *) * (x + 1));
		m.items[x] = (char *)malloc(strlen("Press return to continue.") + 2);
		snprintf(m.items[x], strlen("Press return to continue."),
					"Press return to continue.");
		snprintf(buf, sizeof(buf), "No background jobs are running");
		m.len = 1;

		m.title = strdup(" No jobs currently running ");
	}
	else
	{
		m.title = strdup(" Pid --- Command ");
	}

	setup_menu();
	draw_menu(&m);
	move_to_menu_pos(m.pos, &m);
	enter_menu_mode(&m, view);
	return 0;
}

void
execute_jobs_cb(FileView *view, menu_info *m)
{
	/* TODO write code for job control */
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
