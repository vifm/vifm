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

#ifdef HAVE_MAX_ARG_STRLEN
/* MAX_ARG_STRLEN is the only reason we need these headers. */
#include <linux/binfmts.h>
#include <sys/user.h>
#endif

#include <sys/select.h> /* select() FD_SET FD_ZERO */
#include <sys/stat.h> /* O_* S_* */
#include <sys/time.h> /* timeval futimens() utimes() */
#include <sys/types.h> /* gid_t mode_t pid_t uid_t */
#include <sys/wait.h> /* waitpid */
#include <fcntl.h> /* open() close() */
#include <grp.h> /* getgrnam() getgrgid_r() */
#include <pwd.h> /* getpwnam() getpwuid_r() */
#include <unistd.h> /* X_OK dup() dup2() getpid() isatty() pause() sysconf()
                       ttyname() */

#include <assert.h> /* assert() */
#include <ctype.h> /* isdigit() */
#include <errno.h> /* EINTR errno */
#include <signal.h> /* SIGINT SIGTSTP SIGCHLD SIG_DFL SIG_BLOCK SIG_UNBLOCK
                       sigset_t kill() sigaddset() sigemptyset() signal()
                       sigprocmask() */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* FILE stderr fdopen() fprintf() snprintf() */
#include <stdlib.h> /* atoi() free() */
#include <string.h> /* strchr() strdup() strerror() strlen() strncmp() */

#include "../cfg/config.h"
#include "../compat/fs_limits.h"
#include "../compat/mntent.h" /* mntent setmntent() getmntent() endmntent() */
#include "../compat/os.h"
#include "../compat/reallocarray.h"
#include "../ui/cancellation.h"
#include "../running.h"
#include "../status.h"
#include "env.h"
#include "filemon.h"
#include "fs.h"
#include "fswatch.h"
#include "log.h"
#include "macros.h"
#include "path.h"
#include "str.h"
#include "utils.h"

/* Types of mount point information for get_mount_point_traverser_state. */
typedef enum
{
	MI_MOUNT_POINT, /* Path to the mount point. */
	MI_FS_TYPE,     /* File system name. */
}
mntinfo;

/* State for get_mount_point() traverser. */
typedef struct
{
	mntinfo type;     /* Type of information to put into the buffer. */
	const char *path; /* Path whose mount point we're looking for. */
	size_t buf_len;   /* Output buffer length. */
	char *buf;        /* Output buffer. */
	size_t curr_len;  /* Max length among all found points (len of buf string). */
}
get_mount_point_traverser_state;

static int get_mount_info_traverser(struct mntent *entry, void *arg);
static void free_mnt_entries(struct mntent *entries, unsigned int nentries);
struct mntent * read_mnt_entries(unsigned int *nentries);
static int clone_mnt_entry(struct mntent *lhs, const struct mntent *rhs);
static void free_mnt_entry(struct mntent *entry);
static int starts_with_list_item(const char str[], const char list[]);
static int find_path_prefix_index(const char path[], const char list[]);
static int open_tty(void);

void
pause_shell(void)
{
	run_in_shell_no_cls(PAUSE_CMD);
}

int
run_in_shell_no_cls(char command[])
{
	int pid;
	int result;
	extern char **environ;
	struct sigaction new, old;

	if(command == NULL)
		return 1;

	new.sa_handler = SIG_DFL;
	sigemptyset(&new.sa_mask);
	new.sa_flags = SA_RESTART;
	sigaction(SIGTSTP, &new, &old);

	/* We need to block SIGCHLD signal.  One can't just set it to SIG_DFL, because
	 * it will possibly cause missing of SIGCHLD from a background process
	 * (job). */
	(void)set_sigchld(1);

	pid = fork();
	if(pid == -1)
	{
		sigaction(SIGTSTP, &old, NULL);
		(void)set_sigchld(0);
		return -1;
	}
	if(pid == 0)
	{
		signal(SIGTSTP, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		(void)set_sigchld(0);

		execve(get_execv_path(cfg.shell), make_execv_array(cfg.shell, command),
				environ);
		_Exit(127);
	}

	result = get_proc_exit_status(pid);

	sigaction(SIGTSTP, &old, NULL);
	(void)set_sigchld(0);

	return result;
}

void
recover_after_shellout(void)
{
	/* Do nothing.  No need to recover anything on this platform. */
}

void
wait_for_data_from(pid_t pid, FILE *f, int fd)
{
	const struct timeval ts_init = { .tv_sec = 0, .tv_usec = 1000 };
	struct timeval ts;
	int select_result;

	fd_set read_ready;
	FD_ZERO(&read_ready);

	fd = (f != NULL) ? fileno(f) : fd;

	do
	{
		process_cancel_request(pid);
		ts = ts_init;
		FD_SET(fd, &read_ready);
		select_result = select(fd + 1, &read_ready, NULL, NULL, &ts);
	}
	while(select_result == 0 || (select_result == -1 && errno == EINTR));
}

int
set_sigchld(int block)
{
	const int action = block ? SIG_BLOCK : SIG_UNBLOCK;
	sigset_t sigchld_mask;

	return sigemptyset(&sigchld_mask) == -1
	    || sigaddset(&sigchld_mask, SIGCHLD) == -1
	    || sigprocmask(action, &sigchld_mask, NULL) == -1;
}

void
process_cancel_request(pid_t pid)
{
	if(ui_cancellation_requested())
	{
		if(kill(pid, SIGINT) != 0)
		{
			LOG_SERROR_MSG(errno, "Failed to send SIGINT to %" PRINTF_ULL,
					(unsigned long long)pid);
		}
	}
}

int
get_proc_exit_status(pid_t pid)
{
	do
	{
		int status;
		if(waitpid(pid, &status, 0) == -1)
		{
			if(errno != EINTR)
			{
				LOG_SERROR_MSG(errno, "waitpid()");
				return -1;
			}
		}
		else
		{
			return status;
		}
	}
	while(1);
}

void _gnuc_noreturn
run_from_fork(int pipe[2], int err_only, char cmd[])
{
	int nullfd;

	/* Redirect stderr and maybe stdout to write end of the pipe. */
	if(dup2(pipe[1], STDERR_FILENO) == -1)
	{
		_Exit(EXIT_FAILURE);
	}
	if(err_only)
	{
		close(STDOUT_FILENO);
	}
	else
	{
		if(dup2(pipe[1], STDOUT_FILENO) == -1)
		{
			_Exit(EXIT_FAILURE);
		}
	}

	/* Close read end of pipe. */
	close(pipe[0]);

	close(STDIN_FILENO);

	/* Send stdout, stdin to /dev/null */
	if((nullfd = open("/dev/null", O_RDONLY)) != -1)
	{
		if(dup2(nullfd, STDIN_FILENO) == -1)
		{
			_Exit(EXIT_FAILURE);
		}
		if(err_only && dup2(nullfd, STDOUT_FILENO) == -1)
		{
			_Exit(EXIT_FAILURE);
		}
	}

	execvp(get_execv_path(cfg.shell), make_execv_array(cfg.shell, cmd));
	_Exit(127);
}

char *
get_execv_path(char shell[])
{
	static char name[NAME_MAX];
	(void)extract_cmd_name(shell, 0, sizeof(name), name);
	return name;
}

char **
make_execv_array(char shell[], char cmd[])
{
#ifdef HAVE_MAX_ARG_STRLEN
#ifndef PAGE_SIZE
#define PAGE_SIZE sysconf(_SC_PAGESIZE)
#endif
	/* Don't use maximum length, leave some room or commands fail to run. */
	const size_t safe_arg_len = MIN(MAX_ARG_STRLEN, MAX_ARG_STRLEN - 4096U);
	const size_t npieces = DIV_ROUND_UP(strlen(cmd), safe_arg_len);
#else
	/* Actual value doesn't matter in this case. */
	const size_t safe_arg_len = 1;
	/* Don't break anything if Linux-specific MAX_ARG_STRLEN macro isn't
	 * defined. */
	const size_t npieces = 1U;
#endif

	char name[NAME_MAX];

	char **args = reallocarray(NULL, 4 + npieces + 1, sizeof(*args));
	char *eval_cmd;
	size_t len;
	size_t i;

	char *const sh_arg = extract_cmd_name(shell, 0, sizeof(name), name);
	const int with_sh_arg = (*sh_arg != '\0');

	/* Don't use eval hack unless necessary. */
	if(npieces == 1)
	{
		i = 0U;
		args[i++] = shell;
		if(with_sh_arg)
		{
			args[i++] = sh_arg;
		}
		args[i++] = "-c";
		args[i++] = cmd;
		args[i++] = NULL;
		return args;
	}

	/* Break very long commands into pieces and run like this:
	 * sh -c 'eval "${1}${2}...${10}...${N}"' sh quoted-substring1 ...
	 * see https://lists.gnu.org/archive/html/bug-make/2009-07/msg00012.html */
	eval_cmd = NULL;
	len = 0U;
	(void)strappend(&eval_cmd, &len, "eval \"");
	for(i = 0; i < npieces; ++i)
	{
		char s[32];
		char c;

		snprintf(s, sizeof(s), "${%d}", (int)i);
		(void)strappend(&eval_cmd, &len, s);

		c = cmd[safe_arg_len];
		cmd[safe_arg_len] = '\0';

		args[(with_sh_arg ? 4 : 3) + i] = strdup(cmd);

		cmd[safe_arg_len] = c;
		cmd += safe_arg_len;
	}
	(void)strappend(&eval_cmd, &len, "\"");

	args[i++] = shell;
	if(with_sh_arg)
	{
		args[i++] = sh_arg;
	}
	args[i++] = "-c";
	args[i++] = eval_cmd;
	args[i++ + npieces] = NULL;

	return args;
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
refers_to_slower_fs(const char from[], const char to[])
{
	const int i = find_path_prefix_index(to, cfg.slow_fs_list);
	/* When destination is not on slow file system, no performance penalties are
	 * expected. */
	if(i == -1)
	{
		return 0;
	}

	/* Otherwise, same slowdown as we have for the source location is bearable. */
	return find_path_prefix_index(from, cfg.slow_fs_list) != i;
}

int
is_on_slow_fs(const char full_path[])
{
	char fs_name[PATH_MAX];
	get_mount_point_traverser_state state = {
		.type = MI_FS_TYPE,
		.path = full_path,
		.buf_len = sizeof(fs_name),
		.buf = fs_name,
		.curr_len = 0UL,
	};

	/* Empty list optimization. */
	if(cfg.slow_fs_list[0] == '\0')
	{
		return 0;
	}

	/* If slowfs equals "*" then all file systems are considered slow.  On cygwin
	 * obtaining list of mounts from /etc/mtab, which is linked to /proc/mounts,
	 * is very slow in presence of network drives. */
	if(strcmp(cfg.slow_fs_list, "*") == 0)
	{
		return 1;
	}

	if(traverse_mount_points(&get_mount_info_traverser, &state) == 0)
	{
		if(state.curr_len > 0)
		{
			if(starts_with_list_item(fs_name, cfg.slow_fs_list))
			{
				return 1;
			}
		}
	}

	return find_path_prefix_index(full_path, cfg.slow_fs_list) != -1;
}

int
get_mount_point(const char path[], size_t buf_len, char buf[])
{
	get_mount_point_traverser_state state = {
		.type = MI_MOUNT_POINT,
		.path = path,
		.buf_len = buf_len,
		.buf = buf,
		.curr_len = 0UL,
	};
	return traverse_mount_points(&get_mount_info_traverser, &state);
}

/* traverse_mount_points client that gets mount point info for a given path. */
static int
get_mount_info_traverser(struct mntent *entry, void *arg)
{
	get_mount_point_traverser_state *const state = arg;
	if(path_starts_with(state->path, entry->mnt_dir))
	{
		const size_t new_len = strlen(entry->mnt_dir);
		if(new_len > state->curr_len)
		{
			state->curr_len = new_len;
			switch(state->type)
			{
				case MI_MOUNT_POINT:
					copy_str(state->buf, state->buf_len, entry->mnt_dir);
					break;
				case MI_FS_TYPE:
					copy_str(state->buf, state->buf_len, entry->mnt_type);
					break;

				default:
					assert(0 && "Unknown mount information type.");
					break;
			}
		}
	}
	return 0;
}

int
traverse_mount_points(mptraverser client, void *arg)
{
	/* Cached mount entries, updated only when /etc/mtab changes. */
	static filemon_t mtab_mon;
	static struct mntent *entries;
	static unsigned int nentries;

	filemon_t mon;
	unsigned int i;

	/* Check for cache validity. */
	if(filemon_from_file("/etc/mtab", &mon) != 0 ||
			!filemon_equal(&mon, &mtab_mon))
	{
		filemon_assign(&mtab_mon, &mon);
		free_mnt_entries(entries, nentries);
		entries = read_mnt_entries(&nentries);
	}

	if(nentries == 0U)
	{
		return 1;
	}

	for(i = 0; i < nentries; ++i)
	{
		client(&entries[i], arg);
	}

	return 0;
}

/* Frees array of mount entries. */
static void
free_mnt_entries(struct mntent *entries, unsigned int nentries)
{
	unsigned int i;

	for(i = 0; i < nentries; ++i)
	{
		free_mnt_entry(&entries[i]);
	}

	free(entries);
}

/* Reads in array of mount entries.  Always sets *nentries.  Returns the array,
 * which might be NULL if empty.  On memory allocation error, skips entries. */
struct mntent *
read_mnt_entries(unsigned int *nentries)
{
	FILE *f;
	struct mntent *entries = NULL;
	struct mntent *ent;

	*nentries = 0U;

	if((f = setmntent("/etc/mtab", "r")) == NULL)
	{
		return NULL;
	}

	while((ent = getmntent(f)) != NULL)
	{
		void *p = reallocarray(entries, *nentries + 1, sizeof(*entries));
		if(p != NULL)
		{
			entries = p;
			if(clone_mnt_entry(&entries[*nentries], ent) == 0)
			{
				++*nentries;
			}
		}
	}

	endmntent(f);

	return entries;
}

/* Clones *rhs mount entry into *lhs, which is assumed to do not contain data.
 * Returns zero on success, otherwise non-zero is returned. */
static int
clone_mnt_entry(struct mntent *lhs, const struct mntent *rhs)
{
	lhs->mnt_fsname = strdup(rhs->mnt_fsname);
	lhs->mnt_dir = strdup(rhs->mnt_dir);
	lhs->mnt_type = strdup(rhs->mnt_type);
	lhs->mnt_opts = strdup(rhs->mnt_opts);
	lhs->mnt_freq = rhs->mnt_freq;
	lhs->mnt_passno = rhs->mnt_passno;

	if(lhs->mnt_fsname == NULL || lhs->mnt_dir == NULL || lhs->mnt_type == NULL ||
			lhs->mnt_opts == NULL)
	{
		free_mnt_entry(lhs);
		return 1;
	}

	return 0;
}

/* Frees single mount entry. */
static void
free_mnt_entry(struct mntent *entry)
{
	free(entry->mnt_fsname);
	free(entry->mnt_dir);
	free(entry->mnt_type);
	free(entry->mnt_opts);
}

/* Checks that the str has at least one of comma separated list (the list) items
 * as a prefix.  Returns non-zero if so, otherwise zero is returned. */
static int
starts_with_list_item(const char str[], const char list[])
{
	char *const list_copy = strdup(list);

	char *prefix = list_copy, *state = NULL;
	while((prefix = split_and_get(prefix, ',', &state)) != NULL)
	{
		if(starts_with(str, prefix))
		{
			break;
		}
	}

	free(list_copy);

	return prefix != NULL;
}

/* Finds such element of comma separated list of paths (the list) that the path
 * is prefixed with it.  Returns the index or -1 on failure. */
static int
find_path_prefix_index(const char path[], const char list[])
{
	char *const list_copy = strdup(list);

	char *prefix = list_copy, *state = NULL;
	int i = 0;
	while((prefix = split_and_get(prefix, ',', &state)) != NULL)
	{
		if(path_starts_with(path, prefix))
		{
			break;
		}
		++i;
	}

	free(list_copy);

	return (prefix == NULL) ? -1 : i;
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

int
executable_exists(const char path[])
{
	return os_access(path, X_OK) == 0 && !is_dir(path);
}

int
get_exe_dir(char dir_buf[], size_t dir_buf_len)
{
	/* This operation isn't supported on *nix-like operating systems. */
	return 1;
}

EnvType
get_env_type(void)
{
	return ET_UNIX;
}

ExecEnvType
get_exec_env_type(void)
{
	const char *const term = env_get("TERM");
	if(term != NULL && ends_with(term, "linux"))
	{
		return EET_LINUX_NATIVE;
	}
	else
	{
		const char *const display = env_get("DISPLAY");
		return is_null_or_empty(display) ? EET_EMULATOR : EET_EMULATOR_WITH_X;
	}
}

ShellType
get_shell_type(const char shell_cmd[])
{
	return ST_NORMAL;
}

int
format_help_cmd(char cmd[], size_t cmd_size)
{
	int bg;
	char *const escaped = shell_like_escape(cfg.config_dir, 0);
	snprintf(cmd, cmd_size, "%s %s/" VIFM_HELP, cfg_get_vicmd(&bg), escaped);
	free(escaped);
	return bg;
}

void
display_help(const char cmd[])
{
	(void)shellout(cmd, PAUSE_ON_ERROR, 1);
}

void
wait_for_signal(void)
{
	pause();
}

void
stop_process(void)
{
	struct sigaction new, old;

	new.sa_handler = SIG_DFL;
	sigemptyset(&new.sa_mask);
	new.sa_flags = SA_RESTART;
	sigaction(SIGTSTP, &new, &old);

	kill(0, SIGTSTP);

	sigaction(SIGTSTP, &old, NULL);
}

void
update_terminal_settings(void)
{
	/* Do nothing. */
}

void
get_uid_string(const dir_entry_t *entry, int as_num, size_t buf_len, char buf[])
{
	/* Cache for the last requested user id. */
	static uid_t last_uid = (uid_t)-1;
	static char uid_buf[26];

	if(entry->uid == last_uid)
	{
		copy_str(buf, buf_len, uid_buf);
		return;
	}

	last_uid = entry->uid;
	snprintf(uid_buf, sizeof(uid_buf), "%d", (int)last_uid);

	if(!as_num)
	{
		enum { MAX_TRIES = 4 };
		size_t size = MAX(sysconf(_SC_GETPW_R_SIZE_MAX) + 1, PATH_MAX);
		int i;
		for(i = 0; i < MAX_TRIES; ++i, size *= 2)
		{
			char buf[size];
			struct passwd pwd_b;
			struct passwd *pwd_buf;

			if(getpwuid_r(last_uid, &pwd_b, buf, sizeof(buf), &pwd_buf) == 0 &&
					pwd_buf != NULL)
			{
				copy_str(uid_buf, sizeof(uid_buf), pwd_buf->pw_name);
				break;
			}
		}
	}

	copy_str(buf, buf_len, uid_buf);
}

void
get_gid_string(const dir_entry_t *entry, int as_num, size_t buf_len, char buf[])
{
	/* Cache for the last requested group id. */
	static gid_t last_gid = (gid_t)-1;
	static char gid_buf[26];

	if(entry->gid == last_gid)
	{
		copy_str(buf, buf_len, gid_buf);
		return;
	}

	last_gid = entry->gid;
	snprintf(gid_buf, sizeof(gid_buf), "%d", (int)last_gid);

	if(!as_num)
	{
		enum { MAX_TRIES = 4 };
		size_t size = MAX(sysconf(_SC_GETGR_R_SIZE_MAX) + 1, PATH_MAX);
		int i;
		for(i = 0; i < MAX_TRIES; ++i, size *= 2)
		{
			char buf[size];
			struct group group_b;
			struct group *group_buf;

			if(getgrgid_r(last_gid, &group_b, buf, sizeof(buf), &group_buf) == 0 &&
					group_buf != NULL)
			{
				copy_str(gid_buf, sizeof(gid_buf), group_buf->gr_name);
				break;
			}
		}
	}

	copy_str(buf, buf_len, gid_buf);
}

FILE *
reopen_term_stdout(void)
{
	FILE *fp;
	int outfd, ttyfd;

	outfd = dup(STDOUT_FILENO);
	if(outfd == -1)
	{
		fprintf(stderr, "Failed to store original output stream.\n");
		return NULL;
	}

	fp = fdopen(outfd, "w");
	if(fp == NULL)
	{
		fprintf(stderr, "Failed to open original output stream.\n");
		return NULL;
	}

	ttyfd = open_tty();
	if(ttyfd == -1)
	{
		fprintf(stderr, "Failed to open terminal for output: %s\n",
				strerror(errno));
		fclose(fp);
		return NULL;
	}
	if(dup2(ttyfd, STDOUT_FILENO) == -1)
	{
		close(ttyfd);
		fclose(fp);
		fprintf(stderr, "Failed to setup terminal as standard output stream.\n");
		return NULL;
	}

	close(ttyfd);
	return fp;
}

int
reopen_term_stdin(void)
{
	int ttyfd;

	if(close(STDIN_FILENO))
	{
		fprintf(stderr, "Failed to close original input stream.\n");
		return 1;
	}

	ttyfd = open_tty();
	if(ttyfd != STDIN_FILENO)
	{
		if(ttyfd != -1)
		{
			close(ttyfd);
		}

		fprintf(stderr, "Failed to open terminal for input.\n");
		return 1;
	}

	return 0;
}

/* Opens controlling terminal doing its best at guessing its name.  This might
 * not work if all the standard streams are redirected, in which case /dev/tty
 * is used, which can't be passed around between processes, so some applications
 * won't work in this (quite rare) case.  Returns opened file descriptor or -1
 * on error. */
static int
open_tty(void)
{
	int fd = -1;

	/* XXX: we might be better off duplicating descriptor if possible. */

	if(isatty(STDIN_FILENO))
	{
		fd = open(ttyname(STDIN_FILENO), O_RDWR);
	}
	if(fd == -1 && isatty(STDOUT_FILENO))
	{
		fd = open(ttyname(STDOUT_FILENO), O_RDWR);
	}
	if(fd == -1 && isatty(STDERR_FILENO))
	{
		fd = open(ttyname(STDERR_FILENO), O_RDWR);
	}

	if(fd == -1)
	{
		fd = open("/dev/tty", O_RDWR);
	}

	return fd;
}

FILE *
read_cmd_output(const char cmd[])
{
	FILE *fp;
	pid_t pid;
	int out_pipe[2];

	if(pipe(out_pipe) != 0)
	{
		return NULL;
	}

	pid = fork();
	if(pid == (pid_t)-1)
	{
		return NULL;
	}

	if(pid == 0)
	{
		run_from_fork(out_pipe, 0, (char *)cmd);
		return NULL;
	}

	/* Close write end of pipe. */
	close(out_pipe[1]);

	fp = fdopen(out_pipe[0], "r");
	if(fp == NULL)
	{
		close(out_pipe[0]);
	}
	return fp;
}

const char *
get_installed_data_dir(void)
{
	return PACKAGE_DATA_DIR;
}

void
clone_timestamps(const char path[], const char from[], const struct stat *st)
{
	struct timeval tv[2];

#if defined(HAVE_STRUCT_STAT_ST_MTIM) && defined(HAVE_FUTIMENS)
	const int fd = open(path, O_WRONLY);
	if(fd != -1)
	{
		struct timespec ts[2] = { st->st_atim, st->st_mtim };
		if(futimens(fd, ts) == 0)
		{
			close(fd);
			return;
		}
		close(fd);
		/* Not sure how useful this is, but we can try both functions in a row. */
	}
#endif

#ifdef HAVE_STRUCT_STAT_ST_MTIM
	tv[0].tv_sec = st->st_atim.tv_sec;
	tv[0].tv_usec = st->st_atim.tv_nsec/1000;
	tv[1].tv_sec = st->st_mtim.tv_sec;
	tv[1].tv_usec = st->st_mtim.tv_nsec/1000;
#else
	tv[0].tv_sec = st->st_atime;
	tv[0].tv_usec = 0;
	tv[1].tv_sec = st->st_mtime;
	tv[1].tv_usec = 0;
#endif
	utimes(path, tv);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
