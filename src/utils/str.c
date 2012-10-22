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

#include <ctype.h> /* tolower() isspace() */
#include <stdarg.h> /* va_list va_start() va_copy() va_end() */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* malloc() mbstowcs() wcstombs() */
#include <string.h> /* strncmp() strlen() strcmp() strchr() strrchr() */
#include <wchar.h> /* vswprintf() wchar_t */

#include "str.h"

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
starts_with(const char *str, const char *prefix)
{
	size_t prefix_len = strlen(prefix);
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

#ifndef _WIN32
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
	const char *end = until_first(++str, separator);
	snprintf(part_buf, end - str + 1, "%s", str);
	return end;
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
skip_all(const char string[], char ch)
{
	while(*string == ch)
	{
		string++;
	}
	return (char *)string;
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

#ifdef _WIN32
char *
strtok_r(char str[], const char delim[], char *saveptr[])
{
	return strtok(str, delim);
}
#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
