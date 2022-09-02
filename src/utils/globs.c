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

#include <stdlib.h> /* free() realloc() */
#include <stdio.h> /* sprintf() */
#include <string.h> /* strdup() */

#include "str.h"

char *
globs_to_regex(const char globs[])
{
	char *final_regex = NULL;
	size_t final_regex_len = 0UL;

	char *globs_copy = strdup(globs);
	char *glob = globs_copy, *state = NULL;
	while((glob = split_and_get_dc(glob, &state)) != NULL)
	{
		void *p;
		char *regex;
		size_t new_len;

		regex = glob_to_regex(glob, 0);
		if(regex == NULL)
		{
			break;
		}

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

char *
glob_to_regex(const char glob[], int extended)
{
	static const char CHARS_TO_ESCAPE[] = "^.$()|+{";

	char *result = strdup("^$");
	if(result == NULL)
	{
		return NULL;
	}

	size_t result_len = 1;
	while(*glob != '\0')
	{
		if(char_is_one_of(CHARS_TO_ESCAPE, *glob))
		{
			if(*glob != '^' || result[result_len - 1] != '[')
			{
				if(strappendch(&result, &result_len, '\\') != 0)
				{
					break;
				}
			}
		}
		else if(*glob == '!' && result[result_len - 1] == '[')
		{
			if(strappendch(&result, &result_len, '^') != 0)
			{
				break;
			}
			continue;
		}
		else if(*glob == '\\')
		{
			if(strappendch(&result, &result_len, *glob++) != 0)
			{
				break;
			}
		}
		else if(*glob == '?')
		{
			if(strappendch(&result, &result_len, '.') != 0)
			{
				break;
			}
			++glob;
			continue;
		}
		else if(extended && starts_with_lit(glob, "/**/"))
		{
			if(strappend(&result, &result_len, "(/|/.*/|$)") != 0)
			{
				break;
			}

			glob += 4;
			continue;
		}
		else if(extended && *glob == '*' && glob[1] == '*')
		{
			if(strappend(&result, &result_len, ".*") != 0)
			{
				break;
			}
			glob += 2;
			continue;
		}
		else if(*glob == '*')
		{
			if(result_len == 1)
			{
				const char *const pat = extended ? "[^/]*" : "[^.].*";
				if(strappend(&result, &result_len, pat) != 0)
				{
					break;
				}
			}
			else
			{
				const char *const pat = extended ? "[^/]*" : ".*";
				if(strappend(&result, &result_len, pat) != 0)
				{
					break;
				}
			}
			++glob;
			continue;
		}

		if(strappendch(&result, &result_len, *glob++) != 0)
		{
			break;
		}
	}

	if(*glob != '\0' || strappendch(&result, &result_len, '$') != 0)
	{
		free(result);
		return NULL;
	}

	return result;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
