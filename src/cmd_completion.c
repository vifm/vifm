/* vifm
 * Copyright (C) 2001 Ken Steen.
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

#include "cmd_completion.h"

#ifdef _WIN32
#include <windows.h>
#include <lm.h>
#endif

#include <sys/stat.h> /* stat */
#include <dirent.h> /* DIR dirent */

#ifndef _WIN32
#include <grp.h> /* getgrent setgrent */
#include <pwd.h> /* getpwent setpwent */
#endif

#include <assert.h> /* assert() */
#include <stddef.h> /* NULL size_t */
#include <stdlib.h> /* free() */
#include <stdio.h> /* snprintf() */
#include <string.h> /* memcpy() strdup() strlen() strncasecmp() strncmp()
                       strrchr() */

#include "cfg/config.h"
#include "cfg/info.h"
#include "compat/dtype.h"
#include "compat/fs_limits.h"
#include "compat/os.h"
#include "engine/abbrevs.h"
#include "engine/cmds.h"
#include "engine/completion.h"
#include "engine/functions.h"
#include "engine/options.h"
#include "engine/variables.h"
#include "int/file_magic.h"
#include "int/path_env.h"
#include "lua/vlua.h"
#ifdef _WIN32
#include "menus/menus.h"
#endif
#include "modes/dialogs/msg_dialog.h"
#include "ui/color_scheme.h"
#include "ui/colors.h"
#include "ui/statusbar.h"
#include "utils/env.h"
#include "utils/fs.h"
#include "utils/macros.h"
#include "utils/matchers.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/utils.h"
#include "bmarks.h"
#include "cmd_core.h"
#include "filelist.h"
#include "filetype.h"
#include "plugins.h"
#include "tags.h"

/* State information for making completion. */
typedef struct
{
	/* External input. */
	int id;                     /* Command id. */
	const cmd_info_t *cmd_info; /* Command information. */
	int arg_pos;                /* Start position of completed argument. */
	void *extra_arg;            /* User data for the completion function. */

	const char *arg;    /* The argument being completed (last argument). */
	const char *start;  /* Completion position as pointer into cmd_info->args. */
	const char *slash;  /* Last slash in `arg`. */
	const char *dollar; /* Last dollar in `arg`. */
}
completion_data_t;

static int non_path_completion(completion_data_t *data);
static int path_completion(completion_data_t *data);
static int earg_num(int argc, const char cmdline[]);
static int cmd_ends_with_space(const char cmdline[]);
static int is_option(const cmd_info_t *cmd_info);
static void complete_compare(const char str[]);
static void complete_selective_sync(const char str[]);
static void complete_wincmd(const char str[]);
static void complete_tree(const char *str);
static void complete_help(const char *str);
static void complete_history(const char str[]);
static void complete_invert(const char str[]);
static int complete_chown(const char *str);
static void complete_filetype(const char *str);
static void complete_progs(const char *str, assoc_records_t records);
static void complete_plugin(const char str[], char *argv[], int arg_num);
static int complete_highlight(const char args[], const char arg[], int arg_num);
static void complete_highlight_groups(const char str[], int file_hi_only);
static int complete_highlight_arg(const char *str);
static void complete_envvar(const char str[]);
static int complete_select(completion_data_t *data);
static void complete_winrun(const char str[]);
static void complete_from_string_list(const char str[], const char *items[][2],
		size_t item_count, int ignore_case);
static void complete_command_name(const char beginning[]);
static int filename_completion_in_dir(const char path[], const char str[],
		CompletionType type);
static void filename_completion_internal(DIR *dir, const char dir_path[],
		const char filename[], CompletionType type);
static int is_dirent_targets_exec(const struct dirent *d);
#ifdef _WIN32
static void complete_with_shared(const char *server, const char *file);
#endif
static int file_matches(const char fname[], const char prefix[],
		size_t prefix_len);

int
complete_line(const char cmd_line[], void *extra_arg)
{
	if(!cfg.auto_cd)
	{
		return 0;
	}

	const char *slash = strrchr(cmd_line, '/');
	int offset = (slash == NULL ? 0 : slash - cmd_line + 1);
	offset += filename_completion(cmd_line, CT_DIRONLY, 0);
	return offset;
}

int
complete_args(int id, const cmd_info_t *cmd_info, int arg_pos, void *extra_arg)
{
	if(id == COM_FOREIGN)
	{
		return vlua_complete_cmd(curr_stats.vlua, cmd_info, arg_pos);
	}

	completion_data_t data = {
		.id = id,
		.cmd_info = cmd_info,
		.arg_pos = arg_pos,
		.extra_arg = extra_arg,

		.arg = cmd_info->args + arg_pos,
		.start = data.arg,
		.slash = strrchr(data.arg, '/'),
		.dollar = strrchr(data.arg, '$'),
	};

	int result = non_path_completion(&data);
	if(result >= 0)
	{
		return result;
	}

	return path_completion(&data);
}

/* Handles completion of non-path arguments.  Returns negative number if need to
 * do path completion, otherwise offset in original string is returned. */
static int
non_path_completion(completion_data_t *data)
{
	const char *const args = data->cmd_info->args;
	int argc = data->cmd_info->argc;
	char **const argv = data->cmd_info->argv;

	int emark = data->cmd_info->emark;
	int qmark = data->cmd_info->qmark;

	int id = data->id;
	const char *arg = data->arg;

	if(id == COM_SET || id == COM_SETLOCAL)
	{
		vle_opts_complete(args, &data->start,
				(id == COM_SET) ? OPT_GLOBAL : OPT_LOCAL);
	}
	else if(id == COM_CABBR)
	{
		vle_abbr_complete(args);
		data->start = args;
	}
	else if(command_accepts_expr(id))
	{
		complete_expr(arg, &data->start);
	}
	else if(id == COM_TREE)
	{
		complete_tree(arg);
	}
	else if(id == COM_UNLET)
		complete_variables(arg, &data->start);
	else if(id == COM_HELP)
		complete_help(args);
	else if(id == COM_HISTORY)
	{
		complete_history(args);
		data->start = args;
	}
	else if(id == COM_INVERT)
	{
		complete_invert(args);
		data->start = args;
	}
	else if(id == COM_CHOWN)
		data->start += complete_chown(args);
	else if(id == COM_FILE)
		complete_filetype(args);
	else if(id == COM_PLUGIN)
	{
		complete_plugin(args, argv, earg_num(argc, args));
	}
	else if(id == COM_HIGHLIGHT)
	{
		data->start += complete_highlight(args, arg, earg_num(argc, args));
	}
	else if((id == COM_CD || id == COM_PUSHD || id == COM_EXECUTE ||
			id == COM_SOURCE || id == COM_EDIT || id == COM_GOTO_PATH ||
			id == COM_TABNEW) && data->dollar != NULL && data->dollar > data->slash)
	{
		data->start = data->dollar + 1;
		complete_envvar(data->start);
	}
	else if(id == COM_SELECT)
	{
		return complete_select(data);
	}
	else if(id == COM_KEEPSEL || id == COM_WINDO)
		;
	else if(id == COM_WINRUN)
	{
		if(argc == 0)
			complete_winrun(args);
	}
	else if(id == COM_AUTOCMD)
	{
		/* Complete only first argument. */
		if(earg_num(argc, args) <= 1)
		{
			static const char *events[][2] = {
				{ "DirEnter", "occurs after directory is changed" },
			};
			complete_from_string_list(args, events, ARRAY_LEN(events), 1);
		}
	}
	else if(id == COM_BMARKS && (!emark || earg_num(argc, args) >= 2))
	{
		bmarks_complete(argc, argv, arg);
	}
	else if(id == COM_DELBMARKS && !emark)
	{
		bmarks_complete(argc, argv, arg);
	}
	else if(id == COM_COMPARE)
	{
		complete_compare(arg);
	}
	else if(id == COM_SYNC && emark)
	{
		complete_selective_sync(arg);
	}
	else if(id == COM_COLORSCHEME && earg_num(argc, args) <= 1)
	{
		cs_complete(arg);
	}
	else if(id == COM_SESSION || id == COM_DELSESSION)
	{
		if(earg_num(argc, args) <= 1 && !emark && !qmark)
		{
			sessions_complete(arg);
		}
	}
	else if(id == COM_WINCMD)
	{
		if(earg_num(argc, args) <= 1)
		{
			complete_wincmd(arg);
		}
	}
	else if(is_option(data->cmd_info) && (id == COM_COPY || id == COM_MOVE ||
				id == COM_ALINK || id == COM_RLINK))
	{
		static const char *lines[][2] = {
			{ "-skip", "skip files with conflicting names" }
		};
		complete_from_string_list(arg, lines, ARRAY_LEN(lines), /*ignore_case=*/0);
	}
	else
	{
		return -1;
	}

	return data->start - args;
}

/* Handles completion of path arguments.  Returns offset in original string. */
static int
path_completion(completion_data_t *data)
{
	const char *const args = data->cmd_info->args;
	int argc = data->cmd_info->argc;
	char **const argv = data->cmd_info->argv;

	char *free_me = NULL;
	size_t arg_num = argc;
	data->start = (data->slash == NULL) ? (args + data->arg_pos)
	                                    : (data->slash + 1U);

	if(argc > 0 && !cmd_ends_with_space(args))
	{
		if(ends_with(args, "\""))
		{
			return data->start - args;
		}
		if(ends_with(args, "'"))
		{
			return data->start - args;
		}
		arg_num = argc - 1;
		data->arg = argv[arg_num];
	}

	/* Pre-process input with requested method. */
	const CompletionPreProcessing cpp =
		(CompletionPreProcessing)(uintptr_t)data->extra_arg;
	if(cpp != CPP_NONE)
	{
		if(cpp != CPP_PERCENT_UNESCAPE)
		{
			data->arg = args + data->arg_pos + 1;
			data->start = (data->slash == NULL) ? data->arg : (data->slash + 1);
		}

		free_me = strdup(data->arg);
		data->arg = free_me;

		switch(cpp)
		{
			case CPP_PERCENT_UNESCAPE: expand_percent_escaping(free_me); break;
			case CPP_SQUOTES_UNESCAPE: expand_squotes_escaping(free_me); break;
			case CPP_DQUOTES_UNESCAPE: expand_dquotes_escaping(free_me); break;

			default:
				assert(0 && "Unhandled preprocessing type.");
				break;
		};
	}

	int id = data->id;
	const char *arg = data->arg;

	if(id == COM_COLORSCHEME)
	{
		if(arg_num == 1)
		{
			data->start += filename_completion(arg, CT_DIRONLY, 0);
		}
	}
	else if(id == COM_BMARKS || id == COM_DELBMARKS)
	{
		data->start += filename_completion(arg, CT_ALL, 0);
	}
	else if(id == COM_CD || id == COM_SYNC || id == COM_PUSHD ||
			id == COM_MKDIR || id == COM_TABNEW)
	{
		data->start += filename_completion(arg, CT_DIRONLY, 0);
	}
	else if(id == COM_COPY || id == COM_MOVE || id == COM_ALINK ||
			id == COM_RLINK)
	{
		data->start += filename_completion_in_dir(other_view->curr_dir, arg,
				CT_ALL);
	}
	else if(id == COM_SPLIT || id == COM_VSPLIT)
	{
		data->start += filename_completion_in_dir(flist_get_dir(curr_view), arg,
				CT_DIRONLY);
	}
	else if(id == COM_GREP)
	{
		if(earg_num(argc, args) > 1 && args[0] == '-')
		{
			data->start += filename_completion(arg, CT_DIRONLY, 1);
		}
	}
	else if(id == COM_FIND)
	{
		if(earg_num(argc, args) <= 1)
		{
			data->start += filename_completion(arg, CT_DIRONLY, 1);
		}
	}
	else if(id == COM_EXECUTE)
	{
		if(earg_num(argc, args) <= 1)
		{
			if(*arg == '.' || *arg == '~' || is_path_absolute(arg))
				data->start += filename_completion(arg, CT_DIREXEC, 0);
			else
				complete_command_name(arg);
		}
		else
			data->start += filename_completion(arg, CT_ALL, 0);
	}
	else if(id == COM_TOUCH || id == COM_RENAME)
	{
		data->start += filename_completion(arg, CT_ALL_WOS, 0);
	}
	else
	{
		data->start += filename_completion(arg, CT_ALL, 0);
	}

	free(free_me);
	return data->start - args;
}

/* Calculates effective number of argument being completed.  Returns the
 * number. */
static int
earg_num(int argc, const char cmdline[])
{
	return cmd_ends_with_space(cmdline) ? (argc + 1) : argc;
}

/* Checks whether given command-line ends with a spaces considering \-escaping.
 * Returns non-zero if so, otherwise zero is returned. */
static int
cmd_ends_with_space(const char cmdline[])
{
	while(cmdline[0] != '\0' && cmdline[1] != '\0')
	{
		if(cmdline[0] == '\\')
		{
			++cmdline;
		}
		++cmdline;
	}
	return (cmdline[0] == ' ');
}

/* Checks whether option completion should be performed.  Returns non-zero if
 * so, otherwise zero is returned. */
static int
is_option(const cmd_info_t *cmd_info)
{
	if(cmd_info->argc == 0)
	{
		return 0;
	}

	int i;
	for(i = 0; i < cmd_info->argc; ++i)
	{
		if(cmd_info->argv[i][0] != '-')
		{
			/* Implicit end of options. */
			return 0;
		}
		if(strcmp(cmd_info->argv[i], "--") == 0)
		{
			/* Explicit end of options. */
			return 0;
		}
	}

	return 1;
}

void
complete_expr(const char str[], const char **start)
{
	const char *ampersand = strrchr(str, '&');
	const char *dollar = strrchr(str, '$');
	if(ampersand > dollar)
	{
		OPT_SCOPE scope = OPT_GLOBAL;
		*start = ampersand + 1;

		if(starts_with_lit(*start, "l:"))
		{
			scope = OPT_LOCAL;
			*start += 2;
		}
		else if(starts_with_lit(*start, "g:"))
		{
			*start += 2;
		}

		vle_opts_complete_real(*start, scope);
	}
	else if(dollar == NULL && !starts_with_lit(str, "v:"))
	{
		function_complete_name(str, start);
	}
	else
	{
		complete_variables((dollar > str) ? dollar : str, start);
	}
}

/* Completes properties for directory comparison. */
static void
complete_compare(const char str[])
{
	static const char *lines[][2] = {
		{ "byname",     "compare by file name" },
		{ "bysize",     "compare by file size" },
		{ "bycontents", "compare by file size and hash" },

		{ "ofboth",     "use files of two views" },
		{ "ofone",      "use files of two current view only" },

		{ "listall",    "list all files" },
		{ "listunique", "list only unique files" },
		{ "listdups",   "list only duplicated files" },

		{ "groupids",   "group files in two panes by ids" },
		{ "grouppaths", "group files in two panes by paths" },

		{ "skipempty",  "exclude empty files from comparison" },
	};

	complete_from_string_list(str, lines, ARRAY_LEN(lines), 0);
}

/* Completes properties for selective synchronization. */
static void
complete_selective_sync(const char str[])
{
	static const char *lines[][2] = {
		{ "location",  "current directory" },
		{ "cursorpos", "cursor position" },
		{ "localopts", "values of view-specific options" },
		{ "filters",   "all filters" },
		{ "filelist",  "list of files for custom views" },
		{ "tree",      "tree structure for tree views" },
		{ "all",       "as much as possible" },
	};

	complete_from_string_list(str, lines, ARRAY_LEN(lines), 0);
}

/* Completes argument of :wincmd. */
static void
complete_wincmd(const char str[])
{
	static const char *lines[][2] = {
		{ "H", "move the pane to the far left" },
		{ "J", "move the pane to the very bottom" },
		{ "K", "move the pane to the very top" },
		{ "L", "move the pane to the far right" },

		{ "h", "switch to left pane" },
		{ "j", "switch to pane below" },
		{ "k", "switch to pane above" },
		{ "l", "switch to right pane" },

		{ "b", "switch to bottom-right window" },
		{ "t", "switch to top-left window" },

		{ "p", "switch to previous window" },
		{ "w", "switch to other pane" },

		{ "o", "leave only one pane" },
		{ "s", "split window horizontally" },
		{ "v", "split window vertically" },

		{ "x", "exchange panes" },
		{ "z", "quit preview pane or view modes" },

		{ "-", "decrease size of the view by count" },
		{ "+", "increase size of the view by count" },
		{ "<", "decrease size of the view by count" },
		{ ">", "increase size of the view by count" },

		{ "|", "set current view size to count" },
		{ "_", "set current view size to count" },
		{ "=", "make size of two views equal" },
	};

	complete_from_string_list(str, lines, ARRAY_LEN(lines), 0);
}

/* Completes arguments of :tree. */
static void
complete_tree(const char str[])
{
	static const char *lines[][2] = {
		{ "depth=", "maximum node nesting level before folding" },
	};

	complete_from_string_list(str, lines, ARRAY_LEN(lines), 0);
}

static void
complete_help(const char *str)
{
	int i;

	if(!cfg.use_vim_help)
	{
		return;
	}

	for(i = 0; tags[i] != NULL; i++)
	{
		if(strstr(tags[i], str) != NULL)
		{
			vle_compl_add_match(tags[i], "");
		}
	}
	vle_compl_finish_group();
	vle_compl_add_last_match(str);
}

/* Completes available kinds of histories. */
static void
complete_history(const char str[])
{
	static const char *lines[][2] = {
		{ ".",       "directory visit history" },
		{ "dir",     "directory visit history" },

		{ "@",       "prompt input (e.g. file name)" },
		{ "input",   "prompt input (e.g. file name)" },

		{ "/",       "search patterns (forward)" },
		{ "search",  "search patterns (forward)" },
		{ "fsearch", "search patterns (forward)" },

		{ "?",       "search patterns (backward)" },
		{ "bsearch", "search patterns (backward)" },

		{ ":",       "command-line commands" },
		{ "cmd",     "command-line commands" },

		{ "=",       "local filter patterns" },
		{ "filter",  "local filter patterns" },

		{ "exprreg", "expression register input" },
	};
	complete_from_string_list(str, lines, ARRAY_LEN(lines), 0);
}

/* Completes available inversion kinds. */
static void
complete_invert(const char str[])
{
	static const char *lines[][2] = {
		{ "f", "filter state" },
		{ "s", "file selection" },
		{ "o", "primary sorting key" },
	};
	complete_from_string_list(str, lines, ARRAY_LEN(lines), 0);
}

/* Completes either user or group name for the :chown command. */
static int
complete_chown(const char *str)
{
#ifndef _WIN32
	char *colon = strchr(str, ':');
	if(colon == NULL)
	{
		complete_user_name(str);
		return 0;
	}
	else
	{
		complete_group_name(colon + 1);
		return colon - str + 1;
	}
#else
	vle_compl_add_last_match(str);
	return 0;
#endif
}

static void
complete_filetype(const char *str)
{
	dir_entry_t *const curr = get_current_entry(curr_view);
	char *typed_fname;
	assoc_records_t ft;

	if(fentry_is_fake(curr))
	{
		return;
	}

	typed_fname = get_typed_entry_fpath(curr);
	ft = ft_get_all_programs(typed_fname);

	complete_progs(str, ft);
	ft_assoc_records_free(&ft);

	complete_progs(str, get_magic_handlers(typed_fname));

	vle_compl_finish_group();
	vle_compl_add_last_match(str);

	free(typed_fname);
}

static void
complete_progs(const char *str, assoc_records_t records)
{
	int i;
	const size_t len = strlen(str);

	for(i = 0; i < records.count; i++)
	{
		const assoc_record_t *const assoc = &records.list[i];
		char cmd[NAME_MAX + 1];

		(void)extract_cmd_name(assoc->command, 1, sizeof(cmd), cmd);

		if(strnoscmp(cmd, str, len) == 0)
		{
			vle_compl_put_match(escape_chars(cmd, "|"), assoc->description);
		}
	}
}

/* Completes plugin managing subcommands and their arguments. */
static void
complete_plugin(const char str[], char *argv[], int arg_num)
{
	if(arg_num == 0 || arg_num == 1)
	{
		static const char *subcommands[][2] = {
			{ "load",      "load plugins if they weren't loaded yet" },
			{ "blacklist", "avoid loading this plugin" },
			{ "whitelist", "ignore all but this and other whitelisted plugins" },
		};
		complete_from_string_list(str, subcommands, ARRAY_LEN(subcommands), 0);
	}
	else if(strcmp(argv[0], "blacklist") == 0 ||
			strcmp(argv[0], "whitelist") == 0)
	{
		plugs_complete(curr_stats.plugs, after_last(str, ' '));
	}
}

/* Completes highlight command.  Returns completion start offset. */
static int
complete_highlight(const char args[], const char arg[], int arg_num)
{
	if(arg_num <= 1)
	{
		complete_highlight_groups(args, 0);
		return 0;
	}

	if(starts_with_lit(args, "clear "))
	{
		if(arg_num == 2)
		{
			complete_highlight_groups(arg, 1);
		}
		return 0;
	}

	return complete_highlight_arg(arg);
}

/* Completes highlight groups and subcommand "clear" for :highlight command. */
static void
complete_highlight_groups(const char str[], int file_hi_only)
{
	int i;
	const size_t len = strlen(str);

	col_scheme_t *const cs = curr_stats.cs;
	for(i = 0; i < cs->file_hi_count; ++i)
	{
		const char *const expr = matchers_get_expr(cs->file_hi[i].matchers);
		if(strncasecmp(str, expr, len) == 0)
		{
			vle_compl_add_match(expr, "");
		}
	}

	if(!file_hi_only)
	{
		for(i = 0; i < MAXNUM_COLOR; ++i)
		{
			if(strncasecmp(str, HI_GROUPS[i], len) == 0)
			{
				vle_compl_add_match(HI_GROUPS[i], HI_GROUPS_DESCR[i]);
			}
		}

		if(strncmp(str, "clear", len) == 0)
		{
			vle_compl_add_match("clear", "clear color rules");
		}
	}

	vle_compl_finish_group();
	vle_compl_add_last_match(str);
}

static int
complete_highlight_arg(const char *str)
{
	/* TODO: Refactor this function complete_highlight_arg() */
	const char *equal = strchr(str, '=');
	int result = (equal == NULL) ? 0 : (equal - str + 1);
	size_t len = strlen((equal == NULL) ? str : ++equal);
	if(equal == NULL)
	{
		static const char *const args[][2] = {
			{ "cterm",   "text attributes" },
			{ "ctermfg", "foreground color" },
			{ "ctermbg", "background color" },
			{ "gui",     "text attributes for direct colors" },
			{ "guifg",   "foreground direct color" },
			{ "guibg",   "background direct color" },
		};

		size_t i;

		for(i = 0U; i < ARRAY_LEN(args); ++i)
		{
			if(strncmp(str, args[i][0], len) == 0)
			{
				vle_compl_add_match(args[i][0], args[i][1]);
			}
		}
	}
	else
	{
		if(strncmp(str, "cterm", equal - str - 1) == 0 ||
				strncmp(str, "gui", equal - str - 1) == 0)
		{
			static const char *const STYLES[][2] = {
				{ "bold",      "bold text, lighter color" },
				{ "underline", "underlined text" },
				{ "reverse",   "reversed colors" },
				{ "inverse",   "reversed colors" },
				{ "standout",  "like bold or similar to it" },
				{ "italic",    "on unsupported systems becomes reverse" },
				{ "combine",   "combine attributes with previous level" },
				{ "none",      "no attributes" },
			};

			size_t i;

			const char *const comma = strrchr(equal, ',');
			if(comma != NULL)
			{
				result += comma - equal + 1;
				equal = comma + 1;
				len = strlen(equal);
			}

			for(i = 0U; i < ARRAY_LEN(STYLES); ++i)
			{
				if(strncasecmp(equal, STYLES[i][0], len) == 0)
				{
					vle_compl_add_match(STYLES[i][0], STYLES[i][1]);
				}
			}
		}
		else
		{
			if(strncasecmp(equal, "default", len) == 0)
			{
				vle_compl_add_match("default", "default or transparent color");
			}
			if(strncasecmp(equal, "none", len) == 0)
			{
				vle_compl_add_match("none", "no specific attributes");
			}

			int is_gui = 0;
			if(strncmp(str, "guifg", equal - str - 1) == 0 ||
					strncmp(str, "guibg", equal - str - 1) == 0)
			{
				is_gui = 1;
			}

			size_t i;
			size_t color_limit = (is_gui ? 8 : ARRAY_LEN(XTERM256_COLOR_NAMES));

			for(i = 0U; i < color_limit; ++i)
			{
				if(strncasecmp(equal, XTERM256_COLOR_NAMES[i], len) == 0)
				{
					vle_compl_add_match(XTERM256_COLOR_NAMES[i], "");
				}
			}

			for(i = 0U; !is_gui && i < ARRAY_LEN(LIGHT_COLOR_NAMES); ++i)
			{
				if(strncasecmp(equal, LIGHT_COLOR_NAMES[i], len) == 0)
				{
					vle_compl_add_match(LIGHT_COLOR_NAMES[i], "");
				}
			}
		}
	}
	vle_compl_finish_group();
	vle_compl_add_last_match((equal == NULL) ? str : equal);
	return result;
}

/* Completes name of the environment variables. */
static void
complete_envvar(const char str[])
{
	const size_t len = strlen(str);

	int env_count;
	char **const env_lst = env_list(&env_count);

	int i;
	for(i = 0; i < env_count; ++i)
	{
		if(strncmp(env_lst[i], str, len) == 0)
		{
			vle_compl_add_match(env_lst[i], env_get(env_lst[i]));
		}
	}

	free_string_array(env_lst, env_count);

	vle_compl_finish_group();
	vle_compl_add_last_match(str);
}

/* Completes select command.  Returns completion start offset. */
static int
complete_select(completion_data_t *data)
{
	const char *const args = data->cmd_info->args;

	/* Make sure that it's a filter-argument. */
	if(args[0] != '!' || char_is_one_of("/{", args[1]))
	{
		return data->start - args;
	}

	/* Fake !-command completion by hiding "!" in front and calling this
		* function again. */
	cmd_info_t exec_info = *data->cmd_info;
	char *exec_argv[exec_info.argc];

	++exec_info.args;
	if(exec_info.args[0] == '\0')
	{
		exec_info.argc = 0;
	}

	exec_info.argv = exec_argv;
	memcpy(exec_argv, data->cmd_info->argv, sizeof(exec_argv));
	++exec_argv[0];

	int arg_pos = data->arg_pos;
	if(arg_pos != 0)
	{
		--arg_pos;
	}

	return 1 + complete_args(COM_EXECUTE, &exec_info, arg_pos, data->extra_arg);
}

/* Completes first :winrun argument. */
static void
complete_winrun(const char str[])
{
	static const char *win_marks[][2] = {
		{ "^", "left/top view" },
		{ "$", "right/bottom view" },
		{ "%", "both views" },
		{ ".", "active view" },
		{ ",", "inactive view" },
	};
	complete_from_string_list(str, win_marks, ARRAY_LEN(win_marks), 0);
}

/* Performs str completion using items in the list of length item_count. */
static void
complete_from_string_list(const char str[], const char *items[][2],
		size_t item_count, int ignore_case)
{
	size_t i;
	const size_t prefix_len = strlen(str);
	for(i = 0U; i < item_count; ++i)
	{
		const int cmp = ignore_case
		              ? strncasecmp(str, items[i][0], prefix_len)
		              : strncmp(str, items[i][0], prefix_len);

		if(cmp == 0)
		{
			vle_compl_add_match(items[i][0], items[i][1]);
		}
	}
	vle_compl_finish_group();
	vle_compl_add_last_match(str);
}

char *
fast_run_complete(const char cmd[])
{
	char *result = NULL;
	const char *args;
	char command[NAME_MAX + 1];
	char *completed;

	args = extract_cmd_name(cmd, 0, sizeof(command), command);

	if(is_path_absolute(command) || command[0] == '~')
	{
		return strdup(cmd);
	}

	vle_compl_reset();
	complete_command_name(command);
	vle_compl_unite_groups();
	completed = vle_compl_next();

	if(vle_compl_get_count() > 2)
	{
		int c = vle_compl_get_count() - 1;
		while(c-- > 0)
		{
			if(stroscmp(command, completed) == 0)
			{
				result = strdup(cmd);
				break;
			}
			else
			{
				free(completed);
				completed = vle_compl_next();
			}
		}

		if(result == NULL)
		{
			ui_sb_err("Command beginning is ambiguous");
		}
	}
	else
	{
		free(completed);
		completed = vle_compl_next();
		result = format_str("%s %s", completed, args);
	}
	free(completed);

	return result;
}

/* Fills list of completions with executables in $PATH. */
static void
complete_command_name(const char beginning[])
{
	size_t i;
	char ** paths;
	size_t paths_count;
	char *const cwd = save_cwd();

	paths = get_paths(&paths_count);
	for(i = 0U; i < paths_count; ++i)
	{
		if(vifm_chdir(paths[i]) == 0)
		{
			filename_completion(beginning, CT_EXECONLY, 1);
		}
	}
	vle_compl_add_last_path_match(beginning);

	restore_cwd(cwd);
}

/* Does filename completion outside current working directory.  Returns
 * completion start offset. */
static int
filename_completion_in_dir(const char path[], const char str[],
		CompletionType type)
{
	char buf[PATH_MAX + 1];
	if(is_root_dir(str))
	{
		copy_str(buf, sizeof(buf), str);
	}
	else
	{
		snprintf(buf, sizeof(buf), "%s/%s", path, str);
	}
	return filename_completion(buf, type, 0);
}

/*
 * type: CT_*
 */
int
filename_completion(const char str[], CompletionType type,
		int skip_canonicalization)
{
	/* TODO refactor filename_completion(...) function */
	DIR *dir;
	char *filename;
	char *temp;
	char *cwd;

	char *dirname = expand_tilde(str);
	if(dirname == NULL)
	{
		return 0;
	}

	if(str[0] == '~' && strchr(str, '/') == NULL)
	{
		if(dirname[0] != '~')
		{
			/* expand_tilde() did expand the ~user, just return it as it's a complete
			 * match. */
			vle_compl_put_path_match(dirname);
			return 0;
		}

#ifndef _WIN32
		free(dirname);
		complete_user_name(str + 1);
		return 1;
#endif
	}

	filename = strdup(dirname);
	if(filename == NULL)
	{
		free(dirname);
		return 0;
	}

	temp = cmds_expand_envvars(dirname);
	free(dirname);
	dirname = temp;

	temp = strrchr(dirname, '/');
	if(temp != NULL && type != CT_FILE && type != CT_FILE_WOE)
	{
		strcpy(filename, ++temp);
		*temp = '\0';
	}
	else if(replace_string(&dirname, ".") != 0)
	{
		free(filename);
		free(dirname);
		return 0;
	}

	if(!skip_canonicalization)
	{
		char canonic_path[PATH_MAX + 1];
		to_canonic_path(dirname, flist_get_dir(curr_view), canonic_path,
				sizeof(canonic_path));
		if(replace_string(&dirname, canonic_path) != 0)
		{
			free(filename);
			free(dirname);
			return 0;
		}
	}

#ifdef _WIN32
	if(is_unc_root(dirname) ||
			(stroscmp(dirname, ".") == 0 && is_unc_root(curr_view->curr_dir)) ||
			(stroscmp(dirname, "/") == 0 && is_unc_path(curr_view->curr_dir)))
	{
		char buf[PATH_MAX + 1];
		if(!is_unc_root(dirname))
			copy_str(buf,
					strchr(curr_view->curr_dir + 2, '/') - curr_view->curr_dir + 1,
					curr_view->curr_dir);
		else
			copy_str(buf, sizeof(buf), dirname);

		complete_with_shared(buf, filename);
		free(filename);
		free(dirname);
		return 0;
	}
	if(is_unc_path(curr_view->curr_dir))
	{
		char buf[PATH_MAX + 1];
		if(is_path_absolute(dirname) && !is_unc_root(curr_view->curr_dir))
			copy_str(buf,
					strchr(curr_view->curr_dir + 2, '/') - curr_view->curr_dir + 2,
					curr_view->curr_dir);
		else
			copy_str(buf, sizeof(buf), curr_view->curr_dir);
		strcat(buf, dirname);
		chosp(buf);
		(void)replace_string(&dirname, buf);
	}
#endif

	dir = os_opendir(dirname);

	cwd = save_cwd();

	if(dir == NULL || vifm_chdir(dirname) != 0)
	{
		vle_compl_add_path_match(filename);
	}
	else
	{
		filename_completion_internal(dir, dirname, filename, type);
		(void)vifm_chdir(flist_get_dir(curr_view));
	}

	free(filename);
	free(dirname);

	if(dir != NULL)
	{
		os_closedir(dir);
	}

	restore_cwd(cwd);
	return 0;
}

/* The file completion core of filename_completion(). */
static void
filename_completion_internal(DIR *dir, const char dir_path[],
		const char filename[], CompletionType type)
{
	/* It's OK to use relative paths here, because filename_completion()
	 * guarantees that we are in correct directory. */
	struct dirent *d;

	size_t filename_len = strlen(filename);
	while((d = os_readdir(dir)) != NULL)
	{
		int is_dir;

		if(filename[0] == '\0' && d->d_name[0] == '.')
			continue;
		if(!file_matches(d->d_name, filename, filename_len))
			continue;

		is_dir = is_dirent_targets_dir(d->d_name, d);

		if(type == CT_DIRONLY && !is_dir)
			continue;
		else if(type == CT_EXECONLY && !is_dirent_targets_exec(d))
			continue;
		else if(type == CT_DIREXEC && !is_dir && !is_dirent_targets_exec(d))
			continue;

		if(is_dir && type != CT_ALL_WOS)
		{
			vle_compl_put_path_match(format_str("%s/", d->d_name));
		}
		else
		{
			vle_compl_add_path_match(d->d_name);
		}
	}

	vle_compl_finish_group();
	if(type != CT_EXECONLY)
	{
		vle_compl_add_last_path_match(filename);
	}
}

/* Uses dentry to check file type.  Returns non-zero for directories,
 * otherwise zero is returned.  Symbolic links are dereferenced. */
static int
is_dirent_targets_exec(const struct dirent *d)
{
	/* It's OK to use relative paths here, because filename_completion()
	 * guarantees that we are in correct directory. */
#ifndef _WIN32
	const unsigned char type = get_dirent_type(d, d->d_name);
	if(type == DT_DIR)
		return 0;
	if(type == DT_LNK && get_symlink_type(d->d_name) != SLT_UNKNOWN)
		return 0;
	return os_access(d->d_name, X_OK) == 0;
#else
	return is_win_executable(d->d_name);
#endif
}

#ifndef _WIN32

void
complete_user_name(const char str[])
{
	struct passwd *pw;
	size_t len;

	len = strlen(str);
	setpwent();
	while((pw = getpwent()) != NULL)
	{
		if(strncmp(pw->pw_name, str, len) == 0)
		{
			vle_compl_add_match(pw->pw_name, "");
		}
	}
	vle_compl_finish_group();
	vle_compl_add_last_match(str);
}

void
complete_group_name(const char *str)
{
	struct group *gr;
	size_t len = strlen(str);

	setgrent();
	while((gr = getgrent()) != NULL)
	{
		if(strncmp(gr->gr_name, str, len) == 0)
		{
			vle_compl_add_match(gr->gr_name, "");
		}
	}
	vle_compl_finish_group();
	vle_compl_add_last_match(str);
}

#else

static void
complete_with_shared(const char *server, const char *file)
{
	NET_API_STATUS res;
	size_t len = strlen(file);

	do
	{
		PSHARE_INFO_502 buf_ptr;
		DWORD er = 0, tr = 0, resume = 0;
		wchar_t *wserver = to_wide(server + 2);

		if(wserver == NULL)
		{
			show_error_msg("Memory Error", "Unable to allocate enough memory");
			return;
		}

		res = NetShareEnum(wserver, 502, (LPBYTE *)&buf_ptr, -1, &er, &tr, &resume);
		free(wserver);
		if(res == ERROR_SUCCESS || res == ERROR_MORE_DATA)
		{
			PSHARE_INFO_502 p;
			DWORD i;

			p = buf_ptr;
			for(i = 1; i <= er; i++)
			{
				char buf[512];
				WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)p->shi502_netname, -1, buf,
						sizeof(buf), NULL, NULL);
				strcat(buf, "/");
				if(file_matches(buf, file, len))
				{
					vle_compl_put_match(shell_like_escape(buf, 1), "");
				}
				p++;
			}
			NetApiBufferFree(buf_ptr);
		}
	}
	while(res == ERROR_MORE_DATA);
}

#endif

/* Checks whether file name has specified prefix accounting for system kind and
 * case sensitivity options.  Returns non-zero if so, otherwise zero is
 * returned. */
static int
file_matches(const char fname[], const char prefix[], size_t prefix_len)
{
	int cmp;
	if(cfg.case_override & CO_PATH_COMPL)
	{
		cmp = (cfg.case_ignore & CO_PATH_COMPL)
		    ? strncasecmp(fname, prefix, prefix_len)
		    : strncmp(fname, prefix, prefix_len);
	}
	else
	{
		cmp = strnoscmp(fname, prefix, prefix_len);
	}
	return (cmp == 0);
}

int
external_command_exists(const char cmd[])
{
	if(vlua_handler_cmd(curr_stats.vlua, cmd))
	{
		return vlua_handler_present(curr_stats.vlua, cmd);
	}

	if(curr_stats.shell_type == ST_CMD || curr_stats.shell_type == ST_YORI)
	{
		/* This is a builtin command. */
		if(stroscmp(cmd, "start") == 0)
		{
			return 1;
		}
	}

	char path[PATH_MAX + 1];
	if(get_cmd_path(cmd, sizeof(path), path) == 0)
	{
		return executable_exists(path);
	}

	return 0;
}

int
get_cmd_path(const char cmd[], size_t path_len, char path[])
{
	if(starts_with(cmd, "!!"))
	{
		cmd += 2;
	}

	if(contains_slash(cmd))
	{
		char *const expanded = replace_tilde(expand_envvars(cmd, EEF_NONE));
		copy_str(path, path_len, expanded);
		free(expanded);
		return 0;
	}

	return find_cmd_in_path(cmd, path_len, path);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
