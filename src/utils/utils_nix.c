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

#include "utils_nix.h"
#include "utils_int.h"

#include <sys/stat.h> /* S_* */
#include <sys/types.h> /* gid_t mode_t uid_t */
#include <ctype.h> /* isdigit() */
#include <errno.h> /* EINTR errno */
#include <fcntl.h> /* O_RDONLY open() close() */
#include <grp.h> /* getgrnam() */
#include <pwd.h> /* getpwnam() */
#include <signal.h> /* signal() SIGINT SIGTSTP SIGCHLD SIG_DFL sigset_t
                       sigemptyset() sigaddset() sigprocmask() SIG_BLOCK
                       SIG_UNBLOCK */
#include <stddef.h> /* NULL */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* atoi() */
#include <string.h> /* strchr() strlen() strncmp() */
#include <unistd.h> /* dup2() getpid() */

#include "../cfg/config.h"
#include "fs_limits.h"
#include "log.h"
#include "mntent.h" /* mntent setmntent() getmntent() endmntent() */
#include "path.h"

static int begins_with_list_item(const char pattern[], const char list[]);

void
pause_shell(void)
{
	run_in_shell_no_cls(PAUSE_CMD);
}

int
run_in_shell_no_cls(char command[])
{
	typedef void (*sig_handler)(int);

	int pid;
	int result;
	extern char **environ;
	sig_handler sigtstp_handler;
	sigset_t sigchld_mask;

	if(command == NULL)
		return 1;

	sigtstp_handler = signal(SIGTSTP, SIG_DFL);

	/* We need to block SIGCHLD signal.  One can't just set it to SIG_DFL, because
	 * it will possibly cause missing of SIGCHLD from a background process
	 * (job). */
	sigemptyset(&sigchld_mask);
	sigaddset(&sigchld_mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &sigchld_mask, NULL);

	pid = fork();
	if(pid == -1)
	{
		signal(SIGTSTP, sigtstp_handler);
		sigprocmask(SIG_UNBLOCK, &sigchld_mask, NULL);
		return -1;
	}
	if(pid == 0)
	{
		char *args[4];

		signal(SIGTSTP, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		sigprocmask(SIG_UNBLOCK, &sigchld_mask, NULL);

		args[0] = cfg.shell;
		args[1] = "-c";
		args[2] = command;
		args[3] = NULL;
		execve(cfg.shell, args, environ);
		exit(127);
	}
	do
	{
		int status;
		if(waitpid(pid, &status, 0) == -1)
		{
			if(errno != EINTR)
			{
				LOG_SERROR_MSG(errno, "waitpid()");
				result = -1;
				break;
			}
		}
		else
		{
			result = status;
			break;
		}
	}while(1);
	signal(SIGTSTP, sigtstp_handler);
	sigprocmask(SIG_UNBLOCK, &sigchld_mask, NULL);
	return result;
}

void
recover_after_shellout(void)
{
	/* Do nothing.  No need to recover anything on this platform. */
}

/* if err == 1 then use stderr and close stdin and stdout */
void _gnuc_noreturn
run_from_fork(int pipe[2], int err, char *cmd)
{
	char *args[4];
	int nullfd;

	/* Redirect stderr or stdout to write end of pipe. */
	if(dup2(pipe[1], err ? STDERR_FILENO : STDOUT_FILENO) == -1)
	{
		exit(1);
	}
	close(pipe[0]);        /* Close read end of pipe. */
	close(STDIN_FILENO);
	close(err ? STDOUT_FILENO : STDERR_FILENO);

	/* Send stdout, stdin to /dev/null */
	if((nullfd = open("/dev/null", O_RDONLY)) != -1)
	{
		if(dup2(nullfd, STDIN_FILENO) == -1)
			exit(1);
		if(dup2(nullfd, err ? STDOUT_FILENO : STDERR_FILENO) == -1)
			exit(1);
	}

	args[0] = cfg.shell;
	args[1] = "-c";
	args[2] = cmd;
	args[3] = NULL;

	execvp(args[0], args);
	exit(1);
}

void
get_perm_string(char buf[], int len, mode_t mode)
{
	static const char *const perm_sets[] =
	{ "---", "--x", "-w-", "-wx", "r--", "r-x", "rw-", "rwx" };
	int u, g, o;

	u = (mode & S_IRWXU) >> 6;
	g = (mode & S_IRWXG) >> 3;
	o = (mode & S_IRWXO);

	snprintf(buf, len, "-%s%s%s", perm_sets[u], perm_sets[g], perm_sets[o]);

	if(S_ISLNK(mode))
		buf[0] = 'l';
	else if(S_ISDIR(mode))
		buf[0] = 'd';
	else if(S_ISBLK(mode))
		buf[0] = 'b';
	else if(S_ISCHR(mode))
		buf[0] = 'c';
	else if(S_ISFIFO(mode))
		buf[0] = 'p';
	else if(S_ISSOCK(mode))
		buf[0] = 's';

	if(mode & S_ISVTX)
		buf[9] = (buf[9] == '-') ? 'T' : 't';
	if(mode & S_ISGID)
		buf[6] = (buf[6] == '-') ? 'S' : 's';
	if(mode & S_ISUID)
		buf[3] = (buf[3] == '-') ? 'S' : 's';
}

int
is_on_slow_fs(const char full_path[])
{
	FILE *f;
	struct mntent *ent;
	size_t len = 0;
	char max[PATH_MAX] = "";

	if(cfg.slow_fs_list[0] == '\0')
	{
		return 0;
	}

	if((f = setmntent("/etc/mtab", "r")) == NULL)
	{
		return 0;
	}

	while((ent = getmntent(f)) != NULL)
	{
		if(path_starts_with(full_path, ent->mnt_dir))
		{
			const size_t new_len = strlen(ent->mnt_dir);
			if(new_len > len)
			{
				len = new_len;
				snprintf(max, sizeof(max), "%s", ent->mnt_fsname);
			}
		}
	}

	endmntent(f);
	return (max[0] == '\0') ? 0 : begins_with_list_item(max, cfg.slow_fs_list);
}

static int
begins_with_list_item(const char pattern[], const char list[])
{
	const char *p = list - 1;

	do
	{
		char buf[128];
		const char *t;
		size_t len;

		t = p + 1;
		p = strchr(t, ',');
		if(p == NULL)
			p = t + strlen(t);

		len = snprintf(buf, MIN(p - t + 1, sizeof(buf)), "%s", t);
		if(len != 0 && strncmp(pattern, buf, len) == 0)
			return 1;
	}
	while(*p != '\0');
	return 0;
}

unsigned int
get_pid(void)
{
	return getpid();
}

int
get_uid(const char user[], uid_t *uid)
{
	if(isdigit(user[0]))
	{
		*uid = atoi(user);
	}
	else
	{
		struct passwd *p;

		p = getpwnam(user);
		if(p == NULL)
			return 1;

		*uid = p->pw_uid;
	}
	return 0;
}

int
get_gid(const char group[], gid_t *gid)
{
	if(isdigit(group[0]))
	{
		*gid = atoi(group);
	}
	else
	{
		struct group *g;

		g = getgrnam(group);
		if(g == NULL)
			return 1;

		*gid = g->gr_gid;
	}
	return 0;
}

int
S_ISEXE(mode_t mode)
{
	return ((S_IXUSR | S_IXGRP | S_IXOTH) & mode);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
