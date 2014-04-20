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

#include <fcntl.h>
#include <unistd.h>

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stddef.h> /* NULL size_t ssize_t */
#include <stdlib.h> /* free() malloc() */
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/wait.h> /* WEXITSTATUS() */
#endif

#include "cfg/config.h"
#include "menus/menus.h"
#include "utils/str.h"
#include "utils/utils.h"
#include "commands_completion.h"
#include "status.h"

/* Size of error message reading buffer. */
#define ERR_MSG_LEN 1025

static void job_check(job_t *const job);
static void job_free(job_t *const job);

job_t *jobs;

static pthread_key_t key;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;

void
init_background(void)
{
	/* Initialize state for the main thread. */
	add_inner_bg_job(NULL);
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
	job_t *p = jobs;
	job_t *prev = NULL;

	if(p == NULL)
	{
		return;
	}

	/* SIGCHLD needs to be blocked. */
	if(set_sigchld(1) != 0)
	{
		return;
	}

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
				jobs = p->next;

			p = p->next;
			job_free(j);
		}
		else
		{
			prev = p;
			p = p->next;
		}
	}

	/* Unblock SIGCHLD signal. */
	/* FIXME: maybe store previous state of SIGCHLD and don't unblock if it was
	 *        blocked. */
	(void)set_sigchld(0);
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
	if(job->fd != -1)
	{
		close(job->fd);
	}
#else
	if(job->hprocess != INVALID_HANDLE_VALUE)
	{
		CloseHandle(job->hprocess);
	}
#endif
	free(job->cmd);
	free(job);
}

/* Used for FUSE mounting and unmounting only */
int
background_and_wait_for_status(char *cmd)
{
#ifndef _WIN32
	int pid;
	int status;
	extern char **environ;

	if(cmd == 0)
		return 1;

	pid = fork();
	if(pid == -1)
		return -1;
	if(pid == 0)
	{
		char *args[4];

		args[0] = cfg.shell;
		args[1] = "-c";
		args[2] = cmd;
		args[3] = NULL;
		(void)execve(cfg.shell, args, environ);
		exit(127);
	}
	do
	{
		if(waitpid(pid, &status, 0) == -1)
		{
			if(errno != EINTR)
				return -1;
		}
		else
			return status;
	}while(1);
#else
	return -1;
#endif
}

#ifndef _WIN32
static void
error_msg(const char *title, const char *text)
{
	job_t *job = pthread_getspecific(key);
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
			const int status = get_proc_exit_status(pid);
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

		job = add_background_job(pid, command, error_pipe[0]);
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

		job = add_background_job(pinfo.dwProcessId, command, pinfo.hProcess);
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

#ifndef _WIN32
job_t *
add_background_job(pid_t pid, const char *cmd, int fd)
#else
job_t *
add_background_job(pid_t pid, const char *cmd, HANDLE hprocess)
#endif
{
	job_t *new;

	if((new = malloc(sizeof(job_t))) == 0)
	{
		show_error_msg("Memory error", "Unable to allocate enough memory");
		return NULL;
	}
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
	jobs = new;
	return new;
}

static void
make_key(void)
{
	(void)pthread_key_create(&key, NULL);
}

void
add_inner_bg_job(job_t *job)
{
	pthread_once(&key_once, &make_key);
	(void)pthread_setspecific(key, job);
}

void
inner_bg_next(void)
{
	job_t *job = pthread_getspecific(key);
	if(job == NULL)
		return;

	job->done++;
	assert(job->done <= job->total);
}

void
remove_inner_bg_job(void)
{
	job_t *job = pthread_getspecific(key);
	if(job == NULL)
		return;

	job->running = 0;
	job->exit_code = 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
