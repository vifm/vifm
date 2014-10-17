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

#ifndef VIFM__BACKGROUND_H__
#define VIFM__BACKGROUND_H__

#ifdef _WIN32
#include <windef.h>
#endif

#include <sys/types.h> /* pid_t */

#include <stdio.h>

/* Special value of process id for internal tasks running in background
 * threads. */
#define BG_INTERNAL_TASK_PID ((pid_t)-1)

/* Special value of total amount of work in job_t structure to indicate
 * undefined total number of countable operations. */
#define BG_UNDEFINED_TOTAL (-1)

typedef struct job_t
{
	pid_t pid;
	char *cmd;
	int skip_errors;
	int running;
	int exit_code;
	char *error;

	/* for backgrounded commands */
	int total;
	int done;

#ifndef _WIN32
	int fd;
#else
	HANDLE hprocess;
#endif
	struct job_t *next;
}
job_t;

/* Background task entry point function signature. */
typedef void (*bg_task_func)(void *arg);

extern struct job_t *jobs;

/* Prepare background unit for the work. */
void init_background(void);

/* Returns zero on success, otherwise non-zero is returned. */
int start_background_job(const char *cmd, int skip_errors);

/* Runs command in background not redirecting its streams.  To determine an
 * error uses exit status only.  cancelled can be NULL when cancellable is 0.
 * Returns status on success, otherwise -1 is returned.  Sets correct value of
 * *cancelled even on error. */
int background_and_wait_for_status(char cmd[], int cancellable, int *cancelled);

/* Runs command in background and displays its errors to a user.  To determine
 * an error uses both stderr stream and exit status.  Returns zero on success,
 * otherwise non-zero is returned. */
int background_and_wait_for_errors(char cmd[], int cancellable);

/* Runs command in a background and redirects its stdout and stderr streams to
 * file streams which are set.  Returns id of background process ((pid_t)0 for
 * non-*nix like systems) or (pid_t)-1 on error. */
pid_t background_and_capture(char *cmd, FILE **out, FILE **err);

void add_finished_job(pid_t pid, int status);
void check_background_jobs(void);

void inner_bg_next(void);

/* Start new background task, executed in a separate thread.  Returns zero on
 * success, otherwise non-zero is returned. */
int bg_execute(const char desc[], int total, bg_task_func task_func,
		void *args);

/* Checks whether there are any internal jobs (not external applications tracked
 * by vifm) running in background. */
int bg_has_active_jobs(void);

/* Performs preparations necessary for safe access of the jobs list.  Effect of
 * calling this function must be reverted by calling bg_jobs_unfreeze().
 * Returns zero on success, otherwise non-zero is returned. */
int bg_jobs_freeze(void);

/* Undoes changes made by bg_jobs_freeze(). */
void bg_jobs_unfreeze(void);

#endif /* VIFM__BACKGROUND_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
