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
#include <string.h> /* memcpy() strdup() strncmp() strlen() strcmp() strchr()
                       strrchr() strncpy() strcspn() strspn() */
#include <wchar.h> /* wint_t vswprintf() */
#include <wctype.h> /* iswprint() iswupper() towlower() towupper() */

#include "../compat/reallocarray.h"
#include "macros.h"
#include "test_helpers.h"
#include "utf8.h"
#include "utils.h"

static int transform_ascii_str(const char str[], int (*f)(int), char buf[],
		size_t buf_len);
static int transform_wide_str(const char str[], wint_t (*f)(wint_t), char buf[],
		size_t buf_len);
TSTATIC void squash_double_commas(char str[]);
static char * ellipsis(const char str[], size_t max_width, const char ell[],
		int right);
static size_t copy_substr(char dst[], size_t dst_len, const char src[],
		char terminator);

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

wchar_t *
to_wide(const char s[])
{
#ifndef _WIN32
	wchar_t *result = NULL;
	size_t len;

	len = mbstowcs(NULL, s, 0);
	if(len != (size_t)-1)
	{
		result = reallocarray(NULL, len + 1, sizeof(wchar_t));
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

wchar_t *
to_wide_force(const char s[])
{
	wchar_t *w = to_wide(s);
	wchar_t *p;

	if(w != NULL)
	{
		return w;
	}

	w = reallocarray(NULL, strlen(s) + 1U, sizeof(*w));
	if(w == NULL)
	{
		return NULL;
	}

	/* There must be broken multi-byte sequence, do our best to convert string to
	 * something meaningful rather than just failing. */

	p = w;
	while(*s != '\0')
	{
		const wchar_t wc = get_first_wchar(s++);
		if(iswprint(wc))
		{
			*p++ = wc;
		}
	}
	*p = L'\0';

	return w;
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
	wchar_t * const result = reallocarray(NULL, len, sizeof(wchar_t));
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
	const size_t prefix_len = strlen(prefix);
	return starts_withn(str, prefix, prefix_len);
}

int
skip_prefix(const char **str, const char prefix[])
{
	const size_t prefix_len = strlen(prefix);
	if(starts_withn(*str, prefix, prefix_len))
	{
		*str += prefix_len;
		return 1;
	}
	return 0;
}

int
cut_suffix(char str[], const char suffix[])
{
	if(ends_with(str, suffix))
	{
		str[strlen(str) - strlen(suffix)] = '\0';
		return 1;
	}
	return 0;
}

int
starts_withn(const char str[], const char prefix[], size_t prefix_len)
{
	return strncmp(str, prefix, prefix_len) == 0;
}

int
ends_with(const char str[], const char suffix[])
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
ends_with_case(const char str[], const char suffix[])
{
	size_t str_len = strlen(str);
	size_t suffix_len = strlen(suffix);
	if(str_len < suffix_len)
	{
		return 0;
	}

	return (strcasecmp(suffix, str + str_len - suffix_len) == 0);
}

int
surrounded_with(const char str[], char left, char right)
{
	const size_t len = strlen(str);
	return len > 2 && str[0] == left && str[len - 1] == right;
}

char *
to_multibyte(const wchar_t s[])
{
#ifndef _WIN32
	const size_t len = wcstombs(NULL, s, 0) + 1;
	if(len == 0U)
	{
		return NULL;
	}

	char *const result = malloc(len);
	if(result == NULL)
	{
		return NULL;
	}

	wcstombs(result, s, len);
	return result;
#else
	return utf8_from_utf16(s);
#endif
}

int
str_to_lower(const char str[], char buf[], size_t buf_len)
{
	if(utf8_stro(str) == 0U)
	{
		return transform_ascii_str(str, &tolower, buf, buf_len);
	}
	else
	{
		return transform_wide_str(str, &towlower, buf, buf_len);
	}
}

int
str_to_upper(const char str[], char buf[], size_t buf_len)
{
	if(utf8_stro(str) == 0U)
	{
		return transform_ascii_str(str, &toupper, buf, buf_len);
	}
	else
	{
		return transform_wide_str(str, &towupper, buf, buf_len);
	}
}

/* Transforms characters of the string to while they fit in the buffer by
 * calling specified function on them.  Returns zero on success or non-zero if
 * output buffer is too small. */
static int
transform_ascii_str(const char str[], int (*f)(int), char buf[], size_t buf_len)
{
	int too_small;

	if(buf_len == 0U)
	{
		return 1;
	}

	while(*str != '\0' && buf_len > 1U)
	{
		*buf++ = f(*str++);
		--buf_len;
	}
	/* Check comes before the assignment to allow buf == str. */
	too_small = (*str != '\0');
	*buf = '\0';
	return too_small;
}

/* Transforms characters of the string to while they fit in the buffer by
 * calling specified function on them.  Returns zero on success or non-zero if
 * output buffer is too small. */
static int
transform_wide_str(const char str[], wint_t (*f)(wint_t), char buf[],
		size_t buf_len)
{
	size_t copied;
	int error;
	wchar_t *wstring;
	wchar_t *p;
	char *narrow;

	wstring = to_wide(str);
	if(wstring == NULL)
	{
		(void)utf8_strcpy(buf, str, buf_len);
		return 1;
	}

	p = wstring;
	while(*p != L'\0')
	{
		*p = f(*p);
		++p;
	}

	narrow = to_multibyte(wstring);
	copied = utf8_strcpy(buf, narrow, buf_len);
	error = copied == 0U || narrow[copied - 1U] != '\0';

	free(wstring);
	free(narrow);

	return error;
}

void
wcstolower(wchar_t str[])
{
	while(*str != L'\0')
	{
		*str = towlower(*str);
		++str;
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

int
strossorter(const void *s, const void *t)
{
	return stroscmp(*(const char **)s, *(const char **)t);
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

int
update_string(char **str, const char to[])
{
	if(to == NULL)
	{
		free(*str);
		*str = NULL;
		return 0;
	}
	return replace_string(str, to);
}

int
put_string(char **str, char with[])
{
	if(with == NULL)
	{
		return 1;
	}

	free(*str);
	*str = with;
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

	if(*len == 0)
	{
		new[prefix_len] = '\0';
	}
	else
	{
		memmove(new + prefix_len, new, *len + 1);
	}
	memcpy(new, prefix, prefix_len);
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
left_ellipsis(const char str[], size_t max_width, const char ell[])
{
	return ellipsis(str, max_width, ell, 0);
}

char *
right_ellipsis(const char str[], size_t max_width, const char ell[])
{
	return ellipsis(str, max_width, ell, 1);
}

/* Ensures that str is of width (in character positions) less than or equal to
 * max_width and is aligned appropriately putting ellipsis on one of the ends if
 * needed.  Returns newly allocated modified string. */
static char *
ellipsis(const char str[], size_t max_width, const char ell[], int right)
{
	if(max_width == 0U)
	{
		/* No room to print anything. */
		return strdup("");
	}

	size_t width = utf8_strsw(str);
	if(width <= max_width)
	{
		/* No need to change the string. */
		return strdup(str);
	}

	size_t ell_width = utf8_strsw(ell);
	if(max_width <= ell_width)
	{
		/* Insert as many characters of ellipsis as we can. */
		const int prefix = (int)utf8_nstrsnlen(ell, max_width);
		return format_str("%.*s", prefix, ell);
	}

	if(right)
	{
		const int prefix = utf8_nstrsnlen(str, max_width - ell_width);
		return format_str("%.*s%s", prefix, str, ell);
	}

	while(width > max_width - ell_width)
	{
		width -= utf8_chrsw(str);
		str += utf8_chrw(str);
	}

	return format_str("%s%s", ell, str);
}

char *
break_in_two(char str[], size_t max, const char separator[])
{
	char *break_point = strstr(str, separator);
	if(break_point == NULL)
	{
		return str;
	}

	const size_t separator_len = strlen(separator);
	const size_t len = utf8_strsw(str) - separator_len;
	const size_t size = MAX(strlen(str), max);

	char *result = malloc(size*4 + 2);
	if(result == NULL)
	{
		return NULL;
	}

	copy_str(result, break_point - str + 1, str);

	if(len > max)
	{
		const int l = utf8_strsw(result) - (len - max);
		break_point = str + utf8_strsnlen(str, MAX(l, 0));
	}

	copy_str(result, break_point - str + 1, str);
	int i = break_point - str;
	while(max > len)
	{
		result[i++] = ' ';
		max--;
	}
	result[i] = '\0';

	if(len > max)
		break_point = strstr(str, separator);
	strcat(result, break_point + separator_len);

	free(str);
	return result;
}

int
vifm_swprintf(wchar_t str[], size_t len, const wchar_t format[], ...)
{
	int result;
	va_list ap;

	va_start(ap, format);

#ifdef BROKEN_SWPRINTF
	result = vswprintf(str, format, ap);
#else
	result = vswprintf(str, len, format, ap);
#endif

	va_end(ap);

	return result;
}

const char *
extract_part(const char str[], const char separators[], char part_buf[])
{
	const char *end = NULL;
	str += strspn(str, separators);
	if(str[0] != '\0')
	{
		end = str + strcspn(str, separators);
		copy_str(part_buf, end - str + 1, str);
		if(*end != '\0')
		{
			++end;
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
	if(dup == NULL)
	{
		return NULL;
	}

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

void
unescape(char s[], int regexp)
{
	char *p;

	p = s;
	while(s[0] != '\0')
	{
		if(s[0] == '\\' && (!regexp || s[1] == '/'))
		{
			++s;
		}
		*p++ = s[0];
		if(s[0] != '\0')
		{
			++s;
		}
	}
	*p = '\0';
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
		const size_t char_width = utf8_chrw(line);
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
#ifndef _WIN32
	wchar_t wc[2] = {};
	return (mbstowcs(wc, str, ARRAY_LEN(wc)) == (size_t)-1)
	     ? (unsigned char)str[0]
	     : wc[0];
#else
	int len;
	return utf8_first_char(str, &len);
#endif
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
	/* TODO: rewrite this without call to to_wide(), use utf8_char_to_wchar(). */
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
	/* XXX: shouldn't we return "strlen(src)" instead of "0U"? */
	return (dst == src) ? 0U : copy_substr(dst, dst_len, src, '\0');
}

/* Copies characters from the string pointed to by src and terminated by the
 * terminator to piece of memory of size dst_len pointed to by dst.  Ensures
 * that copied string ends with null character.  Does nothing for zero
 * dst_len.  Returns number of characters written, including terminating null
 * character. */
static size_t
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

int
read_int(const char line[], int *i)
{
	char *endptr;
	const long l = strtol(line, &endptr, 10);

	*i = (l > INT_MAX) ? INT_MAX : ((l < INT_MIN) ? INT_MIN : l);

	return *line != '\0' && *endptr == '\0';
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

char *
split_and_get_dc(char str[], char **state)
{
	if(*state == NULL)
	{
		/* Do nothing on empty input. */
		if(str[0] == '\0')
		{
			return NULL;
		}
	}
	else
	{
		/* Check if we reached end of input. */
		if(**state == '\0')
		{
			return NULL;
		}

		/* Process next item of the list. */
		str = *state;
	}

	while(str != NULL)
	{
		char *ptr = strchr(str, ',');

		if(ptr != NULL)
		{
			while(ptr != NULL && ptr[1] == ',')
			{
				ptr = strchr(ptr + 2, ',');
			}
			if(ptr != NULL)
			{
				*ptr = '\0';
				++ptr;
			}
		}
		if(ptr == NULL)
		{
			ptr = str + strlen(str);
		}

		while(isspace(*str) || *str == ',')
		{
			++str;
		}

		if(str[0] != '\0')
		{
			squash_double_commas(str);
			*state = ptr;
			break;
		}

		str = ptr;
	}

	if(str == NULL)
	{
		*state = NULL;
	}
	return str;
}

/* Squashes two consecutive commas into one in place. */
TSTATIC void
squash_double_commas(char str[])
{
	char *p = str;
	while(*str != '\0')
	{
		if(str[0] == ',')
		{
			if(str[1] == ',')
			{
				*p++ = *str++;
				++str;
				continue;
			}
		}
		*p++ = *str++;
	}
	*p = '\0';
}

int
count_lines(const char text[], int max_width)
{
	const char *start, *end;
	int nlines;

	nlines = 0;

	start = text;
	end = text - 1;
	while((end = strchr(end + 1, '\n')) != NULL)
	{
		if(max_width == INT_MAX)
		{
			++nlines;
		}
		else
		{
			nlines += DIV_ROUND_UP(end - start, max_width);
			if(start == end)
			{
				++nlines;
			}
		}

		start = end + 1;
	}
	if(*start == '\0' || max_width == INT_MAX)
	{
		++nlines;
	}
	else
	{
		const size_t screen_width = utf8_strsw(start);
		nlines += DIV_ROUND_UP(screen_width, max_width);
	}

	if(nlines == 0)
	{
		nlines = 1;
	}

	return nlines;
}

size_t
chars_in_str(const char s[], char c)
{
	size_t char_count = 0;
	while(*s != '\0')
	{
		if(*s++ == c)
		{
			++char_count;
		}
	}
	return char_count;
}

char *
double_char(const char str[], char c)
{
	char *doubled = malloc(strlen(str) + chars_in_str(str, c) + 1);
	if(doubled == NULL)
	{
		return NULL;
	}

	char *p = doubled;
	while(*str != '\0')
	{
		if(*str == c)
		{
			*p++ = c;
		}
		*p++ = *str++;
	}
	*p = '\0';
	return doubled;
}

#ifndef HAVE_STRCASESTR

char *
strcasestr(const char haystack[], const char needle[])
{
	char haystack_us[strlen(haystack) + 1];
	char needle_us[strlen(needle) + 1];
	const char *s;

	strcpy(haystack_us, haystack);
	strcpy(needle_us, needle);

	s = strstr(haystack_us, needle_us);
	return (s == NULL) ? NULL : ((char *)haystack + (s - haystack_us));
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
