/* vifm
 * Copyright (C) 2001 Ken Steen.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <regex.h>

#include <curses.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <pwd.h> /* getpwnam() */
#include <unistd.h>

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
#include "ui.h"

#include "utils.h"

struct Fuse_List *fuse_mounts = NULL;

int
S_ISEXE(mode_t mode)
{
	return ((S_IXUSR & mode) || (S_IXGRP & mode) || (S_IXOTH & mode));
}

int
is_dir(const char *file)
{
	struct stat statbuf;
	if(stat(file, &statbuf) != 0)
	{
		LOG_SERROR_MSG(errno, "Can't stat \"%s\"", file);
		log_cwd();
		return 0;
	}

	return S_ISDIR(statbuf.st_mode);
}

/*
 * Escape the filename for the purpose of inserting it into the shell.
 *
 * Automatically calculates string length when len == 0.
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

	dup = ret = (char *)malloc (len * 2 + 2 + 1);

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
				if (dup == ret)
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

/* returns width of utf8 character */
size_t
get_char_width(const char* string)
{
	if((string[0] & 0xe0) == 0xc0 && (string[1] & 0xc0) == 0x80)
		return 2;
	else if((string[0] & 0xf0) == 0xe0 && (string[1] & 0xc0) == 0x80 &&
			(string[2] & 0xc0) == 0x80)
		return 3;
	else if((string[0] & 0xf8) == 0xf0 && (string[1] & 0xc0) == 0x80 &&
			(string[2] & 0xc0) == 0x80 && (string[3] & 0xc0) == 0x80)
		return 4;
	else if(string[0] == '\0')
		return 0;
	else
		return 1;
}

/* returns count of bytes of whole string or of first max_len utf8 characters */
size_t
get_real_string_width(char *string, size_t max_len)
{
	size_t width = 0;
	while(*string != '\0' && max_len-- != 0)
	{
		size_t char_width = get_char_width(string);
		width += char_width;
		string += char_width;
	}
	return width;
}

static size_t
guess_char_width(char c)
{
	if((c & 0xe0) == 0xc0)
		return 2;
	else if((c & 0xf0) == 0xe0)
		return 3;
	else if((c & 0xf8) == 0xf0)
		return 4;
	else
		return 1;
}

/* returns count utf8 characters excluding incomplete utf8 characters */
size_t
get_normal_utf8_string_length(const char *string)
{
	size_t length = 0;
	while(*string != '\0')
	{
		size_t char_width = guess_char_width(*string);
		if(char_width <= strlen(string))
			length++;
		else
			break;
		string += char_width;
	}
	return length;
}

/* returns count of bytes excluding incomplete utf8 characters */
size_t
get_normal_utf8_string_widthn(const char *string, size_t max)
{
	size_t length = 0;
	while(*string != '\0' && max-- > 0)
	{
		size_t char_width = guess_char_width(*string);
		if(char_width <= strlen(string))
			length += char_width;
		else
			break;
		string += char_width;
	}
	return length;
}

/* returns count of bytes excluding incomplete utf8 characters */
size_t
get_normal_utf8_string_width(const char *string)
{
	size_t length = 0;
	while(*string != '\0')
	{
		size_t char_width = guess_char_width(*string);
		if(char_width <= strlen(string))
			length += char_width;
		else
			break;
		string += char_width;
	}
	return length;
}

/* returns count of utf8 characters in string */
size_t
get_utf8_string_length(const char *string)
{
	size_t length = 0;
	while(*string != '\0')
	{
		size_t char_width = get_char_width(string);
		string += char_width;
		length++;
	}
	return length;
}

/* returns (string_width - string_length) */
size_t
get_utf8_overhead(const char *string)
{
	size_t overhead = 0;
	while(*string != '\0')
	{
		size_t char_width = get_char_width(string);
		string += char_width;
		overhead += char_width - 1;
	}
	return overhead;
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

	close(err ? 2 : 1); /* Close stderr or stdout */
	if(dup(pipe[1]) == -1) /* Redirect stderr or stdout to write end of pipe. */
		exit(-1);
	close(pipe[0]);     /* Close read end of pipe. */
	close(0);           /* Close stdin */
	close(err ? 1 : 2); /* Close stdout or stderr */

	/* Send stdout, stdin to /dev/null */
	if((nullfd = open("/dev/null", O_RDONLY)) != -1)
	{
		dup2(nullfd, 0);
		dup2(nullfd, err ? 1 : 2);
	}

	args[0] = cfg.shell;
	args[1] = "-c";
	args[2] = cmd;
	args[3] = NULL;

	execvp(args[0], args);
	exit(-1);
}

/* I'm really worry about the portability... */
wchar_t *
my_wcsdup(const wchar_t *ws)
{
	wchar_t *result;

	result = (wchar_t *) malloc((wcslen(ws) + 1) * sizeof(wchar_t));
	if(result == NULL)
		return NULL;
	wcscpy(result, ws);
	return result;
}

char *
uchar2str(wchar_t c)
{
	static char buf[8];

	switch(c)
	{
		case L' ':
			strcpy(buf, "<space>");
			break;
		case L'\033':
			strcpy(buf, "<esc>");
			break;
		case L'\177':
			strcpy(buf, "<del>");
			break;
		case KEY_HOME:
			strcpy(buf, "<home>");
			break;
		case KEY_END:
			strcpy(buf, "<end>");
			break;
		case KEY_LEFT:
			strcpy(buf, "<left>");
			break;
		case KEY_RIGHT:
			strcpy(buf, "<right>");
			break;
		case KEY_UP:
			strcpy(buf, "<up>");
			break;
		case KEY_DOWN:
			strcpy(buf, "<down>");
			break;
		case KEY_BACKSPACE:
			strcpy(buf, "<backspace>");
			break;
		case KEY_DC:
			strcpy(buf, "<delete>");
			break;
		case KEY_PPAGE:
			strcpy(buf, "<pageup>");
			break;
		case KEY_NPAGE:
			strcpy(buf, "<pagedown>");
			break;

		default:
			if(c == L'\n' || (c > L' ' && c < 256))
			{
				buf[0] = c;
				buf[1] = '\0';
			}
			else if(c >= KEY_F0 && c < KEY_F0 + 10)
			{
				strcpy(buf, "<f0>");
				buf[2] += c - KEY_F0;
			}
			else if(c >= KEY_F0 + 10 && c < KEY_F0 + 63)
			{
				strcpy(buf, "<f00>");
				buf[2] += c/10 - KEY_F0;
				buf[3] += c%10 - KEY_F0;
			}
			else
			{
				strcpy(buf, "^A");
				buf[1] += c - 1;
			}
			break;
	}
	return buf;
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

	snprintf (buf, len, "-%s%s%s", perm_sets[u], perm_sets[g], perm_sets[o]);

	if (S_ISLNK (mode))
		buf[0] = 'l';
	else if (S_ISDIR (mode))
		buf[0] = 'd';
	else if (S_ISBLK (mode))
		buf[0] = 'b';
	else if (S_ISCHR (mode))
		buf[0] = 'c';
	else if (S_ISFIFO (mode))
		buf[0] = 'f';
	else if (S_ISSOCK (mode))
		buf[0] = 's';

	if (mode & S_ISVTX)
		buf[9] = (buf[9] == '-') ? 'T' : 't';
	if (mode & S_ISGID)
		buf[6] = (buf[6] == '-') ? 'S' : 's';
	if (mode & S_ISUID)
		buf[3] = (buf[3] == '-') ? 'S' : 's';
}

/* When list is NULL returns maximum number of lines, otherwise returns number
 * of filled lines */
int
fill_version_info(char **list)
{
	int x = 0;

	if(list == NULL)
		return 8;

	list[x++] = strdup("Version: " VERSION);
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

	if(strncmp(path, begin, len) != 0)
		return 0;

	return (path[len] == '\0' || path[len] == '/');
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

	static int _gnuc_unused units_size_guard[
		(ARRAY_LEN(iec_units) == ARRAY_LEN(si_units)) ? 1 : -1
	];

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
		struct stat s;
		if(lstat(linkto, &s) != 0)
			return 0;

		if((s.st_mode & S_IFMT) == S_IFDIR)
			return 1;
	}
	else
	{
		LOG_SERROR_MSG(saved_errno, "Can't readlink \"%s\"", filename);
		log_cwd();
	}

	return 0;
}

int
add_to_string_array(char ***array, int len, int count, ...)
{
	char **p;
	va_list va;

	p = realloc(*array, sizeof(char *)*(len + count));
	if(p == NULL)
		return count;
	*array = p;

	va_start(va, count);
	while(count-- > 0)
	{
		if((p[len] = strdup(va_arg(va, char *))) == NULL)
			break;
		len++;
	}
	va_end(va);

	return len;
}

int
is_in_string_array(char **array, size_t len, const char *key)
{
	int i;
	for(i = 0; i < len; i++)
		if(strcmp(array[i], key) == 0)
			return 1;
	return 0;
}

int
string_array_pos(char **array, size_t len, const char *key)
{
	int i;
	for(i = 0; i < len; i++)
		if(strcmp(array[i], key) == 0)
			return i;
	return -1;
}

void
free_string_array(char **array, size_t len)
{
	int i;

	for(i = 0; i < len; i++)
		free(array[i]);
	free(array);
}

void
free_wstring_array(wchar_t **array, size_t len)
{
	free_string_array((char **)array, len);
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
	while(*p != '\0' && (size_t)((q + 1) - buf) < buf_size - 1)
	{
		int prev_dir_present;

		prev_dir_present = (q != buf - 1 && *q == '/');
		if(prev_dir_present && strncmp(p, "./", 2) == 0)
			p++;
		else if(prev_dir_present && strcmp(p, ".") == 0)
			;
		else if(prev_dir_present &&
				(strncmp(p, "../", 3) == 0 || strcmp(p, "..") == 0) &&
				strcmp(buf, "../") != 0)
		{
			p++;
			q--;
			while(q >= buf && *q != '/')
				q--;
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

	while(p[0] != '\0' && p[1] != '\0' && b[0] != '\0' && b[1] != '\0')
	{
		const char *op = p, *ob = b;
		if((p = strchr(p + 1, '/')) == NULL)
			p = path + strlen(path);
		if((b = strchr(b + 1, '/')) == NULL)
			b = base + strlen(base);
		if(p - path != b - base || strncmp(path, base, p - path) != 0)
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

const char *
replace_home_part(const char *directory)
{
	static char buf[PATH_MAX];
	size_t len;

	len = strlen(cfg.home_dir) - 1;
	if(strncmp(directory, cfg.home_dir, len) == 0 &&
			(directory[len] == '\0' || directory[len] == '/'))
		strncat(strcpy(buf, "~"), directory + len, sizeof(buf));
	else
		strncpy(buf, directory, sizeof(buf));
	if(!is_root_dir(buf))
		chosp(buf);

	return buf;
}

char *
expand_tilde(char *path)
{
	char name[NAME_MAX];
	char *p, *result;
	struct passwd *pw;

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
	return (path[0] == '/' && path[1] == '\0');
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
