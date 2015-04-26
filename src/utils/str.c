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
#include <limits.h> /* INT_MAX INT_MIN LONG_MAX LONG_MIN */
#include <stdarg.h> /* va_list va_start() va_copy() va_end() */
#include <stddef.h> /* NULL size_t wchar_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* free() malloc() mbstowcs() memmove() memset() realloc()
                       strtol() wcstombs() */
#include <string.h> /* strdup() strncmp() strlen() strcmp() strchr() strrchr()
                       strncpy() */
#include <wchar.h> /* vswprintf() */
#include <wctype.h> /* towlower() iswupper() */

#include "macros.h"
#include "utf8.h"
#include "utils.h"

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
#ifndef _WIN32
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
#else
	return utf8_to_utf16(s);
#endif
}

size_t
wide_len(const char s[])
{
#ifndef _WIN32
	return mbstowcs(NULL, s, 0);
#else
	return utf8_widen_len(s);
#endif
}

wchar_t *
vifm_wcsdup(const wchar_t ws[])
{
	const size_t len = wcslen(ws) + 1;
	wchar_t * const result = malloc(len*sizeof(wchar_t));
	if(result == NULL)
	{
		return NULL;
	}
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

int
surrounded_with(const char str[], char left, char right)
{
	const size_t len = strlen(str);
	return len > 2 && str[0] == left && str[len - 1] == right;
}

char *
to_multibyte(const wchar_t *s)
{
#ifndef _WIN32
	size_t len;
	char *result;

	len = wcstombs(NULL, s, 0) + 1;
	if((result = malloc(len*sizeof(char))) == NULL)
		return NULL;

	wcstombs(result, s, len);
	return result;
#else
	return utf8_from_utf16(s);
#endif
}

size_t
multibyte_len(const wchar_t wide[])
{
#ifndef _WIN32
	return wcstombs(NULL, wide, 0);
#else
	return utf8_narrowd_len(wide);
#endif
}

void
str_to_lower(char str[])
{
	/* FIXME: handle UTF-8 properly. */
	while(*str != '\0')
	{
		*str = tolower(*str);
		++str;
	}
}

void
str_to_upper(char str[])
{
	/* FIXME: handle UTF-8 properly. */
	while(*str != '\0')
	{
		*str = toupper(*str);
		++str;
	}
}

void
wcstolower(wchar_t s[])
{
	while(*s != L'\0')
	{
		*s = towlower(*s);
		++s;
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

char *
skip_whitespace(const char str[])
{
	while(isspace(*str))
	{
		++str;
	}
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

char *
after_first(const char str[], char c)
{
	const char *result = strchr(str, c);
	result = (result == NULL) ? (str + strlen(str)) : (result + 1);
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
strcatch(char str[], char c)
{
	const char buf[2] = { c, '\0' };
	return strcat(str, buf);
}

int
strprepend(char **str, size_t *len, const char prefix[])
{
	const size_t prefix_len = strlen(prefix);
	char *const new = realloc(*str, prefix_len + *len + 1);
	if(new == NULL)
	{
		return 1;
	}

	memmove(new + prefix_len, new, *len + 1);
	strncpy(new + *len, prefix, prefix_len);
	*str = new;
	*len += prefix_len;

	return 0;
}

int
strappendch(char **str, size_t *len, char c)
{
	const char suffix[] = {c, '\0'};
	return strappend(str, len, suffix);
}

int
strappend(char **str, size_t *len, const char suffix[])
{
	const size_t suffix_len = strlen(suffix);
	char *const new = realloc(*str, *len + suffix_len + 1);
	if(new == NULL)
	{
		return 1;
	}

	strcpy(new + *len, suffix);
	*str = new;
	*len += suffix_len;

	return 0;
}

int
sstrappendch(char str[], size_t *len, size_t size, char c)
{
	const char suffix[] = {c, '\0'};
	return sstrappend(str, len, size, suffix);
}

int
sstrappend(char str[], size_t *len, size_t size, const char suffix[])
{
	const size_t free_space = size - *len;
	const size_t suffix_len = snprintf(str + *len, free_space, "%s", suffix);
	*len += strlen(str + *len);
	return suffix_len > free_space - 1;
}

void
stralign(char str[], size_t width, char pad, int left_align)
{
	const size_t len = strlen(str);
	const int pad_width = width - len;

	if(pad_width <= 0)
	{
		return;
	}

	if(left_align)
	{
		memset(str + len, pad, pad_width);
		str[width] = '\0';
	}
	else
	{
		memmove(str + pad_width, str, len + 1);
		memset(str, pad, pad_width);
	}
}

char *
left_ellipsis(char str[], size_t max_width)
{
	size_t width;
	size_t len;
	const char *tail;

	if(max_width == 0U)
	{
		return NULL;
	}

	width = get_screen_string_length(str);
	if(width <= max_width)
	{
		return str;
	}

	len = strlen(str);
	if(max_width <= 3U)
	{
		copy_str(str, len + 1U, "...");
		str[max_width] = '\0';
		return str;
	}

	tail = str;
	while(width > max_width - 3U)
	{
		width -= utf8_get_screen_width_of_char(tail);
		tail += get_char_width(tail);
	}
	strcpy(str, "...");
	memmove(str + 3U, tail, len - (tail - str) + 1U);

	return str;
}

char *
right_ellipsis(char str[], size_t max_width)
{
	size_t width;
	size_t prefix;

	if(max_width == 0U)
	{
		return NULL;
	}

	width = get_screen_string_length(str);
	if(width <= max_width)
	{
		return str;
	}

	if(max_width <= 3U)
	{
		copy_str(str, strlen(str) + 1U, "...");
		str[max_width] = '\0';
		return str;
	}

	prefix = get_normal_utf8_string_widthn(str, max_width - 3U);
	strcpy(&str[prefix], "...");
	return str;
}

char *
break_in_two(char str[], size_t max)
{
	int i;
	size_t len, size;
	char *result;
	char *break_point = strstr(str, "%=");
	if(break_point == NULL)
		return str;

	len = get_screen_string_length(str) - 2;
	size = strlen(str);
	size = MAX(size, max);
	result = malloc(size*4 + 2);

	snprintf(result, break_point - str + 1, "%s", str);

	if(len > max)
	{
		const int l = get_screen_string_length(result) - (len - max);
		break_point = str + get_real_string_width(str, MAX(l, 0));
	}

	snprintf(result, break_point - str + 1, "%s", str);
	i = break_point - str;
	while(max > len)
	{
		result[i++] = ' ';
		max--;
	}
	result[i] = '\0';

	if(len > max)
		break_point = strstr(str, "%=");
	strcat(result, break_point + 2);

	free(str);
	return result;
}

int
vifm_swprintf(wchar_t str[], size_t len, const wchar_t format[], ...)
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

			col += (char_screen_width == (size_t)-1) ? 1 : char_screen_width;
		}

		line += char_width;
	}
	*buf = '\0';
	return line;
}

wchar_t
get_first_wchar(const char str[])
{
	wchar_t wc[2] = {};
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
	if(wstring != NULL)
	{
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
	}
	return has_uppercase;
}

size_t
copy_str(char dst[], size_t dst_len, const char src[])
{
	/* XXX: shouldn't we return "strlen(src)" instead "0U"? */
	return (dst == src) ? 0U : copy_substr(dst, dst_len, src, '\0');
}

size_t
copy_substr(char dst[], size_t dst_len, const char src[], char terminator)
{
	char *past_end;

	if(dst_len == 0U)
	{
		return 0U;
	}

	past_end = memccpy(dst, src, terminator, dst_len);
	if(past_end == NULL)
	{
		dst[dst_len - 1] = '\0';
		return dst_len;
	}
	else
	{
		past_end[-1] = '\0';
		return past_end - dst;
	}
}

int
str_to_int(const char str[])
{
	const long number = strtol(str, NULL, 10);
	/* Handle overflow and underflow correctly. */
	return (number == LONG_MAX)
	     ? INT_MAX
	     : ((number == LONG_MIN) ? INT_MIN : number);
}

void
replace_char(char str[], char from, char to)
{
	while(*str != '\0')
	{
		if(*str == from)
		{
			*str = to;
		}
		++str;
	}
}

int
is_in_str_list(const char list[], char separator, const char needle[])
{
	char *list_copy = strdup(list);
	char *item = list_copy, *state = NULL;
	while((item = split_and_get(item, separator, &state)) != NULL)
	{
		if(strcasecmp(item, needle) == 0)
		{
			break;
		}
	}
	free(list_copy);

	return (item != NULL);
}

char *
split_and_get(char str[], char sep, char **state)
{
	char *end;

	if(*state != NULL)
	{
		if(**state == '\0')
		{
			return NULL;
		}

		str += strlen(str);
		*str++ = sep;
	}

	end = strchr(str, sep);
	while(end != NULL)
	{
		if(end != str)
		{
			*end = '\0';
			break;
		}

		str = end + 1;
		end = strchr(str, sep);
	}

	*state = (end == NULL) ? (str + strlen(str)) : (end + 1);
	return (*str == '\0') ? NULL : str;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
