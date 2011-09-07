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

#ifdef _WIN32
#include <windows.h>
#include <lm.h>
#endif

#include <regex.h>

#include <curses.h>

#include <sys/types.h> /* passwd */
#ifndef _WIN32
#include <sys/wait.h>
#endif
#include <dirent.h> /* DIR */
#include <unistd.h> /* chdir() */

#include <assert.h>
#include <ctype.h> /* isspace() */
#include <errno.h> /* errno */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h> /*  system() */
#include <string.h> /* strncmp() */
#include <time.h>

#include "background.h"
#include "bookmarks.h"
#include "cmdline.h"
#include "cmds.h"
#include "color_scheme.h"
#include "completion.h"
#include "config.h"
#include "dir_stack.h"
#include "filelist.h"
#include "fileops.h"
#include "filetype.h"
#include "keys.h"
#include "log.h"
#include "macros.h"
#include "menu.h"
#include "menus.h"
#include "modes.h"
#include "opt_handlers.h"
#include "options.h"
#include "permissions_dialog.h"
#include "registers.h"
#include "search.h"
#include "signals.h"
#include "sort.h"
#include "sort_dialog.h"
#include "status.h"
#include "tags.h"
#include "trash.h"
#include "ui.h"
#include "undo.h"
#include "utils.h"
#include "visual.h"

#include "commands.h"

#ifndef _WIN32
#define PAUSE_CMD "vifm-pause"
#define PAUSE_STR "; "PAUSE_CMD
#else
#define PAUSE_CMD "pause"
#define PAUSE_STR
#endif

/* values of type argument for filename_completion() function */
enum {
	FNC_ALL,      /* all files and directories */
	FNC_DIRONLY,  /* only directories */
	FNC_EXECONLY, /* only executable files */
	FNC_DIREXEC   /* directories and executable files */
};

enum {
	COM_GOTO,
	COM_EXECUTE,
	COM_CD,
	COM_COLORSCHEME,
	COM_EDIT,
	COM_FILE,
	COM_FIND,
	COM_GREP,
	COM_HELP,
	COM_HIGHLIGHT,
	COM_HISTORY,
	COM_PUSHD,
	COM_SET,
};

static int complete_args(int id, const char *args, int argc, char **argv,
		int arg_pos);
static int swap_range(void);
static int resolve_mark(char mark);
static char * cmds_expand_macros(const char *str, int *use_menu, int *split);
static void post(int id);
#ifndef TEST
static
#endif
void select_range(int id, const struct cmd_info *cmd_info);
static void exec_completion(const char *str);
static void filename_completion(const char *str, int type);
static void complete_help(const char *str);
static void complete_history(const char *str);
static void complete_highlight_groups(const char *str);
static int complete_highlight_arg(const char *str);
static int is_entry_dir(const struct dirent *d);
static int is_entry_exec(const struct dirent *d);
static void split_path(void);
static wchar_t * substitute_specs(const char *cmd);
static const char *skip_spaces(const char *cmd);
static const char *skip_word(const char *cmd);

static int goto_cmd(const struct cmd_info *cmd_info);
static int emark_cmd(const struct cmd_info *cmd_info);
static int alink_cmd(const struct cmd_info *cmd_info);
static int apropos_cmd(const struct cmd_info *cmd_info);
static int cd_cmd(const struct cmd_info *cmd_info);
static int change_cmd(const struct cmd_info *cmd_info);
static int clone_cmd(const struct cmd_info *cmd_info);
static int cmap_cmd(const struct cmd_info *cmd_info);
static int cnoremap_cmd(const struct cmd_info *cmd_info);
static int copy_cmd(const struct cmd_info *cmd_info);
static int colorscheme_cmd(const struct cmd_info *cmd_info);
static int command_cmd(const struct cmd_info *cmd_info);
static int cunmap_cmd(const struct cmd_info *cmd_info);
static int delete_cmd(const struct cmd_info *cmd_info);
static int delmarks_cmd(const struct cmd_info *cmd_info);
static int dirs_cmd(const struct cmd_info *cmd_info);
static int edit_cmd(const struct cmd_info *cmd_info);
static int empty_cmd(const struct cmd_info *cmd_info);
static int file_cmd(const struct cmd_info *cmd_info);
static int filetype_cmd(const struct cmd_info *cmd_info);
static int filextype_cmd(const struct cmd_info *cmd_info);
static int fileviewer_cmd(const struct cmd_info *cmd_info);
static int filter_cmd(const struct cmd_info *cmd_info);
static int find_cmd(const struct cmd_info *cmd_info);
static int grep_cmd(const struct cmd_info *cmd_info);
static int help_cmd(const struct cmd_info *cmd_info);
static int highlight_cmd(const struct cmd_info *cmd_info);
static const char *get_group_str(int group, Col_attr col);
static int get_color(const char *text);
static int get_attrs(const char *text);
static int history_cmd(const struct cmd_info *cmd_info);
static int invert_cmd(const struct cmd_info *cmd_info);
static int jobs_cmd(const struct cmd_info *cmd_info);
static int locate_cmd(const struct cmd_info *cmd_info);
static int ls_cmd(const struct cmd_info *cmd_info);
static int map_cmd(const struct cmd_info *cmd_info);
static int mark_cmd(const struct cmd_info *cmd_info);
static int marks_cmd(const struct cmd_info *cmd_info);
static int mkdir_cmd(const struct cmd_info *cmd_info);
static int move_cmd(const struct cmd_info *cmd_info);
static int nmap_cmd(const struct cmd_info *cmd_info);
static int nnoremap_cmd(const struct cmd_info *cmd_info);
static int nohlsearch_cmd(const struct cmd_info *cmd_info);
static int noremap_cmd(const struct cmd_info *cmd_info);
static int map_or_remap(const struct cmd_info *cmd_info, int no_remap);
static int nunmap_cmd(const struct cmd_info *cmd_info);
static int only_cmd(const struct cmd_info *cmd_info);
static int popd_cmd(const struct cmd_info *cmd_info);
static int pushd_cmd(const struct cmd_info *cmd_info);
static int pwd_cmd(const struct cmd_info *cmd_info);
static int registers_cmd(const struct cmd_info *cmd_info);
static int rename_cmd(const struct cmd_info *cmd_info);
static int restart_cmd(const struct cmd_info *cmd_info);
static int restore_cmd(const struct cmd_info *cmd_info);
static int rlink_cmd(const struct cmd_info *cmd_info);
static int screen_cmd(const struct cmd_info *cmd_info);
static int set_cmd(const struct cmd_info *cmd_info);
static int shell_cmd(const struct cmd_info *cmd_info);
static int sort_cmd(const struct cmd_info *cmd_info);
static int split_cmd(const struct cmd_info *cmd_info);
static int substitute_cmd(const struct cmd_info *cmd_info);
static int sync_cmd(const struct cmd_info *cmd_info);
static int touch_cmd(const struct cmd_info *cmd_info);
static int tr_cmd(const struct cmd_info *cmd_info);
static int undolist_cmd(const struct cmd_info *cmd_info);
static int unmap_cmd(const struct cmd_info *cmd_info);
static int view_cmd(const struct cmd_info *cmd_info);
static int vifm_cmd(const struct cmd_info *cmd_info);
static int vmap_cmd(const struct cmd_info *cmd_info);
static int vnoremap_cmd(const struct cmd_info *cmd_info);
#ifdef _WIN32
static int volumes_cmd(const struct cmd_info *cmd_info);
#endif
static int do_map(const struct cmd_info *cmd_info, const char *map_type,
		const char *map_cmd, int mode, int no_remap);
static int vunmap_cmd(const struct cmd_info *cmd_info);
static int do_unmap(const char *keys, int mode);
static int write_cmd(const struct cmd_info *cmd_info);
static int quit_cmd(const struct cmd_info *cmd_info);
static int wq_cmd(const struct cmd_info *cmd_info);
static int yank_cmd(const struct cmd_info *cmd_info);
static int get_reg_and_count(const struct cmd_info *cmd_info, int *reg);
static int usercmd_cmd(const struct cmd_info* cmd_info);

static const struct cmd_add commands[] = {
	{ .name = "",                 .abbr = NULL,    .emark = 0,  .id = COM_GOTO,        .range = 1,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = goto_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "!",                .abbr = NULL,    .emark = 1,  .id = COM_EXECUTE,     .range = 1,    .bg = 1, .quote = 0, .regexp = 0,
		.handler = emark_cmd,       .qmark = 0,      .expand = 1, .cust_sep = 0,         .min_args = 1, .max_args = NOT_DEF, .select = 1, },
	{ .name = "alink",            .abbr = NULL,    .emark = 1,  .id = -1,              .range = 1,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = alink_cmd,       .qmark = 1,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 1, },
	{ .name = "apropos",          .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = apropos_cmd,     .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "cd",               .abbr = NULL,    .emark = 1,  .id = COM_CD,          .range = 0,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = cd_cmd,          .qmark = 0,      .expand = 1, .cust_sep = 0,         .min_args = 0, .max_args = 2,       .select = 0, },
	{ .name = "change",           .abbr = "c",     .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = change_cmd,      .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "clone",            .abbr = NULL,    .emark = 1,  .id = -1,              .range = 1,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = clone_cmd,       .qmark = 1,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 1, },
	{ .name = "cmap",             .abbr = "cm",    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = cmap_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "cnoremap",         .abbr = "cno",   .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = cnoremap_cmd,    .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "colorscheme",      .abbr = "colo",  .emark = 1,  .id = COM_COLORSCHEME, .range = 0,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = colorscheme_cmd, .qmark = 1,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 2,       .select = 0, },
  { .name = "command",          .abbr = "com",   .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
    .handler = command_cmd,     .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "copy",             .abbr = "co",    .emark = 1,  .id = -1,              .range = 1,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = copy_cmd,        .qmark = 1,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 1, },
	{ .name = "cunmap",           .abbr = "cu",    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = cunmap_cmd,      .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 1, .max_args = 1,       .select = 0, },
	{ .name = "delete",           .abbr = "d",     .emark = 0,  .id = -1,              .range = 1,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = delete_cmd,      .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 2,       .select = 1, },
	{ .name = "delmarks",         .abbr = "delm",  .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = delmarks_cmd,    .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "display",          .abbr = "di",    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = registers_cmd,   .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "dirs",             .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = dirs_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "edit",             .abbr = "e",     .emark = 0,  .id = COM_EDIT,        .range = 1,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = edit_cmd,        .qmark = 0,      .expand = 1, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 1, },
	{ .name = "empty",            .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = empty_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "exit",             .abbr = "exi",   .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = quit_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "file",             .abbr = "f",     .emark = 0,  .id = COM_FILE,        .range = 0,    .bg = 1, .quote = 0, .regexp = 0,
		.handler = file_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "filetype",         .abbr = "filet", .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = filetype_cmd,    .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 2, .max_args = NOT_DEF, .select = 0, },
	{ .name = "fileviewer",       .abbr = "filev", .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = fileviewer_cmd,  .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 2, .max_args = NOT_DEF, .select = 0, },
	{ .name = "filextype",        .abbr = "filex", .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = filextype_cmd,   .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 2, .max_args = NOT_DEF, .select = 0, },
	{ .name = "filter",           .abbr = NULL,    .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 1, .regexp = 1,
		.handler = filter_cmd,      .qmark = 1,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 1,       .select = 0, },
	{ .name = "find",             .abbr = "fin",   .emark = 0,  .id = COM_FIND,        .range = 1,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = find_cmd,        .qmark = 0,      .expand = 1, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 1, },
	{ .name = "grep",             .abbr = "gr",    .emark = 1,  .id = COM_GREP,        .range = 1,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = grep_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 1, },
	{ .name = "help",             .abbr = "h",     .emark = 0,  .id = COM_HELP,        .range = 0,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = help_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 1,       .select = 0, },
	{ .name = "highlight",        .abbr = "hi",    .emark = 0,  .id = COM_HIGHLIGHT,   .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = highlight_cmd,   .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 4,       .select = 0, },
	{ .name = "history",          .abbr = "his",   .emark = 0,  .id = COM_HISTORY,     .range = 0,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = history_cmd,     .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 1,       .select = 0, },
	{ .name = "invert",           .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = invert_cmd,      .qmark = 1,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "jobs",             .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = jobs_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "locate",           .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = locate_cmd,      .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "ls",               .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = ls_cmd,          .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "map",              .abbr = NULL,    .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = map_cmd,         .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 2, .max_args = NOT_DEF, .select = 0, },
	{ .name = "mark",             .abbr = "ma",    .emark = 0,  .id = -1,              .range = 1,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = mark_cmd,        .qmark = 0,      .expand = 1, .cust_sep = 0,         .min_args = 1, .max_args = 3,       .select = 0, },
	{ .name = "marks",            .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = marks_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "mkdir",            .abbr = NULL,    .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = mkdir_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 1, .max_args = NOT_DEF, .select = 0, },
	{ .name = "move",             .abbr = "m",     .emark = 1,  .id = -1,              .range = 1,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = move_cmd,        .qmark = 1,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 1, },
	{ .name = "nmap",             .abbr = "nm",    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = nmap_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "nnoremap",         .abbr = "nn",    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = nnoremap_cmd,    .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "nohlsearch",       .abbr = "noh",   .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = nohlsearch_cmd,  .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "noremap",          .abbr = "no",    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = noremap_cmd,     .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "nunmap",           .abbr = "nun",   .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = nunmap_cmd,      .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 1, .max_args = 1,       .select = 0, },
	{ .name = "only",             .abbr = "on",    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = only_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "popd",             .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = popd_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "pushd",            .abbr = NULL,    .emark = 1,  .id = COM_PUSHD,       .range = 0,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = pushd_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 2,       .select = 0, },
	{ .name = "pwd",              .abbr = "pw",    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = pwd_cmd,         .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "quit",             .abbr = "q",     .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = quit_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "registers",        .abbr = "reg",   .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = registers_cmd,   .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "rename",           .abbr = NULL,    .emark = 0,  .id = -1,              .range = 1,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = rename_cmd,      .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 1, },
	{ .name = "restart",          .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = restart_cmd,     .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "restore",          .abbr = NULL,    .emark = 0,  .id = -1,              .range = 1,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = restore_cmd,     .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 1, },
	{ .name = "rlink",            .abbr = NULL,    .emark = 1,  .id = -1,              .range = 1,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = rlink_cmd,       .qmark = 1,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 1, },
	{ .name = "screen",           .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = screen_cmd,      .qmark = 1,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "set",              .abbr = "se",    .emark = 0,  .id = COM_SET,         .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = set_cmd,         .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 1, .max_args = NOT_DEF, .select = 0, },
	{ .name = "shell",            .abbr = "sh",    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = shell_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "sort",             .abbr = "sor",   .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = sort_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "split",            .abbr = "sp",    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = split_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 1,       .select = 0, },
	{ .name = "substitute",       .abbr = "s",     .emark = 0,  .id = -1,              .range = 1,    .bg = 0, .quote = 0, .regexp = 1,
		.handler = substitute_cmd,  .qmark = 0,      .expand = 0, .cust_sep = 1,         .min_args = 2, .max_args = 3,       .select = 1, },
	{ .name = "sync",             .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = sync_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "touch",            .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = touch_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 1, .max_args = NOT_DEF, .select = 0, },
	{ .name = "tr",               .abbr = NULL,    .emark = 0,  .id = -1,              .range = 1,    .bg = 0, .quote = 0, .regexp = 1,
		.handler = tr_cmd,          .qmark = 0,      .expand = 0, .cust_sep = 1,         .min_args = 2, .max_args = 2,       .select = 1, },
	{ .name = "undolist",         .abbr = "undol", .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = undolist_cmd,    .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "unmap",            .abbr = "unm",   .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = unmap_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 1, .max_args = 1,       .select = 0, },
	{ .name = "version",          .abbr = "ve",    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = vifm_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "view",             .abbr = "vie",   .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = view_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "vifm",             .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = vifm_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "vmap",             .abbr = "vm",    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = vmap_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "vnoremap",         .abbr = "vn",    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = vnoremap_cmd,    .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
#ifdef _WIN32
	{ .name = "volumes",          .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = volumes_cmd,     .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
#endif
	{ .name = "vunmap",           .abbr = "vu",    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = vunmap_cmd,      .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 1, .max_args = 1,       .select = 0, },
	{ .name = "write",            .abbr = "w",     .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = write_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "wq",               .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = wq_cmd,          .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "xit",              .abbr = "x",     .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = quit_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "yank",             .abbr = "y",     .emark = 0,  .id = -1,              .range = 1,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = yank_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 2,       .select = 1, },

	{ .name = "<USERCMD>",        .abbr = NULL,    .emark = 0,  .id = -1,              .range = 1,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = usercmd_cmd,     .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 1, },
};

static struct cmds_conf cmds_conf = {
	.complete_args = complete_args,
	.swap_range = swap_range,
	.resolve_mark = resolve_mark,
	.expand_macros = cmds_expand_macros,
	.post = post,
	.select_range = select_range,
};

static int need_clean_selection;

static char **paths;
static int paths_count;

static int
cmd_ends_with_space(const char *cmd)
{
	while(cmd[0] != '\0' && cmd[1] != '\0')
	{
		if(cmd[0] == '\\')
			cmd++;
		cmd++;
	}
	return cmd[0] == ' ';
}

static int
complete_args(int id, const char *args, int argc, char **argv, int arg_pos)
{
	const char *arg;
	const char *start;

	arg = strrchr(args, ' ');
	if(arg == NULL)
		arg = args;
	else
		arg++;

	start = arg;

	if(id == COM_COLORSCHEME)
		complete_colorschemes((argc > 0) ? argv[argc - 1] : arg);
	else if(id == COM_SET)
		complete_options(args, &start);
	else if(id == COM_HELP)
		complete_help(args);
	else if(id == COM_HISTORY)
		complete_history(args);
	else if(id == COM_HIGHLIGHT)
	{
		if(argc == 0 || (argc == 1 && !cmd_ends_with_space(args)))
			complete_highlight_groups(args);
		else
			start += complete_highlight_arg(arg);
	}
	else
	{
		start = strrchr(args + arg_pos, '/');
		if(start == NULL)
			start = args + arg_pos;
		else
			start++;

		if(argc > 0 && !cmd_ends_with_space(args))
			arg = argv[argc - 1];

		if(id == COM_CD || id == COM_PUSHD)
			filename_completion(arg, FNC_DIRONLY);
		else if(id == COM_FIND)
		{
			if(argc == 1 && !cmd_ends_with_space(args))
				filename_completion(arg, FNC_DIRONLY);
		}
		else if(id == COM_EXECUTE)
		{
			if(argc == 0 || (argc == 1 && !cmd_ends_with_space(args)))
			{
				if(*arg == '.')
					filename_completion(arg, FNC_DIREXEC);
				else
					exec_completion(arg);
			}
			else
				filename_completion(arg, FNC_ALL);
		}
		else
			filename_completion(arg, FNC_ALL);
	}

	return start - args;
}

static void
exec_completion(const char *str)
{
	int i;

	for(i = 0; i < paths_count; i++)
	{
		if(chdir(paths[i]) != 0)
			continue;
		filename_completion(str, FNC_EXECONLY);
	}
	add_completion(str);
}

#ifdef _WIN32
static void
complete_with_shared(const char *server)
{
	NET_API_STATUS res;

	do
	{
		PSHARE_INFO_502 buf_ptr;
		DWORD er = 0, tr = 0, resume = 0;
		wchar_t *wserver = to_wide(server + 2);

		if(wserver == NULL)
		{
			(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
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
				if(!ends_with(buf, "$"))
					add_completion(buf);
				p++;
			}
			NetApiBufferFree(buf_ptr);
		}
	}
	while(res == ERROR_MORE_DATA);
}
#endif

/*
 * type: FNC_*
 */
static void
filename_completion(const char *str, int type)
{
	/* TODO refactor filename_completion(...) function */
	const char *string;

	DIR *dir;
	struct dirent *d;
	char * dirname;
	char * filename;
	char * temp;
	int filename_len;
	int isdir;

	if(str[0] == '~' && strchr(str, '/') == NULL)
	{
		char *s = expand_tilde(strdup(str));
		add_completion(s);
		free(s);
		return;
	}

	string = str;

	if(string[0] == '~')
	{
		dirname = expand_tilde(strdup(string));
		filename = strdup(dirname);
	}
	else
	{
		if(strlen(string) > 0)
		{
			dirname = strdup(string);
		}
		else
		{
			dirname = malloc(strlen(string) + 2);
			strcpy(dirname, string);
		}
		filename = strdup(string);
	}

	temp = strrchr(dirname, '/');
	if(temp)
	{
		strcpy(filename, ++temp);
		*temp = '\0';
	}
	else
	{
		dirname[0] = '.';
		dirname[1] = '\0';
	}

#ifdef _WIN32
	if(is_unc_root(dirname) ||
			(strcmp(dirname, ".") == 0 && is_unc_root(curr_view->curr_dir)))
	{
		complete_with_shared(dirname);
		free(filename);
		free(dirname);
		return;
	}
#endif

	dir = opendir(dirname);

	if(dir == NULL || chdir(dirname) != 0)
	{
		add_completion(filename);
		free(filename);
		free(dirname);
		return;
	}

	filename_len = strlen(filename);
	while((d = readdir(dir)) != NULL)
	{
		char *escaped;

		if(filename[0] == '\0' && d->d_name[0] == '.')
			continue;
#ifndef _WIN32
		if(strncmp(d->d_name, filename, filename_len) != 0)
			continue;
#else
		if(strncasecmp(d->d_name, filename, filename_len) != 0)
			continue;
#endif

		if(type == FNC_DIRONLY && !is_entry_dir(d))
			continue;
		else if(type == FNC_EXECONLY && !is_entry_exec(d))
			continue;
		else if(type == FNC_DIREXEC && !is_entry_dir(d) && !is_entry_exec(d))
			continue;

		isdir = 0;
		if(is_dir(d->d_name))
		{
			isdir = 1;
		}
		else if(strcmp(dirname, "."))
		{
			char * tempfile = (char *)NULL;
			int len = strlen(dirname) + strlen(d->d_name) + 1;
			tempfile = (char *)malloc((len) * sizeof(char));
			if(!tempfile)
			{
				closedir(dir);
				(void)chdir(curr_view->curr_dir);
				add_completion(filename);
				free(filename);
				free(dirname);
				return;
			}
			snprintf(tempfile, len, "%s%s", dirname, d->d_name);
			if(is_dir(tempfile))
				isdir = 1;
			else
				temp = strdup(d->d_name);

			free(tempfile);
		}
		else
			temp = strdup(d->d_name);

		if(isdir)
		{
			char * tempfile = (char *)NULL;
			tempfile = (char *) malloc((strlen(d->d_name) + 2) * sizeof(char));
			if(!tempfile)
			{
				closedir(dir);
				(void)chdir(curr_view->curr_dir);
				add_completion(filename);
				free(filename);
				free(dirname);
				return;
			}
			snprintf(tempfile, strlen(d->d_name) + 2, "%s/", d->d_name);
			temp = strdup(tempfile);

			free(tempfile);
		}
		escaped = escape_filename(temp, 1);
		add_completion(escaped);
		free(escaped);
		free(temp);
	}

	(void)chdir(curr_view->curr_dir);

	completion_group_end();
	if(type != FNC_EXECONLY)
	{
		if(get_completion_count() == 0)
		{
			add_completion(filename);
		}
		else
		{
			temp = escape_filename(filename, 1);
			add_completion(temp);
			free(temp);
		}
	}

	free(filename);
	free(dirname);
	closedir(dir);
}

static void
complete_help(const char *str)
{
	int i;

	if(!cfg.use_vim_help)
		return;

	for(i = 0; tags[i] != NULL; i++)
	{
		if(strstr(tags[i], str) != NULL)
			add_completion(tags[i]);
	}
	completion_group_end();
	add_completion(str);
}

static void
complete_history(const char *str)
{
	static const char *lines[] = {
		".",
		"dir",
		"@",
		"input",
		"/",
		"search",
		"fsearch",
		"?",
		"bsearch",
		":",
		"cmd",
	};
	int i;
	size_t len = strlen(str);
	for(i = 0; i < ARRAY_LEN(lines); i++)
	{
		if(strncmp(str, lines[i], len) == 0)
			add_completion(lines[i]);
	}
	completion_group_end();
	add_completion(str);
}

static void
complete_highlight_groups(const char *str)
{
	int i;
	size_t len = strlen(str);
	for(i = 0; i < MAXNUM_COLOR - 2; i++)
	{
		if(strncasecmp(str, HI_GROUPS[i], len) == 0)
			add_completion(HI_GROUPS[i]);
	}
	completion_group_end();
	add_completion(str);
}

static int
complete_highlight_arg(const char *str)
{
	int i;
	char *equal = strchr(str, '=');
	int result = (equal == NULL) ? 0 : (equal - str + 1);
	size_t len = strlen((equal == NULL) ? str : ++equal);
	if(equal == NULL)
	{
		static const char *args[] = {
			"cterm",
			"ctermfg",
			"ctermbg",
		};
		for(i = 0; i < ARRAY_LEN(args); i++)
		{
			if(strncmp(str, args[i], len) == 0)
				add_completion(args[i]);
		}
	}
	else
	{
		if(strncmp(str, "cterm", equal - str - 1) == 0)
		{
			static const char *STYLES[] = {
				"bold",
				"underline",
				"reverse",
				"inverse",
				"standout",
				"none",
			};
			char *comma = strrchr(equal, ',');
			if(comma != NULL)
			{
				result += comma - equal + 1;
				equal = comma + 1;
				len = strlen(equal);
			}

			for(i = 0; i < ARRAY_LEN(STYLES); i++)
			{
				if(strncasecmp(equal, STYLES[i], len) == 0)
					add_completion(STYLES[i]);
			}
		}
		else
		{
			if(strncasecmp(equal, "default", len) == 0)
				add_completion("default");
			if(strncasecmp(equal, "none", len) == 0)
				add_completion("none");
			for(i = 0; i < ARRAY_LEN(COLOR_NAMES); i++)
			{
				if(strncasecmp(equal, COLOR_NAMES[i], len) == 0)
					add_completion(COLOR_NAMES[i]);
			}
		}
	}
	completion_group_end();
	add_completion((equal == NULL) ? str : equal);
	return result;
}

static int
is_entry_dir(const struct dirent *d)
{
#ifdef _WIN32
	struct stat st;
	if(stat(d->d_name, &st) != 0)
		return 0;
	return S_ISDIR(st.st_mode);
#else
	if(d->d_type == DT_UNKNOWN)
	{
		struct stat st;
		if(stat(d->d_name, &st) != 0)
			return 0;
		return S_ISDIR(st.st_mode);
	}

	if(d->d_type != DT_DIR && d->d_type != DT_LNK)
		return 0;
	if(d->d_type == DT_LNK && !check_link_is_dir(d->d_name))
		return 0;
	return 1;
#endif
}

static int
is_entry_exec(const struct dirent *d)
{
#ifndef _WIN32
	if(d->d_type == DT_DIR)
		return 0;
	if(d->d_type == DT_LNK && check_link_is_dir(d->d_name))
		return 0;
	if(access(d->d_name, X_OK) != 0)
		return 0;
	return 1;
#else
	return is_win_executable(d->d_name);
#endif
}

static int
swap_range(void)
{
	return query_user_menu("Command Error", "Backwards range given, OK to swap?");
}

static int
resolve_mark(char mark)
{
	int result;

	result = check_mark_directory(curr_view, mark);
	if(result < 0)
		status_bar_errorf("Trying to use an invalid mark: '%c", mark);
	return result;
}

static char *
cmds_expand_macros(const char *str, int *use_menu, int *split)
{
	char *result;

	*use_menu = 0;
	*split = 0;
	if(strchr(str, '%') != NULL)
		result = expand_macros(curr_view, str, NULL, use_menu, split);
	else
		result = strdup(str);
	return result;
}

static void
post(int id)
{
	if(id == COM_GOTO)
		return;
	if(id == COM_FILE)
		return;
	if((curr_view != NULL && !curr_view->selected_files) || !need_clean_selection)
		return;
	if(get_mode() != NORMAL_MODE)
		return;

	clean_selected_files(curr_view);
	load_saving_pos(curr_view, 1);
	move_to_list_pos(curr_view, curr_view->list_pos);
}

#ifndef TEST
static
#endif
void
select_range(int id, const struct cmd_info *cmd_info)
{
	int x;
	int y = 0;

	/* Both a starting range and an ending range are given. */
	if(cmd_info->begin > -1)
	{
		clean_selected_files(curr_view);

		for(x = cmd_info->begin; x <= cmd_info->end; x++)
		{
			if(strcmp(curr_view->dir_entry[x].name, "../") == 0 &&
					cmd_info->begin != cmd_info->end)
				continue;
			curr_view->dir_entry[x].selected = 1;
			y++;
		}
		curr_view->selected_files = y;
	}
	else if(curr_view->selected_files == 0)
	{
		if(cmd_info->end > -1)
		{
			clean_selected_files(curr_view);

			y = 0;
			for(x = cmd_info->end; x < curr_view->list_rows; x++)
			{
				if(y == 1)
					break;
				curr_view->dir_entry[x].selected = 1;
				y++;
			}
			curr_view->selected_files = y;
		}
		else if(id != COM_FIND && id != COM_GREP)
		{
			clean_selected_files(curr_view);

			y = 0;
			for(x = curr_view->list_pos; x < curr_view->list_rows; x++)
			{
				if(y == 1)
					break;

				curr_view->dir_entry[x].selected = 1;
				y++;
			}
			curr_view->selected_files = y;
		}
	}
}

static void
select_count(const struct cmd_info *cmd_info, int count)
{
	int pos;

	/* Both a starting range and an ending range are given. */
	pos = cmd_info->end;
	if(pos < 0)
		pos = curr_view->list_pos;

	clean_selected_files(curr_view);

	while(count-- > 0 && pos < curr_view->list_rows)
	{
		if(strcmp(curr_view->dir_entry[pos].name, "../") != 0)
		{
			curr_view->dir_entry[pos].selected = 1;
			curr_view->selected_files++;
		}
		pos++;
	}
}

void
init_commands(void)
{
	if(cmds_conf.inner == NULL)
	{
		init_cmds(1, &cmds_conf);
		add_buildin_commands((const struct cmd_add *)&commands,
				ARRAY_LEN(commands));

		split_path();
	}
	else
	{
		init_cmds(1, &cmds_conf);
	}
}

static void
split_path(void)
{
	char *path, *p, *q;
	int i;

	path = getenv("PATH");

	paths_count = 1;
	p = path;
	while((p = strchr(p, ':')) != NULL)
	{
		paths_count++;
		p++;
	}

	paths = malloc(paths_count*sizeof(paths[0]));
	if(paths == NULL)
		return;

	i = 0;
	p = path - 1;
	do
	{
		int j;
		char *s;

		p++;
		q = strchr(p, ':');
		if(q == NULL)
		{
			q = p + strlen(p);
		}

		s = malloc((q - p + 1)*sizeof(s[0]));
		if(s == NULL)
		{
			for(j = 0; j < i - 1; j++)
				free(paths[j]);
			paths_count = 0;
			return;
		}
		snprintf(s, q - p + 1, "%s", p);

		p = q;

		s = expand_tilde(s);

		if(access(s, F_OK) != 0)
		{
			free(s);
			continue;
		}

		paths[i++] = s;

		for(j = 0; j < i - 1; j++)
		{
			if(strcmp(paths[j], s) == 0)
			{
				free(s);
				i--;
				break;
			}
		}
	} while (q[0] != '\0');
	paths_count = i;
}

static void
save_history(const char *line, char **hist, int *num, int *len)
{
	int x;

	/* Don't add empty lines */
	if(line[0] == '\0')
		return;

	/* Don't add :!! or :! to history list */
	if(!strcmp(line, "!!") || !strcmp(line, "!"))
		return;

	/* Don't add duplicates */
	for(x = 0; x <= *num; x++)
	{
		if(strcmp(hist[x], line) == 0)
		{
			/* move line to the last position */
			char *t;
			if(x == 0)
				return;
			t = hist[x];
			memmove(hist + 1, hist, sizeof(char *)*x);
			hist[0] = t;
			return;
		}
	}

	if(*num + 1 >= *len)
		*num = x = *len - 1;
	else
		x = *num + 1;

	while(x > 0)
	{
		hist[x] = (char *)realloc(hist[x], strlen(hist[x - 1]) + 1);
		strcpy(hist[x], hist[x - 1]);
		x--;
	}

	hist[0] = (char *)realloc(hist[0], strlen(line) + 1);
	strcpy(hist[0], line);
	(*num)++;
	if(*num >= *len)
		*num = *len - 1;
}

void
save_search_history(const char *pattern)
{
	save_history(pattern, cfg.search_history, &cfg.search_history_num,
			&cfg.search_history_len);
}

void
save_command_history(const char *command)
{
	save_history(command, cfg.cmd_history, &cfg.cmd_history_num,
			&cfg.cmd_history_len);
}

void
save_prompt_history(const char *line)
{
	save_history(line, cfg.prompt_history, &cfg.prompt_history_num,
			&cfg.prompt_history_len);
}

static const char *
find_nth_chr(const char *str, char c, int n)
{
	str--;
	while(n-- > 0 && (str = strchr(str + 1, c)) != NULL);
	return str;
}

static const char *
apply_mod(const char *path, const char *parent, const char *mod)
{
	static char buf[PATH_MAX];

	if(strncmp(mod, ":p", 2) == 0)
	{
		if(is_path_absolute(path))
			return strcpy(buf, path);

		strcpy(buf, parent);
		chosp(buf);
		strcat(buf, "/");
		strcat(buf, path);
	}
	else if(strncmp(mod, ":~", 2) == 0)
	{
		size_t home_len = strlen(cfg.home_dir);
		if(strncmp(path, cfg.home_dir, home_len - 1) != 0)
			return strcpy(buf, path);

		strcpy(buf, "~");
		strcat(buf, path + home_len - 1);
	}
	else if(strncmp(mod, ":.", 2) == 0)
	{
		size_t len = strlen(curr_view->curr_dir);
		if(strncmp(path, curr_view->curr_dir, len) != 0 || path[len] == '\0')
			return strcpy(buf, path);

		strcpy(buf, path + len + 1);
	}
	else if(strncmp(mod, ":h", 2) == 0)
	{
		char *p = strrchr(path, '/');
		if(p == NULL)
			return strcpy(buf, ".");
		if(is_root_dir(path))
			return strcpy(buf, path);

		strcpy(buf, path);
		buf[p - path + 1] = '\0';
		if(!is_root_dir(buf))
			buf[p - path] = '\0';
	}
	else if(strncmp(mod, ":t", 2) == 0)
	{
		char *p = strrchr(path, '/');
		if(p == NULL)
			return strcpy(buf, path);

		strcpy(buf, p + 1);
	}
	else if(strncmp(mod, ":r", 2) == 0)
	{
		char *slash = strrchr(path, '/');
		char *dot = strrchr(path, '.');
		if(dot == NULL || (slash != NULL && dot < slash) || dot == path ||
				dot == slash + 1)
			return strcpy(buf, path);

		strcpy(buf, path);
		buf[dot - path] = '\0';
	}
	else if(strncmp(mod, ":e", 2) == 0)
	{
		char *slash = strrchr(path, '/');
		char *dot = strrchr(path, '.');
		if(dot == NULL || (slash != NULL && dot < slash) || dot == path ||
				dot == slash + 1)
			return strcpy(buf, "");

		strcpy(buf, dot + 1);
	}
	else if(strncmp(mod, ":s", 2) == 0 || strncmp(mod, ":gs", 3) == 0)
	{
		char pattern[256], sub[256];
		char c = (mod[1] == 'g') ? mod++[3] : mod[2];
		const char *t, *p = find_nth_chr(mod, c, 3);
		if(p == NULL)
			return strcpy(buf, path);
		t = find_nth_chr(mod, c, 2);
		snprintf(pattern, t - (mod + 3) + 1, "%s", mod + 3);
		snprintf(sub, p - (t + 1) + 1, "%s", t + 1);
		strcpy(buf, substitute_in_name(path, pattern, sub, (mod[0] == 'g')));
	}
	else
		return NULL;

	return buf;
}

static const char *
apply_mods(const char *path, const char *parent, const char *mod)
{
	static char buf[PATH_MAX];

	strcpy(buf, path);
	while(*mod != '\0')
	{
		const char *p = apply_mod(buf, parent, mod);
		if(p == NULL)
			break;
		strcpy(buf, p);
		mod += 2;
	}

	return buf;
}

#ifndef TEST
static
#endif
char *
append_selected_files(FileView *view, char *expanded, int under_cursor,
		const char *mod)
{
	int dir_name_len = 0;

	if(view == other_view)
		dir_name_len = strlen(other_view->curr_dir) + 1;

	if(view->selected_files && !under_cursor)
	{
		int y, x = 0;
		size_t len = strlen(expanded);
		for(y = 0; y < view->list_rows; y++)
		{
			char *temp;
			char buf[PATH_MAX] = "";

			if(!view->dir_entry[y].selected)
				continue;

			if(dir_name_len != 0)
				strcat(strcpy(buf, view->curr_dir), "/");
			strcat(buf, view->dir_entry[y].name);
			chosp(buf);
			temp = escape_filename(apply_mods(buf, view->curr_dir, mod), 0);
			expanded = (char *)realloc(expanded, len + strlen(temp) + 1 + 1);
			strcat(expanded, temp);
			if(++x != view->selected_files)
				strcat(expanded, " ");

			free(temp);

			len = strlen(expanded);
		}
	}
	else
	{
		char *temp;
		char buf[PATH_MAX] = "";

		if(dir_name_len != 0)
			strcat(strcpy(buf, view->curr_dir), "/");
		strcat(buf, view->dir_entry[view->list_pos].name);
		chosp(buf);
		temp = escape_filename(apply_mods(buf, view->curr_dir, mod), 0);

		expanded = (char *)realloc(expanded, strlen(expanded) + strlen(temp) + 1);
		strcat(expanded, temp);

		free(temp);
	}

	return expanded;
}

static char *
expand_directory_path(FileView *view, char *expanded, const char *mod)
{
	char *t;
	char *escaped;

	if((escaped = escape_filename(apply_mods(view->curr_dir, "/", mod),
			0)) == NULL)
	{
		(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
		free(expanded);
		return NULL;
	}

	t = realloc(expanded, strlen(expanded) + strlen(escaped) + 1);
	if(t == NULL)
	{
		(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
		free(expanded);
		return NULL;
	}
	strcat(t, escaped);
	return t;
}

/* args could be equal NULL
 * The string returned needs to be freed in the calling function */
char *
expand_macros(FileView *view, const char *command, const char *args,
		int *use_menu, int *split)
{
	size_t cmd_len;
	char *expanded;
	size_t x;
	int y = 0;
	int len = 0;

	cmd_len = strlen(command);

	expanded = (char *)calloc(cmd_len + 1, sizeof(char *));

	for(x = 0; x < cmd_len; x++)
		if(command[x] == '%')
			break;

	strncat(expanded, command, x);
	x++;
	len = strlen(expanded);

	do
	{
		switch(command[x])
		{
			case 'a': /* user arguments */
				{
					if(args == NULL)
						break;
					else
					{
						char arg_buf[strlen(args) + 2];

						expanded = (char *)realloc(expanded,
								strlen(expanded) + strlen(args) + 3);
						snprintf(arg_buf, sizeof(arg_buf), "%s", args);
						strcat(expanded, arg_buf);
						len = strlen(expanded);
					}
				}
				break;
			case 'b': /* selected files of both dirs */
				expanded = append_selected_files(curr_view, expanded, 0,
						command + x + 1);
				len = strlen(expanded);
				expanded = realloc(expanded, len + 1 + 1);
				strcat(expanded, " ");
				expanded = append_selected_files(other_view, expanded, 0,
						command + x + 1);
				len = strlen(expanded);
				break;
			case 'c': /* current dir file under the cursor */
				expanded = append_selected_files(curr_view, expanded, 1,
						command + x + 1);
				len = strlen(expanded);
				break;
			case 'C': /* other dir file under the cursor */
				expanded = append_selected_files(other_view, expanded, 1,
						command + x + 1);
				len = strlen(expanded);
				break;
			case 'f': /* current dir selected files */
				expanded = append_selected_files(curr_view, expanded, 0,
						command + x + 1);
				len = strlen(expanded);
				break;
			case 'F': /* other dir selected files */
				expanded = append_selected_files(other_view, expanded, 0,
						command + x + 1);
				len = strlen(expanded);
				break;
			case 'd': /* current directory */
				expanded = expand_directory_path(curr_view, expanded,
						command + x + 1);
				len = strlen(expanded);
				break;
			case 'D': /* other directory */
				expanded = expand_directory_path(other_view, expanded,
						command + x + 1);
				len = strlen(expanded);
				break;
			case 'm': /* use menu */
				*use_menu = 1;
				break;
			case 'M': /* use menu like with :locate and :find */
				*use_menu = 2;
				break;
			case 's': /* split in new screen region */
				*split = 1;
				break;
			case '%':
				expanded = (char *)realloc(expanded, len + 2);
				strcat(expanded, "%");
				len++;
				break;
			default:
				break;
		}
		x++;
		y = x;

		while(x < cmd_len)
		{
			if(x == y)
			{
				if(strncmp(command + x, ":p", 2) == 0)
					y += 2;
				else if(strncmp(command + x, ":~", 2) == 0)
					y += 2;
				else if(strncmp(command + x, ":.", 2) == 0)
					y += 2;
				else if(strncmp(command + x, ":h", 2) == 0)
					y += 2;
				else if(strncmp(command + x, ":t", 2) == 0)
					y += 2;
				else if(strncmp(command + x, ":r", 2) == 0)
					y += 2;
				else if(strncmp(command + x, ":e", 2) == 0)
					y += 2;
				else if(strncmp(command + x, ":s", 2) == 0 ||
						strncmp(command + x, ":gs", 3) == 0)
				{
					const char *p;
					if(command[x + 1] == 'g')
						x += 3;
					else
						x += 2;
					y = x;
					p = find_nth_chr(command + x, command[x], 3);
					if(p != NULL)
						y += (p - (command + x)) + 1;
					else
						y += strlen(command);
					x = y;
					continue;
				}
			}
			if(command[x] == '%')
				break;
			x++;
		}
		expanded = (char *)realloc(expanded, len + cmd_len + 1);
		strncat(expanded, command + y, x - y);
		len = strlen(expanded);
		x++;
	}while(x < cmd_len);

	len++;
	expanded[len] = '\0';
	if(len > cfg.max_args/2)
		(void)show_error_msg("Argument is too long", " FIXME ");

	return expanded;
}

static void
split_screen(FileView *view, char *command)
{
	char buf[1024];

	if(command)
	{
		snprintf(buf, sizeof(buf), "screen -X eval \'chdir %s\'", view->curr_dir);
		my_system(buf);

		snprintf(buf, sizeof(buf), "screen-open-region-with-program \"%s\"",
				command);

		my_system(buf);
	}
	else
	{
		snprintf(buf, sizeof(buf), "screen -X eval \'chdir %s\'", view->curr_dir);
		my_system(buf);

		snprintf(buf, sizeof(buf), "screen-open-region-with-program %s", cfg.shell);
		my_system(buf);
	}
}

/*
 * pause:
 *  > 0 - pause always
 *  = 0 - not pause
 *  < 0 - pause on error
 */
int
shellout(const char *command, int pause)
{
	size_t len = (command != NULL) ? strlen(command) : 0;
	char buf[cfg.max_args];
	int result;

	if(pause > 0 && len > 1 && command[len - 1] == '&')
		pause = -1;

	if(command != NULL)
	{
		if(cfg.use_screen)
		{
			int bg;
			char *escaped;
			char *ptr = (char *)NULL;
			char *title = strstr(command, get_vicmd(&bg));
			char *escaped_sh = escape_filename(cfg.shell, 0);

			/* Needed for symlink directories and sshfs mounts */
			escaped = escape_filename(curr_view->curr_dir, 0);
			snprintf(buf, sizeof(buf), "screen -X setenv PWD %s", escaped);
			free(escaped);

			my_system(buf);

			if(title != NULL)
			{
				if(pause > 0)
					snprintf(buf, sizeof(buf),
							"screen -t \"%s\" %s -c '%s" PAUSE_STR "'",
							title + strlen(get_vicmd(&bg)) + 1, escaped_sh, command);
				else
				{
					escaped = escape_filename(command, 0);
					snprintf(buf, sizeof(buf), "screen -t \"%s\" %s -c %s",
							title + strlen(get_vicmd(&bg)) + 1, escaped_sh, escaped);
					free(escaped);
				}
			}
			else
			{
				ptr = strchr(command, ' ');
				if(ptr != NULL)
				{
					*ptr = '\0';
					title = strdup(command);
					*ptr = ' ';
				}
				else
					title = strdup("Shell");

				if(pause > 0)
					snprintf(buf, sizeof(buf),
							"screen -t \"%.10s\" %s -c '%s" PAUSE_STR "'", title,
							escaped_sh, command);
				else
				{
					escaped = escape_filename(command, 0);
					snprintf(buf, sizeof(buf), "screen -t \"%.10s\" %s -c %s", title,
							escaped_sh, escaped);
					free(escaped);
				}
				free(title);
			}
			free(escaped_sh);
		}
		else
		{
			if(pause > 0)
				snprintf(buf, sizeof(buf), "%s" PAUSE_STR, command);
			else
				snprintf(buf, sizeof(buf), "%s", command);
		}
	}
	else
	{
		if(cfg.use_screen)
		{
			snprintf(buf, sizeof(buf), "screen -X setenv PWD \'%s\'",
					curr_view->curr_dir);

			my_system(buf);

			snprintf(buf, sizeof(buf), "screen");
		}
		else
		{
			snprintf(buf, sizeof(buf), "%s", cfg.shell);
		}
	}

	def_prog_mode();
	endwin();

	result = WEXITSTATUS(my_system(buf));

	if(result != 0 && pause < 0)
		my_system(PAUSE_CMD);

	/* force views update */
	memset(&lwin.dir_mtime, 0, sizeof(lwin.dir_mtime));
	memset(&rwin.dir_mtime, 0, sizeof(rwin.dir_mtime));

	/* always redraw to handle resizing of terminal */
	if(!curr_stats.auto_redraws)
		modes_redraw();

	curs_set(0);

	return result;
}

char *
fast_run_complete(char *cmd)
{
	char *buf = NULL;
	char *p;

	p = strchr(cmd, ' ');
	if(p == NULL)
		p = cmd + strlen(cmd);
	else
		*p = '\0';

	reset_completion();
	exec_completion(cmd);
	free(next_completion());

	if(get_completion_count() > 2)
	{
		status_bar_error("Command beginning is ambiguous");
	}
	else
	{
		char *completed;

		completed = next_completion();
		buf = malloc(strlen(completed) + 1 + strlen(p) + 1);
		sprintf(buf, "%s %s", completed, p);
		free(completed);
	}

	return buf;
}

char *
edit_selection(FileView *view, int *bg)
{
	int use_menu = 0;
	int split = 0;
	char *buf;
	char *files = expand_macros(view, "%f", NULL, &use_menu, &split);

	if((buf = (char *)malloc(strlen(get_vicmd(bg)) + strlen(files) + 2)) != NULL)
		snprintf(buf, strlen(get_vicmd(bg)) + 1 + strlen(files) + 1, "%s %s",
				get_vicmd(bg), files);

	free(files);
	return buf;
}

static int
set_view_filter(FileView *view, const char *filter, int invert)
{
	regex_t re;
	int err;

	if((err = regcomp(&re, filter, REG_EXTENDED)) != 0)
	{
		status_bar_errorf("Filter not set: %s", get_regexp_error(err, &re));
		regfree(&re);
		return 1;
	}
	regfree(&re);

	view->invert = invert;
	view->filename_filter = realloc(view->filename_filter, strlen(filter) + 1);
	strcpy(view->filename_filter, filter);
	load_saving_pos(view, 1);
	return 0;
}

static wchar_t *
substitute_specs(const char *cmd)
{
	wchar_t *buf, *p;

	buf = malloc((strlen(cmd) + 1)*sizeof(wchar_t));
	if(buf == NULL)
	{
		return NULL;
	}

	p = buf;
	while(*cmd != '\0')
	{
		if(strncasecmp(cmd, "<cr>", 4) == 0)
		{
			*p++ = L'\r';
			cmd += 4;
		}
		else if(strncasecmp(cmd, "<space>", 7) == 0)
		{
			*p++ = L' ';
			cmd += 7;
		}
		else if(cmd[0] == '<' && toupper(cmd[1]) == 'F' && isdigit(cmd[2]) &&
				cmd[3] == '>')
		{
			*p++ = KEY_F0 + (cmd[2] - '0');
			cmd += 4;
		}
		else if(cmd[0] == '<' && toupper(cmd[1]) == 'F' && isdigit(cmd[2]) &&
				isdigit(cmd[3]) && cmd[4] == '>')
		{
			int num = (cmd[2] - '0')*10 + (cmd[3] - '0');
			if(num < 64)
			{
				*p++ = KEY_F0 + num;
				cmd += 5;
			}
		}
		else if(cmd[0] == '<' && toupper(cmd[1]) == 'C' && cmd[2] == '-' &&
				strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_", toupper(cmd[3])) != NULL &&
				cmd[4] == '>')
		{
			*p++ = toupper(cmd[3]) - 'A' + 1;
			cmd += 5;
		}
		else
		{
			*p++ = (wchar_t)*cmd++;
		}
	}
	*p = L'\0';

	return buf;
}

static const char *
skip_spaces(const char *cmd)
{
	while(isspace(*cmd) && *cmd != '\0')
		cmd++;
	return cmd;
}

static const char *
skip_word(const char *cmd)
{
	while(!isspace(*cmd) && *cmd != '\0')
		cmd++;
	return cmd;
}

static void
remove_selection(FileView *view)
{
	clean_selected_files(view);
	draw_dir_list(view, view->top_line);
	move_to_list_pos(view, view->list_pos);
}

/* Returns negative value in case of error */
static int
execute_command(FileView *view, char *command, int menu)
{
	int id;
	int result;

	if(command == NULL)
	{
		remove_selection(view);
		return 0;
	}

	while(isspace(*command) || *command == ':')
		command++;

	if(command[0] == '"')
		return 0;

	if((command[0] == '\0') && !menu)
	{
		remove_selection(view);
		return 0;
	}

	if(!menu)
	{
		init_cmds(1, &cmds_conf);
		cmds_conf.begin = 0;
		cmds_conf.current = view->list_pos;
		cmds_conf.end = view->list_rows - 1;
	}

	id = get_cmd_id(command);
	if(id == USER_CMD_ID)
	{
		char buf[COMMAND_GROUP_INFO_LEN];

		snprintf(buf, sizeof(buf), "in %s: %s",
				replace_home_part(curr_view->curr_dir), command);

		cmd_group_begin(buf);
		cmd_group_end();
	}

	need_clean_selection = 1;
	result = execute_cmd(command);

	if(result >= 0)
		return result;

	switch(result)
	{
		case CMDS_ERR_LOOP:
			status_bar_error("Loop in commands");
			break;
		case CMDS_ERR_NO_MEM:
			status_bar_error("Unable to allocate enough memory");
			break;
		case CMDS_ERR_TOO_FEW_ARGS:
			status_bar_error("Too few arguments");
			break;
		case CMDS_ERR_TRAILING_CHARS:
			status_bar_error("Trailing characters");
			break;
		case CMDS_ERR_INCORRECT_NAME:
			status_bar_error("Incorrect command name");
			break;
		case CMDS_ERR_NEED_BANG:
			status_bar_error("Add bang to force");
			break;
		case CMDS_ERR_NO_BUILDIN_REDEFINE:
			status_bar_error("Can't redefine builtin command");
			break;
		case CMDS_ERR_INVALID_CMD:
			status_bar_error("Invalid command name");
			break;
		case CMDS_ERR_NO_BANG_ALLOWED:
			status_bar_error("No ! is allowed");
			break;
		case CMDS_ERR_NO_RANGE_ALLOWED:
			status_bar_error("No range is allowed");
			break;
		case CMDS_ERR_NO_QMARK_ALLOWED:
			status_bar_error("No ? is allowed");
			break;
		case CMDS_ERR_INVALID_RANGE:
			/* message dialog is enough */
			break;
		case CMDS_ERR_NO_SUCH_UDF:
			status_bar_error("No such user defined command");
			break;
		case CMDS_ERR_UDF_IS_AMBIGUOUS:
			status_bar_error("Ambiguous use of user-defined command");
			break;
		case CMDS_ERR_ZERO_COUNT:
			status_bar_error("Zero count");
			break;
		case CMDS_ERR_INVALID_ARG:
			status_bar_error("Invalid argument");
			break;
		default:
			status_bar_error("Unknown error");
			break;
	}
	if(!menu && get_mode() == NORMAL_MODE)
		remove_selection(view);
	return -1;
}

/*
 * Return value:
 *  - 0 not in arg
 *  - 1 skip next char
 *  - 2 in arg
 */
#ifndef TEST
static
#endif
int
line_pos(const char *begin, const char *end, char sep, int rquoting)
{
	int state;
	int count;
	enum { BEGIN, NO_QUOTING, S_QUOTING, D_QUOTING, R_QUOTING };

	state = BEGIN;
	count = 0;
	while(begin != end)
	{
		switch(state)
		{
			case BEGIN:
				if(sep == ' ' && *begin == '\'')
					state = S_QUOTING;
				else if(sep == ' ' && *begin == '"')
					state = D_QUOTING;
				else if(sep == ' ' && *begin == '/' && rquoting)
					state = R_QUOTING;
				else if(*begin != sep)
					state = NO_QUOTING;
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
				if(*begin == '/')
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
		}
		begin++;
	}
	if(state == NO_QUOTING)
	{
		if(sep == ' ')
			return 0;
		else if(count > 0 && count < 3)
			return 2;
	}
	else if(state != BEGIN)
	{
		return 2; /* error: no closing quote */
	}
	else if(sep != ' ' && count > 0 && *end != sep)
		return 2;

	return 0;
}

static int
is_whole_line_command(const char *cmd)
{
	if(*cmd == '!')
		return 1;
	else if(strncmp(cmd, "cm", 2) == 0)
		return 1;
	else if(strncmp(cmd, "nm", 2) == 0)
		return 1;
	else if(strncmp(cmd, "vm", 2) == 0)
		return 1;
	else if(strncmp(cmd, "vn", 2) == 0)
		return 1;
	else if(strncmp(cmd, "nn", 2) == 0)
		return 1;
	else if(strncmp(cmd, "no", 2) == 0)
		return 1;
	else if(strncmp(cmd, "cno", 3) == 0)
		return 1;
	else if(strncmp(cmd, "map", 3) == 0)
		return 1;
	else if(strncmp(cmd, "com", 3) == 0)
		return 1;
	else if(strncmp(cmd, "filet", 5) == 0)
		return 1;
	else if(strncmp(cmd, "filev", 5) == 0)
		return 1;
	else
		return 0;
}

int
exec_commands(char *cmd, FileView *view, int save_hist, int type)
{
	int save_msg = 0;
	char *p, *q;

	if(save_hist && type == GET_COMMAND)
		save_command_history(cmd);

	if(*cmd == '\0')
		return exec_command(cmd, view, type);

	p = cmd;
	q = cmd;
	while(*cmd != '\0')
	{
		if(*p == '\\')
		{
			if(*(p + 1) == '|')
			{
				*q++ = '|';
				p += 2;
			}
			else
			{
				*q++ = *p++;
				*q++ = *p++;
			}
		}
		else if((*p == '|' &&
				line_pos(cmd, q, ' ', strncmp(cmd, "fil", 3) == 0) == 0) || *p == '\0')
		{
			if(*p != '\0')
				p++;

			while(*cmd == ' ' || *cmd == ':')
				cmd++;
			if(is_whole_line_command(cmd))
			{
				save_msg += exec_command(cmd, view, type);
				break;
			}

			*q = '\0';
			q = p;

			save_msg += exec_command(cmd, view, type);

			cmd = q;
		}
		else
			*q++ = *p++;
	}

	return save_msg;
}

char *
find_last_command(char *cmd)
{
	char *p, *q;

	p = cmd;
	q = cmd;
	while(*cmd != '\0')
	{
		if(*p == '\\')
		{
			if(*(p + 1) == '|')
				q++;
			else
				q += 2;
			p += 2;
		}
		else if((*p == '|' &&
				line_pos(cmd, q, ' ', strncmp(cmd, "fil", 3) == 0) == 0) || *p == '\0')
		{
			if(*p != '\0')
				p++;

			while(*cmd == ' ' || *cmd == ':')
				cmd++;
			if(*cmd == '!' || strncmp(cmd, "com", 3) == 0)
				break;

			q = p;

			if(*q == '\0')
				break;

			cmd = q;
		}
		else
		{
			q++;
			p++;
		}
	}

	return cmd;
}

int
exec_command(char *cmd, FileView *view, int type)
{
	if(cmd == NULL)
	{
		if(type == GET_FSEARCH_PATTERN || type == GET_BSEARCH_PATTERN)
			return find_pattern(view, view->regexp, type == GET_BSEARCH_PATTERN, 1);
		if(type == GET_VFSEARCH_PATTERN || type == GET_VBSEARCH_PATTERN)
			return find_vpattern(view, view->regexp, type == GET_VBSEARCH_PATTERN);
		if(type == GET_COMMAND)
			return execute_command(view, cmd, 0);
		return 0;
	}

	if(type == GET_COMMAND)
	{
		return execute_command(view, cmd, 0);
	}
	if(type == GET_MENU_COMMAND)
	{
		return execute_command(view, cmd, 1);
	}
	else if(type == GET_FSEARCH_PATTERN || type == GET_BSEARCH_PATTERN)
	{
		strncpy(view->regexp, cmd, sizeof(view->regexp));
		save_search_history(cmd);
		return find_pattern(view, cmd, type == GET_BSEARCH_PATTERN, 1);
	}
	else if(type == GET_VFSEARCH_PATTERN || type == GET_VBSEARCH_PATTERN)
	{
		strncpy(view->regexp, cmd, sizeof(view->regexp));
		save_search_history(cmd);
		return find_vpattern(view, cmd, type == GET_VBSEARCH_PATTERN);
	}
	return 0;
}

void _gnuc_noreturn
comm_quit(int write_info)
{
	unmount_fuse();

	if(write_info)
		write_info_file();

	if(cfg.vim_filter)
	{
		char buf[PATH_MAX];
		FILE *fp;

		snprintf(buf, sizeof(buf), "%s/vimfiles", cfg.config_dir);
		fp = fopen(buf, "w");
		fclose(fp);
	}

#ifdef _WIN32
	system("cls");
#endif

	endwin();
	exit(0);
}

void
comm_only(void)
{
	only_cmd(NULL);
}

void
comm_split(void)
{
	if(curr_stats.number_of_windows == 2)
		return;

	curr_stats.number_of_windows = 2;
	redraw_window();
}

static int
goto_cmd(const struct cmd_info *cmd_info)
{
	move_to_list_pos(curr_view, cmd_info->end);
	return 0;
}

static int
emark_cmd(const struct cmd_info *cmd_info)
{
	int i = 0;
	char *com = (char *)cmd_info->args;
	char buf[COMMAND_GROUP_INFO_LEN];

	while(isspace(com[i]) && (size_t)i < strlen(com))
		i++;

	if(strlen(com + i) == 0)
		return 0;

	if(cmd_info->usr1)
	{
		show_user_menu(curr_view, com, cmd_info->usr1 == 2);
	}
	else if(cmd_info->usr2)
	{
		if(!cfg.use_screen)
		{
			status_bar_error("The screen program support isn't enabled");
			return 1;
		}

		split_screen(curr_view, com);
	}
	else if(cmd_info->bg)
	{
		start_background_job(com + i);
	}
	else
	{
		clean_selected_files(curr_view);
		if(shellout(com + i, cmd_info->emark ? 1 : (cfg.fast_run ? 0 : -1)) == 127
				&& cfg.fast_run)
		{
			char *buf = fast_run_complete(com + i);
			if(buf == NULL)
				return 1;

			shellout(buf, cmd_info->emark ? 1 : -1);
			free(buf);
		}
	}

	snprintf(buf, sizeof(buf), "in %s: !%s",
			replace_home_part(curr_view->curr_dir), cmd_info->raw_args);
	cmd_group_begin(buf);
	add_operation(OP_USR, strdup(com + i), NULL, "", "");
	cmd_group_end();

	return 0;
}

static int
alink_cmd(const struct cmd_info *cmd_info)
{
	if(cmd_info->qmark)
	{
		if(cmd_info->argc > 0)
		{
			status_bar_error("No arguments are allowed if you use \"!\"");
			return 1;
		}
		return cpmv_files(curr_view, NULL, -1, 0, 1, 0) != 0;
	}
	else
	{
		return cpmv_files(curr_view, cmd_info->argv, cmd_info->argc, 0, 1,
				cmd_info->emark) != 0;
	}
}

static int
apropos_cmd(const struct cmd_info *cmd_info)
{
	static char *last_args;

	if(cmd_info->argc > 0)
	{
		free(last_args);
		last_args = strdup(cmd_info->args);
	}
	else if(last_args == NULL)
	{
		status_bar_error("Nothing to repeat");
		return 1;
	}

	show_apropos_menu(curr_view, last_args);
	return 0;
}

static int
cd(FileView *view, const char *path)
{
	char dir[PATH_MAX];

	if(path != NULL)
	{
		char *arg = expand_tilde(strdup(path));
		if(is_path_absolute(arg))
			snprintf(dir, sizeof(dir), "%s", arg);
#ifdef _WIN32
		else if(*arg == '/' && is_unc_root(view->curr_dir))
			snprintf(dir, sizeof(dir), "%s", view->curr_dir);
		else if(*arg == '/' && is_unc_path(view->curr_dir))
			snprintf(dir, strchr(view->curr_dir + 2, '/') - view->curr_dir + 1, "%s",
					view->curr_dir);
		else if(*arg == '/')
			snprintf(dir, sizeof(dir), "%c:%s", view->curr_dir[0], arg);
#endif
		else if(strcmp(arg, "-") == 0)
			snprintf(dir, sizeof(dir), "%s", view->last_dir);
		else
			snprintf(dir, sizeof(dir), "%s/%s", view->curr_dir, arg);
		free(arg);
	}
	else
	{
		snprintf(dir, sizeof(dir), "%s", cfg.home_dir);
	}

#ifndef _WIN32
	if(access(dir, F_OK) != 0)
#else
	if(access(dir, F_OK) != 0 && !is_unc_root(dir))
#endif
	{
		LOG_SERROR_MSG(errno, "Can't access(,F_OK) \"%s\"", dir);

		(void)show_error_msgf("Destination doesn't exist", "\"%s\"", dir);
		return 0;
	}
#ifndef _WIN32
	if(access(dir, X_OK) != 0)
#else
	if(access(dir, X_OK) != 0 && !is_unc_root(dir))
#endif
	{
		LOG_SERROR_MSG(errno, "Can't access(,X_OK) \"%s\"", dir);

		(void)show_error_msgf("Permission denied", "\"%s\"", dir);
		return 0;
	}

	if(change_directory(view, dir) < 0)
		return 0;

	load_dir_list(view, 0);
	if(view == curr_view)
		move_to_list_pos(view, view->list_pos);
	return 0;
}

static int
cd_cmd(const struct cmd_info *cmd_info)
{
	char dir[PATH_MAX];
	int result;
	if(!cfg.auto_ch_pos)
	{
		clean_positions_in_history(curr_view);
		curr_stats.ch_pos = 0;
	}
	if(cmd_info->argc == 0)
	{
		result = cd(curr_view, cfg.home_dir);
		if(cmd_info->emark)
			result += cd(other_view, cfg.home_dir);
	}
	else if(cmd_info->argc == 1)
	{
		snprintf(dir, sizeof(dir), "%s/%s", curr_view->curr_dir, cmd_info->argv[0]);
		result = cd(curr_view, cmd_info->argv[0]);
		if(cmd_info->emark)
		{
			if(cmd_info->argv[0][0] != '/' && cmd_info->argv[0][0] != '~' &&
					strcmp(cmd_info->argv[0], "-") != 0)
				result += cd(other_view, dir);
			else if(strcmp(cmd_info->argv[0], "-") == 0)
				result += cd(other_view, curr_view->curr_dir);
			else
				result += cd(other_view, cmd_info->argv[0]);
			wrefresh(other_view->win);
		}
	}
	else
	{
		snprintf(dir, sizeof(dir), "%s/%s", curr_view->curr_dir, cmd_info->argv[1]);
		result = cd(curr_view, cmd_info->argv[0]);
		if(cmd_info->argv[1][0] != '/' && cmd_info->argv[1][0] != '~')
			result += cd(other_view, dir);
		else
			result += cd(other_view, cmd_info->argv[1]);
		wrefresh(other_view->win);
	}
	if(!cfg.auto_ch_pos)
		curr_stats.ch_pos = 1;
	return result;
}

static int
change_cmd(const struct cmd_info *cmd_info)
{
	enter_permissions_mode(curr_view);
	return 0;
}

static int
clone_cmd(const struct cmd_info *cmd_info)
{
	if(cmd_info->qmark)
	{
		if(cmd_info->argc > 0)
		{
			status_bar_error("No arguments are allowed if you use \"!\"");
			return 1;
		}
		return clone_files(curr_view, NULL, -1, 0) != 0;
	}
	else
	{
		return clone_files(curr_view, cmd_info->argv, cmd_info->argc,
				cmd_info->emark) != 0;
	}
}

static int
cmap_cmd(const struct cmd_info *cmd_info)
{
	return do_map(cmd_info, "Command Line", "cmap", CMDLINE_MODE, 0) != 0;
}

static int
cnoremap_cmd(const struct cmd_info *cmd_info)
{
	return do_map(cmd_info, "Command Line", "cmap", CMDLINE_MODE, 1) != 0;
}

static int
colorscheme_cmd(const struct cmd_info *cmd_info)
{
	int pos = -1;
	if(cmd_info->argc > 0)
		pos = find_color_scheme(cmd_info->argv[0]);
	if(cmd_info->qmark)
	{
		status_bar_message(col_schemes[cfg.color_scheme_cur].name);
		return 1;
	}
	else if(cmd_info->argc == 0)
	{
		/* show menu with colorschemes listed */
		show_colorschemes_menu(curr_view);
		return 0;
	}
	else if(pos < 0 && cmd_info->emark)
	{
		if(add_color_scheme(cmd_info->argv[0],
				(cmd_info->argc == 1) ? NULL : cmd_info->argv[1]) != 0)
			return 0;
		return load_color_scheme(cmd_info->argv[0]);
	}
	else if(pos >= 0)
	{
		if(cmd_info->emark && cmd_info->argc == 2)
			strcpy(col_schemes[pos].dir, cmd_info->argv[1]);
		return load_color_scheme(cmd_info->argv[0]);
	}
	else
	{
		status_bar_errorf("Cannot find colorscheme %s" , cmd_info->argv[0]);
		return 1;
	}
}

static int
command_cmd(const struct cmd_info *cmd_info)
{
	char *desc;

	if(cmd_info->argc == 0)
		return show_commands_menu(curr_view) != 0;

	if((desc = list_udf_content(cmd_info->argv[0])) == NULL)
		return CMDS_ERR_NO_SUCH_UDF;
	status_bar_message(desc);
	free(desc);
	return 1;
}

static int
copy_cmd(const struct cmd_info *cmd_info)
{
	if(cmd_info->qmark)
	{
		if(cmd_info->argc > 0)
		{
			status_bar_error("No arguments are allowed if you use \"!\"");
			return 1;
		}
		return cpmv_files(curr_view, NULL, -1, 0, 0, 0) != 0;
	}
	else
	{
		return cpmv_files(curr_view, cmd_info->argv, cmd_info->argc, 0, 0,
				cmd_info->emark) != 0;
	}
}

static int
cunmap_cmd(const struct cmd_info *cmd_info)
{
	return do_unmap(cmd_info->argv[0], CMDLINE_MODE);
}

static int
delete_cmd(const struct cmd_info *cmd_info)
{
	int reg = DEFAULT_REG_NAME;
	int result;

	result = get_reg_and_count(cmd_info, &reg);
	if(result == 0)
		result = delete_file(curr_view, reg, 0, NULL, 1) != 0;
	return result;
}

static int
delmarks_cmd(const struct cmd_info *cmd_info)
{
	int i;
	int save_msg = 0;

	if(cmd_info->emark)
	{
		const char *p = valid_bookmarks;
		while(*p != '\0')
		{
			int index = mark2index(*p++);
			remove_bookmark(index);
		}
		return 0;
	}

	if(cmd_info->argc == 0)
		return CMDS_ERR_TOO_FEW_ARGS;

	for(i = 0; i < cmd_info->argc; i++)
	{
		int j;
		for(j = 0; cmd_info->argv[i][j] != '\0'; j++)
		{
			if(strchr(valid_bookmarks, cmd_info->argv[i][j]) == NULL)
				return CMDS_ERR_INVALID_ARG;
		}
	}

	for(i = 0; i < cmd_info->argc; i++)
	{
		int j;
		for(j = 0; cmd_info->argv[i][j] != '\0'; j++)
		{
			int index = mark2index(cmd_info->argv[i][j]);
			save_msg += remove_bookmark(index);
		}
	}
	return save_msg;
}

static int
dirs_cmd(const struct cmd_info *cmd_info)
{
	return show_dirstack_menu(curr_view) != 0;
}

static int
edit_cmd(const struct cmd_info *cmd_info)
{
	if(cmd_info->argc != 0)
	{
		char buf[PATH_MAX];
		size_t len;
		int i;
		int bg;

		if(cfg.vim_filter)
			use_vim_plugin(curr_view, cmd_info->argc, cmd_info->argv); /* no return */

		len = snprintf(buf, sizeof(buf), "%s ", get_vicmd(&bg));
		for(i = 0; i < cmd_info->argc && len < sizeof(buf) - 1; i++)
		{
			char *escaped = escape_filename(cmd_info->argv[i], 0);
			len += snprintf(buf + len, sizeof(buf) - len, "%s ", escaped);
			free(escaped);
		}
		if(bg)
			start_background_job(buf);
		else
			shellout(buf, -1);
		return 0;
	}
	if(!curr_view->selected_files ||
			!curr_view->dir_entry[curr_view->list_pos].selected)
	{
		if(cfg.vim_filter)
			use_vim_plugin(curr_view, cmd_info->argc, cmd_info->argv); /* no return */

		view_file(get_current_file_name(curr_view), -1);
	}
	else
	{
		int i;
		char *cmd;
		int bg;

		for(i = 0; i < curr_view->list_rows; i++)
		{
			struct stat st;
			if(curr_view->dir_entry[i].selected == 0)
				continue;
			if(lstat(curr_view->dir_entry[i].name, &st) == 0 &&
					access(curr_view->dir_entry[i].name, F_OK) != 0)
			{
				(void)show_error_msgf("Access error",
						"Can't access destination of link \"%s\". It might be broken.",
						curr_view->dir_entry[i].name);
				return 0;
			}
		}

		if(cfg.vim_filter)
			use_vim_plugin(curr_view, cmd_info->argc, cmd_info->argv); /* no return */

		if((cmd = edit_selection(curr_view, &bg)) == NULL)
		{
			(void)show_error_msg("Memory error", "Unable to allocate enough memory");
			return 0;
		}
		if(bg)
			start_background_job(cmd);
		else
			shellout(cmd, -1);
		free(cmd);
	}
	return 0;
}

static int
empty_cmd(const struct cmd_info *cmd_info)
{
	empty_trash();
	return 0;
}

static int
file_cmd(const struct cmd_info *cmd_info)
{
	return show_filetypes_menu(curr_view, cmd_info->bg) != 0;
}

static int
filetype_cmd(const struct cmd_info *cmd_info)
{
	const char *progs;

	progs = skip_word(cmd_info->args);
	progs = skip_spaces(progs + 1);

	set_programs(cmd_info->argv[0], progs, 0);
	return 0;
}

static int
filextype_cmd(const struct cmd_info *cmd_info)
{
	const char *progs;

	progs = skip_word(cmd_info->args);
	progs = skip_spaces(progs + 1);

	set_programs(cmd_info->argv[0], progs, 1);
	return 0;
}

static int
fileviewer_cmd(const struct cmd_info *cmd_info)
{
	const char *progs;

	progs = skip_word(cmd_info->args);
	progs = skip_spaces(progs + 1);

	set_fileviewer(cmd_info->argv[0], progs);
	return 0;
}

static int
filter_cmd(const struct cmd_info *cmd_info)
{
	if(cmd_info->qmark)
	{
		if(curr_view->filename_filter[0] == '\0')
			status_bar_message("Filter is empty");
		else
			status_bar_message(curr_view->filename_filter);
		return 1;
	}
	if(cmd_info->argc == 0)
	{
		if(cmd_info->emark)
		{
			curr_view->invert = !curr_view->invert;
			load_dir_list(curr_view, 1);
			move_to_list_pos(curr_view, 0);
		}
		else
		{
			set_view_filter(curr_view, "", !cmd_info->emark);
		}
	}
	else
	{
		return set_view_filter(curr_view, cmd_info->argv[0], !cmd_info->emark) != 0;
	}
	return 0;
}

static int
find_cmd(const struct cmd_info *cmd_info)
{
	static char *last_args;
	static int last_dir;

	if(cmd_info->argc > 0)
	{
		free(last_args);
		if(cmd_info->argc == 1)
			last_dir = 0;
		else if(is_dir(cmd_info->argv[0]))
			last_dir = 1;
		else
			last_dir = 0;
		last_args = strdup(cmd_info->args);
	}
	else if(last_args == NULL)
	{
		status_bar_error("Nothing to repeat");
		return 1;
	}

	return show_find_menu(curr_view, last_dir, last_args) != 0;
}

static int
grep_cmd(const struct cmd_info *cmd_info)
{
	static char *last_args;
	static int last_invert;
	int inv;

	if(cmd_info->argc > 0)
	{
		free(last_args);
		last_args = strdup(cmd_info->args);
		last_invert = cmd_info->emark;
	}
	else if(last_args == NULL)
	{
		status_bar_error("Nothing to repeat");
		return 1;
	}

	inv = last_invert;
	if(cmd_info->argc == 0 && cmd_info->emark)
		inv = !inv;

	return show_grep_menu(curr_view, last_args, inv) != 0;
}

static int
help_cmd(const struct cmd_info *cmd_info)
{
	char buf[PATH_MAX];
	int bg;

	if(cfg.use_vim_help)
	{
		char *escaped = escape_filename(cmd_info->args, 0);
		if(cmd_info->argc > 0)
			snprintf(buf, sizeof(buf), "%s -c help\\ %s -c only", get_vicmd(&bg),
					escaped);
		else
			snprintf(buf, sizeof(buf), "%s -c 'help vifm.txt' -c only",
					get_vicmd(&bg));
		free(escaped);
	}
	else
	{
		snprintf(buf, sizeof(buf), "%s/vifm-help.txt", cfg.config_dir);
		if(access(buf, F_OK) != 0)
		{
			(void)show_error_msgf("No help file", "Can't find \"%s\" file", buf);
			return 0;
		}

		snprintf(buf, sizeof(buf), "%s %s/vifm-help.txt", get_vicmd(&bg),
				cfg.config_dir);
	}

	if(bg)
		start_background_job(buf);
	else
		shellout(buf, -1);
	return 0;
}

static int
highlight_cmd(const struct cmd_info *cmd_info)
{
	int i;
	int pos;

	if(cmd_info->argc == 0)
	{
		char buf[256*(MAXNUM_COLOR - 2)] = "";
		for(i = 0; i < MAXNUM_COLOR - 2; i++)
		{
			strcat(buf, get_group_str(i, col_schemes[cfg.color_scheme_cur].color[i]));
			if(i < MAXNUM_COLOR - 2 - 1)
				strcat(buf, "\n");
		}
		status_bar_message(buf);
		return 1;
	}

	if(!is_in_string_array_case(HI_GROUPS, MAXNUM_COLOR - 2, cmd_info->argv[0]))
	{
		status_bar_errorf("Highlight group not found: %s", cmd_info->argv[0]);
		return 1;
	}

	pos = string_array_pos_case(HI_GROUPS, MAXNUM_COLOR - 2, cmd_info->argv[0]);
	if(cmd_info->argc == 1)
	{
		status_bar_message(get_group_str(pos,
				col_schemes[cfg.color_scheme_cur].color[pos]));
		return 1;
	}

	for(i = 1; i < cmd_info->argc; i++)
	{
		char *equal;
		char arg_name[16];
		if((equal = strchr(cmd_info->argv[i], '=')) == NULL)
		{
			status_bar_errorf("Missing equal sign in \"%s\"", cmd_info->argv[i]);
			return 1;
		}
		else if(equal[1] == '\0')
		{
			status_bar_errorf("Missing argument: %s", cmd_info->argv[i]);
			return 1;
		}
		snprintf(arg_name, MIN(sizeof(arg_name), equal - cmd_info->argv[i] + 1),
				"%s", cmd_info->argv[i]);
		if(strcmp(arg_name, "ctermbg") == 0)
		{
			int col;
			if((col = get_color(equal + 1)) < -1)
			{
				status_bar_errorf("Color name or number not recognized: %s", equal + 1);
				return 1;
			}
			col_schemes[cfg.color_scheme_cur].color[pos].bg = col;
		}
		else if(strcmp(arg_name, "ctermfg") == 0)
		{
			int col;
			if((col = get_color(equal + 1)) < -1)
			{
				status_bar_errorf("Color name or number not recognized: %s", equal + 1);
				return 1;
			}
			col_schemes[cfg.color_scheme_cur].color[pos].fg = col;
		}
		else if(strcmp(arg_name, "cterm") == 0)
		{
			int attrs;
			if((attrs = get_attrs(equal + 1)) == -1)
			{
				status_bar_errorf("Illegal argument: %s", equal + 1);
				return 1;
			}
			col_schemes[cfg.color_scheme_cur].color[pos].attr = attrs;
		}
		else
		{
			status_bar_errorf("Illegal argument: %s", cmd_info->argv[i]);
			return 1;
		}
	}
	if(curr_stats.vifm_started >= 2)
	{
		init_pair(cfg.color_scheme + pos,
				col_schemes[cfg.color_scheme_cur].color[pos].fg,
				col_schemes[cfg.color_scheme_cur].color[pos].bg);
		redraw_window();
	}
	return 0;
}

static const char *
get_group_str(int group, Col_attr col)
{
	static char buf[256];

	char fg_buf[16], bg_buf[16];

	if(col.fg == -1)
		strcpy(fg_buf, "none");
	else if(col.fg < ARRAY_LEN(COLOR_NAMES))
		strcpy(fg_buf, COLOR_NAMES[col.fg]);
	else
		snprintf(fg_buf, sizeof(fg_buf), "%d", col.fg);

	if(col.bg == -1)
		strcpy(bg_buf, "none");
	else if(col.bg < ARRAY_LEN(COLOR_NAMES))
		strcpy(bg_buf, COLOR_NAMES[col.bg]);
	else
		snprintf(bg_buf, sizeof(bg_buf), "%d", col.bg);

	snprintf(buf, sizeof(buf), "%-10s ctermfg=%-7s ctermbg=%-7s cterm=%s",
			HI_GROUPS[group], fg_buf, bg_buf, attrs_to_str(col.attr));
	return buf;
}

static int
get_color(const char *text)
{
	int col_pos = string_array_pos_case(COLOR_NAMES, ARRAY_LEN(COLOR_NAMES),
			text);
	int col_num = isdigit(*text) ? atoi(text) : -1;
	if(strcmp(text, "-1") == 0 || strcasecmp(text, "default") == 0 ||
			strcasecmp(text, "none") == 0)
		return -1;
	if(col_pos < 0 && (col_num < 0 ||
			(curr_stats.vifm_started >= 2 && col_num > COLORS)))
		return -2;
	if(col_pos > 0)
		col_pos = COLOR_VALS[col_pos];
	return MAX(col_pos, col_num);
}

static int
get_attrs(const char *text)
{
	int result = 0;
	while(*text != '\0')
	{
		const char *p;
		char buf[64];

		if((p = strchr(text, ',')) == 0)
			p = text + strlen(text);

		snprintf(buf, p - text + 1, "%s", text);
		if(strcasecmp(buf, "bold") == 0)
			result |= A_BOLD;
		else if(strcasecmp(buf, "underline") == 0)
			result |= A_UNDERLINE;
		else if(strcasecmp(buf, "reverse") == 0)
			result |= A_REVERSE;
		else if(strcasecmp(buf, "inverse") == 0)
			result |= A_REVERSE;
		else if(strcasecmp(buf, "standout") == 0)
			result |= A_STANDOUT;
		else if(strcasecmp(buf, "none") == 0)
			result = 0;
		else
			return -1;

		text = (*p == '\0') ? p : p + 1;
	}
	return result;
}

static int
history_cmd(const struct cmd_info *cmd_info)
{
	const char *type;
	size_t len;

	if(cmd_info->argc == 0)
		return show_history_menu(curr_view) != 0;

	type = cmd_info->argv[0];
	len = strlen(type);
	if(strcmp(type, ":") == 0 || strncmp("cmd", type, len) == 0)
		return show_cmdhistory_menu(curr_view) != 0;
	else if(strcmp(type, "@") == 0 || strncmp("input", type, len) == 0)
		return show_prompthistory_menu(curr_view) != 0;
	else if(strcmp(type, "/") == 0 || strncmp("search", type, len) == 0 ||
			strncmp("fsearch", type, len) == 0)
		return show_fsearchhistory_menu(curr_view) != 0;
	else if(strcmp(type, "?") == 0 || strncmp("bsearch", type, len) == 0)
		return show_bsearchhistory_menu(curr_view) != 0;
	else if(strcmp(type, ".") == 0 || strncmp("dir", type, len) == 0)
		return show_history_menu(curr_view) != 0;
	else
		return CMDS_ERR_TRAILING_CHARS;
}

static int
invert_cmd(const struct cmd_info *cmd_info)
{
	if(cmd_info->qmark)
	{
		if(curr_view->invert)
			status_bar_message("Filter is not inverted");
		else
			status_bar_message("Filter is inverted");
		return 1;
	}

	curr_view->invert = !curr_view->invert;
	load_dir_list(curr_view, 1);
	move_to_list_pos(curr_view, 0);
	return 0;
}

static int
jobs_cmd(const struct cmd_info *cmd_info)
{
	return show_jobs_menu(curr_view) != 0;
}

static int
locate_cmd(const struct cmd_info *cmd_info)
{
	static char *last_args;
	if(cmd_info->argc > 0)
	{
		free(last_args);
		last_args = strdup(cmd_info->args);
	}
	else if(last_args == NULL)
	{
		status_bar_error("Nothing to repeat");
		return 1;
	}
	return show_locate_menu(curr_view, last_args) != 0;
}

static int
ls_cmd(const struct cmd_info *cmd_info)
{
	if(!cfg.use_screen)
	{
		status_bar_message("screen program isn't used");
		return 1;
	}
	my_system("screen -X eval 'windowlist'");
	return 0;
}

static int
map_cmd(const struct cmd_info *cmd_info)
{
	return map_or_remap(cmd_info, 0);
}

static int
mark_cmd(const struct cmd_info *cmd_info)
{
	int result;
	char *tmp;

	if(cmd_info->argv[0][1] != '\0')
		return CMDS_ERR_TRAILING_CHARS;

	if(cmd_info->argc == 1)
	{
		if(cmd_info->end == NOT_DEF)
			return add_bookmark(cmd_info->argv[0][0], curr_view->curr_dir,
					curr_view->dir_entry[curr_view->list_pos].name);
		else
			return add_bookmark(cmd_info->argv[0][0], curr_view->curr_dir,
					curr_view->dir_entry[cmd_info->end].name);
	}

	tmp = expand_tilde(strdup(cmd_info->argv[1]));
	if(tmp[0] != '/')
	{
		free(tmp);
		status_bar_error("Expected full path to the directory");
		return 1;
	}

	if(cmd_info->argc == 2)
	{
		if(cmd_info->end == NOT_DEF || !pane_in_dir(curr_view, tmp))
		{
			if(pane_in_dir(curr_view, tmp))
				result = add_bookmark(cmd_info->argv[0][0], tmp,
						curr_view->dir_entry[curr_view->list_pos].name);
			else
				result = add_bookmark(cmd_info->argv[0][0], tmp, "../");
		}
		else
			result = add_bookmark(cmd_info->argv[0][0], tmp,
					curr_view->dir_entry[cmd_info->end].name);
	}
	else
	{
		result = add_bookmark(cmd_info->argv[0][0], tmp, cmd_info->argv[2]);
	}
	free(tmp);
	return result;
}

static int
marks_cmd(const struct cmd_info *cmd_info)
{
	char buf[256];
	int i, j;

	if(cmd_info->argc == 0)
		return show_bookmarks_menu(curr_view, valid_bookmarks) != 0;

	j = 0;
	buf[0] = '\0';
	for(i = 0; i < cmd_info->argc; i++)
	{
		const char *p = cmd_info->argv[i];
		while(*p != '\0')
		{
			if(strchr(buf, *p) == NULL)
			{
				buf[j++] = *p;
				buf[j] = '\0';
			}
			p++;
		}
	}
	return show_bookmarks_menu(curr_view, buf);
}

static int
mkdir_cmd(const struct cmd_info *cmd_info)
{
	make_dirs(curr_view, cmd_info->argv, cmd_info->argc, cmd_info->emark);
	return 0;
}

static int
move_cmd(const struct cmd_info *cmd_info)
{
	if(cmd_info->qmark)
	{
		if(cmd_info->argc > 0)
		{
			status_bar_error("No arguments are allowed if you use \"!\"");
			return 1;
		}
		return cpmv_files(curr_view, NULL, -1, 1, 0, 0) != 0;
	}
	else
	{
		return cpmv_files(curr_view, cmd_info->argv, cmd_info->argc, 1, 0,
				cmd_info->emark) != 0;
	}
}

static int
nmap_cmd(const struct cmd_info *cmd_info)
{
	return do_map(cmd_info, "Normal", "nmap", NORMAL_MODE, 0) != 0;
}

static int
nnoremap_cmd(const struct cmd_info *cmd_info)
{
	return do_map(cmd_info, "Normal", "nmap", NORMAL_MODE, 1) != 0;
}

static int
nohlsearch_cmd(const struct cmd_info *cmd_info)
{
	if(curr_view->selected_files == 0)
		return 0;

	clean_selected_files(curr_view);
	draw_dir_list(curr_view, curr_view->top_line);
	move_to_list_pos(curr_view, curr_view->list_pos);
	return 0;
}

static int
noremap_cmd(const struct cmd_info *cmd_info)
{
	return map_or_remap(cmd_info, 1);
}

static int
map_or_remap(const struct cmd_info *cmd_info, int no_remap)
{
	int result;
	if(cmd_info->emark)
	{
		result = do_map(cmd_info, "", "map", CMDLINE_MODE, no_remap);
	}
	else
	{
		result = do_map(cmd_info, "", "map", NORMAL_MODE, no_remap);
		if(result == 0)
			result = do_map(cmd_info, "", "map", VISUAL_MODE, no_remap);
	}
	return result != 0;
}

static int
nunmap_cmd(const struct cmd_info *cmd_info)
{
	return do_unmap(cmd_info->argv[0], NORMAL_MODE);
}

static int
only_cmd(const struct cmd_info *cmd_info)
{
	curr_stats.number_of_windows = 1;
	redraw_window();
	return 0;
}

static int
popd_cmd(const struct cmd_info *cmd_info)
{
	if(popd() != 0)
	{
		status_bar_message("Directory stack empty");
		return 1;
	}
	return 0;
}

static int
pushd_cmd(const struct cmd_info *cmd_info)
{
	if(cmd_info->argc == 0)
	{
		if(swap_dirs() != 0)
		{
			status_bar_error("No other directories");
			return 1;
		}
		return 0;
	}
	if(pushd() != 0)
	{
		(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
		return 0;
	}
	cd_cmd(cmd_info);
	return 0;
}

static int
pwd_cmd(const struct cmd_info *cmd_info)
{
	status_bar_message(curr_view->curr_dir);
	return 1;
}

static int
registers_cmd(const struct cmd_info *cmd_info)
{
	char buf[256];
	int i, j;

	if(cmd_info->argc == 0)
		return show_register_menu(curr_view, valid_registers) != 0;

	j = 0;
	buf[0] = '\0';
	for(i = 0; i < cmd_info->argc; i++)
	{
		const char *p = cmd_info->argv[i];
		while(*p != '\0')
		{
			if(strchr(buf, *p) == NULL)
			{
				buf[j++] = *p;
				buf[j] = '\0';
			}
			p++;
		}
	}
	return show_register_menu(curr_view, buf) != 0;
}

static int
rename_cmd(const struct cmd_info *cmd_info)
{
	if(cmd_info->argc == 0)
		return rename_files(curr_view, NULL, 0) != 0;
	else
		return rename_files(curr_view, cmd_info->argv, cmd_info->argc) != 0;
}

static int
restart_cmd(const struct cmd_info *cmd_info)
{
	const char *p;

	/* all user mappings in all modes */
	clear_user_keys();

	/* user defined commands */
	execute_cmd("comclear");

	/* options */
	reset_options_to_default();

	/* file types */
	reset_filetypes();
	reset_xfiletypes();

	/* file viewers */
	reset_fileviewers();

	/* ga command results */
	tree_free(curr_stats.dirsize_cache);
	curr_stats.dirsize_cache = tree_create();

	/* undo list */
	reset_undo_list();

	/* directory history */
	lwin.history_num = 0;
	lwin.history_pos = 0;
	rwin.history_num = 0;
	rwin.history_pos = 0;

	/* command line history */
	cfg.cmd_history_num = 0;

	/* search history */
	cfg.search_history_num = -1;

	/* directory stack */
	clean_stack();

	/* registers */
	p = valid_registers;
	while(*p != '\0')
		clear_register(*p++);

	/* color schemes */
	cfg.color_scheme = 1;
	cfg.color_scheme_cur = 0;
	cfg.color_scheme_num = 0;

	/* bookmarks */
	p = valid_bookmarks;
	while(*p != '\0')
	{
		int index = mark2index(*p++);
		if(is_bookmark(index))
			remove_bookmark(index);
	}

	load_default_configuration();
	read_info_file(1);
	save_view_history(&lwin, NULL, NULL, -1);
	save_view_history(&rwin, NULL, NULL, -1);
	read_color_schemes();
	check_color_schemes();
	load_color_schemes();
	exec_config();
	return 0;
}

static int
restore_cmd(const struct cmd_info *cmd_info)
{
	int i;
	int m = 0;
	int n = curr_view->selected_files;

	i = curr_view->list_pos;
	while(i < curr_view->list_rows - 1 && curr_view->dir_entry[i].selected)
		i++;
	curr_view->list_pos = i;

	cmd_group_begin("restore: ");
	cmd_group_end();
	for(i = 0; i < curr_view->list_rows; i++)
	{
		if(!curr_view->dir_entry[i].selected)
			continue;
		if(restore_from_trash(curr_view->dir_entry[i].name) == 0)
			m++;
	}

	load_saving_pos(curr_view, 1);
	status_bar_messagef("Restored %d of %d", m, n);
	return 1;
}

static int
rlink_cmd(const struct cmd_info *cmd_info)
{
	if(cmd_info->qmark)
	{
		if(cmd_info->argc > 0)
		{
			status_bar_error("No arguments are allowed if you use \"!\"");
			return 1;
		}
		return cpmv_files(curr_view, NULL, -1, 0, 2, 0) != 0;
	}
	else
	{
		return cpmv_files(curr_view, cmd_info->argv, cmd_info->argc, 0, 2,
				cmd_info->emark) != 0;
	}
}

static int
screen_cmd(const struct cmd_info *cmd_info)
{
	if(cmd_info->qmark)
	{
		if(cfg.use_screen)
			status_bar_message("Screen support is enabled");
		else
			status_bar_message("Screen support is disabled");
		return 1;
	}
	cfg.use_screen = !cfg.use_screen;
	return 0;
}

static int
set_cmd(const struct cmd_info *cmd_info)
{
	return process_set_args(cmd_info->args) != 0;
}

static int
shell_cmd(const struct cmd_info *cmd_info)
{
	shellout(getenv("SHELL"), 0);
	return 0;
}

static int
sort_cmd(const struct cmd_info *cmd_info)
{
	enter_sort_mode(curr_view);
	return 0;
}

static int
split_cmd(const struct cmd_info *cmd_info)
{
	if(cmd_info->argc == 1)
		cd(other_view, cmd_info->argv[0]);
	comm_split();
	return 0;
}

static int
substitute_cmd(const struct cmd_info *cmd_info)
{
	static char *last_pattern;
	int ic = 0;
	int glob = cfg.gdefault;

	if(cmd_info->argc == 3)
	{
		int i;
		for(i = 0; cmd_info->argv[2][i] != '\0'; i++)
		{
			if(cmd_info->argv[2][i] == 'i')
				ic = 1;
			else if(cmd_info->argv[2][i] == 'I')
				ic = -1;
			else if(cmd_info->argv[2][i] == 'g')
				glob = !glob;
			else
				return CMDS_ERR_TRAILING_CHARS;
		}
	}

	if(cmd_info->argv[0][0] != '\0')
	{
		free(last_pattern);
		last_pattern = strdup(cmd_info->argv[0]);
	}
	else if(last_pattern == NULL)
	{
		status_bar_error("No previous pattern");
		return 1;
	}

	return substitute_in_names(curr_view, last_pattern, cmd_info->argv[1], ic,
			glob) != 0;
}

static int
sync_cmd(const struct cmd_info *cmd_info)
{
	if(change_directory(other_view, curr_view->curr_dir) >= 0)
	{
		load_dir_list(other_view, 0);
		wrefresh(other_view->win);
	}
	return 0;
}

static int
touch_cmd(const struct cmd_info *cmd_info)
{
	return make_files(curr_view, cmd_info->argv, cmd_info->argc) != 0;
}

static int
tr_cmd(const struct cmd_info *cmd_info)
{
	char buf[strlen(cmd_info->argv[0]) + 1];
	size_t pl, sl;

	if(cmd_info->argv[0][0] == '\0' || cmd_info->argv[1][0] == '\0')
	{
		status_bar_error("Empty argument");
		return 1;
	}

	pl = strlen(cmd_info->argv[0]);
	sl = strlen(cmd_info->argv[1]);
	strcpy(buf, cmd_info->argv[1]);
	if(pl < sl)
	{
		status_bar_error("Second argument cannot be longer");
		return 1;
	}
	else if(pl > sl)
	{
		while(sl < pl)
		{
			buf[sl] = buf[sl - 1];
			sl++;
		}
	}
	return tr_in_names(curr_view, cmd_info->argv[0], buf) != 0;
}

static int
undolist_cmd(const struct cmd_info *cmd_info)
{
	return show_undolist_menu(curr_view, cmd_info->emark) != 0;
}

static int
unmap_cmd(const struct cmd_info *cmd_info)
{
	int result;
	wchar_t *subst;

	subst = substitute_specs(cmd_info->argv[0]);
	if(cmd_info->emark)
	{
		result = remove_user_keys(subst, CMDLINE_MODE) != 0;
	}
	else if(!has_user_keys(subst, NORMAL_MODE))
	{
		result = -1;
	}
	else if(!has_user_keys(subst, VISUAL_MODE))
	{
		result = -2;
	}
	else
	{
		result = remove_user_keys(subst, NORMAL_MODE) != 0;
		result += remove_user_keys(subst, VISUAL_MODE) != 0;
	}
	free(subst);

	if(result == -1)
		status_bar_error("No such mapping in normal mode");
	else if(result == -2)
		status_bar_error("No such mapping in visual mode");
	else
		status_bar_error("Error");
	return result != 0;
}

static int
view_cmd(const struct cmd_info *cmd_info)
{
	if(curr_stats.number_of_windows == 1)
	{
		status_bar_error("Cannot view files in one window mode");
		return 1;
	}
	if(curr_stats.view)
	{
		curr_stats.view = 0;

		if(change_directory(other_view, other_view->curr_dir) >= 0)
			load_dir_list(other_view, 1);
		wrefresh(other_view->win);
	}
	else
	{
		curr_stats.view = 1;
		quick_view_file(curr_view);
	}
	return 0;
}

static int
vifm_cmd(const struct cmd_info *cmd_info)
{
	return show_vifm_menu(curr_view) != 0;
}

static int
vmap_cmd(const struct cmd_info *cmd_info)
{
	return do_map(cmd_info, "Visual", "vmap", VISUAL_MODE, 0) != 0;
}

static int
vnoremap_cmd(const struct cmd_info *cmd_info)
{
	return do_map(cmd_info, "Visual", "vmap", VISUAL_MODE, 1) != 0;
}

static int
do_map(const struct cmd_info *cmd_info, const char *map_type,
		const char *map_cmd, int mode, int no_remap)
{
	wchar_t *keys, *mapping;
	char *raw_rhs, *rhs;
	char t;
	int result;

	if(cmd_info->argc == 1)
	{
		status_bar_errorf("Command Error",
				"The :%s command requires two arguments - :%s lhs rhs", map_cmd,
				map_cmd);
		return -1;
	}

	if(cmd_info->argc == 0)
	{
		show_map_menu(curr_view, map_type, list_cmds(mode));
		return 0;
	}

	raw_rhs = (char *)skip_word(cmd_info->args);
	t = *raw_rhs;
	*raw_rhs = '\0';

	rhs = (char*)skip_spaces(raw_rhs + 1);
	keys = substitute_specs(cmd_info->args);
	mapping = substitute_specs(rhs);
	result = add_user_keys(keys, mapping, mode, no_remap);
	free(mapping);
	free(keys);

	*raw_rhs = t;

	if(result == -1)
		(void)show_error_msg("Mapping Error", "Unable to allocate enough memory");

	return 0;
}

#ifdef _WIN32
static int
volumes_cmd(const struct cmd_info *cmd_info)
{
	show_volumes_menu(curr_view);
	return 0;
}
#endif

static int
vunmap_cmd(const struct cmd_info *cmd_info)
{
	return do_unmap(cmd_info->argv[0], VISUAL_MODE);
}

static int
do_unmap(const char *keys, int mode)
{
	int result;
	wchar_t *subst;

	subst = substitute_specs(keys);
	result = remove_user_keys(subst, mode);
	free(subst);

	if(result != 0)
	{
		status_bar_error("No such mapping");
		return 1;
	}
	return 0;
}

static int
write_cmd(const struct cmd_info *cmd_info)
{
	write_info_file();
	return 0;
}

static int
quit_cmd(const struct cmd_info *cmd_info)
{
	comm_quit(!cmd_info->emark);
	return 0;
}

static int
wq_cmd(const struct cmd_info *cmd_info)
{
	comm_quit(1);
	return 0;
}

static int
yank_cmd(const struct cmd_info *cmd_info)
{
	int reg;
	int result;

	result = get_reg_and_count(cmd_info, &reg);
	if(result == 0)
		result = yank_files(curr_view, DEFAULT_REG_NAME, 0, NULL) != 0;
	return result;
}

/* Returns zero on success */
static int
get_reg_and_count(const struct cmd_info *cmd_info, int *reg)
{
	if(cmd_info->argc == 2)
	{
		int count;

		if(cmd_info->argv[0][1] != '\0')
			return CMDS_ERR_TRAILING_CHARS;
		if(find_register(cmd_info->argv[0][0]) == NULL)
			return CMDS_ERR_TRAILING_CHARS;
		*reg = cmd_info->argv[0][0];

		if(!isdigit(cmd_info->argv[1][0]))
			return CMDS_ERR_TRAILING_CHARS;

		count = atoi(cmd_info->argv[1]);
		if(count == 0)
			return CMDS_ERR_ZERO_COUNT;
		select_count(cmd_info, count);
	}
	else if(cmd_info->argc == 1)
	{
		if(isdigit(cmd_info->argv[0][0]))
		{
			int count = atoi(cmd_info->argv[0]);
			if(count == 0)
				return CMDS_ERR_ZERO_COUNT;
			select_count(cmd_info, count);
		}
		else
		{
			if(cmd_info->argv[0][1] != '\0')
				return CMDS_ERR_TRAILING_CHARS;
			if(find_register(cmd_info->argv[0][0]) == NULL)
				return CMDS_ERR_TRAILING_CHARS;
			*reg = cmd_info->argv[0][0];
		}
	}
	return 0;
}

static int
usercmd_cmd(const struct cmd_info* cmd_info)
{
	char *expanded_com = NULL;
	int use_menu = 0;
	int split = 0;
	size_t len;
	int external = 1;

	if(strchr(cmd_info->cmd, '%') != NULL)
		expanded_com = expand_macros(curr_view, cmd_info->cmd, cmd_info->args,
				&use_menu, &split);
	else
		expanded_com = strdup(cmd_info->cmd);

	len = strlen(expanded_com);
	while(len > 1 && isspace(expanded_com[len - 1]))
		expanded_com[--len] = '\0';

	clean_selected_files(curr_view);

	if(use_menu)
	{
		show_user_menu(curr_view, expanded_com, use_menu == 2);
	}
	else if(split)
	{
		if(!cfg.use_screen)
		{
			free(expanded_com);
			status_bar_message("The screen program support isn't enabled");
			return 1;
		}

		split_screen(curr_view, expanded_com);
	}
	else if(strncmp(expanded_com, "filter ", 7) == 0)
	{
		curr_view->invert = 1;
		curr_view->filename_filter = (char *)realloc(curr_view->filename_filter,
				strlen(strchr(expanded_com, ' ')) + 1);
		snprintf(curr_view->filename_filter, strlen(strchr(expanded_com, ' ')) + 1,
				"%s", strchr(expanded_com, ' ') + 1);

		load_saving_pos(curr_view, 1);
		external = 0;
	}
	else if(!strncmp(expanded_com, "!", 1))
	{
		char buf[strlen(expanded_com) + 1];
		char *tmp = strcpy(buf, expanded_com);
		int pause = 0;
		tmp++;
		if(*tmp == '!')
		{
			pause = 1;
			tmp++;
		}
		while(isspace(*tmp))
			tmp++;

		if(strlen(tmp) > 0 && cmd_info->bg)
			start_background_job(tmp);
		else if(strlen(tmp) > 0)
			shellout(tmp, pause ? 1 : -1);
	}
	else if(expanded_com[0] == '/')
	{
		exec_command(expanded_com + 1, curr_view, GET_FSEARCH_PATTERN);
		external = 0;
		need_clean_selection = 0;
	}
	else if(cmd_info->bg)
	{
		char buf[strlen(expanded_com) + 1];
		strcpy(buf, expanded_com);
		start_background_job(buf);
	}
	else
	{
		shellout(expanded_com, -1);
	}

	if(external)
	{
		cmd_group_continue();
		add_operation(OP_USR, strdup(expanded_com), NULL, "", "");
		cmd_group_end();
	}

	free(expanded_com);

	return 0;
}

/* vim: set cinoptions+=t0 : */
/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
