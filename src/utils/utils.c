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

#include "utils.h"
#include "utils_int.h"

#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#endif

#include <regex.h>

#include <curses.h>

#ifndef _WIN32
#include <sys/wait.h> /* waitpid() */
#endif

#include <unistd.h> /* chdir() */

#include <ctype.h> /* isalnum() isalpha() */
#include <stddef.h> /* size_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* strdup() strchr() strlen() strpbrk() */

#include "../cfg/config.h"
#include "../fuse.h"
#include "env.h"
#include "fs.h"
#include "fs_limits.h"
#include "log.h"
#include "macros.h"
#include "path.h"
#include "str.h"

#ifdef _WIN32

static void unquote(char quoted[]);

#endif

int
my_system(char command[])
{
#ifdef _WIN32
	system("cls");
#endif
	LOG_INFO_MSG("Shell command: %s", command);
	return run_in_shell_no_cls(command);
}

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

char *
expand_path(const char path[])
{
	char *const expanded_envvars = expand_envvars(path, 0);
	/* expand_tilde() frees memory pointed to by expand_envvars. */
	return expand_tilde(expanded_envvars);
}

char *
expand_envvars(const char str[], int escape_vals)
{
	char *result = NULL;
	size_t len = 0;
	int prev_slash = 0;
	while(*str != '\0')
	{
		if(!prev_slash && *str == '$' && isalpha(str[1]))
		{
			char var_name[NAME_MAX];
			const char *p = str + 1;
			char *q = var_name;
			const char *var_value;

			while((isalnum(*p) || *p == '_') && q - var_name < sizeof(var_name) - 1)
				*q++ = *p++;
			*q = '\0';

			var_value = env_get(var_name);
			if(var_value != NULL)
			{
				char *escaped_var_value = NULL;
				if(escape_vals)
				{
					escaped_var_value = escape_filename(var_value, 1);
					var_value = escaped_var_value;
				}

				result = extend_string(result, var_value, &len);
				free(escaped_var_value);

				str = p;
			}
			else
			{
				str++;
			}
		}
		else
		{
			prev_slash = (*str == '\\') ? !prev_slash : 0;

			if(!prev_slash || escape_vals)
			{
				const char single_char[] = { *str, '\0' };
				result = extend_string(result, single_char, &len);
			}

			str++;
		}
	}
	if(result == NULL)
		result = strdup("");
	return result;
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
get_regexp_cflags(const char pattern[])
{
	int result = REG_EXTENDED;
	if(regexp_should_ignore_case(pattern))
	{
		result |= REG_ICASE;
	}
	return result;
}

int
regexp_should_ignore_case(const char pattern[])
{
	int ignore_case = cfg.ignore_case;
	if(cfg.ignore_case && cfg.smart_case)
	{
		if(has_uppercase_letters(pattern))
		{
			ignore_case = 0;
		}
	}
	return ignore_case;
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
make_name_unique(const char filename[])
{
	static char unique[PATH_MAX];
	size_t len;
	int i;

#ifndef _WIN32
	len = snprintf(unique, sizeof(unique), "%s_%u%u_00", filename, getppid(),
			get_pid());
#else
	/* TODO: think about better name uniqualization on Windows. */
	len = snprintf(unique, sizeof(unique), "%s_%u_00", filename, get_pid());
#endif
	i = 0;

	while(path_exists(unique))
	{
		sprintf(unique + len - 2, "%d", ++i);
	}
	return unique;
}

char *
extract_cmd_name(const char line[], int raw, size_t buf_len, char buf[])
{
	const char *result;
#ifdef _WIN32
	int left_quote, right_quote = 0;
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

int
vifm_wcwidth(wchar_t wc)
{
	const int width = wcwidth(wc);
	if(width == -1)
	{
		return ((size_t)wc < (size_t)L' ') ? 2 : 1;
	}
	else
	{
		return width;
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
