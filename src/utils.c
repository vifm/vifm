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

#include "../config.h"

#if !defined(_WIN32) && defined(HAVE_X11)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

#include <regex.h>

#include <curses.h>

#include <fcntl.h>
#include <sys/stat.h> /* mkdir */
#include <sys/time.h>

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
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

#include "../config.h"

#include "config.h"
#include "log.h"
#include "macros.h"
#include "status.h"
#include "string_array.h"
#include "version.h"
#include "utf8.h"
#include "ui.h"

#include "utils.h"

fuse_mount_t *fuse_mounts = NULL;

static struct
{
	int initialized;
	int supported;
#ifndef _WIN32
	char title[512];
#else
	wchar_t title[512];
#endif
}title_state;

static void check_title_supported();
static void save_term_title();
static void restore_term_title();
#if !defined(_WIN32) && defined(HAVE_X11)
static int get_x11_disp_and_win(Display **disp, Window *win);
static void get_x11_window_title(Display *disp, Window win, char *buf,
		size_t buf_len);
static int x_error_check(Display *dpy, XErrorEvent *error_event);
#endif
static void set_terminal_title(const char *path);

int
is_dir(const char *file)
{
#ifndef _WIN32
	struct stat statbuf;
	if(stat(file, &statbuf) != 0)
	{
		LOG_SERROR_MSG(errno, "Can't stat \"%s\"", file);
		log_cwd();
		return 0;
	}

	return S_ISDIR(statbuf.st_mode);
#else
	DWORD attr;

	if(is_path_absolute(file) && !is_unc_path(file))
	{
		char buf[] = {file[0], ':', '\\', '\0'};
		UINT type = GetDriveTypeA(buf);
		if(type == DRIVE_UNKNOWN || type == DRIVE_NO_ROOT_DIR)
			return 0;
	}

	attr = GetFileAttributesA(file);
	if(attr == INVALID_FILE_ATTRIBUTES)
	{
		LOG_SERROR_MSG(errno, "Can't get attributes of \"%s\"", file);
		log_cwd();
		return 0;
	}

	return (attr & FILE_ATTRIBUTE_DIRECTORY);
#endif
}

/* Checks whether path/file exists. path can be NULL */
int
file_exists(const char *path, const char *file)
{
	char full[PATH_MAX];
	if(path == NULL)
		snprintf(full, sizeof(full), "%s", file);
	else
		snprintf(full, sizeof(full), "%s/%s", path, file);
#ifndef _WIN32
	return access(full, F_OK) == 0;
#else
	if(is_path_absolute(full) && !is_unc_path(full))
	{
		char buf[] = {full[0], ':', '\\', '\0'};
		UINT type = GetDriveTypeA(buf);
		if(type == DRIVE_UNKNOWN || type == DRIVE_NO_ROOT_DIR)
			return 0;
	}

	return (GetFileAttributesA(full) != INVALID_FILE_ATTRIBUTES);
#endif
}

int
my_system(char *command)
{
#ifndef _WIN32
	int pid;
	int status;
	extern char **environ;

	if(command == NULL)
		return 1;

	pid = fork();
	if(pid == -1)
		return -1;
	if(pid == 0)
	{
		char *args[4];

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
		if(waitpid(pid, &status, 0) == -1)
		{
			if(errno != EINTR)
				return -1;
		}
		else
			return status;
	}while(1);
#else
	char buf[strlen(cfg.shell) + 5 + strlen(command)*4 + 1 + 1];

	system("cls");

	if(pathcmp(cfg.shell, "cmd") == 0)
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

/*
 * Escape the filename for the purpose of inserting it into the shell.
 *
 * quote_percent means prepend percent sign with a percent sign
 *
 * Returns new string, caller should free it.
 */
char *
escape_filename(const char *string, int quote_percent)
{
	size_t len;
	size_t i;
	char *ret, *dup;

	len = strlen(string);

	dup = ret = malloc(len*2 + 2 + 1);

	if(*string == '-')
	{
		*dup++ = '.';
		*dup++ = '/';
	}
	else if(*string == '~')
	{
		*dup++ = *string++;
	}

	for(i = 0; i < len; i++, string++, dup++)
	{
		switch(*string)
		{
			case '%':
				if(quote_percent)
					*dup++ = '%';
				break;
			case '\'':
			case '\\':
			case '\r':
			case '\n':
			case '\t':
			case '"':
			case ';':
			case ' ':
			case '?':
			case '|':
			case '[':
			case ']':
			case '{':
			case '}':
			case '<':
			case '>':
			case '`':
			case '!':
			case '$':
			case '&':
			case '*':
			case '(':
			case ')':
				*dup++ = '\\';
				break;
			case '~':
			case '#':
				if(dup == ret)
					*dup++ = '\\';
				break;
		}
		*dup = *string;
	}
	*dup = '\0';
	return ret;
}

void
chomp(char *text)
{
	size_t len;

	if(text[0] == '\0')
		return;

	len = strlen(text);
	if(text[len - 1] == '\n')
		text[len - 1] = '\0';
}

/* like chomp() but removes trailing slash */
void
chosp(char *text)
{
	size_t len;

	if(text[0] == '\0')
		return;

	len = strlen(text);
	if(text[len - 1] == '/')
		text[len - 1] = '\0';
}

wchar_t *
to_wide(const char *s)
{
	wchar_t *result;
	int len;

	len = mbstowcs(NULL, s, 0);
	result = malloc((len + 1)*sizeof(wchar_t));
	if(result != NULL)
		mbstowcs(result, s, len + 1);
	return result;
}

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

#ifndef _WIN32
	execvp(args[0], args);
#else
	execvp(args[0], (const char **)args);
#endif
	exit(1);
}

/* I'm really worry about the portability... */
wchar_t *
my_wcsdup(const wchar_t *ws)
{
	wchar_t *result;

	result = malloc((wcslen(ws) + 1)*sizeof(wchar_t));
	if(result == NULL)
		return NULL;
	wcscpy(result, ws);
	return result;
}

void
get_perm_string(char * buf, int len, mode_t mode)
{
#ifndef _WIN32
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
#else
	buf[0] = '\0';
#endif
}

/* When list is NULL returns maximum number of lines, otherwise returns number
 * of filled lines */
int
fill_version_info(char **list)
{
	int x = 0;

	if(list == NULL)
		return 10;

	list[x++] = strdup("Version: " VERSION);
	list[x] = malloc(sizeof("Git commit hash: ") + strlen(GIT_HASH) + 1);
	sprintf(list[x++], "Git commit hash: %s", GIT_HASH);
	list[x++] = strdup("Compiled at: " __DATE__ " " __TIME__);
	list[x++] = strdup("");

#ifdef ENABLE_COMPATIBILITY_MODE
	list[x++] = strdup("Compatibility mode is on");
#else
	list[x++] = strdup("Compatibility mode is off");
#endif

#ifdef ENABLE_EXTENDED_KEYS
	list[x++] = strdup("Support of extended keys is on");
#else
	list[x++] = strdup("Support of extended keys is off");
#endif

#ifdef HAVE_LIBGTK
	list[x++] = strdup("With GTK+ library");
#else
	list[x++] = strdup("Without GTK+ library");
#endif

#ifdef HAVE_LIBMAGIC
	list[x++] = strdup("With magic library");
#else
	list[x++] = strdup("Without magic library");
#endif

#ifdef HAVE_X11
	list[x++] = strdup("With X11 library");
#else
	list[x++] = strdup("Without X11 library");
#endif

#ifdef HAVE_FILE_PROG
	list[x++] = strdup("With file program");
#else
	list[x++] = strdup("Without file program");
#endif

	return x;
}

int
path_starts_with(const char *path, const char *begin)
{
	size_t len = strlen(begin);

	if(len > 0 && begin[len - 1] == '/')
		len--;

	if(pathncmp(path, begin, len) != 0)
		return 0;

	return (path[len] == '\0' || path[len] == '/');
}

int
my_chdir(const char *path)
{
	char curr_path[PATH_MAX];
	if(getcwd(curr_path, sizeof(curr_path)) == curr_path)
	{
		if(pathcmp(curr_path, path) == 0)
			return 0;
	}
	return chdir(path);
}

#ifndef _WIN32
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

	f = setmntent("/etc/mtab", "r");

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

void
friendly_size_notation(unsigned long long num, int str_size, char *str)
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

	if(cfg.use_iec_prefixes)
		units = iec_units;
	else
		units = si_units;

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
}

int
check_link_is_dir(const char *filename)
{
	char linkto[PATH_MAX + NAME_MAX];
	int saved_errno;
	char *filename_copy;
	char *p;

	filename_copy = strdup(filename);
	chosp(filename_copy);

	p = realpath(filename, linkto);
	saved_errno = errno;

	free(filename_copy);

	if(p == linkto)
	{
		return is_dir(linkto);
	}
	else
	{
		LOG_SERROR_MSG(saved_errno, "Can't readlink \"%s\"", filename);
		log_cwd();
	}

	return 0;
}

/* Removes excess slashes, "../" and "./" from the path */
void
canonicalize_path(const char *directory, char *buf, size_t buf_size)
{
	const char *p; /* source string pointer */
	char *q; /* destination string pointer */

	buf[0] = '\0';

	q = buf - 1;
	p = directory;

#ifdef _WIN32
	if(p[0] == '/' && p[1] == '/' && p[2] != '/')
	{
		strcpy(buf, "//");
		q = buf + 1;
		p += 2;
		while(*p != '\0' && *p != '/')
			*++q = *p++;
		buf = q + 1;
	}
#endif

	while(*p != '\0' && (size_t)((q + 1) - buf) < buf_size - 1)
	{
		int prev_dir_present;

		prev_dir_present = (q != buf - 1 && *q == '/');
		if(prev_dir_present && pathncmp(p, "./", 2) == 0)
			p++;
		else if(prev_dir_present && pathcmp(p, ".") == 0)
			;
		else if(prev_dir_present &&
				(pathncmp(p, "../", 3) == 0 || pathcmp(p, "..") == 0) &&
				pathcmp(buf, "../") != 0)
		{
#ifdef _WIN32
			if(*(q - 1) != ':')
#endif
			{
				p++;
				q--;
				while(q >= buf && *q != '/')
					q--;
			}
		}
		else if(*p == '/')
		{
			if(!prev_dir_present)
				*++q = '/';
		}
		else
		{
			*++q = *p;
		}

		p++;
	}

	if(*q != '/')
		*++q = '/';

	*++q = '\0';
}

const char *
make_rel_path(const char *path, const char *base)
{
	static char buf[PATH_MAX];

	const char *p = path, *b = base;
	int i;
	int nslashes;

#ifdef _WIN32
	if(path[1] == ':' && base[1] == ':' && path[0] != base[0])
	{
		canonicalize_path(path, buf, sizeof(buf));
		return buf;
	}
#endif

	while(p[0] != '\0' && p[1] != '\0' && b[0] != '\0' && b[1] != '\0')
	{
		const char *op = p, *ob = b;
		if((p = strchr(p + 1, '/')) == NULL)
			p = path + strlen(path);
		if((b = strchr(b + 1, '/')) == NULL)
			b = base + strlen(base);
		if(p - path != b - base || pathncmp(path, base, p - path) != 0)
		{
			p = op;
			b = ob;
			break;
		}
	}

	canonicalize_path(b, buf, sizeof(buf));
	chosp(buf);

	nslashes = 0;
	for(i = 0; buf[i] != '\0'; i++)
		if(buf[i] == '/')
			nslashes++;

	buf[0] = '\0';
	while(nslashes-- > 0)
		strcat(buf, "../");
	if(*p == '/')
		p++;
	canonicalize_path(p, buf + strlen(buf), sizeof(buf) - strlen(buf));
	chosp(buf);

	if(buf[0] == '\0')
		strcpy(buf, ".");

	return buf;
}

char *
replace_home_part(const char *directory)
{
	static char buf[PATH_MAX];
	size_t len;

	len = strlen(cfg.home_dir) - 1;
	if(pathncmp(directory, cfg.home_dir, len) == 0 &&
			(directory[len] == '\0' || directory[len] == '/'))
		strncat(strcpy(buf, "~"), directory + len, sizeof(buf) - strlen(buf) - 1);
	else
		strncpy(buf, directory, sizeof(buf));
	if(!is_root_dir(buf))
		chosp(buf);

	return buf;
}

char *
expand_tilde(char *path)
{
#ifndef _WIN32
	char name[NAME_MAX];
	char *p, *result;
	struct passwd *pw;
#endif

	if(path[0] != '~')
		return path;

	if(path[1] == '\0' || path[1] == '/')
	{
		char *result;

		result = malloc((strlen(cfg.home_dir) + strlen(path) + 1));
		if(result == NULL)
			return NULL;

		sprintf(result, "%s%s", cfg.home_dir, (path[1] == '/') ? (path + 2) : "");
		free(path);
		return result;
	}

#ifndef _WIN32
	if((p = strchr(path, '/')) == NULL)
	{
		p = path + strlen(path);
		strcpy(name, path + 1);
	}
	else
	{
		snprintf(name, p - (path + 1) + 1, "%s", path + 1);
		p++;
	}

	if((pw = getpwnam(name)) == NULL)
		return path;

	chosp(pw->pw_dir);
	result = malloc(strlen(pw->pw_dir) + strlen(path) + 1);
	if(result == NULL)
		return NULL;
	sprintf(result, "%s/%s", pw->pw_dir, p);
	free(path);

	return result;
#else
	return path;
#endif
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

int
is_root_dir(const char *path)
{
#ifdef _WIN32
	if(isalpha(path[0]) && pathcmp(path + 1, ":/") == 0)
		return 1;

	if(path[0] == '/' && path[1] == '/' && path[2] != '\0')
	{
		char *p = strchr(path + 2, '/');
		if(p == NULL || p[1] == '\0')
			return 1;
	}
#endif
	return (path[0] == '/' && path[1] == '\0');
}

int
is_unc_root(const char *path)
{
#ifdef _WIN32
	if(is_unc_path(path) && path[2] != '\0')
	{
		char *p = strchr(path + 2, '/');
		if(p == NULL || p[1] == '\0')
			return 1;
	}
	return 0;
#else
	return 0;
#endif
}

int
is_path_absolute(const char *path)
{
#ifdef _WIN32
	if(isalpha(path[0]) && path[1] == ':')
		return 1;
	if(path[0] == '/' && path[1] == '/')
		return 1;
#endif
	return (path[0] == '/');
}

int
ends_with(const char* str, const char* suffix)
{
	size_t str_len = strlen(str);
	size_t suf_len = strlen(suffix);

	if(str_len < suf_len)
		return 0;
	else
		return (strcmp(suffix, str + str_len - suf_len) == 0);
}

char *
to_multibyte(const wchar_t *s)
{
	size_t len;
	char *result;

	len = wcstombs(NULL, s, 0) + 1;
	if((result = malloc(len*sizeof(char))) == NULL)
		return NULL;

	wcstombs(result, s, len);
	return result;
}

int
get_link_target(const char *link, char *buf, size_t buf_len)
{
	LOG_FUNC_ENTER;

#ifndef _WIN32
	char *filename;
	ssize_t len;

	filename = strdup(link);
	chosp(filename);

	len = readlink(filename, buf, buf_len);

	free(filename);

	if(len == -1)
		return -1;

	buf[len] = '\0';
	return 0;
#else
	char filename[PATH_MAX];
	DWORD attr;
	HANDLE hfind;
	WIN32_FIND_DATAA ffd;
	HANDLE hfile;
	char rdb[2048];
	char *t;
	REPARSE_DATA_BUFFER *sbuf;
	WCHAR *path;

	attr = GetFileAttributes(link);
	if(attr == INVALID_FILE_ATTRIBUTES)
	{
		LOG_WERROR(GetLastError());
		return -1;
	}

	if(!(attr & FILE_ATTRIBUTE_REPARSE_POINT))
		return -1;

	snprintf(filename, sizeof(filename), "%s", link);
	chosp(filename);
	hfind = FindFirstFileA(filename, &ffd);
	if(hfind == INVALID_HANDLE_VALUE)
	{
		LOG_WERROR(GetLastError());
		return -1;
	}

	if(!FindClose(hfind))
	{
		LOG_WERROR(GetLastError());
	}

	if(ffd.dwReserved0 != IO_REPARSE_TAG_SYMLINK)
		return -1;

	hfile = CreateFileA(filename, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
			NULL);
	if(hfile == INVALID_HANDLE_VALUE)
	{
		LOG_WERROR(GetLastError());
		return -1;
	}

	if(!DeviceIoControl(hfile, FSCTL_GET_REPARSE_POINT, NULL, 0, rdb,
			sizeof(rdb), &attr, NULL))
	{
		LOG_WERROR(GetLastError());
		CloseHandle(hfile);
		return -1;
	}
	CloseHandle(hfile);
	
	sbuf = (REPARSE_DATA_BUFFER *)rdb;
	path = sbuf->SymbolicLinkReparseBuffer.PathBuffer;
	path[sbuf->SymbolicLinkReparseBuffer.PrintNameOffset/sizeof(WCHAR) +
			sbuf->SymbolicLinkReparseBuffer.PrintNameLength/sizeof(WCHAR)] = L'\0';
	t = to_multibyte(path +
			sbuf->SymbolicLinkReparseBuffer.PrintNameOffset/sizeof(WCHAR));
	if(strncmp(t, "\\??\\", 4) == 0)
		strncpy(buf, t + 4, buf_len);
	else
		strncpy(buf, t, buf_len);
	buf[buf_len - 1] = '\0';
	free(t);
	to_forward_slash(buf);
	return 0;
#endif
}

void
strtolower(char *s)
{
	while(*s != '\0')
	{
		*s = tolower(*s);
		s++;
	}
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

/* updates terminal title */
void
set_term_title(const char *full_path)
{
	if(!title_state.initialized)
	{
		check_title_supported();
		if(title_state.supported)
			save_term_title();
		title_state.initialized = 1;
	}
	if(!title_state.supported)
		return;

	if(full_path == NULL)
	{
		restore_term_title();
	}
	else
	{
		set_terminal_title(full_path);
	}
}

/* checks if we can alter terminal emulator title and writes result to
 * title_state.supported */
static void
check_title_supported()
{
#ifdef _WIN32
	title_state.supported = 1;
#else
	/* this list was taken from ranger's sources */
	static char *TERMINALS_WITH_TITLE[] = {
		"xterm", "xterm-256color", "rxvt", "rxvt-256color", "rxvt-unicode",
		"aterm", "Eterm", "screen", "screen-256color"
	};

	title_state.supported = is_in_string_array(TERMINALS_WITH_TITLE,
			ARRAY_LEN(TERMINALS_WITH_TITLE), env_get("TERM"));
#endif
}

/* stores current terminal title into title_state.title */
static void
save_term_title()
{
#ifdef _WIN32
	GetConsoleTitleW(title_state.title, ARRAY_LEN(title_state.title));
#else
#ifdef HAVE_X11
	Display *x11_display = 0;
	Window x11_window = 0;

	/* use X to determine current window title */
	if(get_x11_disp_and_win(&x11_display, &x11_window))
		get_x11_window_title(x11_display, x11_window, title_state.title,
				sizeof(title_state.title));
#endif
#endif
}

/* restores terminal title from title_state.title */
static void
restore_term_title()
{
#ifdef _WIN32
	if(title_state.title[0] != L'\0')
		SetConsoleTitleW(title_state.title);
#else
	if(title_state.title[0] != '\0')
		printf("\033]2;%s\007", title_state.title);
#endif
}

#if !defined(_WIN32) && defined(HAVE_X11)
/* loads X specific variables */
static int
get_x11_disp_and_win(Display **disp, Window *win)
{
	const char *winid;

	if(*win == 0 && (winid = env_get("WINDOWID")) != NULL)
		*win = (Window)atol(winid);

	if(*win != 0 && *disp == NULL)
		*disp = XOpenDisplay(NULL);
	if(*win == 0 || *disp == NULL)
		return 0;

	return 1;
}

/* gets terminal title using X */
static void
get_x11_window_title(Display *disp, Window win, char *buf, size_t buf_len)
{
	int (*old_handler)();
	XTextProperty text_prop;

	old_handler = XSetErrorHandler(x_error_check);
	if(!XGetWMName(disp, win, &text_prop))
	{
		(void)XSetErrorHandler(old_handler);
		return;
	}

	(void)XSetErrorHandler(old_handler);
	snprintf(buf, buf_len, "%s", text_prop.value);
	XFree((void *)text_prop.value);
}

/* callback function for reporting X errors, should return 0 on success */
static int
x_error_check(Display *dpy, XErrorEvent *error_event)
{
	return 0;
}
#endif

/* does real job on setting terminal title */
static void
set_terminal_title(const char *path)
{
#ifdef _WIN32
	wchar_t buf[2048];
	swprintf(buf, L"%S - VIFM", path);
	SetConsoleTitleW(buf);
#else
	printf("\033]2;%s - VIFM\007", path);
#endif
}

const char *
get_mode_str(mode_t mode)
{
	if(S_ISREG(mode))
	{
#ifndef _WIN32
		if((S_IXUSR & mode) || (S_IXGRP & mode) || (S_IXOTH & mode))
			return "exe";
		else
#endif
			return "reg";
	}
	else if(S_ISLNK(mode))
		return "link";
	else if(S_ISDIR(mode))
		return "dir";
	else if(S_ISCHR(mode))
		return "char";
	else if(S_ISBLK(mode))
		return "block";
	else if(S_ISFIFO(mode))
		return "fifo";
#ifndef _WIN32
	else if(S_ISSOCK(mode))
		return "sock";
#endif
	else
		return "?";
}

int
symlinks_available(void)
{
#ifndef _WIN32
	return 1;
#else
	return is_vista_and_above();
#endif
}

int
make_dir(const char *dir_name, mode_t mode)
{
#ifndef _WIN32
	return mkdir(dir_name, mode);
#else
	return mkdir(dir_name);
#endif
}

int
pathcmp(const char *file_name_a, const char *file_name_b)
{
#ifndef _WIN32
	return strcmp(file_name_a, file_name_b);
#else
	return strcasecmp(file_name_a, file_name_b);
#endif
}

int
pathncmp(const char *file_name_a, const char *file_name_b, size_t n)
{
#ifndef _WIN32
	return strncmp(file_name_a, file_name_b, n);
#else
	return strncasecmp(file_name_a, file_name_b, n);
#endif
}

/* Replaces the first found occurrence of c char in str with '\0' */
void
break_at(char *str, char c)
{
	char *p = strchr(str, c);
	if(p != NULL)
		*p = '\0';
}

/* Replaces the last found occurrence of c char in str with '\0' */
void
break_atr(char *str, char c)
{
	char *p = strrchr(str, c);
	if(p != NULL)
		*p = '\0';
}

/* Skips consecutive non-whitespace characters. */
char *
skip_non_whitespace(const char *str)
{
	while(!isspace(*str) && *str != '\0')
		str++;
	return (char *)str;
}

/* Skips consecutive whitespace characters. */
char *
skip_whitespace(const char *str)
{
	while(isspace(*str))
		str++;
	return (char *)str;
}

char *
get_line(FILE *fp, char *buf, size_t bufsz)
{
	int c = '\0';
	char *start = buf;

	while(bufsz-- > 1 && (c = get_char(fp)) != EOF)
	{
		*buf++ = c;
		if(c == '\n')
			break;
		bufsz--;
	}
	*buf = '\0';

	return (c == EOF && buf == start) ? NULL : start;
}

int
get_char(FILE *fp)
{
	int c = fgetc(fp);
	if(c == '\r')
	{
		int c2 = fgetc(fp);
		if(c2 != '\n')
			ungetc(c2, fp);
		c = '\n';
	}
	return c;
}

void
skip_until_eol(FILE *fp)
{
	while(get_char(fp) != '\n' && !feof(fp));
}

void
remove_eol(FILE *fp)
{
	int c = fgetc(fp);
	if(c == '\r')
		c = fgetc(fp);
	if(c != '\n')
		ungetc(c, fp);
}

const char *
env_get(const char *name)
{
	return getenv(name);
}

void
env_set(const char *name, const char *value)
{
#ifndef _WIN32
	setenv(name, value, 1);
#else
	char buf[strlen(name) + 1 + strlen(value) + 1];
	sprintf(buf, "%s=%s", name, value);
	putenv(buf);
#endif
}

void
env_remove(const char *name)
{
#ifndef _WIN32
	unsetenv(name);
#else
	env_set(name, "");
#endif
}

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
	return ((S_IXUSR & mode) || (S_IXGRP & mode) || (S_IXOTH & mode));
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
S_ISLNK(mode_t mode)
{
	return 0;
}

int
readlink(const char *path, char *buf, size_t len)
{
	return -1;
}

char *
realpath(const char *path, char *buf)
{
	if(get_link_target(path, buf, PATH_MAX) == 0)
		return buf;

	buf[0] = '\0';
	if(!is_path_absolute(path) && GetCurrentDirectory(PATH_MAX, buf) > 0)
	{
		int i;

		for(i = 0; buf[i] != '\0'; i++)
		{
			if(buf[i] == '\\')
				buf[i] = '/';
		}

		chosp(buf);
		strcat(buf, "/");
	}

	strcat(buf, path);
	return buf;
}

int
is_unc_path(const char *path)
{
	return (path[0] == '/' && path[1] == '/' && path[2] != '/');
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
is_win_executable(const char *name)
{
	const char *path, *p, *q;
	char name_buf[NAME_MAX];

	path = env_get("PATHEXT");
	if(path == NULL || path[0] == '\0')
		path = ".bat;.exe;.com";

	snprintf(name_buf, sizeof(name_buf), "%s", name);
	strtoupper(name_buf);

	p = path - 1;
	do
	{
		char ext_buf[16];

		p++;
		q = strchr(p, ';');
		if(q == NULL)
			q = p + strlen(p);

		snprintf(ext_buf, q - p + 1, "%s", p);
		strtoupper(ext_buf);
		p = q;

		if(ends_with(name_buf, ext_buf))
			return 1;
	}
	while(q[0] != '\0');
	return 0;
}

int
is_vista_and_above(void)
{
	DWORD v = GetVersion();
	return ((v & 0xff) >= 6);
}

void
to_forward_slash(char *path)
{
	int i;
	for(i = 0; path[i] != '\0'; i++)
	{
		if(path[i] == '\\')
			path[i] = '/';
	}
}

void
to_back_slash(char *path)
{
	int i;
	for(i = 0; path[i] != '\0'; i++)
	{
		if(path[i] == '/')
			path[i] = '\\';
	}
}

int
is_on_fat_volume(const char *path)
{
	char buf[NAME_MAX];
	char fs[16];
	if(is_unc_path(path))
	{
		int i = 4, j = 0;
		snprintf(buf, sizeof(buf), "%s", path);
		while(i > 0 && buf[j] != '\0')
			if(buf[j++] == '/')
				i--;
		if(i == 0)
			buf[j - 1] = '\0';
	}
	else
	{
		strcpy(buf, "x:\\");
		buf[0] = path[0];
	}
	if(GetVolumeInformationA(buf, NULL, 0, NULL, NULL, NULL, fs, sizeof(fs)))
	{
		if(strncasecmp(fs, "fat", 3) == 0)
			return 1;
	}
	return 0;
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

/* Checks specified drive for existence */
int
drive_exists(TCHAR letter)
{
	TCHAR drive[] = TEXT("?:\\");
	drive[0] = letter;
	int type = GetDriveType(drive);

	switch(type)
	{
		case DRIVE_CDROM:
		case DRIVE_REMOTE:
		case DRIVE_RAMDISK:
		case DRIVE_REMOVABLE:
		case DRIVE_FIXED:
			return 1;

		case DRIVE_UNKNOWN:
		case DRIVE_NO_ROOT_DIR:
		default:
			return 0;
	}
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
