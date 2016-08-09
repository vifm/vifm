/* vifm
 * Copyright (C) 2012 xaizek.
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

#include "builtin_functions.h"

#include <sys/types.h> /* mode_t */

#include <assert.h> /* assert() */
#include <stddef.h> /* NULL size_t */
#include <stdlib.h> /* free() */
#include <string.h> /* strcmp() strdup() strpbrk() */

#include "engine/functions.h"
#include "engine/var.h"
#include "ui/cancellation.h"
#include "ui/ui.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/string_array.h"
#include "utils/utils.h"
#include "filelist.h"
#include "macros.h"
#include "types.h"

static var_t chooseopt_builtin(const call_info_t *call_info);
static var_t executable_builtin(const call_info_t *call_info);
static var_t expand_builtin(const call_info_t *call_info);
static var_t filetype_builtin(const call_info_t *call_info);
static int get_fnum(const char position[]);
static var_t getpanetype_builtin(const call_info_t *call_info);
static var_t has_builtin(const call_info_t *call_info);
static var_t layoutis_builtin(const call_info_t *call_info);
static var_t paneisat_builtin(const call_info_t *call_info);
static var_t system_builtin(const call_info_t *call_info);

static const function_t functions[] = {
	/* Name          Description                 Argc  Handler  */
	{ "chooseopt",   "query choose options",       1, &chooseopt_builtin },
	{ "executable",  "check for executable file",  1, &executable_builtin },
	{ "expand",      "expand macros in a string",  1, &expand_builtin },
	{ "filetype",    "retrieve type of a file",    1, &filetype_builtin },
	{ "getpanetype", "retrieve type of file list", 0, &getpanetype_builtin},
	{ "has",         "check for specific ability", 1, &has_builtin },
	{ "layoutis",    "query current layout",       1, &layoutis_builtin },
	{ "paneisat",    "query pane location",        1, &paneisat_builtin },
	{ "system",      "execute external command",   1, &system_builtin },
};

void
init_builtin_functions(void)
{
	size_t i;
	for(i = 0; i < ARRAY_LEN(functions); ++i)
	{
		int result = function_register(&functions[i]);
		assert(result == 0 && "Builtin function registration error");
		(void)result;
	}
}

/* Retrieves values of options related to file choosing as a string.  On unknown
 * arguments empty string is returned. */
static var_t
chooseopt_builtin(const call_info_t *call_info)
{
	var_val_t var_val = { .string = NULL };
	char *type;

	type = var_to_string(call_info->argv[0]);
	if(strcmp(type, "files") == 0)
	{
		var_val.string = curr_stats.chosen_files_out;
	}
	else if(strcmp(type, "dir") == 0)
	{
		var_val.string = curr_stats.chosen_dir_out;
	}
	else if(strcmp(type, "cmd") == 0)
	{
		var_val.string = curr_stats.on_choose;
	}
	else if(strcmp(type, "delimiter") == 0)
	{
		var_val.string = curr_stats.output_delimiter;
	}
	free(type);

	if(var_val.string == NULL)
	{
		var_val.string = "";
	}

	return var_new(VTYPE_STRING, var_val);
}

/* Checks whether executable exists at absolute path or in directories listed in
 * $PATH when path isn't absolute.  Checks for various executable extensions on
 * Windows.  Returns boolean value describing result of the check. */
static var_t
executable_builtin(const call_info_t *call_info)
{
	int exists;
	char *str_val;

	str_val = var_to_string(call_info->argv[0]);

	if(strpbrk(str_val, PATH_SEPARATORS) != NULL)
	{
		exists = executable_exists(str_val);
	}
	else
	{
		exists = (find_cmd_in_path(str_val, 0UL, NULL) == 0);
	}

	free(str_val);

	return var_from_bool(exists);
}

/* Returns string after expanding expression. */
static var_t
expand_builtin(const call_info_t *call_info)
{
	var_t result;
	var_val_t var_val;
	char *str_val;
	char *env_expanded_str_val;

	str_val = var_to_string(call_info->argv[0]);
	env_expanded_str_val = expand_envvars(str_val, 0);
	var_val.string = expand_macros(env_expanded_str_val, NULL, NULL, 0);
	free(env_expanded_str_val);
	free(str_val);

	result = var_new(VTYPE_STRING, var_val);
	free(var_val.string);

	return result;
}

/* Gets string representation of file type.  Returns the string. */
static var_t
filetype_builtin(const call_info_t *call_info)
{
	char *str_val = var_to_string(call_info->argv[0]);
	const int fnum = get_fnum(str_val);
	var_val_t var_val = { .string = "" };
	free(str_val);

	if(fnum >= 0)
	{
		const FileType type = curr_view->dir_entry[fnum].type;
		var_val.const_string = get_type_str(type);
	}
	return var_new(VTYPE_STRING, var_val);
}

/* Returns file type from position or -1 if the position has wrong value. */
static int
get_fnum(const char position[])
{
	if(strcmp(position, ".") == 0)
	{
		return curr_view->list_pos;
	}
	return -1;
}

/* Retrieves type of current pane as a string. */
static var_t
getpanetype_builtin(const call_info_t *call_info)
{
	FileView *const view = curr_view;
	var_val_t var_val;

	if(flist_custom_active(view))
	{
		var_val.string = (view->custom.unsorted ? "very-custom" : "custom");
	}
	else
	{
		var_val.string = "regular";
	}

	return var_new(VTYPE_STRING, var_val);
}

/* Checks current layout configuration.  Returns boolean value that reflects
 * state of specified layout type. */
static var_t
layoutis_builtin(const call_info_t *call_info)
{
	char *type;
	int result;

	type = var_to_string(call_info->argv[0]);
	if(strcmp(type, "only") == 0)
	{
		result = (curr_stats.number_of_windows == 1);
	}
	else if(strcmp(type, "split") == 0)
	{
		result = (curr_stats.number_of_windows == 2);
	}
	else if(strcmp(type, "vsplit") == 0)
	{
		result = (curr_stats.number_of_windows == 2 && curr_stats.split == VSPLIT);
	}
	else if(strcmp(type, "hsplit") == 0)
	{
		result = (curr_stats.number_of_windows == 2 && curr_stats.split == HSPLIT);
	}
	else
	{
		result = 0;
	}
	free(type);

	return var_from_bool(result);
}

/* Allows examining internal parameters from scripts to e.g. figure out
 * environment in which application is running. */
static var_t
has_builtin(const call_info_t *call_info)
{
	var_t result;

	char *const str_val = var_to_string(call_info->argv[0]);

	if(strcmp(str_val, "unix") == 0)
	{
		result = var_from_bool(get_env_type() == ET_UNIX);
	}
	else if(strcmp(str_val, "win") == 0)
	{
		result = var_from_bool(get_env_type() == ET_WIN);
	}
	else
	{
		result = var_false();
	}

	free(str_val);

	return result;
}

/* Checks for relative position of current pane.  Returns boolean value that
 * reflects state of specified location. */
static var_t
paneisat_builtin(const call_info_t *call_info)
{
	char *loc;
	int result;

	const int only = (curr_stats.number_of_windows == 1);
	const int vsplit = (curr_stats.split == VSPLIT);
	const int first = (curr_view == &lwin);

	loc = var_to_string(call_info->argv[0]);
	if(strcmp(loc, "top") == 0)
	{
		result = (only || vsplit || first);
	}
	else if(strcmp(loc, "bottom") == 0)
	{
		result = (only || vsplit || !first);
	}
	else if(strcmp(loc, "left") == 0)
	{
		result = (only || !vsplit || first);
	}
	else if(strcmp(loc, "right") == 0)
	{
		result = (only || !vsplit || !first);
	}
	else
	{
		result = 0;
	}
	free(loc);

	return var_from_bool(result);
}

/* Runs the command in shell and returns its output (joined standard output and
 * standard error streams).  All trailing newline characters are stripped to
 * allow easy appending to command output.  Returns the output. */
static var_t
system_builtin(const call_info_t *call_info)
{
	var_t result;
	char *cmd;
	FILE *cmd_stream;
	size_t cmd_out_len;
	var_val_t var_val;

	cmd = var_to_string(call_info->argv[0]);
	cmd_stream = read_cmd_output(cmd);
	free(cmd);

	ui_cancellation_enable();
	var_val.string = read_nonseekable_stream(cmd_stream, &cmd_out_len, NULL,
			NULL);
	ui_cancellation_disable();
	fclose(cmd_stream);

	if(var_val.string == NULL)
	{
		var_val.string = "";
		return var_new(VTYPE_STRING, var_val);
	}

	/* Remove trailing new line characters. */
	while(cmd_out_len != 0U && var_val.string[cmd_out_len - 1] == '\n')
	{
		var_val.string[cmd_out_len - 1] = '\0';
		--cmd_out_len;
	}

	result = var_new(VTYPE_STRING, var_val);
	free(var_val.string);
	return result;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
