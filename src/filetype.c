/* vifm
 * Copyright (C) 2001 Ken Steen.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <curses.h>
#include <regex.h>

#include <stdlib.h>
#include <string.h>

#include "config.h"

#include "filetype.h"

static char *
to_regex(const char *global)
{
	char *result = strdup("^");
	size_t result_len = 1;
	while(*global != '\0')
	{
		if(strchr("^.[$()|+?{\\", *global) != NULL)
		{
			result = realloc(result, result_len + 2 + 1 + 1);
			result[result_len++] = '\\';
		}
		else if(*global == '*')
		{
			if(result_len == 1)
			{
				result = realloc(result, result_len + 9 + 1 + 1);
				result[result_len++] = '(';
				result[result_len++] = '[';
				result[result_len++] = '^';
				result[result_len++] = '.';
				result[result_len++] = ']';
				result[result_len++] = '.';
				result[result_len++] = '*';
				result[result_len++] = ')';
				result[result_len++] = '?';
			}
			else
			{
				result = realloc(result, result_len + 2 + 1 + 1);
				result[result_len++] = '.';
				result[result_len++] = '*';
			}
			global++;
			continue;
		}
		else
		{
			result = realloc(result, result_len + 1 + 1 + 1);
		}
		result[result_len++] = *global++;
	}
	result[result_len++] = '$';
	result[result_len] = '\0';
	return result;
}

static int
global_matches(const char *global, const char *file)
{
	char *regex;
	regex_t re;

	regex = to_regex(global);

	if(regcomp(&re, regex, REG_EXTENDED | REG_ICASE) == 0)
	{
		if(regexec(&re, file, 0, NULL, 0) == 0)
		{
			regfree(&re);
			free(regex);
			return 1;
		}
	}
	regfree(&re);
	free(regex);
	return 0;
}

static int
get_filetype_number(const char *file)
{
	int x;

	for(x = 0; x < cfg.filetypes_num; x++)
	{
		char *exptr = NULL;

		/* Only one extension */
		if((exptr = strchr(filetypes[x].ext, ',')) == NULL)
		{
			if(global_matches(filetypes[x].ext, file))
				return x;
		}
		else
		{
			char *ex_copy = strdup(filetypes[x].ext);
			char *free_this = ex_copy;
			char *exptr2 = NULL;
			while((exptr = exptr2 = strchr(ex_copy, ',')) != NULL)
			{
				*exptr = '\0';
				exptr2++;

				if(global_matches(ex_copy, file))
				{
					free(free_this);
					return x;
				}

				ex_copy = exptr2;
			}
			if(global_matches(ex_copy, file))
			{
				free(free_this);
				return x;
			}
			free(free_this);
		}
	}
	return -1;
}

char *
get_default_program_for_file(char *file)
{
	int x = get_filetype_number(file);
	char *strptr = NULL;
	char *ptr = NULL;
	char *program_name = NULL;

	if(x < 0)
		return NULL;

	strptr = strdup(filetypes[x].programs);

	/* Only one program */
	if((ptr = strchr(strptr, ',')) == NULL)
	{
		program_name = strdup(strptr);
	}
	else
	{
		*ptr = '\0';
		program_name = strdup(strptr);
	}
	free(strptr);
	return program_name;
}

char *
get_viewer_for_file(char *file)
{
	int x = get_filetype_number(file);

	if(x < 0)
		return NULL;

	return fileviewers[x].viewer;
}

char *
get_all_programs_for_file(char *file)
{
	int x = get_filetype_number(file);

	if(x > -1)
		return filetypes[x].programs;

	return NULL;
}

void
add_filetype(const char *description, const char *extension,
		const char *programs)
{
	filetypes = realloc(filetypes, (cfg.filetypes_num + 1) * sizeof(filetype_t));

	filetypes[cfg.filetypes_num].type = strdup(description);

	filetypes[cfg.filetypes_num].ext = strdup(extension);
	filetypes[cfg.filetypes_num].programs = strdup(programs);
	cfg.filetypes_num++;
}

void
add_fileviewer(char *extension, char *viewer)
{
	fileviewers = realloc(fileviewers,
			(cfg.fileviewers_num + 1) * sizeof(fileviewer_t));

	fileviewers[cfg.fileviewers_num].ext = strdup(extension);
	fileviewers[cfg.fileviewers_num].viewer = strdup(viewer);
	cfg.fileviewers_num++;
}

void
set_programs(char *extension, char *programs)
{
	int x;

	x = get_filetype_number(extension);
	if(x < 0)
	{
		add_filetype("", extension + 1, programs);
		add_fileviewer(extension + 1, programs);
		return;
	}

	free(filetypes[x].programs);
	filetypes[x].programs = strdup(programs);
}

void
reset_filetypes(void)
{
	int i;

	for(i = 0; i < cfg.filetypes_num; i++)
	{
		free(filetypes[i].type);
		free(filetypes[i].ext);
		free(filetypes[i].programs);
	}

	free(filetypes);
	filetypes = NULL;
	cfg.filetypes_num = 0;
}

void
reset_fileviewers(void)
{
	int i;

	for(i = 0; i < cfg.fileviewers_num; i++)
	{
		free(fileviewers[i].ext);
		free(fileviewers[i].viewer);
	}

	free(fileviewers);
	fileviewers = NULL;
	cfg.fileviewers_num = 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
