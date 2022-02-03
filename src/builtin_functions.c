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

#include "compat/fs_limits.h"
#include "compat/os.h"
#include "engine/functions.h"
#include "engine/text_buffer.h"
#include "engine/var.h"
#include "lua/vlua.h"
#include "ui/cancellation.h"
#include "ui/tabs.h"
#include "ui/ui.h"
#include "utils/filemon.h"
#include "utils/fs.h"
#include "utils/fsddata.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/test_helpers.h"
#include "utils/trie.h"
#include "utils/utils.h"
#include "filelist.h"
#include "macros.h"
#include "types.h"

/* A single entry of cache of external commands. */
typedef struct
{
	fsddata_t *caches; /* A set of named caches. */
	filemon_t mon;     /* File monitor. */
}
extcache_t;

static var_t chooseopt_builtin(const call_info_t *call_info);
static var_t executable_builtin(const call_info_t *call_info);
static var_t expand_builtin(const call_info_t *call_info);
static var_t extcached_builtin(const call_info_t *call_info);
TSTATIC void set_extcached_monitor_type(FileMonType type);
static var_t filetype_builtin(const call_info_t *call_info);
static int get_fnum(var_t fnum);
static const char * type_of_link_target(const dir_entry_t *entry);
static var_t fnameescape_builtin(const call_info_t *call_info);
static var_t getpanetype_builtin(const call_info_t *call_info);
static var_t has_builtin(const call_info_t *call_info);
static var_t layoutis_builtin(const call_info_t *call_info);
static var_t paneisat_builtin(const call_info_t *call_info);
static var_t system_builtin(const call_info_t *call_info);
static var_t tabpagenr_builtin(const call_info_t *call_info);
static var_t term_builtin(const call_info_t *call_info);
static var_t execute_cmd(var_t cmd_arg, int interactive, int preserve_stdin);

/* Function descriptions. */
static const function_t functions[] = {
	/* Name          Description                    Args   Handler  */
	{ "chooseopt",   "query choose options",       {1,1}, &chooseopt_builtin },
	{ "executable",  "check for executable file",  {1,1}, &executable_builtin },
	{ "expand",      "expand macros in a string",  {1,1}, &expand_builtin },
	{ "extcached",   "caches result of a command", {3,3}, &extcached_builtin },
	{ "filetype",    "retrieve type of a file",    {1,2}, &filetype_builtin },
	{ "fnameescape", "escapes string for a :cmd",  {1,1}, &fnameescape_builtin },
	{ "getpanetype", "retrieve type of file list", {0,0}, &getpanetype_builtin},
	{ "has",         "check for specific ability", {1,1}, &has_builtin },
	{ "layoutis",    "query current layout",       {1,1}, &layoutis_builtin },
	{ "paneisat",    "query pane location",        {1,1}, &paneisat_builtin },
	{ "system",      "execute external command",   {1,1}, &system_builtin },
	{ "tabpagenr",   "number of current/last tab", {0,1}, &tabpagenr_builtin },
	{ "term",        "run interactive command",    {1,1}, &term_builtin },
};

/* Kind of monitor used by the extcached(). */
static FileMonType extcached_mon_type = FMT_CHANGED;

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
	const char *result = NULL;
	char *type;

	type = var_to_str(call_info->argv[0]);
	if(strcmp(type, "files") == 0)
	{
		result = curr_stats.chosen_files_out;
	}
	else if(strcmp(type, "dir") == 0)
	{
		result = curr_stats.chosen_dir_out;
	}
	else if(strcmp(type, "cmd") == 0)
	{
		result = curr_stats.on_choose;
	}
	else if(strcmp(type, "delimiter") == 0)
	{
		result = curr_stats.output_delimiter;
	}
	free(type);

	return var_from_str(result == NULL ? "" : result);
}

/* Checks whether executable exists at absolute path or in directories listed in
 * $PATH when path isn't absolute.  Checks for various executable extensions on
 * Windows.  Returns boolean value describing result of the check. */
static var_t
executable_builtin(const call_info_t *call_info)
{
	int exists;
	char *str_val;

	str_val = var_to_str(call_info->argv[0]);

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
	char *arg = var_to_str(call_info->argv[0]);

	char *env_expanded = expand_envvars(arg,
			EEF_KEEP_ESCAPES | EEF_DOUBLE_PERCENTS);
	free(arg);

	char *macro_expanded = ma_expand(env_expanded, NULL, NULL, MER_DISPLAY);
	free(env_expanded);

	var_t result = var_from_str(macro_expanded);
	free(macro_expanded);

	return result;
}

/* Returns cached value of an external command.  Cache validity is bound to a
 * file. */
static var_t
extcached_builtin(const call_info_t *call_info)
{
	static trie_t *cache;
	if(cache == NULL)
	{
		cache = trie_create(&free);
	}

	char *cache_name = var_to_str(call_info->argv[0]);
	if(cache_name == NULL)
	{
		return var_error();
	}

	char *path = var_to_str(call_info->argv[1]);
	if(path == NULL)
	{
		return var_error();
	}

	char canonic[PATH_MAX + 1];
	to_canonic_path(path, flist_get_dir(curr_view), canonic, sizeof(canonic));
	replace_string(&path, canonic);

	extcache_t *cached = NULL;
	void *data;
	if(trie_get(cache, path, &data) == 0)
	{
		cached = data;
	}

	filemon_t current_mon = {};
	if(filemon_from_file(path, extcached_mon_type, &current_mon) == 0 &&
			cached != NULL)
	{
		if(filemon_equal(&current_mon, &cached->mon))
		{
			void *cached_output;
			if(fsddata_get(cached->caches, cache_name, &cached_output) == 0)
			{
				free(cache_name);
				free(path);
				return var_from_str(cached_output);
			}
		}
		else
		{
			fsddata_free(cached->caches);
			cached->caches = NULL;
		}
	}

	if(cached == NULL)
	{
		cached = malloc(sizeof(*cached));
		if(cached == NULL)
		{
			free(cache_name);
			free(path);
			return var_error();
		}

		cached->caches = NULL;

		if(trie_set(cache, path, cached) < 0)
		{
			free(cached);
			free(cache_name);
			free(path);
			return var_error();
		}
	}

	if(cached->caches == NULL)
	{
		cached->caches = fsddata_create(0, 0);
	}

	cached->mon = current_mon;
	free(path);

	var_t output = execute_cmd(call_info->argv[2], call_info->interactive, 0);
	(void)fsddata_set(cached->caches, cache_name, var_to_str(output));
	free(cache_name);

	return output;
}

/* Modifies kind of monitor used by the extcached(). */
TSTATIC void
set_extcached_monitor_type(FileMonType type)
{
	extcached_mon_type = type;
}

/* Gets string representation of file type.  Returns the string. */
static var_t
filetype_builtin(const call_info_t *call_info)
{
	const int fnum = get_fnum(call_info->argv[0]);
	int resolve_links = (call_info->argc > 1 && var_to_bool(call_info->argv[1]));

	const char *result_str = "";
	if(fnum >= 0)
	{
		const dir_entry_t *entry = &curr_view->dir_entry[fnum];
		if(entry->type == FT_LINK && resolve_links)
		{
			result_str = type_of_link_target(entry);
		}
		else
		{
			result_str = get_type_str(entry->type);
		}
	}
	return var_from_str(result_str);
}

/* Turns {fnum} into file position.  Returns the position or -1 if the argument
 * is wrong. */
static int
get_fnum(var_t fnum)
{
	char *str_val = var_to_str(fnum);

	int pos = -1;
	if(strcmp(str_val, ".") == 0)
	{
		pos = curr_view->list_pos;
	}
	else
	{
		int int_val = var_to_int(fnum);
		if(int_val > 0 && int_val <= curr_view->list_rows)
		{
			pos = int_val - 1;
		}
	}

	free(str_val);
	return pos;
}

/* Resoves file type of link target.  The entry parameter is expected to point
 * at symbolic link.  Returns the type as a string. */
static const char *
type_of_link_target(const dir_entry_t *entry)
{
	char path[PATH_MAX + 1];
	struct stat s;

	get_full_path_of(entry, sizeof(path), path);
	if(get_link_target_abs(path, entry->origin, path, sizeof(path)) != 0 ||
			os_stat(path, &s) != 0)
	{
		return "broken";
	}
	return get_type_str(get_type_from_mode(s.st_mode));
}

/* Escapes argument to make it suitable for use as an argument in :commands. */
static var_t
fnameescape_builtin(const call_info_t *call_info)
{
	var_t result;

	char *const str_val = var_to_str(call_info->argv[0]);
	char *const escaped = shell_like_escape(str_val, 1);
	free(str_val);

	result = var_from_str(escaped);
	free(escaped);
	return result;
}

/* Retrieves type of current pane as a string. */
static var_t
getpanetype_builtin(const call_info_t *call_info)
{
	if(!flist_custom_active(curr_view))
	{
		return var_from_str("regular");
	}

	switch(curr_view->custom.type)
	{
		case CV_REGULAR:
			return var_from_str("custom");

		case CV_VERY:
			return var_from_str("very-custom");

		case CV_CUSTOM_TREE:
		case CV_TREE:
			return var_from_str("tree");

		case CV_DIFF:
		case CV_COMPARE:
			return var_from_str("compare");
	}
	return var_from_str("UNKNOWN");
}

/* Checks current layout configuration.  Returns boolean value that reflects
 * state of specified layout type. */
static var_t
layoutis_builtin(const call_info_t *call_info)
{
	char *type;
	int result;

	type = var_to_str(call_info->argv[0]);
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
	int result = 0;

	char *const str_val = var_to_str(call_info->argv[0]);

	if(strcmp(str_val, "unix") == 0)
	{
		result = (get_env_type() == ET_UNIX);
	}
	else if(strcmp(str_val, "win") == 0)
	{
		result = (get_env_type() == ET_WIN);
	}
	else if(str_val[0] == '#')
	{
		result = vlua_handler_present(curr_stats.vlua, str_val);
	}

	free(str_val);

	return var_from_bool(result);
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

	loc = var_to_str(call_info->argv[0]);
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

/* Runs the command in a shell and returns its output (joined standard output
 * and standard error streams).  All trailing newline characters are stripped to
 * allow easy appending to command output.  Returns the output. */
static var_t
system_builtin(const call_info_t *call_info)
{
	return execute_cmd(call_info->argv[0], call_info->interactive, 0);
}

/* Retrieves number of current or last tab page.  Returns integer value with the
 * number base one. */
static var_t
tabpagenr_builtin(const call_info_t *call_info)
{
	int first = 1;

	if(call_info->argc != 0)
	{
		char *const type = var_to_str(call_info->argv[0]);
		if(strcmp(type, "$") != 0)
		{
			vle_tb_append_linef(vle_err, "Invalid argument (expected \"$\"): %s",
					type);
			free(type);
			return var_error();
		}
		free(type);
		first = 0;
	}

	return var_from_int(first ? tabs_current(curr_view) + 1
	                          : tabs_count(curr_view));
}

/* Runs interactive command in a shell and returns its output (joined standard
 * output and standard error streams).  All trailing newline characters are
 * stripped to allow easy appending to command output.  Returns the output. */
static var_t
term_builtin(const call_info_t *call_info)
{
	ui_shutdown();
	return execute_cmd(call_info->argv[0], call_info->interactive, 1);
}

/* Runs interactive command in a shell and returns its output (joined standard
 * output and standard error streams).  All trailing newline characters are
 * stripped to allow easy appending to command output.  Returns the output. */
static var_t
execute_cmd(var_t cmd_arg, int interactive, int preserve_stdin)
{
	var_t result;
	char *cmd;
	FILE *cmd_stream;
	size_t cmd_out_len;
	char *result_str;

	cmd = var_to_str(cmd_arg);
	cmd_stream = read_cmd_output(cmd, preserve_stdin);
	free(cmd);

	if(interactive)
	{
		ui_cancellation_push_on();
	}
	else
	{
		ui_cancellation_push_off();
	}

	result_str = read_nonseekable_stream(cmd_stream, &cmd_out_len, NULL, NULL);
	fclose(cmd_stream);

	ui_cancellation_pop();

	if(result_str == NULL)
	{
		return var_from_str("");
	}

	/* Remove trailing new line characters. */
	while(cmd_out_len != 0U && result_str[cmd_out_len - 1] == '\n')
	{
		result_str[cmd_out_len - 1] = '\0';
		--cmd_out_len;
	}

	result = var_from_str(result_str);
	free(result_str);
	return result;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
