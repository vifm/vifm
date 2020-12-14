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

#include <stddef.h> /* NULL */
#include <stdio.h> /* snprintf() */
#include <string.h> /* strlen() strdup() */

#include "../compat/reallocarray.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../modes/menu.h"
#include "../ui/ui.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../background.h"
#include "menus.h"

static int execute_jobs_cb(view_t *view, menu_data_t *m);
static KHandlerResponse jobs_khandler(view_t *view, menu_data_t *m,
		const wchar_t keys[]);
static int cancel_job(menu_data_t *m, bg_job_t *job);
static void show_job_errors(view_t *view, menu_data_t *m, bg_job_t *job);
static KHandlerResponse errs_khandler(view_t *view, menu_data_t *m,
		const wchar_t keys[]);

/* Menu jobs description. */
static menu_data_t jobs_m;

int
show_jobs_menu(view_t *view)
{
	bg_job_t *p;
	int i;

	menus_init_data(&jobs_m, view, strdup("Pid --- Command"),
			strdup("No jobs currently running"));
	jobs_m.execute_handler = &execute_jobs_cb;
	jobs_m.key_handler = &jobs_khandler;

	bg_check();

	p = bg_jobs;

	i = 0;
	while(p != NULL)
	{
		if(bg_job_is_running(p))
		{
			char info_buf[24];
			char item_buf[sizeof(info_buf) + strlen(p->cmd) + 1024];

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

			snprintf(item_buf, sizeof(item_buf), "%-8s  %s%s", info_buf, p->cmd,
					bg_job_cancelled(p) ? " (cancelling...)" : "");
			i = add_to_string_array(&jobs_m.items, i, item_buf);
			jobs_m.void_data = reallocarray(jobs_m.void_data, i,
					sizeof(*jobs_m.void_data));
			jobs_m.void_data[i - 1] = p;
		}

		p = p->next;
	}

	jobs_m.len = i;

	return menus_enter(jobs_m.state, view);
}

/* Callback that is called when menu item is selected.  Should return non-zero
 * to stay in menu mode. */
static int
execute_jobs_cb(view_t *view, menu_data_t *m)
{
	/* TODO: write code for job control. */
	return 0;
}

/* Menu-specific shortcut handler.  Returns code that specifies both taken
 * actions and what should be done next. */
static KHandlerResponse
jobs_khandler(view_t *view, menu_data_t *m, const wchar_t keys[])
{
	if(wcscmp(keys, L"dd") == 0)
	{
		if(!cancel_job(m, m->void_data[m->pos]))
		{
			show_error_msg("Job cancellation", "The job has already stopped");
			return KHR_REFRESH_WINDOW;
		}

		menus_partial_redraw(m->state);
		return KHR_REFRESH_WINDOW;
	}
	else if(wcscmp(keys, L"e") == 0)
	{
		show_job_errors(view, m, m->void_data[m->pos]);
		return KHR_REFRESH_WINDOW;
	}
	/* TODO: maybe use DD for forced termination? */
	return KHR_UNHANDLED;
}

/* Cancels the job if it's still running.  Returns non-zero if operation was
 * cancelled, otherwise it's already finished and zero is returned. */
static int
cancel_job(menu_data_t *m, bg_job_t *job)
{
	bg_job_t *p;

	/* We have to make sure the job pointer is still valid and the job is
	 * running. */
	for(p = bg_jobs; p != NULL; p = p->next)
	{
		if(p == job && bg_job_is_running(job))
		{
			if(bg_job_cancel(job))
			{
				char *new_line = format_str("%s (cancelling...)", m->items[m->pos]);
				free(m->items[m->pos]);
				m->items[m->pos] = new_line;
			}
			break;
		}
	}

	return (p != NULL);
}

/* Shows job errors if there is something and the job is still running.
 * Switches to separate menu description. */
static void
show_job_errors(view_t *view, menu_data_t *m, bg_job_t *job)
{
	char *cmd = NULL, *errors = NULL;
	size_t errors_len = 0U;
	bg_job_t *p;

	/* We have to make sure the job pointer is still valid and the job is
	 * running. */
	for(p = bg_jobs; p != NULL; p = p->next)
	{
		if(p == job)
		{
			cmd = strdup(job->cmd);
			errors = strdup(job->errors == NULL ? "" : job->errors);
			errors_len = job->errors_len;
			break;
		}
	}

	if(p == NULL)
	{
		show_error_msg("Job errors", "The job has already finished");
	}
	else if(is_null_or_empty(errors))
	{
		show_error_msg("Job errors", "No errors to show");
	}
	else
	{
		static menu_data_t m;

		menus_init_data(&m, view, format_str("Job errors (%s)", cmd), NULL);
		m.key_handler = &errs_khandler;
		m.items = break_into_lines(errors, errors_len, &m.len, 0);

		modmenu_reenter(&m);
	}
	free(cmd);
	free(errors);
}

/* Menu-specific shortcut handler.  Returns code that specifies both taken
 * actions and what should be done next. */
static KHandlerResponse
errs_khandler(view_t *view, menu_data_t *m, const wchar_t keys[])
{
	if(wcscmp(keys, L"h") == 0)
	{
		modmenu_reenter(&jobs_m);
		return KHR_REFRESH_WINDOW;
	}
	return KHR_UNHANDLED;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
