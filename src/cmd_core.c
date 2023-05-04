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

#include "cmd_core.h"

#include <curses.h>

#include <sys/stat.h> /* gid_t uid_t */
#include <unistd.h> /* unlink() */

#include <assert.h> /* assert() */
#include <ctype.h> /* isspace() */
#include <errno.h> /* errno */
#include <limits.h> /* INT_MAX */
#include <signal.h>
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* strcmp() strcpy() strlen() */

#include "cfg/config.h"
#include "cfg/info.h"
#include "compat/fs_limits.h"
#include "compat/os.h"
#include "engine/autocmds.h"
#include "engine/cmds.h"
#include "engine/mode.h"
#include "engine/parsing.h"
#include "engine/var.h"
#include "engine/variables.h"
#include "int/vim.h"
#include "modes/dialogs/msg_dialog.h"
#include "modes/modes.h"
#include "modes/normal.h"
#include "modes/view.h"
#include "modes/visual.h"
#include "ui/color_scheme.h"
#include "ui/colors.h"
#include "ui/fileview.h"
#include "ui/statusbar.h"
#include "ui/ui.h"
#include "utils/file_streams.h"
#include "utils/filter.h"
#include "utils/fs.h"
#include "utils/hist.h"
#include "utils/int_stack.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/test_helpers.h"
#include "utils/utils.h"
#include "bracket_notation.h"
#include "cmd_completion.h"
#include "cmd_handlers.h"
#include "filelist.h"
#include "filtering.h"
#include "flist_sel.h"
#include "macros.h"
#include "marks.h"
#include "opt_handlers.h"
#include "undo.h"

/* Type of command arguments. */
typedef enum
{
	CAT_REGULAR,       /* Can be separated by a |. */
	CAT_EXPR,          /* Accept expressions with || and terminate on |. */
	CAT_UNTIL_THE_END, /* Take the rest of line including all |. */
}
CmdArgsType;

/* Values for if_levels stack. */
typedef enum
{
	IF_SCOPE_GUARD,  /* Command scope marker, prevents mixing of levels. */
	IF_BEFORE_MATCH, /* Before condition that evaluates to true is found. */
	IF_MATCH,        /* Just found true condition and processing that branch. */
	IF_AFTER_MATCH,  /* Left branch corresponding to true condition. */
	IF_ELSE,         /* Else branch that should be run (no matches before it). */
	IF_FINISH,       /* After else branch, only endif is expected by now. */
}
IfFrame;

static int swap_range(void);
static int resolve_mark(char mark);
static char * cmds_expand_macros(const char str[], int for_shell, int *usr1,
		int *usr2);
static void prepare_extcmd_file(FILE *fp, const char beginning[],
		CmdInputType type);
static hist_t * history_by_type(CmdInputType type);
static char * get_file_first_line(const char path[]);
static void execute_extcmd(const char command[], CmdInputType type);
static void save_extcmd(const char command[], CmdInputType type);
static void post(int id);
TSTATIC void cmds_select_range(int id, const cmd_info_t *cmd_info);
static int skip_at_beginning(int id, const char args[]);
static void create_builtin_vars(void);
static void session_changed_callback(const char new_session[]);
static char * pattern_expand_hook(const char pattern[]);
static int is_implicit_cd(view_t *view, const char cmd[], int cmd_error);
static int cmd_should_be_processed(int cmd_id);
TSTATIC char ** break_cmdline(const char cmdline[], int for_menu);
static int is_out_of_arg(const char cmd[], const char pos[]);
TSTATIC int line_pos(const char begin[], const char end[], char sep,
		int rquoting, int max_args);
static CmdArgsType get_cmd_args_type(const char cmd[]);
static char * skip_to_cmd_name(const char cmd[]);
static int repeat_command(view_t *view, CmdInputType type);
static int is_at_scope_bottom(const int_stack_t *scope_stack);

/* Settings for the cmds unit. */
static cmds_conf_t cmds_conf = {
	.complete_line = &complete_line,
	.complete_args = &complete_args,
	.swap_range = &swap_range,
	.resolve_mark = &resolve_mark,
	.expand_macros = &cmds_expand_macros,
	.expand_envvars = &cmds_expand_envvars,
	.post = &post,
	.select_range = &cmds_select_range,
	.skip_at_beginning = &skip_at_beginning,
};

/* Shows whether view selection should be preserved on command-line finishing.
 * By default it's reset. */
static int keep_view_selection;
/* Stores condition evaluation result for all nesting if-endif statements as
 * well as file scope marks (SCOPE_GUARD). */
static int_stack_t if_levels;

static int
swap_range(void)
{
	return prompt_msg("Command Error", "Backwards range given, OK to swap?");
}

static int
resolve_mark(char mark)
{
	const int result = marks_find_in_view(curr_view, mark);
	if(result < 0)
	{
		ui_sb_errf("Trying to use an invalid mark: '%c", mark);
	}
	return result;
}

/* Implementation of macros expansion callback for cmds unit.  Returns newly
 * allocated memory. */
static char *
cmds_expand_macros(const char str[], int for_shell, int *usr1, int *usr2)
{
	char *result;
	MacroFlags flags = MF_NONE;

	result = ma_expand(str, NULL, &flags, for_shell ? MER_SHELL_OP : MER_OP);

	*usr1 = flags;

	return result;
}

char *
cmds_expand_envvars(const char str[])
{
	return expand_envvars(str, EEF_ESCAPE_VALS | EEF_KEEP_ESCAPES);
}

void
cmds_run_ext(const char line[], size_t line_pos, CmdInputType type)
{
	char *const cmd = cmds_get_ext(line, line_pos, type);
	if(cmd == NULL)
	{
		save_extcmd(line, type);
	}
	else
	{
		save_extcmd(cmd, type);
		execute_extcmd(cmd, type);
		free(cmd);
	}
}

char *
cmds_get_ext(const char beginning[], size_t line_pos, CmdInputType type)
{
	char cmd_file[PATH_MAX + 1];
	FILE *file = make_file_in_tmp("vifm.cmdline", 0600, /*auto_delete=*/0,
			cmd_file, sizeof(cmd_file));
	if(file == NULL)
	{
		show_error_msgf("External Editing", "Failed to create a temporary file: %s",
				strerror(errno));
		return NULL;
	}

	prepare_extcmd_file(file, beginning, type);
	fclose(file);

	char *cmd = NULL;
	if(vim_view_file(cmd_file, 1, line_pos, 0) == 0)
	{
		cmd = get_file_first_line(cmd_file);
	}

	unlink(cmd_file);
	return cmd;
}

/* Fills the file with history (more recent goes first). */
static void
prepare_extcmd_file(FILE *fp, const char beginning[], CmdInputType type)
{
	const int is_cmd = (type == CIT_COMMAND);
	const hist_t *const hist = history_by_type(type);
	int i;

	fprintf(fp, "%s\n", beginning);
	for(i = 0; i < hist->size; i++)
	{
		fprintf(fp, "%s\n", hist->items[i].text);
	}

	if(is_cmd)
	{
		fputs("\" vim: set filetype=vifm-cmdedit syntax=vifm :\n", fp);
	}
	else
	{
		fputs("\" vim: set filetype=vifm-edit :\n", fp);
	}
}

/* Picks history by command type.  Returns pointer to history. */
static hist_t *
history_by_type(CmdInputType type)
{
	switch(type)
	{
		case CIT_COMMAND:
			return &curr_stats.cmd_hist;
		case CIT_MENU_COMMAND:
			return &curr_stats.menucmd_hist;
		case CIT_PROMPT_INPUT:
			return &curr_stats.prompt_hist;
		case CIT_EXPRREG_INPUT:
			return &curr_stats.exprreg_hist;
		case CIT_FILTER_PATTERN:
			return &curr_stats.filter_hist;

		case CIT_FSEARCH_PATTERN:
		case CIT_BSEARCH_PATTERN:
		case CIT_VFSEARCH_PATTERN:
		case CIT_VBSEARCH_PATTERN:
		case CIT_VWFSEARCH_PATTERN:
		case CIT_VWBSEARCH_PATTERN:
			return &curr_stats.search_hist;
	}

	/* Should never happen. */
	return NULL;
}

/* Reads the first line of the file specified by the path.  Returns NULL on
 * error or an empty file, otherwise a newly allocated string, which should be
 * freed by the caller, is returned. */
static char *
get_file_first_line(const char path[])
{
	FILE *const fp = os_fopen(path, "rb");
	char *result = NULL;
	if(fp != NULL)
	{
		result = read_line(fp, NULL);
		fclose(fp);
	}
	return result;
}

/* Executes the command of the type. */
static void
execute_extcmd(const char command[], CmdInputType type)
{
	if(type == CIT_COMMAND)
	{
		cmds_scope_start();
		curr_stats.save_msg = cmds_dispatch(command, curr_view, type);
		if(cmds_scope_finish() != 0)
		{
			curr_stats.save_msg = 1;
		}
	}
	else
	{
		curr_stats.save_msg = cmds_dispatch1(command, curr_view, type);
	}
}

/* Saves the command to the appropriate history. */
static void
save_extcmd(const char command[], CmdInputType type)
{
	if(type == CIT_COMMAND)
	{
		hists_commands_save(command);
	}
	else
	{
		hists_search_save(command);
	}
}

int
cmds_goes_to_history(const char command[])
{
	/* Don't add :!! or :! to history list. */
	return strcmp(command, "!!") != 0 && strcmp(command, "!") != 0;
}

int
cmds_has_expr_args(int cmd_id)
{
	return cmd_id == COM_ECHO
	    || cmd_id == COM_EXE
	    || cmd_id == COM_IF_STMT
	    || cmd_id == COM_ELSEIF_STMT
	    || cmd_id == COM_LET;
}

char *
cmds_insertion_escape(const char cmd_line[], int pos, const char str[])
{
	const CmdLineLocation ipt = cmds_classify_pos(cmd_line, cmd_line + pos);
	switch(ipt)
	{
		case CLL_R_QUOTING:
			/* XXX: Use of filename escape, while special one might be needed. */
		case CLL_OUT_OF_ARG:
		case CLL_NO_QUOTING:
			return posix_like_escape(str, /*type=*/0);

		case CLL_S_QUOTING:
			return escape_for_squotes(str, 0);

		case CLL_D_QUOTING:
			return escape_for_dquotes(str, 0);

		default:
			return NULL;
	}
}

static void
post(int id)
{
	/* When one command in a sequence enters visual mode, we don't want to break
	 * it by resetting selection. */
	if(!keep_view_selection && vle_mode_is(NORMAL_MODE))
	{
		flist_sel_stash_if_nonempty(curr_view);
	}

	/* Marking made by this unit is for a single command only and shouldn't be
	 * processed outside of that command. */
	curr_view->pending_marking = 0;
}

TSTATIC void
cmds_select_range(int id, const cmd_info_t *cmd_info)
{
	if(curr_view->pending_marking)
	{
		/* It must come from parent :command, so keep it. */
		return;
	}

	int mark_current = (id != COM_FIND && id != COM_GREP);
	curr_view->pending_marking = flist_sel_range(curr_view, cmd_info->begin,
			cmd_info->end, mark_current);
}

/* Command prefix remover for command parsing unit.  Returns < 0 to do nothing
 * or x to skip command name and x chars. */
static int
skip_at_beginning(int id, const char args[])
{
	if(id == COM_KEEPSEL || id == COM_WINDO)
	{
		return 0;
	}

	if(id == COM_WINRUN)
	{
		args = vle_cmds_at_arg(args);
		if(*args != '\0')
		{
			return 1;
		}
	}
	return -1;
}

void
cmds_init(void)
{
	if(cmds_conf.inner != NULL)
	{
		vle_cmds_init(1, &cmds_conf);
		return;
	}

	/* We get here when cmds_init() is called the first time. */

	vle_cmds_init(1, &cmds_conf);
	vle_cmds_add(cmds_list, cmds_list_size);

	/* Initialize modules used by this one. */
	init_bracket_notation();
	init_variables();

	create_builtin_vars();
	sessions_set_callback(&session_changed_callback);

	vle_aucmd_set_expand_hook(&pattern_expand_hook);
}

/* Creates builtin variables with some default values. */
static void
create_builtin_vars(void)
{
	var_t var = var_from_int(0);
	setvar("v:count", var);
	setvar("v:count1", var);
	var_free(var);

	var = var_from_str("");
	setvar("v:session", var);
	var_free(var);

	var = var_from_int(0);
	setvar("v:jobcount", var);
	var_free(var);
}

/* Callback to be invoked when active session has changed. */
static void
session_changed_callback(const char new_session[])
{
	var_t var = var_from_str(new_session);
	setvar("v:session", var);
	var_free(var);
}

void
cmds_vars_set_count(int count)
{
	var_t var;

	var = var_from_int(count == NO_COUNT_GIVEN ? 0 : count);
	setvar("v:count", var);
	var_free(var);

	var = var_from_int(count == NO_COUNT_GIVEN ? 1 : count);
	setvar("v:count1", var);
	var_free(var);
}

/* Performs custom pattern expansion.  Allocate new expanded string. */
static char *
pattern_expand_hook(const char pattern[])
{
	char *const no_tilde = expand_tilde(pattern);
	char *const expanded_pattern = expand_envvars(no_tilde, EEF_NONE);
	free(no_tilde);

	return expanded_pattern;
}

int
cmds_exec(const char cmd[], view_t *view, int menu, int keep_sel)
{
	int id;
	int result;

	if(cmd == NULL)
	{
		flist_sel_stash_if_nonempty(view);
		return 0;
	}

	cmd = skip_to_cmd_name(cmd);

	if(cmd[0] == '"')
		return 0;

	if(cmd[0] == '\0' && !menu)
	{
		flist_sel_stash_if_nonempty(view);
		return 0;
	}

	if(!menu)
	{
		vle_cmds_init(1, &cmds_conf);
		cmds_conf.begin = 0;
		cmds_conf.current = view->list_pos;
		cmds_conf.end = view->list_rows - 1;
	}

	id = vle_cmds_identify(cmd);

	if(!cmd_should_be_processed(id))
	{
		return 0;
	}

	if(id == USER_CMD_ID)
	{
		char undo_msg[COMMAND_GROUP_INFO_LEN];

		snprintf(undo_msg, sizeof(undo_msg), "in %s: %s",
				replace_home_part(flist_get_dir(view)), cmd);

		un_group_open(undo_msg);
		un_group_close();
	}

	keep_view_selection = keep_sel;
	result = vle_cmds_run(cmd);

	if(result >= 0)
		return result;

	if(is_implicit_cd(view, cmd, result))
	{
		return cd(view, flist_get_dir(view), cmd);
	}

	switch(result)
	{
		case CMDS_ERR_LOOP:
			ui_sb_err("Loop in commands");
			break;
		case CMDS_ERR_NO_MEM:
			ui_sb_err("Unable to allocate enough memory");
			break;
		case CMDS_ERR_TOO_FEW_ARGS:
			ui_sb_err("Too few arguments");
			break;
		case CMDS_ERR_TRAILING_CHARS:
			ui_sb_err("Trailing characters");
			break;
		case CMDS_ERR_INCORRECT_NAME:
			ui_sb_err("Incorrect command name");
			break;
		case CMDS_ERR_NEED_BANG:
			ui_sb_err("Add bang to force");
			break;
		case CMDS_ERR_NO_BUILTIN_REDEFINE:
			ui_sb_err("Can't redefine builtin command");
			break;
		case CMDS_ERR_INVALID_CMD:
			ui_sb_err("Invalid command name");
			break;
		case CMDS_ERR_NO_BANG_ALLOWED:
			ui_sb_err("No ! is allowed");
			break;
		case CMDS_ERR_NO_RANGE_ALLOWED:
			ui_sb_err("No range is allowed");
			break;
		case CMDS_ERR_NO_QMARK_ALLOWED:
			ui_sb_err("No ? is allowed");
			break;
		case CMDS_ERR_INVALID_RANGE:
			ui_sb_err("Invalid range");
			break;
		case CMDS_ERR_NO_SUCH_UDF:
			ui_sb_err("No such user defined command");
			break;
		case CMDS_ERR_UDF_IS_AMBIGUOUS:
			ui_sb_err("Ambiguous use of user-defined command");
			break;
		case CMDS_ERR_INVALID_ARG:
			ui_sb_err("Invalid argument");
			break;
		case CMDS_ERR_CUSTOM:
			/* error message is posted by command handler */
			break;
		default:
			ui_sb_err("Unknown error");
			break;
	}

	if(!menu && vle_mode_is(NORMAL_MODE))
	{
		flist_sel_stash_if_nonempty(view);
	}

	return -1;
}

/* Whether need to process command-line command as implicit :cd.  Returns
 * non-zero if so, otherwise zero is returned. */
static int
is_implicit_cd(view_t *view, const char cmd[], int cmd_error)
{
	if(!cfg.auto_cd)
	{
		return 0;
	}

	if(cmd_error != CMDS_ERR_INVALID_CMD && cmd_error != CMDS_ERR_TRAILING_CHARS)
	{
		return 0;
	}

	char *expanded = expand_tilde(cmd);
	int exists = path_exists_at(flist_get_dir(view), expanded, NODEREF);
	free(expanded);
	return exists;
}

/* Decides whether next command with id cmd_id should be processed or not,
 * taking state of conditional statements into account.  Returns non-zero if the
 * command should be processed, otherwise zero is returned. */
static int
cmd_should_be_processed(int cmd_id)
{
	static int skipped_nested_if_stmts;

	if(is_at_scope_bottom(&if_levels) || int_stack_top_is(&if_levels, IF_MATCH)
			|| int_stack_top_is(&if_levels, IF_ELSE))
	{
		return 1;
	}

	/* Get here only when in false branch of if statement. */

	if(cmd_id == COM_IF_STMT)
	{
		++skipped_nested_if_stmts;
		return 0;
	}

	if(cmd_id == COM_ELSEIF_STMT)
	{
		return (skipped_nested_if_stmts == 0);
	}

	if(cmd_id == COM_ELSE_STMT || cmd_id == COM_ENDIF_STMT)
	{
		if(skipped_nested_if_stmts > 0)
		{
			if(cmd_id == COM_ENDIF_STMT)
			{
				--skipped_nested_if_stmts;
			}
			return 0;
		}
		return 1;
	}

	return 0;
}

/* Determines current position in the command line.  The rquoting parameter when
 * non-zero specifies number of rquoted arguments by its absolute value, while
 * negative sign means that last argument doesn't need to be terminated.
 * Returns:
 *  - 0, if not inside an argument;
 *  - 1, if next character should be skipped (XXX: what does it mean?);
 *  - 2, if inside escaped argument;
 *  - 3, if inside single quoted argument;
 *  - 4, if inside double quoted argument;
 *  - 5, if inside regexp quoted argument. */
TSTATIC int
line_pos(const char begin[], const char end[], char sep, int rquoting,
		int max_args)
{
	int count;
	enum { BEGIN, NO_QUOTING, S_QUOTING, D_QUOTING, R_QUOTING } state;
	int args_left = max_args;
	/* Actual separator used for rargs or '\0' if unset. */
	char rsep = '\0';

	const char *args = vle_cmds_args(begin);
	if(args >= end)
	{
		return 0;
	}
	if(args == NULL)
	{
		count = 0;
	}
	else
	{
		begin = args;
		count = 1;
	}

	if(rquoting < 0)
	{
		rquoting = -(rquoting + 1);
	}

	state = BEGIN;
	while(begin != end)
	{
		switch(state)
		{
			case BEGIN:
				if(sep == ' ' && *begin == '\'')
					state = S_QUOTING;
				else if(sep == ' ' && *begin == '"')
					state = D_QUOTING;
				else if(rquoting && ((sep == ' ' && *begin == '/') || *begin == sep) &&
						(rsep == '\0' || *begin == rsep))
					state = R_QUOTING;
				else if(*begin == '&' && begin == end - 1)
					state = BEGIN;
				else if(*begin != sep)
					state = rquoting && (rsep == '\0' || *begin == rsep) ? R_QUOTING
					                                                     : NO_QUOTING;
				break;
			case NO_QUOTING:
				if(*begin == sep)
				{
					state = BEGIN;
					count++;
				}
				else if(*begin == '\'')
				{
					state = S_QUOTING;
				}
				else if(*begin == '"')
				{
					state = D_QUOTING;
				}
				else if(*begin == '\\')
				{
					begin++;
					if(begin == end)
						return 1;
				}
				break;
			case S_QUOTING:
				if(*begin == '\'')
					state = BEGIN;
				break;
			case D_QUOTING:
				if(*begin == '"')
				{
					state = BEGIN;
				}
				else if(*begin == '\\')
				{
					begin++;
					if(begin == end)
						return 1;
				}
				break;
			case R_QUOTING:
				if(rsep == '\0')
				{
					rsep = *(begin - 1);
				}
				if(*begin == rsep || *begin == sep)
				{
					--args_left;
					--rquoting;
					if(args_left == 0 || rquoting == 0)
					{
						state = BEGIN;
					}
				}
				else if(*begin == '\\')
				{
					begin++;
					if(begin == end)
						return 1;
				}
				break;
		}
		begin++;
	}
	if(state == NO_QUOTING)
	{
		if(sep == ' ')
		{
			/* First element is not an argument. */
			return (count > 0) ? 2 : 0;
		}
		else if(count > 0 && count < 3)
		{
			return 2;
		}
	}
	else if(state != BEGIN)
	{
		/* "Error": no closing quote. */
		switch(state)
		{
			case S_QUOTING: return 3;
			case D_QUOTING: return 4;
			case R_QUOTING: return (max_args <= 1 || args_left > 1 ? 5 : 2);

			default:
				assert(0 && "Unexpected state.");
				break;
		}
	}
	else if(sep != ' ' && count > 0 && *end != sep)
		return 2;

	return 0;
}

int
cmds_dispatch(const char cmdline[], view_t *view, CmdInputType type)
{
	int save_msg = 0;
	char **cmds = break_cmdline(cmdline, type == CIT_MENU_COMMAND);
	char **cmd = cmds;

	while(*cmd != NULL)
	{
		const int ret = cmds_dispatch1(*cmd, view, type);
		if(ret != 0)
		{
			save_msg = (ret < 0) ? -1 : 1;
		}

		free(*cmd++);
	}
	free(cmds);

	return save_msg;
}

/* Breaks command-line into sub-commands.  Returns NULL-terminated list of
 * sub-commands. */
TSTATIC char **
break_cmdline(const char cmdline[], int for_menu)
{
	char **cmds = NULL;
	int len = 0;

	char cmdline_copy[strlen(cmdline) + 1];
	char *raw, *processed;

	CmdArgsType args_kind;

	if(*cmdline == '\0')
	{
		len = add_to_string_array(&cmds, len, cmdline);
		goto finish;
	}

	strcpy(cmdline_copy, cmdline);
	cmdline = cmdline_copy;

	raw = cmdline_copy;
	processed = cmdline_copy;

	/* For non-menu commands set command-line mode configuration before calling
	 * is_out_of_arg() and get_cmd_args_type() function, which calls functions of
	 * the cmds module of the engine that use context. */
	if(!for_menu)
	{
		vle_cmds_init(1, &cmds_conf);
	}

	cmdline = skip_to_cmd_name(cmdline);
	args_kind = get_cmd_args_type(cmdline);

	/* Throughout the following loop local variables have the following meaning:
	 * - save_msg  -- resultant state of message indication;
	 * - raw       -- not yet processed part of string that can contain \;
	 * - processed -- ready to consume part of string;
	 * - cmdline   -- points to the start of the last command. */
	while(*cmdline != '\0')
	{
		if(args_kind == CAT_REGULAR && raw[0] == '\\' && raw[1] != '\0')
		{
			if(*(raw + 1) == '|')
			{
				*processed++ = '|';
				raw += 2;
			}
			else
			{
				*processed++ = *raw++;
				*processed++ = *raw++;
			}
		}
		else if((*raw == '|' && is_out_of_arg(cmdline, processed)) || *raw == '\0')
		{
			if(*raw != '\0')
			{
				++raw;
			}
			else
			{
				*processed = '\0';
			}

			/* Don't break line for whole line commands. */
			if(args_kind != CAT_UNTIL_THE_END)
			{
				if(args_kind == CAT_EXPR)
				{
					/* Move breaking point forward by consuming parts of the string after
					 * || until end of the string or | is found. */
					while(processed[0] == '|' && processed[1] == '|' &&
							processed[2] != '|')
					{
						processed = until_first(processed + 2, '|');
						raw = (processed[0] == '\0') ? processed : (processed + 1);
					}
				}

				*processed = '\0';
				processed = raw;
			}

			len = add_to_string_array(&cmds, len, cmdline);

			if(args_kind == CAT_UNTIL_THE_END)
			{
				/* Whole line command takes the rest of the string, nothing more to
				 * process. */
				break;
			}

			cmdline = skip_to_cmd_name(processed);
			args_kind = get_cmd_args_type(cmdline);
		}
		else
		{
			*processed++ = *raw++;
		}
	}

finish:
	(void)put_into_string_array(&cmds, len, NULL);
	return cmds;
}

/* Checks whether character at given position in the given command-line is
 * outside quoted argument.  Returns non-zero if so, otherwise zero is
 * returned. */
static int
is_out_of_arg(const char cmd[], const char pos[])
{
	const CmdLineLocation location = cmds_classify_pos(cmd, pos);

	if(location == CLL_NO_QUOTING)
	{
		if(get_cmd_args_type(cmd) == CAT_REGULAR)
		{
			return 1;
		}
		if(get_cmd_args_type(cmd) == CAT_EXPR &&
				pos != cmd && pos[0] == '|' && pos[-1] != '|' && pos[1] != '|')
		{
			/* For "*[^|]|[^|]*" report that we're out of argument. */
			return 1;
		}
	}

	return location == CLL_OUT_OF_ARG;
}

CmdLineLocation
cmds_classify_pos(const char cmd[], const char *pos)
{
	char separator;
	int regex_quoting;

	cmd_info_t info;
	const cmd_t *const c = vle_cmds_parse(cmd, &info);
	const int id = (c == NULL ? -1 : c->id);
	const int max_args = (c == NULL)
	                   ? -1
	                   : (c->max_args == NOT_DEF ? INT_MAX : c->max_args);

	switch(id)
	{
		case COM_CDS:
		case COM_HIGHLIGHT:
		case COM_FILTER:
		case COM_SELECT:
			separator = ' ';
			regex_quoting = -2;
			break;
		case COM_SUBSTITUTE:
			separator = info.sep;
			regex_quoting = -3;
			break;
		case COM_TR:
			separator = info.sep;
			regex_quoting = 2;
			break;

		default:
			separator = ' ';
			regex_quoting = 0;
			break;
	}

	switch(line_pos(cmd, pos, separator, regex_quoting, max_args))
	{
		case 0: return CLL_OUT_OF_ARG;
		case 1: /* Fall through. */
		case 2: return CLL_NO_QUOTING;
		case 3: return CLL_S_QUOTING;
		case 4: return CLL_D_QUOTING;
		case 5: return CLL_R_QUOTING;

		default:
			assert(0 && "Unexpected return code.");
			return CLL_NO_QUOTING;
	}
}

/* Determines what kind of processing should be applied to the command pointed
 * to by the cmd.  Returns the kind. */
static CmdArgsType
get_cmd_args_type(const char cmd[])
{
	const int cmd_id = vle_cmds_identify(cmd);
	switch(cmd_id)
	{
		case COMMAND_CMD_ID:
		case COM_AMAP:
		case COM_ANOREMAP:
		case COM_AUTOCMD:
		case COM_EXECUTE:
		case COM_CABBR:
		case COM_CMAP:
		case COM_CNOREMAP:
		case COM_COMMAND:
		case COM_DMAP:
		case COM_DNOREMAP:
		case COM_FILETYPE:
		case COM_FILEVIEWER:
		case COM_FILEXTYPE:
		case COM_KEEPSEL:
		case COM_MAP:
		case COM_MMAP:
		case COM_MNOREMAP:
		case COM_NMAP:
		case COM_NNOREMAP:
		case COM_NORMAL:
		case COM_QMAP:
		case COM_QNOREMAP:
		case COM_VMAP:
		case COM_VNOREMAP:
		case COM_NOREMAP:
		case COM_WINCMD:
		case COM_WINDO:
		case COM_WINRUN:
			return CAT_UNTIL_THE_END;

		default:
			return cmds_has_expr_args(cmd_id) ? CAT_EXPR : CAT_REGULAR;
	}
}

const char *
cmds_find_last(const char cmds[])
{
	const char *p, *q;

	p = cmds;
	q = cmds;
	while(*cmds != '\0')
	{
		if(*p == '\\')
		{
			q += (p[1] == '|') ? 1 : 2;
			p += 2;
		}
		else if(*p == '\0' || (*p == '|' && is_out_of_arg(cmds, q)))
		{
			if(*p != '\0')
			{
				++p;
			}

			cmds = skip_to_cmd_name(cmds);
			if(*cmds == '!' || starts_with_lit(cmds, "com"))
			{
				break;
			}

			q = p;

			if(*q == '\0')
			{
				break;
			}

			cmds = q;
		}
		else
		{
			++q;
			++p;
		}
	}

	return cmds;
}

/* Skips consecutive whitespace or colon characters at the beginning of the
 * command.  Returns pointer to the first non whitespace and color character. */
static char *
skip_to_cmd_name(const char cmd[])
{
	while(isspace(*cmd) || *cmd == ':')
	{
		++cmd;
	}
	return (char *)cmd;
}

int
cmds_dispatch1(const char cmd[], view_t *view, CmdInputType type)
{
	int menu;
	int backward;
	int found;

	if(cmd == NULL)
	{
		return repeat_command(view, type);
	}

	menu = 0;
	backward = 0;
	switch(type)
	{
		case CIT_BSEARCH_PATTERN: backward = 1; /* Fall through. */
		case CIT_FSEARCH_PATTERN:
			return modnorm_find(view, cmd, backward, /*print_msg=*/1, &found);

		case CIT_VBSEARCH_PATTERN: backward = 1; /* Fall through. */
		case CIT_VFSEARCH_PATTERN:
			return modvis_find(view, cmd, backward, /*print_msg=*/1, &found);

		case CIT_VWBSEARCH_PATTERN: backward = 1; /* Fall through. */
		case CIT_VWFSEARCH_PATTERN:
			return modview_find(cmd, backward);

		case CIT_MENU_COMMAND: menu = 1; /* Fall through. */
		case CIT_COMMAND:
			return cmds_exec(cmd, view, menu, /*keep_sel=*/0);

		case CIT_FILTER_PATTERN:
			if(view->custom.type != CV_DIFF)
			{
				local_filter_apply(view, cmd);
			}
			else
			{
				show_error_msg("Filtering", "No local filter for diff views");
			}
			return 0;

		default:
			assert(0 && "Command execution request of unknown/unexpected type.");
			return 0;
	}
}

/* Repeats last command of the specified type.  Returns 0 on success if no
 * message should be saved in the status bar, > 0 to save message on successful
 * execution and < 0 in case of error with error message. */
static int
repeat_command(view_t *view, CmdInputType type)
{
	int backward = 0;
	int found;
	switch(type)
	{
		case CIT_BSEARCH_PATTERN: backward = 1; /* Fall through. */
		case CIT_FSEARCH_PATTERN:
			return modnorm_find(view, hists_search_last(), backward, /*print_msg=*/1,
					&found);

		case CIT_VBSEARCH_PATTERN: backward = 1; /* Fall through. */
		case CIT_VFSEARCH_PATTERN:
			return modvis_find(view, hists_search_last(), backward, /*print_msg=*/1,
					&found);

		case CIT_VWBSEARCH_PATTERN: backward = 1; /* Fall through. */
		case CIT_VWFSEARCH_PATTERN:
			return modview_find(NULL, backward);

		case CIT_COMMAND:
			return cmds_exec(/*cmd=*/NULL, view, /*menu=*/0, /*keep_sel=*/0);

		case CIT_FILTER_PATTERN:
			local_filter_apply(view, "");
			return 0;

		default:
			assert(0 && "Command repetition request of unexpected type.");
			return 0;
	}
}

void
cmds_scope_start(void)
{
	(void)int_stack_push(&if_levels, IF_SCOPE_GUARD);
}

void
cmds_scope_escape(void)
{
	while(!is_at_scope_bottom(&if_levels))
	{
		int_stack_pop(&if_levels);
	}
}

int
cmds_scope_finish(void)
{
	if(!is_at_scope_bottom(&if_levels))
	{
		ui_sb_err("Missing :endif");
		int_stack_pop_seq(&if_levels, IF_SCOPE_GUARD);
		return 1;
	}

	int_stack_pop(&if_levels);
	return 0;
}

void
cmds_scoped_if(int cond)
{
	(void)int_stack_push(&if_levels, cond ? IF_MATCH : IF_BEFORE_MATCH);
	cmds_preserve_selection();
}

int
cmds_scoped_elseif(int cond)
{
	IfFrame if_frame;

	if(is_at_scope_bottom(&if_levels))
	{
		return 1;
	}

	if_frame = int_stack_get_top(&if_levels);
	if(if_frame == IF_ELSE || if_frame == IF_FINISH)
	{
		return 1;
	}

	int_stack_set_top(&if_levels, (if_frame == IF_BEFORE_MATCH) ?
			(cond ? IF_MATCH : IF_BEFORE_MATCH) :
			IF_AFTER_MATCH);
	cmds_preserve_selection();
	return 0;
}

int
cmds_scoped_else(void)
{
	IfFrame if_frame;

	if(is_at_scope_bottom(&if_levels))
	{
		return 1;
	}

	if_frame = int_stack_get_top(&if_levels);
	if(if_frame == IF_ELSE || if_frame == IF_FINISH)
	{
		return 1;
	}

	int_stack_set_top(&if_levels,
			(if_frame == IF_BEFORE_MATCH) ? IF_ELSE : IF_FINISH);
	cmds_preserve_selection();
	return 0;
}

int
cmds_scoped_endif(void)
{
	if(is_at_scope_bottom(&if_levels))
	{
		return 1;
	}

	int_stack_pop(&if_levels);
	return 0;
}

int
cmds_scoped_empty(void)
{
	return int_stack_is_empty(&if_levels);
}

/* Checks that bottom of block scope is reached.  Returns non-zero if so,
 * otherwise zero is returned. */
static int
is_at_scope_bottom(const int_stack_t *scope_stack)
{
	return int_stack_is_empty(scope_stack)
	    || int_stack_top_is(scope_stack, IF_SCOPE_GUARD);
}

char *
cmds_eval_args(const char args[], const char **stop_ptr)
{
	size_t len = 0;
	char *eval_result = NULL;

	assert(args[0] != '\0');

	while(args[0] != '\0')
	{
		char *free_this = NULL;
		const char *tmp_result = NULL;

		parsing_result_t result = vle_parser_eval(args, /*interactive=*/1);
		if(result.error == PE_INVALID_EXPRESSION && result.ends_with_whitespace)
		{
			if(result.value.type != VTYPE_ERROR)
			{
				tmp_result = free_this = var_to_str(result.value);
				args = result.last_parsed_char;
			}
		}
		else if(result.error == PE_NO_ERROR)
		{
			tmp_result = free_this = var_to_str(result.value);
			args = result.last_position;
		}
		else if(result.error != PE_INVALID_EXPRESSION)
		{
			vle_parser_report(&result);
		}

		if(tmp_result == NULL)
		{
			var_free(result.value);
			break;
		}

		if(!is_null_or_empty(eval_result))
		{
			eval_result = extend_string(eval_result, " ", &len);
		}
		eval_result = extend_string(eval_result, tmp_result, &len);

		var_free(result.value);
		free(free_this);

		args = skip_whitespace(args);
	}
	if(args[0] == '\0')
	{
		return eval_result;
	}
	else
	{
		free(eval_result);
		*stop_ptr = args;
		return NULL;
	}
}

void
cmds_preserve_selection(void)
{
	keep_view_selection = 1;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
