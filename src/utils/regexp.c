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

#include <regex.h> /* regex_t regmatch_t regcomp() regerror() regexec() */

#include <ctype.h> /* isdigit() */
#include <stdlib.h> /* free() malloc() */
#include <string.h> /* strlen() */

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

	/* Look for \c and \C in the pattern, they have the highest priority. */
	const char *s = pattern;
	while(s[0] != '\0')
	{
		if(s[0] == '\\' && s[1] != '\0')
		{
			if(s[1] == 'c')
			{
				ignore_case = 1;
			}
			else if(s[1] == 'C')
			{
				ignore_case = 0;
			}
			++s;
		}

		++s;
	}

	return ignore_case;
}

int
regexp_compile(regex_t *re, const char pattern[], int cflags)
{
	char *mod_pattern = malloc(strlen(pattern) + 1);
	if(mod_pattern == NULL)
	{
		return REG_ESPACE;
	}

	char *p = mod_pattern;
	const char *s = pattern;
	while(s[0] != '\0')
	{
		if(s[0] == '\\' && s[1] != '\0')
		{
			if(s[1] == 'c')
			{
				cflags |= REG_ICASE;
				s += 2;
				continue;
			}
			else if(s[1] == 'C')
			{
				cflags &= ~REG_ICASE;
				s += 2;
				continue;
			}

			*p++ = *s++;
		}

		*p++ = *s++;
	}
	*p = '\0';

	int result = regcomp(re, mod_pattern, cflags);
	free(mod_pattern);

	return result;
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

const char *
regexp_replace(const char line[], const char pattern[], const char sub[],
		int glob, int ignore_case)
{
	static char buf[PATH_MAX + 1];
	regex_t re;
	regmatch_t matches[10];
	const char *dst;

	copy_str(buf, sizeof(buf), line);

	const int flags = REG_EXTENDED | (ignore_case ? REG_ICASE : 0);
	if(regexp_compile(&re, pattern, flags) != 0)
	{
		regfree(&re);
		return buf;
	}

	if(regexec(&re, line, ARRAY_LEN(matches), matches, 0) != 0)
	{
		regfree(&re);
		return buf;
	}

	if(glob && pattern[0] != '^')
		dst = regexp_gsubst(&re, line, sub, matches);
	else
		dst = regexp_subst(line, sub, matches, NULL);
	copy_str(buf, sizeof(buf), dst);

	regfree(&re);
	return buf;
}

const char *
regexp_gsubst(regex_t *re, const char src[], const char sub[],
		regmatch_t matches[])
{
	static char buf[NAME_MAX + 1];
	int off = 0;

	copy_str(buf, sizeof(buf), src);
	do
	{
		int i;
		for(i = 0; i < 10; ++i)
		{
			matches[i].rm_so += off;
			matches[i].rm_eo += off;
		}

		copy_str(buf, sizeof(buf), regexp_subst(buf, sub, matches, &off));

		if(matches[0].rm_eo == matches[0].rm_so)
		{
			/* If we found an empty match, repeating will cause infinite loop. */
			break;
		}
	}
	while(regexec(re, buf + off, 10, matches, 0) == 0);

	return buf;
}

const char *
regexp_subst(const char src[], const char sub[], const regmatch_t matches[],
		int *off)
{
	static char buf[NAME_MAX + 1];
	char *dst = buf;
	int i;

	for(i = 0; i < matches[0].rm_so; ++i)
	{
		*dst++ = src[i];
	}

	while(*sub != '\0')
	{
		if(*sub == '\\')
		{
			if(sub[1] == '\0')
				break;
			else if(isdigit(sub[1]))
			{
				int n = sub[1] - '0';
				for(i = matches[n].rm_so; i < matches[n].rm_eo; i++)
					*dst++ = src[i];
				sub += 2;
				continue;
			}
			else
				sub++;
		}
		*dst++ = *sub++;
	}
	if(off != NULL)
		*off = dst - buf;

	for(i = matches[0].rm_eo; src[i] != '\0'; i++)
		*dst++ = src[i];

	*dst = '\0';

	return buf;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
