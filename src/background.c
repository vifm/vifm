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

#include <pthread.h>

#include <fcntl.h> /* open() */
#include <unistd.h>

#include <assert.h>
#include <errno.h> /* errno */
#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() malloc() */
#include <string.h>
#include <sys/stat.h> /* O_RDONLY */
#include <sys/types.h> /* pid_t ssize_t */
#ifndef _WIN32
#include <sys/select.h> /* FD_* select */
#include <sys/time.h> /* timeval */
#include <sys/wait.h> /* WEXITSTATUS() waitpid() */
#endif

#include "cfg/config.h"
#include "modes/dialogs/msg_dialog.h"
#include "ui/cancellation.h"
#include "utils/log.h"
#include "utils/str.h"
#include "utils/utils.h"
#include "commands_completion.h"

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
	job_t *job;        /* Job identifier that corresponds to the task. */
}
background_task_args;

static void job_check(job_t *const job);
static void job_free(job_t *const job);
#ifndef _WIN32
static job_t * add_background_job(pid_t pid, const char cmd[], int fd,
		BgJobType type);
#else
static job_t * add_background_job(pid_t pid, const char cmd[], HANDLE hprocess,
		BgJobType type);
#endif
static void * background_task_bootstrap(void *arg);
static void set_current_job(job_t *job);
static void make_current_job_key(void);
static void finish_current_job(void);

job_t *jobs;

static pthread_key_t current_job;
static pthread_once_t current_job_once = PTHREAD_ONCE_INIT;

void
init_background(void)
{
	/* Initialize state for the main thread. */
	set_current_job(NULL);
}

void
add_finished_job(pid_t pid, int status)
{
	job_t *job;

	/* Mark any finished jobs */
	job = jobs;
	while(job != NULL)
	{
		if(job->pid == pid)
		{
			job->running = 0;
		}
		job = job->next;
	}
}

void
check_background_jobs(void)
{
	job_t *head = jobs;
	job_t *prev;
	job_t *p;

	/* Quit if there is no jobs or list is unavailable (e.g. used by another
	 * invocation of this function). */
	if(head == NULL)
	{
		return;
	}

	if(bg_jobs_freeze() != 0)
	{
		return;
	}

	head = jobs;
	jobs = NULL;

	p = head;
	prev = NULL;
	while(p != NULL)
	{
		job_check(p);

		/* Remove job if it is finished now. */
		if(!p->running)
		{
			job_t *j = p;
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

	assert(jobs == NULL && "Job list shouldn't be used by anyone.");
	jobs = head;

	bg_jobs_unfreeze();
}

/* Checks status of the job.  Processes error stream or checks whether process
 * is still running. */
static void
job_check(job_t *const job)
{
#ifndef _WIN32
	fd_set ready;
	int max_fd = 0;
	struct timeval ts = { .tv_sec = 0, .tv_usec = 1000 };

	/* Setup pipe for reading */
	FD_ZERO(&ready);
	if(job->fd >= 0)
	{
		FD_SET(job->fd, &ready);
		max_fd = job->fd;
	}

	if(job->error != NULL)
	{
		if(!job->skip_errors)
		{
			job->skip_errors = prompt_error_msg("Background Process Error",
					job->error);
		}
		free(job->error);
		job->error = NULL;
	}

	while(select(max_fd + 1, &ready, NULL, NULL, &ts) > 0)
	{
		char err_msg[ERR_MSG_LEN];

		const ssize_t nread = read(job->fd, err_msg, sizeof(err_msg) - 1);
		if(nread == 0)
		{
			break;
		}
		else if(nread > 0 && !job->skip_errors)
		{
			err_msg[nread] = '\0';
			job->skip_errors = prompt_error_msg("Background Process Error", err_msg);
		}
	}
#else
	DWORD retcode;
	if(GetExitCodeProcess(job->hprocess, &retcode) != 0)
	{
		if(retcode != STILL_ACTIVE)
		{
			job->running = 0;
		}
	}
#endif
}

/* Frees resources allocated by the job as well as the job_t structure itself.
 * The job can be NULL. */
static void
job_free(job_t *const job)
{
	if(job == NULL)
	{
		return;
	}

#ifndef _WIN32
	if(job->fd != NO_JOB_ID)
	{
		close(job->fd);
	}
#else
	if(job->hprocess != NO_JOB_ID)
	{
		CloseHandle(job->hprocess);
	}
#endif
	free(job->cmd);
	free(job);
}

/* Used for FUSE mounting and unmounting only. */
int
background_and_wait_for_status(char cmd[], int cancellable, int *cancelled)
{
#ifndef _WIN32
	pid_t pid;
	int status;

	if(cancellable)
	{
		*cancelled = 0;
	}

	if(cmd == NULL)
	{
		return 1;
	}

	(void)set_sigchld(1);

	pid = fork();
	if(pid == (pid_t)-1)
	{
		(void)set_sigchld(0);
		LOG_SERROR_MSG(errno, "Forking has failed.");
		return -1;
	}

	if(pid == (pid_t)0)
	{
		extern char **environ;

		char *args[4];

		(void)set_sigchld(0);

		args[0] = cfg.shell;
		args[1] = "-c";
		args[2] = cmd;
		args[3] = NULL;
		(void)execve(cfg.shell, args, environ);
		exit(127);
	}

	if(cancellable)
	{
		ui_cancellation_enable();
	}

	while(waitpid(pid, &status, 0) == -1)
	{
		if(errno != EINTR)
		{
			LOG_SERROR_MSG(errno, "Failed waiting for process: "PRINTF_PID_T, pid);
			status = -1;
			break;
		}
		process_cancel_request(pid);
	}

	if(cancellable)
	{
		if(ui_cancellation_requested())
		{
			*cancelled = 1;
		}
		ui_cancellation_disable();
	}

	(void)set_sigchld(0);

	return status;

#else
	return -1;
#endif
}

#ifndef _WIN32
static void
error_msg(const char *title, const char *text)
{
	job_t *job = pthread_getspecific(current_job);
	if(job == NULL)
	{
		show_error_msg(title, text);
	}
	else
	{
		(void)replace_string(&job->error, text);
	}
}
#endif

int
background_and_wait_for_errors(char cmd[], int cancellable)
{
#ifndef _WIN32
	pid_t pid;
	int error_pipe[2];
	int result = 0;

	if(pipe(error_pipe) != 0)
	{
		error_msg("File pipe error", "Error creating pipe");
		return -1;
	}

	(void)set_sigchld(1);

	if((pid = fork()) == -1)
	{
		(void)set_sigchld(0);
		return -1;
	}

	if(pid == 0)
	{
		(void)set_sigchld(0);
		run_from_fork(error_pipe, 1, cmd);
	}
	else
	{
		char buf[80*10];
		char linebuf[80];
		int nread = 0;

		close(error_pipe[1]); /* Close write end of pipe. */

		if(cancellable)
		{
			ui_cancellation_enable();
		}

		wait_for_data_from(pid, NULL, error_pipe[0]);

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

			wait_for_data_from(pid, NULL, error_pipe[0]);
		}
		close(error_pipe[0]);

		if(cancellable)
		{
			ui_cancellation_disable();
		}

		if(result != 0)
		{
			error_msg("Background Process Error", buf);
		}
		else
		{
			/* Don't use "const int" variables with WEXITSTATUS() as they cause
			 * compilation errors in case __USE_BSD is defined.  Anonymous type with
			 * "const int" is composed via compound literal expression. */
			int status = get_proc_exit_status(pid);
			result = WEXITSTATUS(status);
		}
	}

	(void)set_sigchld(0);

	return result;
#else
	return -1;
#endif
}

#ifndef _WIN32
pid_t
background_and_capture(char *cmd, FILE **out, FILE **err)
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
		char *args[4];

		close(out_pipe[0]);
		close(error_pipe[0]);
		if(dup2(out_pipe[1], STDOUT_FILENO) == -1)
			exit(-1);
		if(dup2(error_pipe[1], STDERR_FILENO) == -1)
			exit(-1);

		args[0] = "/bin/sh";
		args[1] = "-c";
		args[2] = cmd;
		args[3] = NULL;

		execvp(args[0], args);
		exit(-1);
	}

	close(out_pipe[1]);
	close(error_pipe[1]);
	*out = fdopen(out_pipe[0], "r");
	*err = fdopen(error_pipe[0], "r");

	return pid;
}
#else
/* Runs command in a background and redirects its stdout and stderr streams to
 * file streams which are set.  Returns (pid_t)0 or (pid_t)-1 on error. */
static pid_t
background_and_capture_internal(char *cmd, FILE **out, FILE **err,
		int out_pipe[2], int err_pipe[2])
{
	char *args[4];

	if(_dup2(out_pipe[1], _fileno(stdout)) != 0)
		return (pid_t)-1;
	if(_dup2(err_pipe[1], _fileno(stderr)) != 0)
		return (pid_t)-1;

	args[0] = "cmd";
	args[1] = "/C";
	args[2] = cmd;
	args[3] = NULL;

	if(_spawnvp(P_NOWAIT, args[0], (const char **)args) == 0)
		return (pid_t)-1;

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
background_and_capture(char *cmd, FILE **out, FILE **err)
{
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

	out_fd = dup(_fileno(stdout));
	err_fd = dup(_fileno(stderr));

	pid = background_and_capture_internal(cmd, out, err, out_pipe, err_pipe);

	_close(out_pipe[1]);
	_close(err_pipe[1]);

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
start_background_job(const char *cmd, int skip_errors)
{
	job_t *job = NULL;
#ifndef _WIN32
	pid_t pid;
	char *args[4];
	int error_pipe[2];
	char *command;

	command = cfg.fast_run ? fast_run_complete(cmd) : strdup(cmd);
	if(command == NULL)
	{
		return -1;
	}

	if(pipe(error_pipe) != 0)
	{
		show_error_msg("File pipe error", "Error creating pipe");
		free(command);
		return -1;
	}

	if((pid = fork()) == -1)
	{
		free(command);
		return -1;
	}

	if(pid == 0)
	{
		extern char **environ;

		int nullfd;
		/* Redirect stderr to write end of pipe. */
		if(dup2(error_pipe[1], STDERR_FILENO) == -1)
		{
			perror("dup");
			exit(-1);
		}
		close(error_pipe[0]); /* Close read end of pipe. */
		close(0); /* Close stdin */
		close(1); /* Close stdout */

		/* Send stdout, stdin to /dev/null */
		if((nullfd = open("/dev/null", O_RDONLY)) != -1)
		{
			dup2(nullfd, 0);
			dup2(nullfd, 1);
		}

		args[0] = cfg.shell;
		args[1] = "-c";
		args[2] = command;
		args[3] = NULL;

		setpgid(0, 0);

		execve(cfg.shell, args, environ);
		exit(-1);
	}
	else
	{
		close(error_pipe[1]); /* Close write end of pipe. */

		job = add_background_job(pid, command, error_pipe[0], BJT_COMMAND);
		if(job == NULL)
		{
			free(command);
			return -1;
		}
	}
	free(command);
#else
	BOOL ret;
	STARTUPINFO startup = {};
	PROCESS_INFORMATION pinfo;
	char *command;

	command = cfg.fast_run ? fast_run_complete(cmd) : strdup(cmd);
	if(command == NULL)
	{
		return -1;
	}

	ret = CreateProcess(NULL, command, NULL, NULL, 0, 0, NULL, NULL, &startup,
			&pinfo);
	if(ret != 0)
	{
		CloseHandle(pinfo.hThread);

		job = add_background_job(pinfo.dwProcessId, command, pinfo.hProcess,
				BJT_COMMAND);
		if(job == NULL)
		{
			free(command);
			return -1;
		}
	}
	free(command);
	if(ret == 0)
	{
		return 1;
	}
#endif

	if(job != NULL)
	{
		job->skip_errors = skip_errors;
	}
	return 0;
}

void
inner_bg_next(void)
{
	job_t *job = pthread_getspecific(current_job);
	if(job != NULL)
	{
		++job->done;
		assert(job->done <= job->total);
	}
}

int
bg_execute(const char desc[], int total, int important, bg_task_func task_func,
		void *args)
{
	pthread_t id;

	background_task_args *const task_args = malloc(sizeof(*task_args));
	if(task_args == NULL)
	{
		return 1;
	}

	task_args->func = task_func;
	task_args->args = args;
	task_args->job = add_background_job(WRONG_PID, desc, NO_JOB_ID,
			important ? BJT_OPERATION : BJT_TASK);

	if(task_args->job == NULL)
	{
		free(task_args);
		return 2;
	}

	task_args->job->total = total;

	if(pthread_create(&id, NULL, background_task_bootstrap, task_args) != 0)
	{
		free(task_args);
		return 3;
	}

	return 0;
}

/* Creates structure that describes background job and registers it in the list
 * of jobs. */
#ifndef _WIN32
static job_t *
add_background_job(pid_t pid, const char cmd[], int fd, BgJobType type)
#else
static job_t *
add_background_job(pid_t pid, const char cmd[], HANDLE hprocess, BgJobType type)
#endif
{
	job_t *new;

	if((new = malloc(sizeof(job_t))) == 0)
	{
		show_error_msg("Memory error", "Unable to allocate enough memory");
		return NULL;
	}
	new->type = type;
	new->pid = pid;
	new->cmd = strdup(cmd);
	new->next = jobs;
#ifndef _WIN32
	new->fd = fd;
#else
	new->hprocess = hprocess;
#endif
	new->skip_errors = 0;
	new->running = 1;
	new->error = NULL;

	new->total = 0;
	new->done = 0;

	jobs = new;
	return new;
}

/* Pthreads entry point for a new background task.  Performs correct
 * startup/exit with related updates of internal data structures.  Returns
 * result for this thread. */
static void *
background_task_bootstrap(void *arg)
{
	background_task_args *const task_args = arg;

	set_current_job(task_args->job);

	task_args->func(task_args->args);

	finish_current_job();

	free(task_args);

	return NULL;
}

/* Stores pointer to the job in a thread-local storage. */
static void
set_current_job(job_t *job)
{
	pthread_once(&current_job_once, &make_current_job_key);
	(void)pthread_setspecific(current_job, job);
}

/* current_job initializer for pthread_once(). */
static void
make_current_job_key(void)
{
	(void)pthread_key_create(&current_job, NULL);
}

/* Marks current job stored in a thread-local storage as finished
 * successfully. */
static void
finish_current_job(void)
{
	job_t *const job = pthread_getspecific(current_job);
	if(job != NULL)
	{
		job->running = 0;
		job->exit_code = 0;
	}
}

int
bg_has_active_jobs(void)
{
	const job_t *job;
	int bg_op_count;

	if(bg_jobs_freeze() != 0)
	{
		/* Failed to lock jobs list and using safe choice: pretend there are active
		 * tasks. */
		return 1;
	}

	bg_op_count = 0;
	for(job = jobs; job != NULL; job = job->next)
	{
		if(job->running && job->type == BJT_OPERATION)
		{
			++bg_op_count;
		}
	}

	bg_jobs_unfreeze();

	return bg_op_count > 0;
}

int
bg_jobs_freeze(void)
{
	/* SIGCHLD needs to be blocked anytime the jobs list is accessed from anywhere
	 * except the received_sigchld(). */
	return set_sigchld(1);
}

void
bg_jobs_unfreeze(void)
{
	/* Unblock SIGCHLD signal. */
	/* FIXME: maybe store previous state of SIGCHLD and don't unblock if it was
	 *        blocked. */
	(void)set_sigchld(0);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
