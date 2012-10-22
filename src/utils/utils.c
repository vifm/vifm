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

#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#endif

#include <regex.h>

#include <curses.h>

#include <fcntl.h>

#ifndef _WIN32
#include <grp.h> /* getgrnam() */
#include <pwd.h> /* getpwnam() */
#include <sys/wait.h> /* waitpid() */
#endif

#if !defined(_WIN32) && !defined(__APPLE__)
#include <mntent.h> /* getmntent() */
#endif

#include <unistd.h> /* chdir() */

#include <ctype.h>
#include <errno.h>
#include <limits.h> /* PATH_MAX */
#include <signal.h> /* signal() SIGINT SIGTSTP SIG_DFL */
#include <stddef.h> /* size_t */
#include <string.h>
#include <wctype.h>

#include "../cfg/config.h"
#include "../fuse.h"
#include "../status.h"
#include "../ui.h"
#ifdef _WIN32
#include "env.h"
#endif
#include "fs.h"
#include "fs_limits.h"
#include "macros.h"
#include "path.h"
#include "str.h"

#include "utils.h"

#ifdef _WIN32

static const char PATHEXT_EXT[] = ".bat;.exe;.com";

static void unquote(char quoted[]);
#endif

int
my_system(char *command)
{
#ifndef _WIN32
	int pid;
	int result;
	extern char **environ;
	void (*saved_dfl_sig_handler)(int);

	if(command == NULL)
		return 1;

	saved_dfl_sig_handler = signal(SIGTSTP, SIG_DFL);

	pid = fork();
	if(pid == -1)
	{
		signal(SIGTSTP, saved_dfl_sig_handler);
		return -1;
	}
	if(pid == 0)
	{
		char *args[4];

		signal(SIGTSTP, SIG_DFL);
		signal(SIGINT, SIG_DFL);

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
	signal(SIGTSTP, saved_dfl_sig_handler);
	return result;
#else
	char buf[strlen(cfg.shell) + 5 + strlen(command)*4 + 1 + 1];

	system("cls");

	if(stroscmp(cfg.shell, "cmd") == 0)
	{
		snprintf(buf, sizeof(buf), "%s /C \"%s\"", cfg.shell, command);
		return system(buf);
	}
	else
	{
		char *p;

		strcpy(buf, cfg.shell);
		strcat(buf, " -c '");

		p = buf + strlen(buf);
		while(*command != '\0')
		{
			if(*command == '\\')
				*p++ = '\\';
			*p++ = *command++;
		}
		*p = '\0';

		strcat(buf, "'");
		return exec_program(buf);
	}
#endif
}

#ifndef _WIN32

/* if err == 1 then use stderr and close stdin and stdout */
void _gnuc_noreturn
run_from_fork(int pipe[2], int err, char *cmd)
{
	char *args[4];
	int nullfd;

	close(err ? STDERR_FILENO : STDOUT_FILENO);
	if(dup(pipe[1]) == -1) /* Redirect stderr or stdout to write end of pipe. */
		exit(1);
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
get_perm_string(char * buf, int len, mode_t mode)
{
	char *perm_sets[] =
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

#endif

int
my_chdir(const char *path)
{
	char curr_path[PATH_MAX];
	if(getcwd(curr_path, sizeof(curr_path)) == curr_path)
	{
		if(stroscmp(curr_path, path) == 0)
			return 0;
	}
	return chdir(path);
}

#if !defined(_WIN32) && !defined(__APPLE__)
static int
begins_with_list_item(const char *pattern, const char *list)
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
#endif

int
is_on_slow_fs(const char *full_path)
{
#if defined(_WIN32) || defined(__APPLE__)
	return 0;
#else
	FILE *f;
	struct mntent *ent;
	size_t len = 0;
	char max[PATH_MAX] = "";

	if((f = setmntent("/etc/mtab", "r")) == NULL)
	{
		return 0;
	}

	while((ent = getmntent(f)) != NULL)
	{
		if(path_starts_with(full_path, ent->mnt_dir))
		{
			size_t new_len = strlen(ent->mnt_dir);
			if(new_len > len)
			{
				len = new_len;
				snprintf(max, sizeof(max), "%s", ent->mnt_fsname);
			}
		}
	}

	if(max[0] != '\0')
	{
		if(begins_with_list_item(max, cfg.slow_fs_list))
		{
			endmntent(f);
			return 1;
		}
	}

	endmntent(f);
	return 0;
#endif
}

int
friendly_size_notation(uint64_t num, int str_size, char *str)
{
	static const char* iec_units[] = {
		"  B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"
	};
	static const char* si_units[] = {
		"B", "K", "M", "G", "T", "P", "E", "Z", "Y"
	};
	ARRAY_GUARD(iec_units, ARRAY_LEN(si_units));

	const char** units;
	size_t u;
	double d = num;

	units = cfg.use_iec_prefixes ? iec_units : si_units;

	u = 0;
	while(d >= 1023.5 && u < ARRAY_LEN(iec_units) - 1)
	{
		d /= 1024.0;
		u++;
	}
	if(u == 0)
	{
		snprintf(str, str_size, "%.0f %s", d, units[u]);
	}
	else
	{
		if(d > 9)
			snprintf(str, str_size, "%.0f %s", d, units[u]);
		else
		{
			size_t len = snprintf(str, str_size, "%.1f %s", d, units[u]);
			if(str[len - strlen(units[u]) - 1 - 1] == '0')
				snprintf(str, str_size, "%.0f %s", d, units[u]);
		}
	}

	return u > 0;
}

int
get_regexp_cflags(const char *pattern)
{
	int result;

	result = REG_EXTENDED;
	if(cfg.ignore_case)
		result |= REG_ICASE;

	if(cfg.ignore_case && cfg.smart_case)
	{
		wchar_t *wstring, *p;
		wstring = to_wide(pattern);
		p = wstring - 1;
		while(*++p != L'\0')
			if(iswupper(*p))
			{
				result &= ~REG_ICASE;
				break;
			}
		free(wstring);
	}
	return result;
}

const char *
get_regexp_error(int err, regex_t *re)
{
	static char buf[360];

	regerror(err, re, buf, sizeof(buf));
	return buf;
}

/* Returns pointer to a statically allocated buffer */
const char *
enclose_in_dquotes(const char *str)
{
	static char buf[PATH_MAX];
	char *p;

	p = buf;
	*p++ = '"';
	while(*str != '\0')
	{
		if(*str == '\\' || *str == '"')
			*p++ = '\\';
		*p++ = *str;

		str++;
	}
	*p++ = '"';
	*p = '\0';
	return buf;
}

const char *
make_name_unique(const char *filename)
{
	static char unique[PATH_MAX];
	size_t len;
	int i;

#ifndef _WIN32
	len = snprintf(unique, sizeof(unique), "%s_%u%u_00", filename, getppid(),
			getpid());
#else
	/* TODO: fix name uniqualization on Windows */
	len = snprintf(unique, sizeof(unique), "%s_%u%u_00", filename, 0, 0);
#endif
	i = 0;

	while(path_exists(unique))
	{
		sprintf(unique + len - 2, "%d", ++i);
	}
	return unique;
}

char *
get_command_name(const char line[], int raw, size_t buf_len, char buf[])
{
	const char *result;
#ifdef _WIN32
	int left_quote, right_quote;
#endif

	line = skip_whitespace(line);

#ifdef _WIN32
	if((left_quote = (line[0] == '"')))
	{
		result = strchr(line + 1, '"');
	}
	else
#endif
	{
		result = strchr(line, ' ');
	}
	if(result == NULL)
	{
		result = line + strlen(line);
	}

#ifdef _WIN32
	if(left_quote && (right_quote = (result[0] == '"')))
	{
		result++;
	}
#endif
	snprintf(buf, MIN(result - line + 1, buf_len), "%s", line);
#ifdef _WIN32
	if(!raw && left_quote && right_quote)
	{
		unquote(buf);
	}
#endif
	if(!raw)
	{
		remove_mount_prefixes(buf);
	}
	result = skip_whitespace(result);

	return (char *)result;
}

#ifdef _WIN32
/* Removes first and the last charater of the string, if they are quotes. */
static void
unquote(char quoted[])
{
	size_t len = strlen(quoted);
	if(len > 2 && quoted[0] == quoted[len - 1] && strpbrk(quoted, "\"'`") != NULL)
	{
		memmove(quoted, quoted + 1, len - 2);
		quoted[len - 2] = '\0';
	}
}
#endif

#ifndef _WIN32
int
get_uid(const char *user, uid_t *uid)
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
get_gid(const char *group, gid_t *gid)
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

#else

int
wcwidth(wchar_t c)
{
	return 1;
}

int
wcswidth(const wchar_t *str, size_t len)
{
	return MIN(len, wcslen(str));
}

int
exec_program(TCHAR *cmd)
{
	BOOL ret;
	DWORD exitcode;
	STARTUPINFO startup = {};
	PROCESS_INFORMATION pinfo;

	ret = CreateProcessA(NULL, cmd, NULL, NULL, 0, 0, NULL, NULL, &startup,
			&pinfo);
	if(ret == 0)
		return -1;

	CloseHandle(pinfo.hThread);

	if(WaitForSingleObject(pinfo.hProcess, INFINITE) != WAIT_OBJECT_0)
	{
		CloseHandle(pinfo.hProcess);
		return -1;
	}
	if(GetExitCodeProcess(pinfo.hProcess, &exitcode) == 0)
	{
		CloseHandle(pinfo.hProcess);
		return -1;
	}
	CloseHandle(pinfo.hProcess);
	return exitcode;
}

static void
strtoupper(char *s)
{
	while(*s != '\0')
	{
		*s = toupper(*s);
		s++;
	}
}

int
win_executable_exists(const char *path)
{
	const char *p;
	char path_buf[NAME_MAX];
	size_t pos;

	if(strchr(after_last(path, '/'), '.') != NULL)
	{
		return path_exists(path);
	}

	snprintf(path_buf, sizeof(path_buf), "%s", path);
	pos = strlen(path_buf);

	p = env_get_def("PATHEXT", PATHEXT_EXT) - 1;
	while((p = extract_part(p, ';', path_buf + pos))[0] != '\0')
	{
		if(path_exists(path_buf))
		{
			return 1;
		}
	}
	return 0;
}

int
is_win_executable(const char *name)
{
	const char *p;
	char name_buf[NAME_MAX];
	char ext_buf[16];

	snprintf(name_buf, sizeof(name_buf), "%s", name);
	strtoupper(name_buf);

	p = env_get_def("PATHEXT", PATHEXT_EXT) - 1;
	while((p = extract_part(p, ';', ext_buf))[0] != '\0')
	{
		strtoupper(ext_buf);
		if(ends_with(name_buf, ext_buf))
		{
			return 1;
		}
	}
	return 0;
}

int
is_vista_and_above(void)
{
	DWORD v = GetVersion();
	return ((v & 0xff) >= 6);
}

/* Converts Windows attributes to a string.
 * Returns pointer to a statically allocated buffer */
const char *
attr_str(DWORD attr)
{
	static char buf[5 + 1];
	buf[0] = '\0';
	if(attr & FILE_ATTRIBUTE_ARCHIVE)
		strcat(buf, "A");
	if(attr & FILE_ATTRIBUTE_HIDDEN)
		strcat(buf, "H");
	if(attr & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED)
		strcat(buf, "I");
	if(attr & FILE_ATTRIBUTE_READONLY)
		strcat(buf, "R");
	if(attr & FILE_ATTRIBUTE_SYSTEM)
		strcat(buf, "S");

	return buf;
}

/* Converts Windows attributes to a long string containing all attribute values.
 * Returns pointer to a statically allocated buffer */
const char *
attr_str_long(DWORD attr)
{
	static char buf[10 + 1];
	snprintf(buf, sizeof(buf), "ahirscdepz");
	buf[0] ^= ((attr & FILE_ATTRIBUTE_ARCHIVE) != 0)*0x20;
	buf[1] ^= ((attr & FILE_ATTRIBUTE_HIDDEN) != 0)*0x20;
	buf[2] ^= ((attr & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED) != 0)*0x20;
	buf[3] ^= ((attr & FILE_ATTRIBUTE_READONLY) != 0)*0x20;
	buf[4] ^= ((attr & FILE_ATTRIBUTE_SYSTEM) != 0)*0x20;
	buf[5] ^= ((attr & FILE_ATTRIBUTE_COMPRESSED) != 0)*0x20;
	buf[6] ^= ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0)*0x20;
	buf[7] ^= ((attr & FILE_ATTRIBUTE_ENCRYPTED) != 0)*0x20;
	buf[8] ^= ((attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0)*0x20;
	buf[9] ^= ((attr & FILE_ATTRIBUTE_SPARSE_FILE) != 0)*0x20;

	return buf;
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
