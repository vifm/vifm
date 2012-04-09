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
#include <stdlib.h> /* mbstowcs() wcstombs() */
#include <string.h> /* strncmp() strlen() strcmp() strchr() strrchr() */

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
	wchar_t *result;

	result = malloc((wcslen(ws) + 1)*sizeof(wchar_t));
	if(result == NULL)
		return NULL;
	wcscpy(result, ws);
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

/* Skips consecutive whitespace characters. */
char *
skip_whitespace(const char *str)
{
	while(isspace(*str))
		str++;
	return (char *)str;
}

char *
after_last(const char *str, char c)
{
	char *result = strrchr(str, c);
	result = (result == NULL) ? ((char *)str) : (result + 1);
	return result;
}

void
replace_string(char **str, const char *with)
{
	char *new = strdup(with);
	if(new != NULL)
	{
		free(*str);
		*str = new;
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
