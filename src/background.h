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

#include "compat/pthread.h"

/* Special value of total amount of work in job_t structure to indicate
 * undefined total number of countable operations. */
#define BG_UNDEFINED_TOTAL (-1)

/* Type of background job. */
typedef enum
{
	BJT_COMMAND,   /* Tracked external command started by Vifm. */
	BJT_OPERATION, /* Important internal background operation, e.g. copying. */
	BJT_TASK,      /* Unimportant internal background operations, e.g. calculation
	                  of directory size. */
}
BgJobType;

/* Auxiliary structure to be updated by background tasks while they progress. */
typedef struct bg_op_t
{
	int total; /* Total number of coarse operations. */
	int done;  /* Number of already processed coarse operations. */

	int progress; /* Progress in percents.  -1 if task doesn't provide one. */
	char *descr;  /* Description of current activity, can be NULL. */
}
bg_op_t;

/* Description of background activity. */
typedef struct job_t
{
	BgJobType type; /* Type of background job. */
	pid_t pid;
	char *cmd;
	int skip_errors;
	char *error;

	/* The lock is meant to guard running and exit_code updates in background
	 * jobs. */
	pthread_spinlock_t status_lock_for_bg;
	int running;
	/* TODO: use or remove this (set to correct value, but not used). */
	int exit_code;

	/* For background operations and tasks. */
	pthread_spinlock_t bg_op_lock;
	bg_op_t bg_op;

#ifndef _WIN32
	int fd;
#else
	HANDLE hprocess;
#endif
	struct job_t *next;
}
job_t;

/* Background task entry point function signature. */
typedef void (*bg_task_func)(bg_op_t *bg_op, void *arg);

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
pid_t background_and_capture(char cmd[], int user_sh, FILE **out, FILE **err);

/* Mark background job specified by its process id, which is finished with the
 * exit_code. */
void add_finished_job(pid_t pid, int exit_code);

void check_background_jobs(void);

/* Starts new background task, which is run in a separate thread.  Returns zero
 * on success, otherwise non-zero is returned. */
int bg_execute(const char descr[], const char op_descr[], int total,
		int important, bg_task_func task_func, void *args);

/* Checks whether there are any internal jobs (not external applications tracked
 * by vifm) running in background. */
int bg_has_active_jobs(void);

/* Performs preparations necessary for safe access of the jobs list.  Effect of
 * calling this function must be reverted by calling bg_jobs_unfreeze().
 * Returns zero on success, otherwise non-zero is returned. */
int bg_jobs_freeze(void);

/* Undoes changes made by bg_jobs_freeze(). */
void bg_jobs_unfreeze(void);

/* Temporary locks bg_op_t structure to ensure that it's not modified by
 * anyone during reading/updating its fields.  The structure must be part of
 * job_t. */
void bg_op_lock(bg_op_t *bg_op);

/* Unlocks bg_op_t structure.  The structure must be part of job_t. */
void bg_op_unlock(bg_op_t *bg_op);

/* Callback-like function to report that state of background operation
 * changed. */
void bg_op_changed(bg_op_t *bg_op);

/* Convenience method to update description of background job, use
 * lock -> <change> -> unlock -> changed sequence for more generic cases.  Fires
 * operation change. */
void bg_op_set_descr(bg_op_t *bg_op, const char descr[]);

#endif /* VIFM__BACKGROUND_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
