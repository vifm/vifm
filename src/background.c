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

#include "background.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <fcntl.h> /* open() */
#include <sys/stat.h> /* O_RDONLY */
#include <sys/types.h> /* pid_t ssize_t */
#ifndef _WIN32
#include <sys/wait.h> /* waitpid() */
#endif
#include <signal.h> /* SIG* kill() */
#include <unistd.h> /* execve() fork() setsid() */

#include <assert.h> /* assert() */
#include <errno.h> /* errno */
#include <stddef.h> /* NULL wchar_t */
#include <stdint.h> /* uintptr_t */
#include <stdlib.h> /* EXIT_FAILURE _Exit() free() malloc() */
#include <string.h> /* strdup() */

#include "cfg/config.h"
#include "compat/pthread.h"
#include "engine/var.h"
#include "engine/variables.h"
#include "modes/dialogs/msg_dialog.h"
#include "ui/cancellation.h"
#include "ui/statusline.h"
#include "ui/ui.h"
#include "utils/cancellation.h"
#include "utils/env.h"
#include "utils/fs.h"
#include "utils/log.h"
#include "utils/path.h"
#include "utils/selector.h"
#include "utils/str.h"
#include "utils/utils.h"
#include "cmd_completion.h"
#include "status.h"

/**
 * This unit implements three kinds of backgrounded operations:
 *  - external applications run from vifm (commands);
 *  - threads that perform auxiliary work (tasks), like counting size of
 *    directories;
 *  - threads that perform important work (operations), like file copying,
 *    deletion, etc.
 *
 * All jobs can be viewed via :jobs menu.
 *
 * Tasks and operations can provide progress information for displaying it in
 * UI.
 *
 * Operations are displayed on designated job bar.
 *
 * On non-Windows systems background thread reads data from error streams of
 * external applications, which are then displayed by main thread.  This thread
 * maintains its own list of jobs (via err_next field), which is added to by
 * building a temporary list with new_err_jobs pointing to its head.  Every job
 * that has associated external process has the following life cycle:
 *  1. Created by main thread and passed to error thread through new_err_jobs.
 *  2. Either gets marked by signal handler or its stream reaches EOF.
 *  3. Its use_count field is decremented.
 *  4. Main thread frees corresponding entry.
 */

/* Turns pointer (P) to field (F) of a structure (S) to address of that
 * structure. */
#define STRUCT_FROM_FIELD(S, F, P) ((S *)((char *)P - offsetof(S, F)))

/* Special value of process id for internal tasks running in background
 * threads. */
#define WRONG_PID ((pid_t)-1)

/* Size of error message reading buffer. */
#define ERR_MSG_LEN 1025

/* Value of job communication mean for internal jobs. */
#ifndef _WIN32
#define NO_JOB_ID (-1)
#else
#define NO_JOB_ID INVALID_HANDLE_VALUE
#endif

/* Structure with passed to background_task_bootstrap() so it can perform
 * correct initialization/cleanup. */
typedef struct
{
	bg_task_func func; /* Function to execute in a background thread. */
	void *args;        /* Argument to pass. */
	bg_job_t *job;     /* Job identifier that corresponds to the task. */
}
background_task_args;

static void set_jobcount_var(int count);
static void job_check(bg_job_t *job);
static void job_free(bg_job_t *job);
static void * error_thread(void *p);
static void update_error_jobs(bg_job_t **jobs);
static void free_drained_jobs(bg_job_t **jobs);
static void import_error_jobs(bg_job_t **jobs);
static void make_ready_list(const bg_job_t *jobs, selector_t *selector);
#ifndef _WIN32
static void rip_children(void);
static void rip_child(pid_t pid, int status);
static void report_error_msg(const char title[], const char text[]);
#endif
static bg_job_t * launch_external(const char cmd[], BgJobFlags flags,
		ShellRequester by);
static void append_error_msg(bg_job_t *job, const char err_msg[]);
static void place_on_job_bar(bg_job_t *job);
static void get_off_job_bar(bg_job_t *job);
static bg_job_t * add_background_job(pid_t pid, const char cmd[],
		uintptr_t err, uintptr_t data, BgJobType type, int with_bg_op);
static void * background_task_bootstrap(void *arg);
static void set_current_job(bg_job_t *job);
static void make_current_job_key(void);
static int update_job_status(bg_job_t *job);
static void mark_job_finished(bg_job_t *job, int exit_code);
static int bg_op_cancel(bg_op_t *bg_op);

bg_job_t *bg_jobs = NULL;

/* Head of list of newly started jobs. */
static bg_job_t *new_err_jobs;
/* Mutex to protect new_err_jobs. */
static pthread_mutex_t new_err_jobs_lock = PTHREAD_MUTEX_INITIALIZER;
/* Conditional variable to signal availability of new jobs in new_err_jobs. */
static pthread_cond_t new_err_jobs_cond = PTHREAD_COND_INITIALIZER;

/* Thread local storage for bg_job_t associated with active thread. */
static pthread_key_t current_job;

void
bg_init(void)
{
	pthread_t id;
	int err = pthread_create(&id, NULL, &error_thread, NULL);
	assert(err == 0);
	(void)err;

	/* Initialize state for the main thread. */
	set_current_job(NULL);
}

void
bg_check(void)
{
#ifndef _WIN32
	rip_children();
#endif

	/* Quit if there is no jobs or list is unavailable (e.g. used by another
	 * invocation of this function). */
	if(bg_jobs == NULL)
	{
		set_jobcount_var(0);
		return;
	}

	int active_jobs = 0;

	bg_job_t *head = bg_jobs;
	bg_jobs = NULL;

	bg_job_t *p = head;
	bg_job_t *prev = NULL;
	while(p != NULL)
	{
		job_check(p);

		pthread_spin_lock(&p->status_lock);
		int running = p->running;
		int can_remove = (!p->running && p->use_count == 0);
		pthread_spin_unlock(&p->status_lock);

		active_jobs += (running != 0 && p->in_menu);

		if(!running && p->on_job_bar)
		{
			get_off_job_bar(p);
		}

		/* Remove job if it is finished now. */
		if(can_remove)
		{
			bg_job_t *j = p;
			if(prev != NULL)
				prev->next = p->next;
			else
				head = p->next;

			p = p->next;

			job_free(j);
		}
		else
		{
			prev = p;
			p = p->next;
		}
	}

	assert(bg_jobs == NULL && "Job list shouldn't be used by anyone.");
	bg_jobs = head;

	set_jobcount_var(active_jobs);
}

/* Updates builtin variable that holds number of active jobs.  Schedules UI
 * redraw on change. */
static void
set_jobcount_var(int count)
{
	int old_count = var_to_int(getvar("v:jobcount"));
	if(count != old_count)
	{
		var_t var = var_from_int(count);
		setvar("v:jobcount", var);
		var_free(var);

		stats_redraw_later();
	}
}

/* Checks status of the job.  Processes error stream or checks whether process
 * is still running. */
static void
job_check(bg_job_t *job)
{
	char *new_errors;

	/* Display portions of errors from the job while there are any. */
	do
	{
		pthread_spin_lock(&job->errors_lock);
		new_errors = job->new_errors;
		job->new_errors = NULL;
		job->new_errors_len = 0U;
		pthread_spin_unlock(&job->errors_lock);

		if(new_errors != NULL && !job->skip_errors)
		{
			job->skip_errors = prompt_error_msg("Background Process Error",
					new_errors);
		}
		free(new_errors);
	}
	while(new_errors != NULL);

	(void)update_job_status(job);
}

/* Frees resources allocated by the job as well as the bg_job_t structure
 * itself.  The job can be NULL. */
static void
job_free(bg_job_t *job)
{
	if(job == NULL)
	{
		return;
	}

	pthread_spin_destroy(&job->errors_lock);
	pthread_spin_destroy(&job->status_lock);
	if(job->with_bg_op)
	{
		pthread_spin_destroy(&job->bg_op_lock);
	}

#ifndef _WIN32
	if(job->err_stream != NO_JOB_ID)
	{
		close(job->err_stream);
	}
#else
	if(job->err_stream != NO_JOB_ID)
	{
		CloseHandle(job->err_stream);
	}
	if(job->hprocess != NO_JOB_ID)
	{
		CloseHandle(job->hprocess);
	}
	if(job->hjob != NO_JOB_ID)
	{
		CloseHandle(job->hjob);
	}
#endif
	if(job->input != NULL)
	{
		fclose(job->input);
	}
	if(job->output != NULL)
	{
		fclose(job->output);
	}
	free(job->bg_op.descr);
	free(job->cmd);
	free(job->new_errors);
	free(job->errors);
	free(job);
}

int
bg_and_wait_for_errors(char cmd[], const struct cancellation_t *cancellation)
{
#ifndef _WIN32
	pid_t pid;
	int error_pipe[2];
	int result = 0;

	if(pipe(error_pipe) != 0)
	{
		report_error_msg("File pipe error", "Error creating pipe");
		return -1;
	}

	if((pid = fork()) == -1)
	{
		return -1;
	}

	if(pid == 0)
	{
		run_from_fork(error_pipe, 1, 0, cmd, SHELL_BY_APP);
	}
	else
	{
		char buf[80*10];
		char linebuf[80];
		int nread = 0;

		close(error_pipe[1]); /* Close write end of pipe. */

		wait_for_data_from(pid, NULL, error_pipe[0], cancellation);

		buf[0] = '\0';
		while((nread = read(error_pipe[0], linebuf, sizeof(linebuf) - 1)) > 0)
		{
			const int read_empty_line = nread == 1 && linebuf[0] == '\n';
			result = -1;
			linebuf[nread] = '\0';

			if(!read_empty_line)
			{
				strncat(buf, linebuf, sizeof(buf) - strlen(buf) - 1);
			}

			wait_for_data_from(pid, NULL, error_pipe[0], cancellation);
		}
		close(error_pipe[0]);

		if(result != 0)
		{
			report_error_msg("Background Process Error", buf);
		}
		else
		{
			result = status_to_exit_code(get_proc_exit_status(pid));
		}
	}

	return result;
#else
	return -1;
#endif
}

/* Entry point of a thread which reads input from input of active background
 * programs.  Does not return. */
static void *
error_thread(void *p)
{
	enum { ERROR_SELECT_TIMEOUT_MS = 250 };

	bg_job_t *jobs = NULL;

	selector_t *selector = selector_alloc();
	if(selector == NULL)
	{
		return NULL;
	}

	(void)pthread_detach(pthread_self());
	block_all_thread_signals();

	while(1)
	{
		update_error_jobs(&jobs);
		make_ready_list(jobs, selector);
		while(selector_wait(selector, ERROR_SELECT_TIMEOUT_MS))
		{
			int need_update_list = (jobs == NULL);

			bg_job_t **job = &jobs;
			while(*job != NULL)
			{
				bg_job_t *const j = *job;
				char err_msg[ERR_MSG_LEN];
				ssize_t nread;

				if(!selector_is_ready(selector, j->err_stream))
				{
					goto next_job;
				}

#ifndef _WIN32
				nread = read(j->err_stream, err_msg, sizeof(err_msg) - 1U);
#else
				nread = -1;
				DWORD bytes_read;
				if(ReadFile(j->err_stream, err_msg, sizeof(err_msg) - 1U, &bytes_read,
							NULL))
				{
					nread = bytes_read;
				}
#endif
				if(nread < 0)
				{
					need_update_list = 1;
					j->drained = 1;
					goto next_job;
				}

				if(nread == 0)
				{
					/* Reached EOF, exclude corresponding file descriptor from the set,
					 * cut the job out of our list and decrement its use counter. */
					selector_remove(selector, j->err_stream);
					*job = j->err_next;
					bg_job_decref(j);
					continue;
				}

				err_msg[nread] = '\0';
				append_error_msg(j, err_msg);

			next_job:
				job = &j->err_next;
			}

			if(!need_update_list)
			{
				pthread_mutex_lock(&new_err_jobs_lock);
				need_update_list = (new_err_jobs != NULL);
				pthread_mutex_unlock(&new_err_jobs_lock);
			}
			if(need_update_list)
			{
				break;
			}
		}
	}

	selector_free(selector);
	return NULL;
}

/* Updates *jobs by removing finished tasks and adding new ones. */
static void
update_error_jobs(bg_job_t **jobs)
{
	free_drained_jobs(jobs);
	import_error_jobs(jobs);
}

/* Updates *jobs by removing finished tasks. */
static void
free_drained_jobs(bg_job_t **jobs)
{
	bg_job_t **job = jobs;
	while(*job != NULL)
	{
		bg_job_t *const j = *job;

		if(j->drained)
		{
			pthread_spin_lock(&j->status_lock);
			/* If finished, decrement use_count and drop it from the list. */
			if(!j->running)
			{
				--j->use_count;
				*job = j->err_next;
				pthread_spin_unlock(&j->status_lock);
				continue;
			}
			pthread_spin_unlock(&j->status_lock);
		}

		job = &j->err_next;
	}
}

/* Updates *jobs by adding new tasks. */
static void
import_error_jobs(bg_job_t **jobs)
{
	bg_job_t *new_jobs;

	/* Add new tasks to internal list, wait if there are no jobs. */
	pthread_mutex_lock(&new_err_jobs_lock);
	while(*jobs == NULL && new_err_jobs == NULL)
	{
		pthread_cond_wait(&new_err_jobs_cond, &new_err_jobs_lock);
	}
	new_jobs = new_err_jobs;
	new_err_jobs = NULL;
	pthread_mutex_unlock(&new_err_jobs_lock);

	/* Prepend new jobs to the list. */
	while(new_jobs != NULL)
	{
		bg_job_t *const new_job = new_jobs;
		new_jobs = new_jobs->err_next;

		assert(new_job->type == BJT_COMMAND &&
				"Only external commands should be here.");

		/* Mark a this job as an interesting one to avoid it being killed until we
		 * have a chance to read error stream. */
		new_job->drained = 0;

		new_job->err_next = *jobs;
		*jobs = new_job;
	}
}

/* Reinitializes the selector with up-to-date list of objects to watch. */
static void
make_ready_list(const bg_job_t *jobs, selector_t *selector)
{
	selector_reset(selector);

	while(jobs != NULL)
	{
		selector_add(selector, jobs->err_stream);
		jobs = jobs->err_next;
	}
}

#ifndef _WIN32

/* Rips children updating status of jobs in the process. */
static void
rip_children(void)
{
	int status;
	pid_t pid;

	/* This needs to be a loop in case of multiple blocked signals. */
	while((pid = waitpid(-1, &status, WNOHANG)) > 0)
	{
		if(WIFEXITED(status) || WIFSIGNALED(status))
		{
			rip_child(pid, status);
		}
	}
}

/* Looks up a child in job list and rips it if found. */
static void
rip_child(pid_t pid, int status)
{
	bg_job_t *job;
	for(job = bg_jobs; job != NULL; job = job->next)
	{
		if(job->pid == pid)
		{
			mark_job_finished(job, status_to_exit_code(status));
			break;
		}
	}
}

/* Either displays error message to the user for foreground operations or saves
 * it for displaying on the next invocation of bg_check(). */
static void
report_error_msg(const char title[], const char text[])
{
	bg_job_t *const job = pthread_getspecific(current_job);
	if(job == NULL)
	{
		ui_cancellation_push_off();
		show_error_msg(title, text);
		ui_cancellation_pop();
	}
	else
	{
		append_error_msg(job, text);
	}
}

#endif

/* Appends message to error-related fields of the job. */
static void
append_error_msg(bg_job_t *job, const char err_msg[])
{
	if(err_msg[0] != '\0')
	{
		pthread_spin_lock(&job->errors_lock);
		(void)strappend(&job->errors, &job->errors_len, err_msg);
		(void)strappend(&job->new_errors, &job->new_errors_len, err_msg);
		pthread_spin_unlock(&job->errors_lock);
	}
}

#ifndef _WIN32
pid_t
bg_run_and_capture(char cmd[], int user_sh, FILE *in, FILE **out, FILE **err)
{
	pid_t pid;
	int out_pipe[2];
	int error_pipe[2];

	if(pipe(out_pipe) != 0)
	{
		show_error_msg("File pipe error", "Error creating pipe");
		return (pid_t)-1;
	}

	if(pipe(error_pipe) != 0)
	{
		show_error_msg("File pipe error", "Error creating pipe");
		close(out_pipe[0]);
		close(out_pipe[1]);
		return (pid_t)-1;
	}

	if(in != NULL)
	{
		fflush(in);
	}

	if((pid = fork()) == -1)
	{
		close(out_pipe[0]);
		close(out_pipe[1]);
		close(error_pipe[0]);
		close(error_pipe[1]);
		return (pid_t)-1;
	}

	if(pid == 0)
	{
		char *sh;
		char *sh_flag;

		close(out_pipe[0]);
		close(error_pipe[0]);
		if(dup2(out_pipe[1], STDOUT_FILENO) == -1)
		{
			_Exit(EXIT_FAILURE);
		}
		if(dup2(error_pipe[1], STDERR_FILENO) == -1)
		{
			_Exit(EXIT_FAILURE);
		}

		if(in != NULL)
		{
			rewind(in);

			if(dup2(fileno(in), STDIN_FILENO) == -1)
			{
				_Exit(EXIT_FAILURE);
			}

			fclose(in);
		}

		sh = user_sh ? get_execv_path(cfg.shell) : "/bin/sh";
		sh_flag = user_sh ? cfg.shell_cmd_flag : "-c";
		prepare_for_exec();
		execvp(sh, make_execv_array(sh, sh_flag, cmd));
		_Exit(127);
	}

	close(out_pipe[1]);
	close(error_pipe[1]);
	*out = fdopen(out_pipe[0], "r");
	*err = fdopen(error_pipe[0], "r");

	return pid;
}
#else
/* Runs command in a background and redirects its stdout and stderr streams to
 * file streams which are set.  Input is redirected only if in parameter isn't
 * NULL.  Don't pass pipe for input, it can cause deadlock.  Returns (pid_t)0 or
 * (pid_t)-1 on error. */
static pid_t
background_and_capture_internal(char cmd[], int user_sh, FILE *in, FILE **out,
		FILE **err, int out_pipe[2], int err_pipe[2])
{
	const wchar_t *args[4];
	char *cwd;
	int code;
	wchar_t *final_wide_cmd;
	wchar_t *wide_sh = NULL;
	wchar_t *wide_sh_flag;
	const int use_cmd = (!user_sh || curr_stats.shell_type == ST_CMD);

	if(in != NULL)
	{
		fflush(in);
		rewind(in);

		if(_dup2(_fileno(in), _fileno(stdin)) != 0)
			return (pid_t)-1;
	}

	if(_dup2(out_pipe[1], _fileno(stdout)) != 0)
		return (pid_t)-1;
	if(_dup2(err_pipe[1], _fileno(stderr)) != 0)
		return (pid_t)-1;

	/* At least cmd.exe is incapable of handling UNC paths. */
	cwd = save_cwd();
	if(cwd != NULL && is_unc_path(cwd))
	{
		(void)chdir(get_tmpdir());
	}

	wide_sh = to_wide(use_cmd ? "cmd" : cfg.shell);
	wide_sh_flag = to_wide(user_sh ? cfg.shell_cmd_flag : "/C");

	if(use_cmd)
	{
		final_wide_cmd = to_wide(cmd);

		args[0] = wide_sh;
		args[1] = wide_sh_flag;
		args[2] = final_wide_cmd;
		args[3] = NULL;
	}
	else
	{
		/* Nobody cares that there is an array of arguments, all arguments just get
		 * concatenated anyway...  Therefore we need to take care of escaping stuff
		 * ourselves. */
		char *const modified_cmd = win_make_sh_cmd(cmd,
				user_sh ? SHELL_BY_USER : SHELL_BY_APP);
		final_wide_cmd = to_wide(modified_cmd);
		free(modified_cmd);

		args[0] = final_wide_cmd;
		args[1] = NULL;
	}

	code = _wspawnvp(P_NOWAIT, wide_sh, args);

	free(wide_sh_flag);
	free(wide_sh);
	free(final_wide_cmd);

	restore_cwd(cwd);

	if(code == 0)
	{
		return (pid_t)-1;
	}

	if((*out = _fdopen(out_pipe[0], "r")) == NULL)
		return (pid_t)-1;
	if((*err = _fdopen(err_pipe[0], "r")) == NULL)
	{
		fclose(*out);
		return (pid_t)-1;
	}

	return 0;
}

pid_t
bg_run_and_capture(char cmd[], int user_sh, FILE *in, FILE **out, FILE **err)
{
	int in_fd;
	int out_fd, out_pipe[2];
	int err_fd, err_pipe[2];
	pid_t pid;

	if(_pipe(out_pipe, 512, O_NOINHERIT) != 0)
	{
		show_error_msg("File pipe error", "Error creating pipe");
		return (pid_t)-1;
	}

	if(_pipe(err_pipe, 512, O_NOINHERIT) != 0)
	{
		show_error_msg("File pipe error", "Error creating pipe");
		close(out_pipe[0]);
		close(out_pipe[1]);
		return (pid_t)-1;
	}

	in_fd = dup(_fileno(stdin));
	out_fd = dup(_fileno(stdout));
	err_fd = dup(_fileno(stderr));

	pid = background_and_capture_internal(cmd, user_sh, in, out, err, out_pipe,
			err_pipe);

	_close(out_pipe[1]);
	_close(err_pipe[1]);

	_dup2(in_fd, _fileno(stdin));
	_dup2(out_fd, _fileno(stdout));
	_dup2(err_fd, _fileno(stderr));

	if(pid == (pid_t)-1)
	{
		_close(out_pipe[0]);
		_close(err_pipe[0]);
	}

	return pid;
}
#endif

int
bg_run_external(const char cmd[], int skip_errors, ShellRequester by,
		FILE **input)
{
	char *command = (cfg.fast_run ? fast_run_complete(cmd) : strdup(cmd));
	if(command == NULL)
	{
		return 1;
	}

	const BgJobFlags flags = (input == NULL ? BJF_NONE : BJF_SUPPLY_INPUT);
	bg_job_t *job = launch_external(command, flags, by);
	free(command);
	if(job == NULL)
	{
		if(input != NULL)
		{
			*input = NULL;
		}
		return 1;
	}

	if(input != NULL)
	{
		*input = job->input;
		job->input = NULL;
	}

	/* It's safe to do this here because bg_check() is executed on the same
	 * thread as this function. */
	job->skip_errors = skip_errors;
	return 0;
}

bg_job_t *
bg_run_external_job(const char cmd[], BgJobFlags flags)
{
	bg_job_t *job = launch_external(cmd, flags, SHELL_BY_APP);
	if(job == NULL)
	{
		return NULL;
	}

	/* It's safe to do this here because bg_check() is executed on the same
	 * thread as this function. */
	bg_job_incref(job);
	job->skip_errors = 1;

	if(flags & BJF_JOB_BAR_VISIBLE)
	{
		place_on_job_bar(job);
	}

	job->in_menu = (flags & BJF_MENU_VISIBLE);

	return job;
}

/* Starts a new external command job.  Returns the new job or NULL on error. */
static bg_job_t *
launch_external(const char cmd[], BgJobFlags flags, ShellRequester by)
{
	/* TODO: simplify this function (launch_external()) somehow, maybe split in
	 *       two. */
	const int jb_visible = (flags & BJF_JOB_BAR_VISIBLE);
	const int supply_input = (flags & BJF_SUPPLY_INPUT);
	const int capture_output = (flags & BJF_CAPTURE_OUT);
	const int merge_streams = (capture_output && (flags & BJF_MERGE_STREAMS));

#ifndef _WIN32
	const int keep_session = (flags & BJF_KEEP_SESSION);

	pid_t pid;
	int input_pipe[2] = { -1, -1 };
	int output_pipe[2] = { -1, -1 };

	/* For the sake of simplicity just use -1, calling close(-1) won't hurt. */
	int error_pipe[2] = { -1, -1 };
	if(!merge_streams && pipe(error_pipe) != 0)
	{
		show_error_msg("File pipe error", "Error creating error pipe");
		return NULL;
	}

	if(supply_input)
	{
		if(pipe(input_pipe) != 0)
		{
			show_error_msg("File pipe error", "Error creating input pipe");
			close(error_pipe[0]);
			close(error_pipe[1]);
			return NULL;
		}
	}

	if(capture_output)
	{
		if(pipe(output_pipe) != 0)
		{
			show_error_msg("File pipe error", "Error creating output pipe");
			close(input_pipe[0]);
			close(input_pipe[1]);
			close(error_pipe[0]);
			close(error_pipe[1]);
			return NULL;
		}
	}

	if((pid = fork()) == -1)
	{
		close(error_pipe[0]);
		close(error_pipe[1]);
		if(supply_input)
		{
			close(input_pipe[0]);
			close(input_pipe[1]);
		}
		if(capture_output)
		{
			close(output_pipe[0]);
			close(output_pipe[1]);
		}
		return NULL;
	}

	if(pid == 0)
	{
		extern char **environ;

		int stderr_pipe = (merge_streams ? output_pipe[1] : error_pipe[1]);

		/* Redirect stderr to write end of pipe. */
		if(dup2(stderr_pipe, STDERR_FILENO) == -1)
		{
			perror("dup2");
			_Exit(EXIT_FAILURE);
		}
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		/* Close read end of pipe. */
		if(error_pipe[0] != -1)
		{
			close(error_pipe[0]);
		}

		if(supply_input)
		{
			if(dup2(input_pipe[0], STDIN_FILENO) == -1)
			{
				perror("dup2");
				_Exit(EXIT_FAILURE);
			}
			/* Close write end of pipe. */
			close(input_pipe[1]);
		}

		if(capture_output)
		{
			if(dup2(output_pipe[1], STDOUT_FILENO) == -1)
			{
				perror("dup2");
				_Exit(EXIT_FAILURE);
			}
			/* Close read end of pipe. */
			close(output_pipe[0]);
		}

		/* Attach stdin and optionally stdout to /dev/null. */
		int nullfd = open("/dev/null", O_RDWR);
		if(nullfd != -1)
		{
			if(!supply_input && dup2(nullfd, STDIN_FILENO) == -1)
			{
				perror("dup2 for stdin");
				_Exit(EXIT_FAILURE);
			}
			if(!capture_output && dup2(nullfd, STDOUT_FILENO) == -1)
			{
				perror("dup2 for stdout");
				_Exit(EXIT_FAILURE);
			}
		}

		if(keep_session)
		{
			if(setpgid(0, 0) == -1)
			{
				perror("setpgid");
				_Exit(EXIT_FAILURE);
			}
		}
		/* setsid() creates process group as well and doesn't work if current
		 * process is a group leader, so don't do setpgid(). */
		else if(setsid() == (pid_t)-1)
		{
			perror("setsid");
			_Exit(EXIT_FAILURE);
		}

		prepare_for_exec();
		char *sh_flag = (by == SHELL_BY_USER ? cfg.shell_cmd_flag : "-c");
		execve(get_execv_path(cfg.shell),
				make_execv_array(cfg.shell, sh_flag, strdup(cmd)), environ);
		_Exit(127);
	}

	/* Close unused ends of pipes. */
	if(error_pipe[1] != -1)
	{
		close(error_pipe[1]);
	}
	if(supply_input)
	{
		close(input_pipe[0]);
	}
	if(capture_output)
	{
		close(output_pipe[1]);
	}

	bg_job_t *job = add_background_job(pid, cmd, (uintptr_t)error_pipe[0], 0,
			BJT_COMMAND, jb_visible);

	if(supply_input)
	{
		job->input = fdopen(input_pipe[1], "w");
		if(job->input == NULL)
		{
			close(input_pipe[1]);
		}
	}

	if(capture_output)
	{
		job->output = fdopen(output_pipe[0], "r");
		if(job->output == NULL)
		{
			close(output_pipe[0]);
		}
	}

	return job;
#else
	STARTUPINFOW startup = { .dwFlags = STARTF_USESTDHANDLES };
	PROCESS_INFORMATION pinfo;
	char *sh_cmd;
	wchar_t *wide_cmd;

	HANDLE hnul = CreateFileA("NUL", GENERIC_READ | GENERIC_WRITE, 0, NULL,
			OPEN_EXISTING, 0, NULL);
	if(hnul == INVALID_HANDLE_VALUE)
	{
		return NULL;
	}
	startup.hStdInput = hnul;
	startup.hStdOutput = hnul;

	HANDLE herr = INVALID_HANDLE_VALUE;
	if(!merge_streams && !CreatePipe(&herr, &startup.hStdError, NULL, 16*1024))
	{
		CloseHandle(hnul);
		return NULL;
	}

	HANDLE hin = INVALID_HANDLE_VALUE;
	if(supply_input && !CreatePipe(&startup.hStdInput, &hin, NULL, 16*1024))
	{
		CloseHandle(herr);
		CloseHandle(hnul);
		return NULL;
	}

	HANDLE hout = INVALID_HANDLE_VALUE;
	if(capture_output)
	{
		if(!CreatePipe(&hout, &startup.hStdOutput, NULL, 16*1024))
		{
			CloseHandle(herr);
			CloseHandle(hin);
			CloseHandle(hnul);
			return NULL;
		}

		if(merge_streams)
		{
			startup.hStdError = startup.hStdOutput;
		}
	}

	SetHandleInformation(startup.hStdInput, HANDLE_FLAG_INHERIT, 1);
	SetHandleInformation(startup.hStdOutput, HANDLE_FLAG_INHERIT, 1);
	SetHandleInformation(startup.hStdError, HANDLE_FLAG_INHERIT, 1);

	sh_cmd = win_make_sh_cmd(cmd, by);

	wide_cmd = to_wide(sh_cmd);
	int started = CreateProcessW(NULL, wide_cmd, NULL, NULL, 1, CREATE_SUSPENDED,
			NULL, NULL, &startup, &pinfo);
	free(wide_cmd);
	CloseHandle(hnul);
	if(startup.hStdInput != INVALID_HANDLE_VALUE)
	{
		CloseHandle(startup.hStdInput);
	}
	CloseHandle(startup.hStdOutput);
	if(startup.hStdError != startup.hStdOutput)
	{
		CloseHandle(startup.hStdError);
	}

	if(!started)
	{
		free(sh_cmd);
		CloseHandle(hout);
		CloseHandle(hin);
		return NULL;
	}

	/* Put the process into its own job object and start its main thread. */
	HANDLE hjob = CreateJobObject(NULL, NULL);
	AssignProcessToJobObject(hjob, pinfo.hProcess);
	ResumeThread(pinfo.hThread);
	CloseHandle(pinfo.hThread);

	bg_job_t *job = add_background_job(pinfo.dwProcessId, sh_cmd,
			(uintptr_t)herr, (uintptr_t)pinfo.hProcess, BJT_COMMAND, jb_visible);
	free(sh_cmd);

	if(job == NULL)
	{
		CloseHandle(herr);
		CloseHandle(hin);
		CloseHandle(hout);
		CloseHandle(pinfo.hProcess);
		CloseHandle(hjob);
		return NULL;
	}

	job->hjob = hjob;

	if(supply_input)
	{
		const int fd = _open_osfhandle((intptr_t)hin, _O_WRONLY);
		if(fd == -1)
		{
			CloseHandle(hin);
			return NULL;
		}

		job->input = fdopen(fd, "w");
		if(job->input == NULL)
		{
			close(fd);
		}
	}

	if(capture_output)
	{
		const int fd = _open_osfhandle((intptr_t)hout, _O_RDONLY);
		if(fd == -1)
		{
			CloseHandle(hout);
			return NULL;
		}

		job->output = fdopen(fd, "r");
		if(job->output == NULL)
		{
			close(fd);
		}
	}

	return job;
#endif
}

int
bg_execute(const char descr[], const char op_descr[], int total, int important,
		bg_task_func task_func, void *args)
{
	pthread_t id;
	int ret;

	background_task_args *const task_args = malloc(sizeof(*task_args));
	if(task_args == NULL)
	{
		return 1;
	}

	task_args->func = task_func;
	task_args->args = args;
	task_args->job = add_background_job(WRONG_PID, descr, (uintptr_t)NO_JOB_ID,
			(uintptr_t)NO_JOB_ID, important ? BJT_OPERATION : BJT_TASK, 1);

	if(task_args->job == NULL)
	{
		free(task_args);
		return 1;
	}

	replace_string(&task_args->job->bg_op.descr, op_descr);
	task_args->job->bg_op.total = total;

	if(task_args->job->type == BJT_OPERATION)
	{
		place_on_job_bar(task_args->job);
	}

	ret = 0;
	if(pthread_create(&id, NULL, &background_task_bootstrap, task_args) != 0)
	{
		/* Mark job as finished with error. */
		pthread_spin_lock(&task_args->job->status_lock);
		task_args->job->running = 0;
		task_args->job->exit_code = 1;
		pthread_spin_unlock(&task_args->job->status_lock);

		free(task_args);
		ret = 1;
	}

	return ret;
}

/* Makes the job appear on the job bar. */
static void
place_on_job_bar(bg_job_t *job)
{
	assert(job->with_bg_op && "This function requires bg_op data.");
	assert(!job->on_job_bar  && "This function requires should be called once.");
	ui_stat_job_bar_add(&job->bg_op);
	job->on_job_bar = 1;
}

/* Removes the job from the job bar. */
static void
get_off_job_bar(bg_job_t *job)
{
	assert(job->with_bg_op && "This function requires bg_op data.");
	assert(job->on_job_bar  && "This function requires should be called once.");
	ui_stat_job_bar_remove(&job->bg_op);
	job->on_job_bar = 0;
}

/* Creates structure that describes background job and registers it in the list
 * of jobs. */
static bg_job_t *
add_background_job(pid_t pid, const char cmd[], uintptr_t err, uintptr_t data,
		BgJobType type, int with_bg_op)
{
	bg_job_t *new = malloc(sizeof(*new));
	if(new == NULL)
	{
		show_error_msg("Memory error", "Unable to allocate enough memory");
		return NULL;
	}
	new->type = type;
	new->pid = pid;
	new->cmd = strdup(cmd);
	new->next = bg_jobs;
	new->skip_errors = 0;
	new->new_errors = NULL;
	new->new_errors_len = 0U;
	new->errors = NULL;
	new->errors_len = 0U;
	new->cancelled = 0;

	pthread_spin_init(&new->errors_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&new->status_lock, PTHREAD_PROCESS_PRIVATE);
	new->running = 1;
	new->use_count = 0;
	new->exit_code = -1;

	new->input = NULL;
	new->output = NULL;

#ifndef _WIN32
	new->err_stream = (int)err;
#else
	new->err_stream = (HANDLE)err;
	new->hprocess = (HANDLE)data;
	new->hjob = INVALID_HANDLE_VALUE;
#endif

	if(new->err_stream != NO_JOB_ID)
	{
		++new->use_count;
		pthread_mutex_lock(&new_err_jobs_lock);
		new->err_next = new_err_jobs;
		new_err_jobs = new;
		pthread_mutex_unlock(&new_err_jobs_lock);
		pthread_cond_signal(&new_err_jobs_cond);
	}

	new->with_bg_op = with_bg_op;
	new->on_job_bar = 0;
	if(new->with_bg_op)
	{
		pthread_spin_init(&new->bg_op_lock, PTHREAD_PROCESS_PRIVATE);
	}
	new->bg_op.total = 0;
	new->bg_op.done = 0;
	new->bg_op.progress = -1;
	new->bg_op.descr = NULL;
	new->bg_op.cancelled = 0;

	new->in_menu = 1;

	bg_jobs = new;
	return new;
}

/* pthreads entry point for a new background task.  Performs correct
 * startup/exit with related updates of internal data structures.  Returns
 * result for this thread. */
static void *
background_task_bootstrap(void *arg)
{
	background_task_args *const task_args = arg;

	(void)pthread_detach(pthread_self());
	block_all_thread_signals();
	set_current_job(task_args->job);

	task_args->func(&task_args->job->bg_op, task_args->args);

	/* Mark task as finished normally. */
	mark_job_finished(task_args->job, 0);

	free(task_args);

	return NULL;
}

/* Stores pointer to the job in a thread-local storage. */
static void
set_current_job(bg_job_t *job)
{
	static pthread_once_t once = PTHREAD_ONCE_INIT;
	pthread_once(&once, &make_current_job_key);

	(void)pthread_setspecific(current_job, job);
}

/* current_job initializer for pthread_once(). */
static void
make_current_job_key(void)
{
	(void)pthread_key_create(&current_job, NULL);
}

int
bg_has_active_jobs(int important_only)
{
	bg_job_t *job;
	int running = 0;
	for(job = bg_jobs; job != NULL && !running; job = job->next)
	{
		if((important_only && job->type == BJT_OPERATION) ||
				(!important_only && job->type != BJT_COMMAND))
		{
			running |= bg_job_is_running(job);
		}
	}

	return running;
}

int
bg_job_cancel(bg_job_t *job)
{
	int was_cancelled;

	if(job->type != BJT_COMMAND)
	{
		return !bg_op_cancel(&job->bg_op);
	}

	was_cancelled = job->cancelled;
#ifndef _WIN32
	if(kill(job->pid, SIGINT) == 0)
	{
		job->cancelled = 1;
	}
	else
	{
		LOG_SERROR_MSG(errno, "Failed to send SIGINT to %" PRINTF_ULL,
				(unsigned long long)job->pid);
	}
#else
	if(win_cancel_process(job->pid) == 0)
	{
		job->cancelled = 1;
	}
	else
	{
		LOG_SERROR_MSG(errno, "Failed to send WM_CLOSE to %" PRINTF_ULL,
				(unsigned long long)job->pid);
	}
#endif
	return !was_cancelled;
}

int
bg_job_cancelled(bg_job_t *job)
{
	if(job->type != BJT_COMMAND)
	{
		return bg_op_cancelled(&job->bg_op);
	}
	return job->cancelled;
}

void
bg_job_terminate(bg_job_t *job)
{
	if(job->type != BJT_COMMAND || !bg_job_is_running(job))
	{
		return;
	}

#ifndef _WIN32
	if(kill(job->pid, SIGKILL) != 0)
	{
		LOG_SERROR_MSG(errno, "Failed to send SIGKILL to %" PRINTF_ULL,
				(unsigned long long)job->pid);
	}
#else
	if(!TerminateJobObject(job->hjob, 0))
	{
		LOG_ERROR_MSG("Failed to terminate job of process %" PRINTF_ULL,
				(unsigned long long)job->pid);
		LOG_WERROR(GetLastError());
	}
#endif
}

int
bg_job_is_running(bg_job_t *job)
{
	pthread_spin_lock(&job->status_lock);
	int running = job->running;
	pthread_spin_unlock(&job->status_lock);
	return (running && update_job_status(job));
}

int
bg_job_was_killed(bg_job_t *job)
{
	pthread_spin_lock(&job->status_lock);
	int running = job->running;
	int exit_code = job->exit_code;
	pthread_spin_unlock(&job->status_lock);
	return (!running && exit_code >= 0);
}

int
bg_job_wait(bg_job_t *job)
{
	assert(job->type == BJT_COMMAND &&
			"Only external commands can be waited for.");

	if(!bg_job_is_running(job))
	{
		return 0;
	}

	/* Close input to avoid situation when the job is blocked on read. */
	if(job->input != NULL)
	{
		fclose(job->input);
		job->input = NULL;
	}

	/* Close output to avoid situation when the job is blocked on write. */
	if(job->output != NULL)
	{
		fclose(job->output);
		job->output = NULL;
	}

#ifndef _WIN32
	int status = get_proc_exit_status(job->pid);
	if(status == -1)
	{
		return 1;
	}
	mark_job_finished(job, status_to_exit_code(status));
	return 0;
#else
	if(WaitForSingleObject(job->hprocess, INFINITE) != WAIT_OBJECT_0)
	{
		return 1;
	}
	return update_job_status(job);
#endif
}

/* Retrieves exit code of a process associated with the job.  Returns zero on
 * success (job has just finished), otherwise non-zero is returned. */
static int
update_job_status(bg_job_t *job)
{
#ifndef _WIN32
	int status;
	if(job->pid != -1 && waitpid(job->pid, &status, WNOHANG) == job->pid)
	{
		mark_job_finished(job, status_to_exit_code(status));
		return 0;
	}
	return 1;
#else
	DWORD retcode;
	if(GetExitCodeProcess(job->hprocess, &retcode))
	{
		if(retcode != STILL_ACTIVE)
		{
			mark_job_finished(job, retcode);
			return 0;
		}
	}
	return 1;
#endif
}

/* Marks job as finished with the specified exit code. */
static void
mark_job_finished(bg_job_t *job, int exit_code)
{
	pthread_spin_lock(&job->status_lock);
	job->running = 0;
	job->exit_code = exit_code;
	pthread_spin_unlock(&job->status_lock);
}

void
bg_job_incref(bg_job_t *job)
{
	pthread_spin_lock(&job->status_lock);
	++job->use_count;
	pthread_spin_unlock(&job->status_lock);
}

void
bg_job_decref(bg_job_t *job)
{
	pthread_spin_lock(&job->status_lock);
	--job->use_count;
	pthread_spin_unlock(&job->status_lock);
}

void
bg_op_lock(bg_op_t *bg_op)
{
	bg_job_t *const job = STRUCT_FROM_FIELD(bg_job_t, bg_op, bg_op);
	assert(job->with_bg_op && "This function requires bg_op data.");
	pthread_spin_lock(&job->bg_op_lock);
}

void
bg_op_unlock(bg_op_t *bg_op)
{
	bg_job_t *const job = STRUCT_FROM_FIELD(bg_job_t, bg_op, bg_op);
	assert(job->with_bg_op && "This function requires bg_op data.");
	pthread_spin_unlock(&job->bg_op_lock);
}

void
bg_op_changed(bg_op_t *bg_op)
{
	ui_stat_job_bar_changed(bg_op);
}

void
bg_op_set_descr(bg_op_t *bg_op, const char descr[])
{
	bg_op_lock(bg_op);
	replace_string(&bg_op->descr, descr);
	bg_op_unlock(bg_op);

	bg_op_changed(bg_op);
}

/* Convenience method to cancel background job.  Returns previous version of the
 * cancellation flag. */
static int
bg_op_cancel(bg_op_t *bg_op)
{
	int was_cancelled;

	bg_op_lock(bg_op);
	was_cancelled = bg_op->cancelled;
	bg_op->cancelled = 1;
	bg_op_unlock(bg_op);

	bg_op_changed(bg_op);
	return was_cancelled;
}

int
bg_op_cancelled(bg_op_t *bg_op)
{
	int cancelled;

	bg_op_lock(bg_op);
	cancelled = bg_op->cancelled;
	bg_op_unlock(bg_op);

	return cancelled;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
