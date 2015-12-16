/* vifm
 * Copyright (C) 2015 xaizek.
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

#include "regexp.h"

#include <regex.h> /* regex_t regmatch_t regerror() regexec() */

#include "../cfg/config.h"
#include "str.h"

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
get_regexp_error(int err, const regex_t *re)
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

regmatch_t
get_group_match(const regex_t *re, const char str[])
{
	regmatch_t matches[2];

	if(regexec(re, str, 2, matches, 0) != 0 || matches[1].rm_so == -1)
	{
		matches[1].rm_so = 0;
		matches[1].rm_eo = 0;
	}

	return matches[1];
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
