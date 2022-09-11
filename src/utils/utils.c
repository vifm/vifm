/* vifm
 * Copyright (C) 2001 Ken Steen.
 * Copyright (C) 2007-05-26 Markus Kuhn.
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

#include <sys/types.h> /* pid_t */
#include <unistd.h>

#include <ctype.h> /* isalnum() isalpha() iscntrl() */
#include <errno.h> /* errno */
#include <math.h> /* modf() pow() */
#include <stddef.h> /* size_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* free() malloc() qsort() */
#include <string.h> /* memcpy() strdup() strchr() strlen() strpbrk() strtol() */
#include <time.h> /* tm localtime() strftime() */
#include <wchar.h> /* wcwidth() */

#include "../cfg/config.h"
#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "../compat/reallocarray.h"
#include "../engine/keys.h"
#include "../engine/variables.h"
#include "../int/fuse.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../ui/cancellation.h"
#include "../ui/ui.h"
#include "../background.h"
#include "../filelist.h"
#include "../registers.h"
#include "../status.h"
#include "env.h"
#include "file_streams.h"
#include "fs.h"
#include "log.h"
#include "macros.h"
#include "path.h"
#include "str.h"
#include "string_array.h"
#include "utf8.h"

static void show_progress_cb(const void *descr);
static const char ** get_size_suffixes(void);
static double split_size_double(double d, unsigned long long *ifraction,
		int *fraction_width);
#ifdef _WIN32
static void unquote(char quoted[]);
#endif
static int is_line_spec(const char str[]);

/* Size suffixes of different kinds. */
static const char *iec_i_units[] = {
	"  B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB"
};
static const char *iec_units[] = {
	"B", "K", "M", "G", "T", "P", "E", "Z", "Y"
};
static const char *si_units[] = {
	" B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"
};
ARRAY_GUARD(iec_units, ARRAY_LEN(iec_i_units));
ARRAY_GUARD(si_units, ARRAY_LEN(iec_units));

int
vifm_system(char command[], ShellRequester by)
{
#ifdef _WIN32
	/* The check is primarily for tests, otherwise screen is reset. */
	if(curr_stats.load_stage != 0)
	{
		system("cls");
	}
#endif
	LOG_INFO_MSG("Shell command: %s", command);
	return run_in_shell_no_cls(command, by);
}

int
vifm_system_input(char command[], FILE *input, ShellRequester by)
{
#ifdef _WIN32
	/* The check is primarily for tests, otherwise screen is reset. */
	if(curr_stats.load_stage != 0)
	{
		system("cls");
	}
#endif
	LOG_INFO_MSG("Shell command with custom input: %s", command);
	return run_with_input(command, input, by);
}

int
process_cmd_output(const char descr[], const char cmd[], FILE *input,
		int user_sh, int interactive, cmd_output_handler handler, void *arg)
{
	FILE *file, *err;
	pid_t pid;
	int nlines;
	char **lines;
	int i;

	LOG_INFO_MSG("Capturing output of the command: %s", cmd);

	pid = bg_run_and_capture((char *)cmd, user_sh, input, &file, &err);
	if(pid == (pid_t)-1)
	{
		return 1;
	}

	ui_cancellation_push_on();

	if(!interactive)
	{
		show_progress("", 0);
	}

	wait_for_data_from(pid, file, 0, &ui_cancellation_info);
	lines = read_stream_lines(file, &nlines, 1,
			interactive ? NULL : &show_progress_cb, descr);

	ui_cancellation_pop();
	fclose(file);

	for(i = 0; i < nlines; ++i)
	{
		handler(lines[i], arg);
	}
	free_string_array(lines, nlines);

	show_errors_from_file(err, descr);
	return 0;
}

/* Callback implementation for read_stream_lines(). */
static void
show_progress_cb(const void *descr)
{
	show_progress(descr, -250);
}

int
vifm_chdir(const char path[])
{
	char curr_path[PATH_MAX + 1];
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
	char *const expanded_envvars = expand_envvars(path, EEF_NONE);
	/* replace_tilde() frees memory returned by expand_envvars. */
	return replace_tilde(expanded_envvars);
}

char *
expand_envvars(const char str[], int flags)
{
	const int escape_vals = (flags & EEF_ESCAPE_VALS);
	const int keep_escapes = (flags & EEF_KEEP_ESCAPES);
	const int double_percents = (flags & EEF_DOUBLE_PERCENTS);

	char *result = NULL;
	size_t len = 0;
	int prev_slash = 0;
	while(*str != '\0')
	{
		if(!prev_slash && *str == '$' && isalpha(str[1]))
		{
			char var_name[NAME_MAX + 1];
			const char *p = str + 1;
			char *q = var_name;
			const char *var_value;

			while((isalnum(*p) || *p == '_') &&
					(size_t)(q - var_name) < sizeof(var_name) - 1)
			{
				*q++ = *p++;
			}
			*q = '\0';

			var_value = local_getenv(var_name);
			if(!is_null_or_empty(var_value))
			{
				char *escaped_var_value = NULL;
				char *doubled_percents_var_value = NULL;

				if(escape_vals)
				{
					escaped_var_value = shell_like_escape(var_value, 2);
					var_value = escaped_var_value;
				}

				if(double_percents)
				{
					doubled_percents_var_value = double_char(var_value, '%');
					var_value = doubled_percents_var_value;
				}

				result = extend_string(result, var_value, &len);
				free(escaped_var_value);
				free(doubled_percents_var_value);

				str = p;
			}
			else
			{
				result = extend_string(result, "$", &len);
				str++;
			}
		}
		else
		{
			prev_slash = (*str == '\\') ? !prev_slash : 0;

			if(!prev_slash || (keep_escapes && (str[1] != '$' || !isalpha(str[2]))))
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
friendly_size_notation(uint64_t num, int str_size, char str[])
{
	unsigned long long fraction = 0ULL;
	int fraction_width;
	const char **const units = get_size_suffixes();
	size_t u = 0U;
	double d = num;

	while(d >= cfg.sizefmt.base - 0.5 && u < ARRAY_LEN(iec_i_units) - 1U)
	{
		d /= cfg.sizefmt.base;
		++u;
	}

	/* Fractional part is ignored when it's absent (we didn't divide anything) and
	 * when we format size in previously used manner and hide it for values
	 * greater than 9. */
	if(u != 0U && !(cfg.sizefmt.precision == 0 && d > 9.0))
	{
		d = split_size_double(d, &fraction, &fraction_width);
	}

	if(fraction == 0)
	{
		snprintf(str, str_size, "%.0f%s%s", d, cfg.sizefmt.space ? " " : "",
				units[u]);
	}
	else
	{
		snprintf(str, str_size, "%.0f.%0*" PRINTF_ULL "%s%s", d, fraction_width,
				fraction, cfg.sizefmt.space ? " " : "", units[u]);
	}

	return u > 0U;
}

/* Picks size suffixes as per configuration.  Returns one of *_units arrays. */
static const char **
get_size_suffixes(void)
{
	return (cfg.sizefmt.base == 1000) ? si_units :
	       (cfg.sizefmt.ieci_prefixes ? iec_i_units : iec_units);
}

/* Breaks size (floating point, not integer) into integer and fractional parts
 * rounding and truncating them as necessary.  Sets *ifraction and
 * *fraction_width.  Returns new value for the size (truncated and/or
 * rounded). */
static double
split_size_double(double size, unsigned long long *ifraction,
		int *fraction_width)
{
	double ten_power, dfraction, integer;

	*fraction_width = (cfg.sizefmt.precision == 0) ? 1 : cfg.sizefmt.precision;

	ten_power = pow(10, *fraction_width + 1);
	while(ten_power > (double)ULLONG_MAX)
	{
		ten_power /= 10;
		--*fraction_width;
	}

	dfraction = modf(size, &integer);
	*ifraction = DIV_ROUND_UP(ten_power*dfraction, 10);

	if(*ifraction >= ten_power/10.0)
	{
		/* Overflow into integer part during rounding. */
		integer += 1.0;
		*ifraction -= ten_power/10.0;
	}
	else if(*ifraction == 0 && dfraction != 0.0)
	{
		/* Fractional part is too small, "round up" to make it visible. */
		*ifraction = 1;
	}

	/* Skip trailing zeroes. */
	for(; *fraction_width != 0 && *ifraction%10 == 0; --*fraction_width)
	{
		*ifraction /= 10;
	}

	return (*ifraction == 0) ? size : integer;
}

const char *
enclose_in_dquotes(const char str[], ShellType shell_type)
{
	static char buf[1 + PATH_MAX*2 + 1 + 1];
	char *p;

	p = buf;
	*p++ = '"';
	for(; *str != '\0'; ++str)
	{
		char c = *str;

		if(c == '"' ||
				(shell_type == ST_NORMAL && (c == '\\' || c == '$' || c == '`')))
		{
			*p++ = '\\';
		}

		*p++ = c;
	}
	*p++ = '"';
	*p = '\0';
	return buf;
}

const char *
make_name_unique(const char filename[])
{
	static char unique[PATH_MAX + 1];
	size_t len;
	int i;

#ifndef _WIN32
	snprintf(unique, sizeof(unique), "%s_%u%u_00", filename, getppid(),
			get_pid());
#else
	/* TODO: think about better name uniqualization on Windows. */
	snprintf(unique, sizeof(unique), "%s_%u_00", filename, get_pid());
#endif
	len = strlen(unique);
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

	size_t len = strlen(string);

	char *escaped = malloc(len*2 + 1);
	char *out = escaped;
	if(escaped == NULL)
	{
		return NULL;
	}

	/* Copy prefix not escaping it. */
	offset = MIN(len, offset);
	memcpy(out, string, offset);
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

	size_t len = strlen(string);

	char *escaped = malloc(len*2 + 1);
	char *out = escaped;
	if(escaped == NULL)
	{
		return NULL;
	}

	/* Copy prefix not escaping it. */
	offset = MIN(len, offset);
	memcpy(out, string, offset);
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

char *
escape_unreadable(const char str[])
{
	int str_len = strlen(str);

	char *escaped = malloc(str_len*2 + 1);
	if(escaped == NULL)
	{
		return NULL;
	}

	char *out = escaped;
	while(str_len > 0)
	{
		int char_len;

		wchar_t ucs = utf8_first_char(str, &char_len);
		if(unichar_isprint(ucs))
		{
			memcpy(out, str, char_len);
			out += char_len;
		}
		else
		{
			*out++ = '^';
			if(char_len == 1 && iscntrl(*str))
			{
				*out++ = *str ^ 64;
			}
			else
			{
				*out++ = '?';
			}
		}

		str += char_len;
		str_len -= char_len;
	}
	*out = '\0';

	return escaped;
}

int
escape_unreadableo(const char str[], int prefix_len)
{
	int overhead = 0;
	const char *in = str;
	while(prefix_len > 0)
	{
		int char_len;

		wchar_t ucs = utf8_first_char(in, &char_len);
		if(!unichar_isprint(ucs))
		{
			overhead += 2 - char_len;
		}

		in += char_len;
		prefix_len -= char_len;
	}
	return overhead;
}

wchar_t *
escape_unreadablew(const wchar_t str[])
{
	int str_len = wcslen(str);
	wchar_t *escaped = reallocarray(NULL, str_len*2 + 1, sizeof(*escaped));

	wchar_t *out = escaped;
	while(str_len > 0)
	{
		if(unichar_isprint(*str))
		{
			*out++ = *str;
		}
		else
		{
			*out++ = L'^';
			if(*str <= (wchar_t)0xff && iscntrl(*str))
			{
				*out++ = *str ^ 64;
			}
			else
			{
				*out++ = L'?';
			}
		}

		++str;
		--str_len;
	}
	*out = '\0';

	return escaped;
}

int
unichar_isprint(wchar_t ucs)
{
	/* Sorted list of non-overlapping intervals of non-spacing characters
	 * generated by "uniset +cat=Cc +cat=Cf c". */
	static const interval_t non_printing[] = {
		{ 0x0000, 0x001F }, { 0x007F, 0x009F }, { 0x00AD, 0x00AD },
		{ 0x0600, 0x0605 }, { 0x061C, 0x061C }, { 0x06DD, 0x06DD },
		{ 0x070F, 0x070F }, { 0x08E2, 0x08E2 }, { 0x180E, 0x180E },
		{ 0x200B, 0x200F }, { 0x202A, 0x202E }, { 0x2060, 0x2064 },
		{ 0x2066, 0x206F }, { 0xFEFF, 0xFEFF }, { 0xFFF9, 0xFFFB },
		{ 0x110BD, 0x110BD }, { 0x1BCA0, 0x1BCA3 }, { 0x1D173, 0x1D17A },
		{ 0xE0001, 0xE0001 }, { 0xE0020, 0xE007F }
	};

	return !unichar_bisearch(ucs, non_printing, ARRAY_LEN(non_printing) - 1);
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
parse_line_for_path(const char line[], const char cwd[])
{
	int line_num;
	/* Skip empty lines. */
	return (skip_whitespace(line)[0] == '\0')
	     ? NULL
	     : parse_file_spec(line, &line_num, cwd);
}

char *
parse_file_spec(const char spec[], int *line_num, const char cwd[])
{
#ifdef _WIN32
	enum { MIN_COLON_OFFSET = 2 };
#else
	enum { MIN_COLON_OFFSET = 0 };
#endif

	char *path_buf;
	const char *colon;
	const size_t bufs_len = strlen(cwd) + 1U + strlen(spec) + 1U + 1U;
	char canonicalized[PATH_MAX + 1];

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
		snprintf(path_buf, bufs_len, "%s/", cwd);
	}

#ifdef _WIN32
	colon = strchr(spec + (is_path_absolute(spec) ? MIN_COLON_OFFSET : 0), ':');
	if(colon != NULL && !is_line_spec(colon + 1))
	{
		colon = NULL;
	}
#else
	colon = strchr(spec, ':');
	while(colon != NULL)
	{
		if(is_line_spec(colon + 1))
		{
			char path[bufs_len];
			strcpy(path, path_buf);
			strncat(path, spec, colon - spec);
			if(path_exists(path, NODEREF))
			{
				break;
			}
		}

		colon = strchr(colon + 1, ':');
	}
#endif

	if(colon != NULL)
	{
		strncat(path_buf, spec, colon - spec);
		*line_num = atoi(colon + 1);
	}
	else
	{
		strcat(path_buf, spec);
		*line_num = 1;

		while(!path_exists(path_buf, NODEREF) &&
				strchr(path_buf + MIN_COLON_OFFSET, ':') != NULL)
		{
			break_atr(path_buf, ':');
		}
	}

	chomp(path_buf);
	system_to_internal_slashes(path_buf);

	canonicalize_path(path_buf, canonicalized, sizeof(canonicalized));

	if(!ends_with_slash(path_buf) && !is_root_dir(canonicalized) &&
			strcmp(canonicalized, "./") != 0)
	{
		chosp(canonicalized);
	}

	free(path_buf);

	return replace_tilde(strdup(canonicalized));
}

/* Checks whether str points to a valid line number.  Returns non-zero if so,
 * otherwise zero is returned. */
static int
is_line_spec(const char str[])
{
	char *endptr;
	errno = 0;
	(void)strtol(str, &endptr, 10);
	return (endptr != str && errno == 0 && *endptr == ':');
}

void
safe_qsort(void *base, size_t nmemb, size_t size,
		int (*compar)(const void *, const void *))
{
	if(nmemb != 0U)
	{
		/* Even when there are no entries, qsort() shouldn't be called with a NULL
		 * parameter.  It isn't allowed by the standard. */
		qsort(base, nmemb, size, compar);
	}
}

void
format_position(char buf[], size_t buf_len, int top, int total, int visible)
{
	if(total <= visible)
	{
		copy_str(buf, buf_len, "All");
	}
	else if(top == 0)
	{
		copy_str(buf, buf_len, "Top");
	}
	else if(total - top <= visible)
	{
		copy_str(buf, buf_len, "Bot");
	}
	else
	{
		snprintf(buf, buf_len, "%2d%%", (top*100)/(total - visible));
	}
}

FILE *
make_in_file(view_t *view, MacroFlags flags)
{
	if(ma_flags_missing(flags, MF_PIPE_FILE_LIST) &&
			ma_flags_missing(flags, MF_PIPE_FILE_LIST_Z))
	{
		return NULL;
	}

	FILE *input_tmp = os_tmpfile();
	const int null_sep = ma_flags_present(flags, MF_PIPE_FILE_LIST_Z);
	write_marked_paths(input_tmp, view, null_sep);
	return input_tmp;
}

void
write_marked_paths(FILE *file, view_t *view, int null_sep)
{
	const char separator = (null_sep ? '\0' : '\n');
	dir_entry_t *entry = NULL;
	while(iter_marked_entries(view, &entry))
	{
		const char *const sep = (ends_with_slash(entry->origin) ? "" : "/");
		fprintf(file, "%s%s%s%c", entry->origin, sep, entry->name, separator);
	}
}

void
format_iso_time(time_t t, char buf[], size_t buf_size)
{
	struct tm *const tm = localtime(&t);
	if(tm == NULL)
	{
		copy_str(buf, buf_size, "");
		return;
	}

	strftime(buf, buf_size, "%a, %d %b %Y %H:%M:%S", tm);
}

int
unichar_bisearch(wchar_t ucs, const interval_t table[], int max)
{
	if(ucs < table[0].first || ucs > table[max].last)
	{
		return 0;
	}

	int min = 0;
	while(min <= max)
	{
		const int mid = min + (max - min)/2;
		if(ucs > table[mid].last)
		{
			min = mid + 1;
		}
		else if(ucs < table[mid].first)
		{
			max = mid - 1;
		}
		else
		{
			return 1;
		}
	}

	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
