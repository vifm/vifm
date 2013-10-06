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

#include "filetype.h"

#include <curses.h>

#include <ctype.h> /* isspace() */
#include <string.h> /* strchr() strdup() strcasecmp() */

#include "cfg/config.h"
#include "menus/menus.h"
#include "utils/fs_limits.h"
#include "utils/str.h"
#include "utils/utils.h"
#include "globals.h"

const assoc_record_t NONE_PSEUDO_PROG =
{
	.command = "",
	.description = "",
};

/* Internal list that stores only currently active associations.
 * Since it holds only copies of structures from filetype and filextype lists,
 * it doesn't consume much memory, and its items shouldn't be freed */
static assoc_list_t active_filetypes;

/* Used to set type of new association records */
static assoc_record_type_t new_records_type = ART_CUSTOM;

/* Pointer to external command existence check function. */
static external_command_exists_t external_command_exists_func;

TSTATIC void replace_double_comma(char cmd[], int put_null);
static int get_filetype_number(const char *file, assoc_list_t assoc_list);
static void assoc_programs(const char pattern[], const char programs[],
		int for_x, int in_x);
static void register_assoc(assoc_t assoc, int for_x, int in_x);
static void add_assoc(assoc_list_t *assoc_list, assoc_t assoc);
static void assoc_viewer(const char *pattern, const char *viewer);
static void reset_all_list(void);
static void add_defaults(int in_x);
static void reset_list(assoc_list_t *assoc_list);
static void reset_list_head(assoc_list_t *assoc_list);
static void free_assoc(assoc_t *assoc);
static void safe_free(char **adr);

void
config_filetypes(external_command_exists_t ece_func)
{
	external_command_exists_func = ece_func;
}

int
get_default_program_for_file(const char *file, assoc_record_t *result)
{
	int j;
	assoc_records_t records;
	assoc_record_t prog;

	j = 0;
	records = get_all_programs_for_file(file);
	while(j < records.count)
	{
		char name_buf[NAME_MAX];
		(void)extract_cmd_name(records.list[j].command, 0, sizeof(name_buf),
				name_buf);
		if(external_command_exists_func == NULL ||
				external_command_exists_func(name_buf))
			break;
		j++;
	}
	if(j >= records.count)
	{
		free(records.list);
		return 0;
	}

	prog = records.list[j];
	result->command = strdup(prog.command);
	result->description = strdup(prog.description);
	free(records.list);

	if(result->command == NULL || result->description == NULL)
	{
		free_assoc_record(result);
		return 0;
	}

	return 1;
}

char *
get_viewer_for_file(const char file[])
{
	int i = get_filetype_number(file, fileviewers);

	if(i < 0)
	{
		return NULL;
	}

	return fileviewers.list[i].records.list[0].command;
}

static int
get_filetype_number(const char *file, assoc_list_t assoc_list)
{
	int i;
	for(i = 0; i < assoc_list.count; i++)
	{
		if(global_matches(assoc_list.list[i].pattern, file))
		{
			return i;
		}
	}
	return -1;
}

assoc_records_t
get_all_programs_for_file(const char *file)
{
	int i;
	assoc_records_t result = {};

	for(i = 0; i < active_filetypes.count; i++)
	{
		assoc_records_t progs;
		int j;

		if(!global_matches(active_filetypes.list[i].pattern, file))
		{
			continue;
		}

		progs = active_filetypes.list[i].records;
		for(j = 0; j < progs.count; j++)
		{
			assoc_record_t prog = progs.list[j];
			add_assoc_record(&result, prog.command, prog.description);
		}
	}

	return result;
}

void
set_programs(const char patterns[], const char programs[], int for_x, int in_x)
{
	char *exptr;
	char *ex_copy = strdup(patterns);
	char *free_this = ex_copy;
	while((exptr = strchr(ex_copy, ',')) != NULL)
	{
		*exptr = '\0';

		assoc_programs(ex_copy, programs, for_x, in_x);

		ex_copy = exptr + 1;
	}
	assoc_programs(ex_copy, programs, for_x, in_x);
	free(free_this);
}

/* Associates patter with list of comma separated programs either for X or non-X
 * associations and depending on current execution environment. */
static void
assoc_programs(const char pattern[], const char programs[], int for_x, int in_x)
{
	assoc_t assoc;
	char *prog;
	char *free_this;

	if(pattern[0] == '\0')
	{
		return;
	}

	assoc.pattern = strdup(pattern);
	assoc.records.list = NULL;
	assoc.records.count = 0;

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
			add_assoc_record(&assoc.records, prog, description);
		}
		prog = ptr;
	}

	free(free_this);

	register_assoc(assoc, for_x, in_x);
}

TSTATIC void
replace_double_comma(char cmd[], int put_null)
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

/* Registers association in appropriate associations list and possibly in list
 * of active associations, which depends on association type and execution
 * environment. */
static void
register_assoc(assoc_t assoc, int for_x, int in_x)
{
	add_assoc(for_x ? &xfiletypes : &filetypes, assoc);
	if(!for_x || in_x)
	{
		add_assoc(&active_filetypes, assoc);
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

	for(i = 0; i < fileviewers.count; i++)
	{
		if(strcasecmp(fileviewers.list[i].pattern, pattern) == 0)
		{
			break;
		}
	}
	if(i == fileviewers.count)
	{
		assoc_t assoc =
		{
			.pattern = strdup(pattern),
			.records.list = NULL,
			.records.count = 0,
		};
		add_assoc_record(&assoc.records, viewer, "");
		add_assoc(&fileviewers, assoc);
	}
	else
	{
		(void)replace_string(&fileviewers.list[i].records.list[0].command, viewer);
	}
}

static void
add_assoc(assoc_list_t *assoc_list, assoc_t assoc)
{
	void *p = realloc(assoc_list->list, (assoc_list->count + 1)*sizeof(assoc_t));
	if(p == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	assoc_list->list = p;
	assoc_list->list[assoc_list->count] = assoc;
	assoc_list->count++;
}

void
reset_all_file_associations(int in_x)
{
	reset_all_list();
	add_defaults(in_x);
}

static void
reset_all_list(void)
{
	reset_list(&filetypes);
	reset_list(&xfiletypes);
	reset_list(&fileviewers);

	reset_list_head(&active_filetypes);
}

/* Loads default (builtin) associations. */
static void
add_defaults(int in_x)
{
	new_records_type = ART_BUILTIN;
	set_programs("*/", "{Enter directory}" VIFM_PSEUDO_CMD, 0, in_x);
	new_records_type = ART_CUSTOM;
}

static void
reset_list(assoc_list_t *assoc_list)
{
	int i;
	for(i = 0; i < assoc_list->count; i++)
	{
		free_assoc(&assoc_list->list[i]);
	}
	reset_list_head(assoc_list);
}

static void
reset_list_head(assoc_list_t *assoc_list)
{
	free(assoc_list->list);
	assoc_list->list = NULL;
	assoc_list->count = 0;
}

static void
free_assoc(assoc_t *assoc)
{
	safe_free(&assoc->pattern);
	free_assoc_records(&assoc->records);
}

void
free_assoc_records(assoc_records_t *records)
{
	int i;
	for(i = 0; i < records->count; i++)
	{
		free_assoc_record(&records->list[i]);
	}

	free(records->list);
	records->list = NULL;
	records->count = 0;
}

void
free_assoc_record(assoc_record_t *record)
{
	safe_free(&record->command);
	safe_free(&record->description);
}

void
add_assoc_record(assoc_records_t *records, const char *command,
		const char *description)
{
	void *p = realloc(records->list, sizeof(assoc_record_t)*(records->count + 1));
	if(p == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	records->list = p;
	records->list[records->count].command = strdup(command);
	records->list[records->count].description = strdup(description);
	records->list[records->count].type = new_records_type;
	records->count++;
}

void
add_assoc_records(assoc_records_t *assocs, const assoc_records_t *src)
{
	int i;
	void *p;
	const int src_count = src->count;

	if(src_count == 0)
	{
		return;
	}

	p = realloc(assocs->list, sizeof(assoc_record_t)*(assocs->count + src_count));
	if(p == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	assocs->list = p;

	for(i = 0; i < src_count; i++)
	{
		assocs->list[assocs->count + i].command = strdup(src->list[i].command);
		assocs->list[assocs->count + i].description =
				strdup(src->list[i].description);
		assocs->list[assocs->count + i].type = src->list[i].type;
	}

	assocs->count += src_count;
}

static void
safe_free(char **adr)
{
	free(*adr);
	*adr = NULL;
}

int
assoc_prog_is_empty(const assoc_record_t *record)
{
	return record->command == NULL && record->description == NULL;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
