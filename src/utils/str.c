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

#include "str.h"

#include <ctype.h> /* tolower() isspace() */
#include <stdarg.h> /* va_list va_start() va_copy() va_end() */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* free() malloc() mbstowcs() wcstombs() realloc() */
#include <string.h> /* strncmp() strlen() strcmp() strchr() strrchr()
                       strncpy() */
#include <wchar.h> /* vswprintf() wchar_t */
#include <wctype.h> /* towlower() iswupper() */

#include "macros.h"
#include "utf8.h"
#include "utils.h"

#include "macros.h"

void
chomp(char str[])
{
	if(str[0] != '\0')
	{
		const size_t last_char_pos = strlen(str) - 1;
		if(str[last_char_pos] == '\n')
		{
			str[last_char_pos] = '\0';
		}
	}
}

size_t
trim_right(char *text)
{
	size_t len;

	len = strlen(text);
	while(len > 0 && isspace(text[len - 1]))
	{
		text[--len] = '\0';
	}

	return len;
}

wchar_t *
to_wide(const char s[])
{
	wchar_t *result = NULL;
	size_t len;

	len = mbstowcs(NULL, s, 0);
	if(len != (size_t)-1)
	{
		result = malloc((len + 1)*sizeof(wchar_t));
		if(result != NULL)
		{
			(void)mbstowcs(result, s, len + 1);
		}
	}
	return result;
}

/* I'm really worry about the portability... */
wchar_t *
my_wcsdup(const wchar_t *ws)
{
	const size_t len = wcslen(ws) + 1;
	wchar_t * const result = malloc(len*sizeof(wchar_t));
	if(result == NULL)
		return NULL;
	wcsncpy(result, ws, len);
	return result;
}

int
starts_with(const char str[], const char prefix[])
{
	size_t prefix_len = strlen(prefix);
	return starts_withn(str, prefix, prefix_len);
}

int
starts_withn(const char str[], const char prefix[], size_t prefix_len)
{
	return strncmp(str, prefix, prefix_len) == 0;
}

int
ends_with(const char *str, const char *suffix)
{
	size_t str_len = strlen(str);
	size_t suffix_len = strlen(suffix);

	if(str_len < suffix_len)
	{
		return 0;
	}
	return strcmp(suffix, str + str_len - suffix_len) == 0;
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

void
strtolower(char *s)
{
	while(*s != '\0')
	{
		*s = tolower(*s);
		s++;
	}
}

void
wcstolower(wchar_t s[])
{
	while(*s != L'\0')
	{
		*s = towlower(*s);
		s++;
	}
}

void
break_at(char str[], char c)
{
	char *const p = strchr(str, c);
	if(p != NULL)
	{
		*p = '\0';
	}
}

void
break_atr(char str[], char c)
{
	char *const p = strrchr(str, c);
	if(p != NULL)
	{
		*p = '\0';
	}
}

/* Skips consecutive non-whitespace characters. */
char *
skip_non_whitespace(const char *str)
{
	while(!isspace(*str) && *str != '\0')
		str++;
	return (char *)str;
}

char *
skip_whitespace(const char *str)
{
	while(isspace(*str))
		str++;
	return (char *)str;
}

int
char_is_one_of(const char *list, char c)
{
	return c != '\0' && strchr(list, c) != NULL;
}

int
stroscmp(const char *s, const char *t)
{
#ifndef _WIN32
	return strcmp(s, t);
#else
	return strcasecmp(s, t);
#endif
}

int
strnoscmp(const char *s, const char *t, size_t n)
{
#ifndef _WIN32
	return strncmp(s, t, n);
#else
	return strncasecmp(s, t, n);
#endif
}

char *
after_last(const char *str, char c)
{
	char *result = strrchr(str, c);
	result = (result == NULL) ? ((char *)str) : (result + 1);
	return result;
}

char *
until_first(const char str[], char c)
{
	const char *result = strchr(str, c);
	if(result == NULL)
	{
		result = str + strlen(str);
	}
	return (char *)result;
}

int
replace_string(char **str, const char with[])
{
	if(*str != with)
	{
		char *new = strdup(with);
		if(new == NULL)
		{
			return 1;
		}
		free(*str);
		*str = new;
	}
	return 0;
}

char *
strcatch(char *str, char c)
{
	const char buf[2] = { c, '\0' };
	return strcat(str, buf);
}

int
my_swprintf(wchar_t *str, size_t len, const wchar_t *format, ...)
{
	int result;
	va_list ap;

	va_start(ap, format);

#if !defined(_WIN32) || defined(_WIN64)
	result = vswprintf(str, len, format, ap);
#else
	result = vswprintf(str, format, ap);
#endif

	va_end(ap);

	return result;
}

const char *
extract_part(const char str[], char separator, char part_buf[])
{
	const char *end = NULL;
	str = skip_char(str, separator);
	if(str[0] != '\0')
	{
		end = until_first(str, separator);
		snprintf(part_buf, end - str + 1, "%s", str);
		if(*end != '\0')
		{
			end++;
		}
	}
	return end;
}

char *
skip_char(const char str[], char c)
{
	while(*str == c)
	{
		str++;
	}
	return (char *)str;
}

char *
escape_chars(const char string[], const char chars[])
{
	size_t len;
	size_t i;
	char *ret, *dup;

	len = strlen(string);

	dup = ret = malloc(len*2 + 2 + 1);

	for(i = 0; i < len; i++)
	{
		if(string[i] == '\\' || char_is_one_of(chars, string[i]))
		{
			*dup++ = '\\';
		}
		*dup++ = string[i];
	}
	*dup = '\0';
	return ret;
}

int
is_null_or_empty(const char string[])
{
	return string == NULL || string[0] == '\0';
}

char *
format_str(const char format[], ...)
{
	va_list ap;
	va_list aq;
	size_t len;
	char *result_buf;

	va_start(ap, format);
	va_copy(aq, ap);

	len = vsnprintf(NULL, 0, format, ap);
	va_end(ap);

	if((result_buf = malloc(len + 1)) != NULL)
	{
		(void)vsprintf(result_buf, format, aq);
	}
	va_end(aq);

	return result_buf;
}

const char *
expand_tabulation(const char line[], size_t max, size_t tab_stops, char buf[])
{
	size_t col = 0;
	while(col < max && *line != '\0')
	{
		const size_t char_width = get_char_width(line);
		const size_t char_screen_width = wcwidth(get_first_wchar(line));
		if(char_screen_width != (size_t)-1 && col + char_screen_width > max)
		{
			break;
		}

		if(char_width == 1 && *line == '\t')
		{
			const size_t space_count = tab_stops - col%tab_stops;

			memset(buf, ' ', space_count);
			buf += space_count;

			col += space_count;
		}
		else
		{
			strncpy(buf, line, char_width);
			buf += char_width;

			col += char_screen_width;
		}

		line += char_width;
	}
	*buf = '\0';
	return line;
}

wchar_t
get_first_wchar(const char str[])
{
	wchar_t wc[2];
	return (mbstowcs(wc, str, ARRAY_LEN(wc)) >= 1) ? wc[0] : str[0];
}

char *
extend_string(char str[], const char with[], size_t *len)
{
	size_t with_len = strlen(with);
	char *new = realloc(str, *len + with_len + 1);
	if(new == NULL)
	{
		return str;
	}

	strncpy(new + *len, with, with_len + 1);
	*len += with_len;
	return new;
}

int
has_uppercase_letters(const char str[])
{
	int has_uppercase = 0;
	wchar_t *const wstring = to_wide(str);
	const wchar_t *p = wstring - 1;
	while(*++p != L'\0')
	{
		if(iswupper(*p))
		{
			has_uppercase = 1;
			break;
		}
	}
	free(wstring);
	return has_uppercase;
}

void
copy_str(char dst[], size_t dst_len, const char src[])
{
	if(dst_len != 0U)
	{
		if(memccpy(dst, src, '\0', dst_len) == NULL)
		{
			dst[dst_len - 1] = '\0';
		}
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
