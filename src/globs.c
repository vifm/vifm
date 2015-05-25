/* vifm
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

#include "globs.h"

#include <regex.h> /* regex_t regcomp() regexec() regfree() */

#include <stdlib.h> /* realloc() free() */
#include <stdio.h> /* sprintf() */
#include <string.h> /* strdup() */

#include "utils/str.h"

static char * globs_to_regex(const char globs[]);
static char * glob_to_regex(const char glob[]);

int
globs_matches(const char globs[], const char file[])
{
	int matches;
	regex_t re;

	matches = 0;
	if(globs_compile_as_re(globs, &re) == 0)
	{
		if(regexec(&re, file, 0, NULL, 0) == 0)
		{
			matches = 1;
		}
	}

	regfree(&re);
	return matches;
}

int
globs_compile_as_re(const char glob[], regex_t *re)
{
	char *regex;
	int result;

	regex = globs_to_regex(glob);
	result = regcomp(re, regex, REG_EXTENDED | REG_ICASE);
	free(regex);

	return result;
}

/* Converts comma-separated list of globs into equivalent regular expression.
 * Returns pointer to a newly allocated string, which should be freed by the
 * caller, or NULL if there is not enough memory or no patters are given. */
static char *
globs_to_regex(const char globs[])
{
	char *final_regex = NULL;
	size_t final_regex_len = 0UL;

	char *globs_copy = strdup(globs);
	char *glob = globs_copy, *state = NULL;
	while((glob = split_and_get(glob, ',', &state)) != NULL)
	{
		void *p;
		char *regex;
		size_t new_len;

		regex = glob_to_regex(glob);

		new_len = final_regex_len + 1 + 1 + strlen(regex) + 1;
		p = realloc(final_regex, new_len + 1);
		if(p != NULL)
		{
			final_regex = p;
			final_regex_len += sprintf(final_regex + final_regex_len, "%s(%s)",
					(final_regex_len != 0UL) ? "|" : "", regex);
		}

		free(regex);
	}
	free(globs_copy);

	return final_regex;
}

/* Converts the glob into equivalent regular expression.  Returns pointer to
 * a newly allocated string, which should be freed by the caller, or NULL if
 * there is not enough memory. */
static char *
glob_to_regex(const char glob[])
{
	static const char CHARS_TO_ESCAPE[] = "^.$()|+{";
	char *result = strdup("^$");
	int result_len = 1;
	while(*glob != '\0')
	{
		if(char_is_one_of(CHARS_TO_ESCAPE, *glob))
		{
		  if(*glob != '^' || result[result_len - 1] != '[')
			{
				result = realloc(result, result_len + 2 + 1 + 1);
				result[result_len++] = '\\';
			}
		}
		else if(*glob == '!' && result[result_len - 1] == '[')
		{
			result = realloc(result, result_len + 2 + 1 + 1);
			result[result_len++] = '^';
			continue;
		}
		else if(*glob == '\\')
		{
			result = realloc(result, result_len + 2 + 1 + 1);
			result[result_len++] = *glob++;
		}
		else if(*glob == '?')
		{
			result = realloc(result, result_len + 1 + 1 + 1);
			result[result_len++] = '.';
			++glob;
			continue;
		}
		else if(*glob == '*')
		{
			if(result_len == 1)
			{
				result = realloc(result, result_len + 9 + 1 + 1);
				result[result_len++] = '[';
				result[result_len++] = '^';
				result[result_len++] = '.';
				result[result_len++] = ']';
				result[result_len++] = '.';
				result[result_len++] = '*';
			}
			else
			{
				result = realloc(result, result_len + 2 + 1 + 1);
				result[result_len++] = '.';
				result[result_len++] = '*';
			}
			++glob;
			continue;
		}
		else
		{
			result = realloc(result, result_len + 1 + 1 + 1);
		}
		result[result_len++] = *glob++;
	}
	result[result_len++] = '$';
	result[result_len] = '\0';
	return result;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
