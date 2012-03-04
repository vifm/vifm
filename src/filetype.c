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

#include <curses.h>
#include <regex.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "menus.h"
#include "status.h"
#include "utils.h"

#include "filetype.h"
const assoc_prog_t VIFM_PREUDO_PROG =
{
	.com = VIFM_PREUDO_CMD,
	.description = "Enter directory",
};

/* Internal list that stores only currently active associations.
 * Since it holds only copies of structures from filetype and filextype lists,
 * it doesn't consume much memory, and its items shouldn't be freed */
static assoc_t *active_filetypes;
static int nactive_filetypes;

TESTABLE_STATIC void replace_double_comma(char *cmd, int put_null);
static void assoc_programs(const char *pattern, const char *programs,
		int for_x);
static void register_assoc(assoc_t assoc, int for_x);
static int add_assoc(assoc_t **arr, int count, assoc_t assoc);
static void assoc_viewer(const char *pattern, const char *viewer);
static void free_assoc(assoc_t *assoc);
static void safe_free(char **adr);

static char *
to_regex(const char *global)
{
	char *result = strdup("^$");
	int result_len = 1;
	while(*global != '\0')
	{
		if(strchr("^.$()|+{", *global) != NULL)
		{
		  if(*global != '^' || result[result_len - 1] != '[')
			{
				result = realloc(result, result_len + 2 + 1 + 1);
				result[result_len++] = '\\';
			}
		}
		else if(*global == '!' && result[result_len - 1] == '[')
		{
			result = realloc(result, result_len + 2 + 1 + 1);
			result[result_len++] = '^';
			continue;
		}
		else if(*global == '\\')
		{
			result = realloc(result, result_len + 2 + 1 + 1);
			result[result_len++] = *global++;
		}
		else if(*global == '?')
		{
			result = realloc(result, result_len + 1 + 1 + 1);
			result[result_len++] = '.';
			global++;
			continue;
		}
		else if(*global == '*')
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
get_filetype_number(const char *file, int count, assoc_t *array)
{
	int i;
	for(i = 0; i < count; i++)
	{
		if(global_matches(array[i].pattern, file))
		{
			return i;
		}
	}
	return -1;
}

TESTABLE_STATIC void
replace_double_comma(char *cmd, int put_null)
{
	char *p = cmd;
	while(*cmd != '\0')
	{
		if(cmd[0] == ',')
		{
			if(cmd[1] == ',')
			{
				*p++ = *cmd++;
				cmd++;
				continue;
			}
			else if(put_null)
			{
				break;
			}
		}
		*p++ = *cmd++;
	}
	*p = '\0';
}

int
get_default_program_for_file(const char *file, assoc_prog_t *result)
{
	assoc_prog_t prog;
	int i;

	i = get_filetype_number(file, nactive_filetypes, active_filetypes);
	if(i < 0)
	{
		return 0;
	}

	prog = active_filetypes[i].programs.list[0];
	result->com = strdup(prog.com);
	result->description = strdup(prog.description);

	replace_double_comma(result->com, 1);
	return 1;
}

char *
get_viewer_for_file(char *file)
{
	int i = get_filetype_number(file, cfg.fileviewers_num, fileviewers);

	if(i < 0)
	{
		return NULL;
	}

	return fileviewers[i].programs.list[0].com;
}

assoc_progs_t
get_all_programs_for_file(const char *file)
{
	int i;
	assoc_progs_t result = {};

	for(i = 0; i < nactive_filetypes; i++)
	{
		assoc_progs_t progs;
		int j;

		if(!global_matches(active_filetypes[i].pattern, file))
		{
			continue;
		}

		progs = active_filetypes[i].programs;
		for(j = 0; j < progs.count; j++)
		{
			assoc_prog_t prog = progs.list[j];
			add_assoc_prog(&result, prog.com, prog.description);
		}
	}

	return result;
}

void
set_programs(const char *patterns, const char *programs, int x)
{
	char *exptr;
	char *ex_copy = strdup(patterns);
	char *free_this = ex_copy;
	while((exptr = strchr(ex_copy, ',')) != NULL)
	{
		*exptr = '\0';

		assoc_programs(ex_copy, programs, x);

		ex_copy = exptr + 1;
	}
	assoc_programs(ex_copy, programs, x);
	free(free_this);
}

static void
assoc_programs(const char *pattern, const char *programs, int for_x)
{
	assoc_t assoc;
	char *prog;
	char *free_this;

	if(pattern[0] == '\0')
	{
		return;
	}

	assoc.pattern = strdup(pattern);
	assoc.programs.list = NULL;
	assoc.programs.count = 0;

	prog = strdup(programs);
	free_this = prog;

	while(prog != NULL)
	{
		char *ptr;
		const char *description = "";

		if((ptr = strchr(prog, ',')) != NULL)
		{
			while(ptr != NULL && ptr[1] == ',')
			{
				ptr = strchr(ptr + 2, ',');
			}
			if(ptr != NULL)
			{
				*ptr = '\0';
				ptr++;
			}
		}

		while(isspace(*prog) || *prog == ',')
		{
			prog++;
		}

		if(*prog == '{')
		{
			char *p = strchr(prog + 1, '}');
			if(p != NULL)
			{
				*p = '\0';
				description = prog + 1;
				prog = skip_whitespace(p + 1);
			}
		}

		if(prog[0] != '\0')
		{
			replace_double_comma(prog, 0);
			add_assoc_prog(&assoc.programs, prog, description);
		}
		prog = ptr;
	}

	free(free_this);

	register_assoc(assoc, for_x);
}

static void
register_assoc(assoc_t assoc, int for_x)
{
	if(for_x)
	{
		cfg.xfiletypes_num = add_assoc(&xfiletypes, cfg.xfiletypes_num, assoc);
	}
	else
	{
		cfg.filetypes_num = add_assoc(&filetypes, cfg.filetypes_num, assoc);
	}
	if(!for_x || !curr_stats.is_console)
	{
		nactive_filetypes = add_assoc(&active_filetypes, nactive_filetypes, assoc);
	}
}

void
set_fileviewer(const char *patterns, const char *viewer)
{
	char *exptr;
	char *ex_copy = strdup(patterns);
	char *free_this = ex_copy;
	while((exptr = strchr(ex_copy, ',')) != NULL)
	{
		*exptr = '\0';

		assoc_viewer(ex_copy, viewer);

		ex_copy = exptr + 1;
	}
	assoc_viewer(ex_copy, viewer);
	free(free_this);
}

static void
assoc_viewer(const char *pattern, const char *viewer)
{
	int i;

	if(pattern[0] == '\0')
	{
		return;
	}

	for(i = 0; i < cfg.fileviewers_num; i++)
	{
		if(strcasecmp(fileviewers[i].pattern, pattern) == 0)
		{
			break;
		}
	}
	if(i == cfg.fileviewers_num)
	{
		assoc_t assoc;
		assoc.pattern = strdup(pattern);
		assoc.programs.list = malloc(sizeof(assoc_prog_t));
		assoc.programs.count = 1;
		assoc.programs.list[0].com = strdup(viewer);
		assoc.programs.list[0].description = strdup("");

		cfg.fileviewers_num = add_assoc(&fileviewers, cfg.fileviewers_num, assoc);
	}
	else
	{
		free(fileviewers[i].programs.list[0].com);
		fileviewers[i].programs.list[0].com = strdup(viewer);
	}
}

static int
add_assoc(assoc_t **arr, int count, assoc_t assoc)
{
	*arr = realloc(*arr, (count + 1)*sizeof(assoc_t));
	(*arr)[count] = assoc;
	return count + 1;
}

static void
reset_list(assoc_t **arr, int *size)
{
	int i;

	for(i = 0; i < *size; i++)
	{
		free_assoc(&(*arr)[i]);
	}

	free(*arr);
	*arr = NULL;
	*size = 0;
}

void
reset_all_file_associations(void)
{
	reset_list(&filetypes, &cfg.filetypes_num);
	reset_list(&xfiletypes, &cfg.xfiletypes_num);
	reset_list(&fileviewers, &cfg.fileviewers_num);

	free(active_filetypes);
	active_filetypes = NULL;
	nactive_filetypes = 0;
}

static void
free_assoc(assoc_t *assoc)
{
	int i;
	safe_free(&assoc->pattern);
	for(i = 0; i < assoc->programs.count; i++)
	{
		free_assoc_prog(&assoc->programs.list[i]);
	}

	free(assoc->programs.list);
	assoc->programs.list = NULL;
	assoc->programs.count = 0;
}

void
free_assoc_prog(assoc_prog_t *assoc_prog)
{
	safe_free(&assoc_prog->com);
	safe_free(&assoc_prog->description);
}

void
add_assoc_prog(assoc_progs_t *assocs, const char *program,
		const char *description)
{
	void *p = realloc(assocs->list, sizeof(assoc_prog_t)*(assocs->count + 1));
	if(p == NULL)
	{
		(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	assocs->list = p;
	assocs->list[assocs->count].com = strdup(program);
	assocs->list[assocs->count].description = strdup(description);
	assocs->count++;
}

static void
safe_free(char **adr)
{
	free(*adr);
	*adr = NULL;
}

int
assoc_prog_is_empty(const assoc_prog_t *assoc_prog)
{
	return assoc_prog->com == NULL && assoc_prog->description == NULL;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
