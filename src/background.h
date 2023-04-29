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
#include "utils/utils.h"

/* Special value of total amount of work in bg_job_t structure to indicate
 * undefined total number of countable operations. */
#define BG_UNDEFINED_TOTAL (-1)

struct bg_job_t;

/* Type of a function to invoke when the job is done. */
typedef void (*bg_job_exit_func)(struct bg_job_t *job, void *data);

/* Type of background job. */
typedef enum
{
	BJT_COMMAND,   /* Tracked external command started by Vifm. */
	BJT_OPERATION, /* Important internal background operation, e.g. copying. */
	BJT_TASK,      /* Unimportant internal background operations, e.g. calculation
	                  of directory size. */
}
BgJobType;

/* Job flags passed to bg_run_external_job(). */
typedef enum
{
	BJF_NONE            = 0,      /* No flags set. */
	BJF_JOB_BAR_VISIBLE = 1 << 0, /* Makes the job appear on the job bar. */
	BJF_MENU_VISIBLE    = 1 << 1, /* Makes the job appear in :jobs menu. */
	BJF_SUPPLY_INPUT    = 1 << 2, /* Open a pipe for standard input stream. */
	BJF_CAPTURE_OUT     = 1 << 3, /* Capture output stream(s). */
	BJF_MERGE_STREAMS   = 1 << 4, /* Merge error stream into output stream. */
	BJF_KEEP_IN_FG      = 1 << 5, /* Do not detach from terminal session or
	                                 process group. */
}
BgJobFlags;

/* Auxiliary structure to be updated by background tasks while they progress. */
typedef struct bg_op_t
{
	int total; /* Total number of coarse operations. */
	int done;  /* Number of already processed coarse operations. */

	int progress; /* Progress in percents.  -1 if task doesn't provide one. */
	char *descr;  /* Description of current activity, can be NULL. */

	int cancelled; /* Whether cancellation has been requested. */
}
bg_op_t;

/* Description of background activity. */
typedef struct bg_job_t
{
	BgJobType type; /* Type of background job. */
	int cancelled;  /* Whether cancellation has been requested. */
	pid_t pid;
	char *cmd;

	int skip_errors;   /* Do not show future errors. */

	/* The lock is meant to guard error-related fields. */
	pthread_spinlock_t errors_lock;
	char *new_errors;      /* Portion of stderr which hasn't been shown yet. */
	size_t new_errors_len; /* Length of the new_errors field. */
	char *errors;          /* Whole error stream collected. */
	size_t errors_len;     /* Length of the errors field. */

	/* The lock is meant to guard state-related fields. */
	pthread_spinlock_t status_lock;
	int running;   /* Whether this job is still running. */
	int use_count; /* Count of uses of this job entry. */
	int exit_code; /* Exit code of external command. */

	FILE *input;  /* File stream of standard input or NULL. */
	FILE *output; /* File stream of standard output or NULL. */

	bg_job_exit_func exit_cb; /* Function to invoke when the job exits. */
	void *exit_cb_arg;        /* Argument to pass to that function. */

	int with_bg_op;                /* Whether bg_op* fields are active. */
	int on_job_bar;                /* Whether this task was put on a job bar. */
	pthread_spinlock_t bg_op_lock; /* Lock for accessing bg_op field. */
	bg_op_t bg_op;                 /* Progress and cancellation information. */

#ifndef _WIN32
	int err_stream;    /* stderr stream of the job or -1. */
#else
	HANDLE err_stream; /* stderr stream of the job or invalid handle. */
	HANDLE hprocess;   /* Handle to the process of the job or invalid handle. */
	HANDLE hjob;       /* Handle to the process job object. */
#endif

	struct bg_job_t *next;     /* Link to the next element in bg_jobs list. */

	/* Used by error thread for BJT_COMMAND jobs. */
	struct bg_job_t *err_next; /* Link to the next element in error read list. */
	int drained;               /* Whether error stream of no interest anymore. */

	int in_menu; /* Whether this task is visible in :jobs menu. */
}
bg_job_t;

/* Background task entry point function signature. */
typedef void (*bg_task_func)(bg_op_t *bg_op, void *arg);

/* List of background jobs. */
extern bg_job_t *bg_jobs;

/* Prepare background unit for the work.  Returns zero on success. */
int bg_init(void);

/* Creates background job running external command.  Returns zero on success,
 * otherwise non-zero is returned.  If *input is not NULL, it's set to input
 * pipe of the background process.  The caller becomes responsible for the
 * stream. */
int bg_run_external(const char cmd[], int skip_errors, ShellRequester by,
		FILE **input);

/* Creates background job running external command which does not interact with
 * the user and is detached from controlling terminal.  Upon creation the job
 * has one extra use, which needs to be decremented for it to be freed.  Returns
 * the job or NULL on error. */
bg_job_t * bg_run_external_job(const char cmd[], BgJobFlags flags);

struct cancellation_t;

/* Runs command in background and displays its errors to a user.  To determine
 * an error uses both stderr stream and exit status.  Returns zero on success,
 * otherwise non-zero is returned. */
int bg_and_wait_for_errors(char cmd[],
		const struct cancellation_t *cancellation);

/* Runs command in a background and redirects its streams to specified streams
 * that are non-NULL, otherwise they are left connected to the terminal.  Don't
 * pass pipe for input, it can cause deadlock.  Returns id of background
 * process ((pid_t)0 for non-*nix like systems) or (pid_t)-1 on error. */
pid_t bg_run_and_capture(char cmd[], int user_sh, FILE *in, FILE **out,
		FILE **err);

/* Checks status of background jobs (their streams and state).  Removes finished
 * ones from the list, displays any pending error messages, corrects job bar if
 * needed. */
void bg_check(void);

/* Starts new background task, which is run in a separate thread.  Returns zero
 * on success, otherwise non-zero is returned. */
int bg_execute(const char descr[], const char op_descr[], int total,
		int important, bg_task_func task_func, void *args);

/* Checks whether there are any internal jobs (important_only is non-zero) or
 * jobs or tasks (important_only is zero) running in background.  External
 * applications whose state is tracked are always ignored by this function. */
int bg_has_active_jobs(int important_only);

/* Sets exit callback for the job. */
void bg_job_set_exit_cb(bg_job_t *job, bg_job_exit_func cb, void *arg);

/* Cancels the job.  Returns non-zero if job wasn't cancelled before, but is
 * after this call, otherwise zero is returned. */
int bg_job_cancel(bg_job_t *job);

/* Checks whether the job has been cancelled.  Returns non-zero if so, otherwise
 * zero is returned. */
int bg_job_cancelled(bg_job_t *job);

/* Terminates the job in a forceful way leaving it no chance to respond. */
void bg_job_terminate(bg_job_t *job);

/* Checks whether the job is still running.  Returns non-zero if so, otherwise
 * zero is returned. */
int bg_job_is_running(bg_job_t *job);

/* Checks whether the job was killed.  Returns non-zero if so, otherwise zero is
 * returned. */
int bg_job_was_killed(bg_job_t *job);

/* Waits for external command to finish (don't pass any other kind of job).
 * Returns zero on success, otherwise non-zero is returned. */
int bg_job_wait(bg_job_t *job);

/* Increases use counter of the job.  Doing this prevents object deletion while
 * it's still in use. */
void bg_job_incref(bg_job_t *job);

/* Decreases use counter of the job.  Assume the object is freed after calling
 * this functions (unless you've incremented the counter more than once). */
void bg_job_decref(bg_job_t *job);

/* Temporary locks bg_op_t structure to ensure that it's not modified by
 * anyone during reading/updating its fields.  The structure must be part of
 * bg_job_t.  Returns non-zero on success. */
int bg_op_lock(bg_op_t *bg_op);

/* Unlocks bg_op_t structure.  The structure must be part of bg_job_t. */
void bg_op_unlock(bg_op_t *bg_op);

/* Callback-like function to report that state of background operation
 * changed. */
void bg_op_changed(bg_op_t *bg_op);

/* Convenience method to update description of background job, use
 * lock -> <change> -> unlock -> changed sequence for more generic cases.  Fires
 * operation change. */
void bg_op_set_descr(bg_op_t *bg_op, const char descr[]);

/* Convenience method to check for background job cancellation, use
 * lock -> <check> -> unlock -> changed sequence for more generic cases.
 * Returns non-zero if cancellation requested, otherwise zero is returned. */
int bg_op_cancelled(bg_op_t *bg_op);

#endif /* VIFM__BACKGROUND_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
