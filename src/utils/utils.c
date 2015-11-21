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

#include "utf8.h"
#endif

#include <regex.h>

#include <sys/types.h> /* pid_t */
#include <unistd.h>

#include <ctype.h> /* isalnum() isalpha() */
#include <stddef.h> /* size_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* free() malloc() */
#include <string.h> /* strdup() strchr() strlen() strpbrk() */
#include <wchar.h> /* wcwidth() */

#include "../cfg/config.h"
#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "../engine/keys.h"
#include "../int/fuse.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../ui/cancellation.h"
#include "../background.h"
#include "../registers.h"
#include "env.h"
#include "file_streams.h"
#include "fs.h"
#include "log.h"
#include "macros.h"
#include "path.h"
#include "str.h"

#ifdef _WIN32

static void unquote(char quoted[]);

#endif

int
vifm_system(char command[])
{
#ifdef _WIN32
	system("cls");
#endif
	LOG_INFO_MSG("Shell command: %s", command);
	return run_in_shell_no_cls(command);
}

int
process_cmd_output(const char descr[], const char cmd[], int user_sh,
		cmd_output_handler handler, void *arg)
{
	FILE *file, *err;
	char *line;
	pid_t pid;

	LOG_INFO_MSG("Capturing output of the command: %s", cmd);

	pid = background_and_capture((char *)cmd, user_sh, &file, &err);
	if(pid == (pid_t)-1)
	{
		return 1;
	}

	show_progress("", 0);

	ui_cancellation_reset();
	ui_cancellation_enable();

	wait_for_data_from(pid, file, 0);

	line = NULL;
	while((line = read_line(file, line)) != NULL)
	{
		show_progress(descr, 1000);
		handler(line, arg);
		wait_for_data_from(pid, file, 0);
	}

	ui_cancellation_disable();

	fclose(file);
	show_errors_from_file(err, descr);

	return 0;
}

int
vifm_chdir(const char path[])
{
	char curr_path[PATH_MAX];
	if(get_cwd(curr_path, sizeof(curr_path)) == curr_path)
	{
		if(stroscmp(curr_path, path) == 0)
		{
			return 0;
		}
	}
	return os_chdir(path);
}

char *
expand_path(const char path[])
{
	char *const expanded_envvars = expand_envvars(path, 0);
	/* replace_tilde() frees memory pointed to by expand_envvars. */
	return replace_tilde(expanded_envvars);
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

			while((isalnum(*p) || *p == '_') &&
					(size_t)(q - var_name) < sizeof(var_name) - 1)
			{
				*q++ = *p++;
			}
			*q = '\0';

			var_value = env_get(var_name);
			if(var_value != NULL)
			{
				char *escaped_var_value = NULL;
				if(escape_vals)
				{
					escaped_var_value = shell_like_escape(var_value, 1);
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

int
parse_case_flag(const char flags[], int *case_sensitive)
{
	/* TODO: maybe generalize code with substitute_cmd(). */

	while(*flags != '\0')
	{
		switch(*flags)
		{
			case 'i': *case_sensitive = 0; break;
			case 'I': *case_sensitive = 1; break;

			default:
				return 1;
		}

		++flags;
	}

	return 0;
}

const char *
enclose_in_dquotes(const char str[])
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

	while(path_exists(unique, DEREF))
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
	copy_str(buf, MIN((size_t)(result - line + 1), buf_len), line);
#ifdef _WIN32
	if(!raw && left_quote && right_quote)
	{
		unquote(buf);
	}
#endif
	if(!raw)
	{
		fuse_strip_mount_metadata(buf);
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
	return width;
}

int
vifm_wcswidth(const wchar_t str[], size_t n)
{
	int width = 0;
	while(*str != L'\0' && n--)
	{
		width += vifm_wcwidth(*str++);
	}
	return width;
}

char *
escape_for_squotes(const char string[], size_t offset)
{
	/* TODO: maybe combine code with escape_for_dquotes(). */

	size_t len;
	char *escaped, *out;

	len = strlen(string);

	escaped = malloc(len*2 + 1);
	out = escaped;

	/* Copy prefix not escaping it. */
	offset = MIN(len, offset);
	strncpy(out, string, offset);
	out += offset;
	string += offset;

	while(*string != '\0')
	{
		if(*string == '\'')
		{
			*out++ = '\'';
		}

		*out++ = *string++;
	}
	*out = '\0';
	return escaped;
}

char *
escape_for_dquotes(const char string[], size_t offset)
{
	/* TODO: maybe combine code with escape_for_squotes(). */

	size_t len;
	char *escaped, *out;

	len = strlen(string);

	escaped = malloc(len*2 + 1);
	out = escaped;

	/* Copy prefix not escaping it. */
	offset = MIN(len, offset);
	strncpy(out, string, offset);
	out += offset;
	string += offset;

	while(*string != '\0')
	{
		switch(*string)
		{
			case '\a': *out++ = '\\'; *out++ = 'a'; break;
			case '\b': *out++ = '\\'; *out++ = 'b'; break;
			case '\f': *out++ = '\\'; *out++ = 'f'; break;
			case '\n': *out++ = '\\'; *out++ = 'n'; break;
			case '\r': *out++ = '\\'; *out++ = 'r'; break;
			case '\t': *out++ = '\\'; *out++ = 't'; break;
			case '\v': *out++ = '\\'; *out++ = 'v'; break;
			case '"':  *out++ = '\\'; *out++ = '"'; break;

			default:
				*out++ = *string;
				break;
		}
		++string;
	}
	*out = '\0';
	return escaped;
}

void
expand_percent_escaping(char s[])
{
	char *p;

	p = s;
	while(s[0] != '\0')
	{
		if(s[0] == '%' && s[1] == '%')
		{
			++s;
		}
		*p++ = *s++;
	}
	*p = '\0';
}

void
expand_squotes_escaping(char s[])
{
	char *p;
	int sq_found;

	p = s++;
	sq_found = *p == '\'';
	while(*p != '\0')
	{
		if(*s == '\'' && sq_found)
		{
			sq_found = 0;
		}
		else
		{
			*++p = *s;
			sq_found = *s == '\'';
		}
		s++;
	}
}

void
expand_dquotes_escaping(char s[])
{
	static const char table[] =
						/* 00  01  02  03  04  05  06  07  08  09  0a  0b  0c  0d  0e  0f */
	/* 00 */	"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
	/* 10 */	"\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
	/* 20 */	"\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f"
	/* 30 */	"\x00\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f"
	/* 40 */	"\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f"
	/* 50 */	"\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5a\x5b\x5c\x5d\x5e\x5f"
	/* 60 */	"\x60\x07\x0b\x63\x64\x65\x0c\x67\x68\x69\x6a\x6b\x6c\x6d\x0a\x6f"
	/* 70 */	"\x70\x71\x0d\x73\x09\x75\x0b\x77\x78\x79\x7a\x7b\x7c\x7d\x7e\x7f"
	/* 80 */	"\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f"
	/* 90 */	"\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f"
	/* a0 */	"\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf"
	/* b0 */	"\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf"
	/* c0 */	"\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf"
	/* d0 */	"\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf"
	/* e0 */	"\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef"
	/* f0 */	"\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff";

	char *str = s;
	char *p;

	p = s;
	while(*s != '\0')
	{
		if(*s != '\\')
		{
			*p++ = *s++;
			continue;
		}
		s++;
		if(*s == '\0')
		{
			LOG_ERROR_MSG("Escaped eol in \"%s\"", str);
			break;
		}
		*p++ = table[(int)*s++];
	}
	*p = '\0';
}

int
def_reg(int reg)
{
	return (reg == NO_REG_GIVEN) ? DEFAULT_REG_NAME : reg;
}

int
def_count(int count)
{
	return (count == NO_COUNT_GIVEN) ? 1 : count;
}

char *
parse_file_spec(const char spec[], int *line_num)
{
	char *path_buf;
	const char *colon;
	int colon_lookup_offset = 0;
	const size_t bufs_len = 2 + strlen(spec) + 1 + 1;

	path_buf = malloc(bufs_len);
	if(path_buf == NULL)
	{
		return NULL;
	}

	if(is_path_absolute(spec) || spec[0] == '~')
	{
		path_buf[0] = '\0';
	}
	else
	{
		copy_str(path_buf, bufs_len, "./");
	}

#ifdef _WIN32
	if(is_path_absolute(spec))
	{
		colon_lookup_offset = 2;
	}
#endif

	colon = strchr(spec + colon_lookup_offset, ':');
	if(colon != NULL)
	{
		strncat(path_buf, spec, colon - spec);
		*line_num = atoi(colon + 1);
	}
	else
	{
		strcat(path_buf, spec);
		*line_num = 1;
	}

	chomp(path_buf);

#ifdef _WIN32
	to_forward_slash(path_buf);
#endif

	return replace_tilde(path_buf);
}

int
is_graphics_viewer(const char viewer[])
{
	/* %pw and %ph can be useful for text output, but %px and %py are useful
	 * for graphics and basically must have both. */
	return (strstr(viewer, "%px") != NULL && strstr(viewer, "%py") != NULL);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
