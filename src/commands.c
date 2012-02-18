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

#ifdef _WIN32
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <lm.h>
#endif

#include <regex.h>

#include <curses.h>

#include <sys/types.h> /* passwd */
#ifndef _WIN32
#include <grp.h>
#include <pwd.h>
#include <sys/wait.h>
#endif
#include <dirent.h> /* DIR */

#include <assert.h>
#include <ctype.h> /* isspace() */
#include <errno.h> /* errno */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h> /*  system() */
#include <string.h> /* strncmp() */
#include <time.h>

#include "attr_dialog.h"
#include "background.h"
#include "bookmarks.h"
#include "bracket_notation.h"
#include "change_dialog.h"
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
#include "normal.h"
#include "opt_handlers.h"
#include "options.h"
#include "registers.h"
#include "signals.h"
#include "sort.h"
#include "sort_dialog.h"
#include "status.h"
#include "string_array.h"
#include "tags.h"
#include "trash.h"
#include "ui.h"
#include "undo.h"
#include "utils.h"
#include "variables.h"
#include "view.h"
#include "visual.h"

#include "commands.h"

#ifndef _WIN32
#define PAUSE_CMD "vifm-pause"
#define PAUSE_STR "; "PAUSE_CMD
#else
#define PAUSE_CMD "vifm-pause"
#define PAUSE_STR " && pause || pause"
#endif

enum
{
	/* commands without completion */
	COM_FILTER = -100,
	COM_SUBSTITUTE,
	COM_TR,

	/* commands with completion */
	COM_CD,
	COM_CHOWN,
	COM_COLORSCHEME,
	COM_EDIT,
	COM_EXECUTE,
	COM_FILE,
	COM_FIND,
	COM_GOTO = 0,
	COM_GREP,
	COM_HELP,
	COM_HIGHLIGHT,
	COM_HISTORY,
	COM_PUSHD,
	COM_SET,
	COM_SOURCE,
	COM_SYNC,
	COM_LET,
	COM_UNLET,
	COM_WINDO,
	COM_WINRUN,
};

static int complete_args(int id, const char *args, int argc, char **argv,
		int arg_pos);
static int swap_range(void);
static int resolve_mark(char mark);
static char * cmds_expand_macros(const char *str, int *use_menu, int *split);
static char * cmds_expand_envvars(const char *str);
static void post(int id);
#ifndef TEST
static
#endif
void select_range(int id, const cmd_info_t *cmd_info);
static int skip_at_beginning(int id, const char *args);
static void exec_completion(const char *str);
static void complete_help(const char *str);
static void complete_history(const char *str);
static int complete_chown(const char *str);
static void complete_filetype(const char *str);
static void complete_highlight_groups(const char *str);
static int complete_highlight_arg(const char *str);
static void complete_winrun(const char *str);
static void complete_envvar(const char *str);
static int is_entry_dir(const struct dirent *d);
static int is_entry_exec(const struct dirent *d);
static void split_path(void);
static int apply_p_mod(const char *path, const char *parent, char *buf,
		size_t buf_len);
static int apply_tilde_mod(const char *path, char *buf, size_t buf_len);
static int apply_dot_mod(const char *path, char *buf, size_t buf_len);
static int apply_h_mod(const char *path, char *buf, size_t buf_len);
#ifdef _WIN32
static int apply_u_mod(const char *path, char *buf, size_t buf_len);
#endif
static int apply_t_mod(const char *path, char *buf, size_t buf_len);
static int apply_r_mod(const char *path, char *buf, size_t buf_len);
static int apply_e_mod(const char *path, char *buf, size_t buf_len);;;
static int apply_s_gs_mod(const char *path, const char *mod,
		char *buf, size_t buf_len);
static const char * apply_mods(const char *path, const char *parent,
		const char *mod);
static const char * apply_mod(const char *path, const char *parent,
		const char *mod, int *mod_len);
static wchar_t * substitute_specs(const char *cmd);
static void print_func(int error, const char *msg, const char *description);

static int goto_cmd(const cmd_info_t *cmd_info);
static int emark_cmd(const cmd_info_t *cmd_info);
static int alink_cmd(const cmd_info_t *cmd_info);
static int apropos_cmd(const cmd_info_t *cmd_info);
static int cd_cmd(const cmd_info_t *cmd_info);
static int change_cmd(const cmd_info_t *cmd_info);
static int chmod_cmd(const cmd_info_t *cmd_info);
#ifndef _WIN32
static int chown_cmd(const cmd_info_t *cmd_info);
#endif
static int clone_cmd(const cmd_info_t *cmd_info);
static int cmap_cmd(const cmd_info_t *cmd_info);
static int cnoremap_cmd(const cmd_info_t *cmd_info);
static int copy_cmd(const cmd_info_t *cmd_info);
static int colorscheme_cmd(const cmd_info_t *cmd_info);
static int command_cmd(const cmd_info_t *cmd_info);
static int cunmap_cmd(const cmd_info_t *cmd_info);
static int delete_cmd(const cmd_info_t *cmd_info);
static int delmarks_cmd(const cmd_info_t *cmd_info);
static int dirs_cmd(const cmd_info_t *cmd_info);
static int edit_cmd(const cmd_info_t *cmd_info);
static int empty_cmd(const cmd_info_t *cmd_info);
static int exe_cmd(const cmd_info_t *cmd_info);
static int file_cmd(const cmd_info_t *cmd_info);
static int filetype_cmd(const cmd_info_t *cmd_info);
static int filextype_cmd(const cmd_info_t *cmd_info);
static int fileviewer_cmd(const cmd_info_t *cmd_info);
static int filter_cmd(const cmd_info_t *cmd_info);
static int find_cmd(const cmd_info_t *cmd_info);
static int grep_cmd(const cmd_info_t *cmd_info);
static int help_cmd(const cmd_info_t *cmd_info);
static int highlight_cmd(const cmd_info_t *cmd_info);
static const char *get_group_str(int group, col_attr_t col);
static int get_color(const char *text);
static int get_attrs(const char *text);
static int history_cmd(const cmd_info_t *cmd_info);
static int invert_cmd(const cmd_info_t *cmd_info);
static int jobs_cmd(const cmd_info_t *cmd_info);
static int let_cmd(const cmd_info_t *cmd_info);
static int locate_cmd(const cmd_info_t *cmd_info);
static int ls_cmd(const cmd_info_t *cmd_info);
static int map_cmd(const cmd_info_t *cmd_info);
static int mark_cmd(const cmd_info_t *cmd_info);
static int marks_cmd(const cmd_info_t *cmd_info);
static int messages_cmd(const cmd_info_t *cmd_info);
static int mkdir_cmd(const cmd_info_t *cmd_info);
static int move_cmd(const cmd_info_t *cmd_info);
static int nmap_cmd(const cmd_info_t *cmd_info);
static int nnoremap_cmd(const cmd_info_t *cmd_info);
static int nohlsearch_cmd(const cmd_info_t *cmd_info);
static int noremap_cmd(const cmd_info_t *cmd_info);
static int map_or_remap(const cmd_info_t *cmd_info, int no_remap);
static int nunmap_cmd(const cmd_info_t *cmd_info);
static int only_cmd(const cmd_info_t *cmd_info);
static int popd_cmd(const cmd_info_t *cmd_info);
static int pushd_cmd(const cmd_info_t *cmd_info);
static int pwd_cmd(const cmd_info_t *cmd_info);
static int registers_cmd(const cmd_info_t *cmd_info);
static int rename_cmd(const cmd_info_t *cmd_info);
static int restart_cmd(const cmd_info_t *cmd_info);
static int restore_cmd(const cmd_info_t *cmd_info);
static int rlink_cmd(const cmd_info_t *cmd_info);
static int screen_cmd(const cmd_info_t *cmd_info);
static int set_cmd(const cmd_info_t *cmd_info);
static int shell_cmd(const cmd_info_t *cmd_info);
static int sort_cmd(const cmd_info_t *cmd_info);
static int source_cmd(const cmd_info_t *cmd_info);
static int split_cmd(const cmd_info_t *cmd_info);
static int substitute_cmd(const cmd_info_t *cmd_info);
static int sync_cmd(const cmd_info_t *cmd_info);
static int touch_cmd(const cmd_info_t *cmd_info);
static int tr_cmd(const cmd_info_t *cmd_info);
static int undolist_cmd(const cmd_info_t *cmd_info);
static int unmap_cmd(const cmd_info_t *cmd_info);
static int unlet_cmd(const cmd_info_t *cmd_info);
static int view_cmd(const cmd_info_t *cmd_info);
static int vifm_cmd(const cmd_info_t *cmd_info);
static int vmap_cmd(const cmd_info_t *cmd_info);
static int vnoremap_cmd(const cmd_info_t *cmd_info);
#ifdef _WIN32
static int volumes_cmd(const cmd_info_t *cmd_info);
#endif
static int vsplit_cmd(const cmd_info_t *cmd_info);
static int do_split(const cmd_info_t *cmd_info, int vertical);
static int do_map(const cmd_info_t *cmd_info, const char *map_type,
		const char *map_cmd, int mode, int no_remap);
static int vunmap_cmd(const cmd_info_t *cmd_info);
static int do_unmap(const char *keys, int mode);
static int windo_cmd(const cmd_info_t *cmd_info);
static int winrun_cmd(const cmd_info_t *cmd_info);
static int winrun(FileView *view, char *cmd);
static int write_cmd(const cmd_info_t *cmd_info);
static int quit_cmd(const cmd_info_t *cmd_info);
static int wq_cmd(const cmd_info_t *cmd_info);
static int yank_cmd(const cmd_info_t *cmd_info);
static int get_reg_and_count(const cmd_info_t *cmd_info, int *reg);
static int usercmd_cmd(const cmd_info_t* cmd_info);

static const cmd_add_t commands[] = {
	{ .name = "",                 .abbr = NULL,    .emark = 0,  .id = COM_GOTO,        .range = 1,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = goto_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "!",                .abbr = NULL,    .emark = 1,  .id = COM_EXECUTE,     .range = 1,    .bg = 1, .quote = 0, .regexp = 0,
		.handler = emark_cmd,       .qmark = 0,      .expand = 1, .cust_sep = 0,         .min_args = 1, .max_args = NOT_DEF, .select = 1, },
	{ .name = "alink",            .abbr = NULL,    .emark = 1,  .id = -1,              .range = 1,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = alink_cmd,       .qmark = 1,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 1, },
	{ .name = "apropos",          .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = apropos_cmd,     .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "cd",               .abbr = NULL,    .emark = 1,  .id = COM_CD,          .range = 0,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = cd_cmd,          .qmark = 0,      .expand = 3, .cust_sep = 0,         .min_args = 0, .max_args = 2,       .select = 0, },
	{ .name = "change",           .abbr = "c",     .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = change_cmd,      .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
#ifndef _WIN32
	{ .name = "chmod",            .abbr = NULL,    .emark = 1,  .id = -1,              .range = 1,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = chmod_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 1, },
	{ .name = "chown",            .abbr = NULL,    .emark = 0,  .id = COM_CHOWN,       .range = 1,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = chown_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 1,       .select = 1, },
#else
	{ .name = "chmod",            .abbr = NULL,    .emark = 1,  .id = -1,              .range = 1,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = chmod_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 1, },
#endif
	{ .name = "clone",            .abbr = NULL,    .emark = 1,  .id = -1,              .range = 1,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = clone_cmd,       .qmark = 1,      .expand = 1, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 1, },
	{ .name = "cmap",             .abbr = "cm",    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = cmap_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "cnoremap",         .abbr = "cno",   .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = cnoremap_cmd,    .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "colorscheme",      .abbr = "colo",  .emark = 0,  .id = COM_COLORSCHEME, .range = 0,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = colorscheme_cmd, .qmark = 1,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 2,       .select = 0, },
  { .name = "command",          .abbr = "com",   .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
    .handler = command_cmd,     .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "copy",             .abbr = "co",    .emark = 1,  .id = -1,              .range = 1,    .bg = 1, .quote = 1, .regexp = 0,
		.handler = copy_cmd,        .qmark = 1,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 1, },
	{ .name = "cunmap",           .abbr = "cu",    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = cunmap_cmd,      .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 1, .max_args = 1,       .select = 0, },
	{ .name = "delete",           .abbr = "d",     .emark = 1,  .id = -1,              .range = 1,    .bg = 1, .quote = 0, .regexp = 0,
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
	{ .name = "execute",          .abbr = "exe",   .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = exe_cmd,         .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "exit",             .abbr = "exi",   .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = quit_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "file",             .abbr = "f",     .emark = 0,  .id = COM_FILE,        .range = 0,    .bg = 1, .quote = 0, .regexp = 0,
		.handler = file_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 1,       .select = 0, },
	{ .name = "filetype",         .abbr = "filet", .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = filetype_cmd,    .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 2, .max_args = NOT_DEF, .select = 0, },
	{ .name = "fileviewer",       .abbr = "filev", .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = fileviewer_cmd,  .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 2, .max_args = NOT_DEF, .select = 0, },
	{ .name = "filextype",        .abbr = "filex", .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = filextype_cmd,   .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 2, .max_args = NOT_DEF, .select = 0, },
	{ .name = "filter",           .abbr = NULL,    .emark = 1,  .id = COM_FILTER,      .range = 0,    .bg = 0, .quote = 1, .regexp = 1,
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
	{ .name = "let",              .abbr = NULL,    .emark = 0,  .id = COM_LET,         .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = let_cmd,         .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 1, .max_args = 3,       .select = 0, },
	{ .name = "locate",           .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = locate_cmd,      .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "ls",               .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = ls_cmd,          .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "map",              .abbr = NULL,    .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = map_cmd,         .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 2, .max_args = NOT_DEF, .select = 0, },
	{ .name = "mark",             .abbr = "ma",    .emark = 0,  .id = -1,              .range = 1,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = mark_cmd,        .qmark = 2,      .expand = 1, .cust_sep = 0,         .min_args = 1, .max_args = 3,       .select = 0, },
	{ .name = "marks",            .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = marks_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "messages",         .abbr = "mes",   .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = messages_cmd,    .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "mkdir",            .abbr = NULL,    .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = mkdir_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 1, .max_args = NOT_DEF, .select = 0, },
	{ .name = "move",             .abbr = "m",     .emark = 1,  .id = -1,              .range = 1,    .bg = 1, .quote = 1, .regexp = 0,
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
		.handler = pushd_cmd,       .qmark = 0,      .expand = 2, .cust_sep = 0,         .min_args = 0, .max_args = 2,       .select = 0, },
	{ .name = "pwd",              .abbr = "pw",    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = pwd_cmd,         .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "quit",             .abbr = "q",     .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = quit_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "registers",        .abbr = "reg",   .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = registers_cmd,   .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "rename",           .abbr = NULL,    .emark = 1,  .id = -1,              .range = 1,    .bg = 0, .quote = 1, .regexp = 0,
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
		.handler = set_cmd,         .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "shell",            .abbr = "sh",    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = shell_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "sort",             .abbr = "sor",   .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = sort_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "source",           .abbr = "so",    .emark = 0,  .id = COM_SOURCE,      .range = 0,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = source_cmd,      .qmark = 0,      .expand = 2, .cust_sep = 0,         .min_args = 1, .max_args = 1,       .select = 0, },
	{ .name = "split",            .abbr = "sp",    .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = split_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 1,       .select = 0, },
	{ .name = "substitute",       .abbr = "s",     .emark = 0,  .id = COM_SUBSTITUTE,  .range = 1,    .bg = 0, .quote = 0, .regexp = 1,
		.handler = substitute_cmd,  .qmark = 0,      .expand = 0, .cust_sep = 1,         .min_args = 0, .max_args = 3,       .select = 1, },
	{ .name = "sync",             .abbr = NULL,    .emark = 0,  .id = COM_SYNC,        .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = sync_cmd,        .qmark = 0,      .expand = 1, .cust_sep = 0,         .min_args = 0, .max_args = 1,       .select = 0, },
	{ .name = "touch",            .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = touch_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 1, .max_args = NOT_DEF, .select = 0, },
	{ .name = "tr",               .abbr = NULL,    .emark = 0,  .id = COM_TR,          .range = 1,    .bg = 0, .quote = 0, .regexp = 1,
		.handler = tr_cmd,          .qmark = 0,      .expand = 0, .cust_sep = 1,         .min_args = 2, .max_args = 2,       .select = 1, },
	{ .name = "undolist",         .abbr = "undol", .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = undolist_cmd,    .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "unmap",            .abbr = "unm",   .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = unmap_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 1, .max_args = 1,       .select = 0, },
	{ .name = "unlet",            .abbr = "unl",   .emark = 1,  .id = COM_UNLET,       .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = unlet_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 1, .max_args = NOT_DEF, .select = 0, },
	{ .name = "version",          .abbr = "ve",    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = vifm_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "view",             .abbr = "vie",   .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
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
	{ .name = "vsplit",           .abbr = "vs",    .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = vsplit_cmd,      .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 1,       .select = 0, },
	{ .name = "vunmap",           .abbr = "vu",    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = vunmap_cmd,      .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 1, .max_args = 1,       .select = 0, },
	{ .name = "windo",            .abbr = NULL,    .emark = 0,  .id = COM_WINDO,       .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = windo_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "winrun",           .abbr = NULL,    .emark = 0,  .id = COM_WINRUN,      .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = winrun_cmd,      .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "write",            .abbr = "w",     .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = write_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "wq",               .abbr = NULL,    .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = wq_cmd,          .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "xit",              .abbr = "x",     .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = quit_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "yank",             .abbr = "y",     .emark = 0,  .id = -1,              .range = 1,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = yank_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 2,       .select = 1, },

	{ .name = "<USERCMD>",        .abbr = NULL,    .emark = 0,  .id = -1,              .range = 1,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = usercmd_cmd,     .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 1, },
};

static cmds_conf_t cmds_conf = {
	.complete_args = complete_args,
	.swap_range = swap_range,
	.resolve_mark = resolve_mark,
	.expand_macros = cmds_expand_macros,
	.expand_envvars = cmds_expand_envvars,
	.post = post,
	.select_range = select_range,
	.skip_at_beginning = skip_at_beginning,
};

static int need_clean_selection;

static char **paths;
static int paths_count;

static char print_buf[320*80];

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
	const char *slash;
	const char *dollar;

	arg = strrchr(args, ' ');
	if(arg == NULL)
		arg = args;
	else
		arg++;

	start = arg;
	dollar = strrchr(arg, '$');
	slash = strrchr(args + arg_pos, '/');

	if(id == COM_COLORSCHEME)
		complete_colorschemes((argc > 0) ? argv[argc - 1] : arg);
	else if(id == COM_SET)
		complete_options(args, &start);
	else if(id == COM_LET)
		complete_variables(args, &start);
	else if(id == COM_UNLET)
		complete_variables(arg, &start);
	else if(id == COM_HELP)
		complete_help(args);
	else if(id == COM_HISTORY)
		complete_history(args);
	else if(id == COM_CHOWN)
		start += complete_chown(args);
	else if(id == COM_FILE)
		complete_filetype(args);
	else if(id == COM_HIGHLIGHT)
	{
		if(argc == 0 || (argc == 1 && !cmd_ends_with_space(args)))
			complete_highlight_groups(args);
		else
			start += complete_highlight_arg(arg);
	}
	else if((id == COM_CD || id == COM_PUSHD || id == COM_EXECUTE ||
			id == COM_SOURCE) && dollar != NULL && dollar > slash)
	{
		start = dollar + 1;
		complete_envvar(start);
	}
	else if(id == COM_WINDO)
		;
	else if(id == COM_WINRUN)
	{
		if(argc == 0)
			complete_winrun(args);
	}
	else
	{
		start = slash;
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
		if(my_chdir(paths[i]) != 0)
			continue;
		filename_completion(str, FNC_EXECONLY);
	}
	add_completion(str);
}

#ifdef _WIN32
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
				strcat(buf, "/");
				if(pathncmp(buf, file, len) == 0)
				{
					char *escaped = escape_filename(buf, 1);
					add_completion(escaped);
					free(escaped);
				}
				p++;
			}
			NetApiBufferFree(buf_ptr);
		}
	}
	while(res == ERROR_MORE_DATA);
}
#endif

#ifdef _WIN32
/* Returns pointer to a statically allocated buffer */
static const char *
escape_for_cd(const char *str)
{
	static char buf[PATH_MAX*2];
	char *p;

	p = buf;
	while(*str != '\0')
	{
		if(strchr("\\ $", *str) != NULL)
			*p++ = '\\';
		else if(*str == '%')
			*p++ = '%';
		*p++ = *str;

		str++;
	}
	*p = '\0';
	return buf;
}
#endif

/*
 * type: FNC_*
 */
void
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
#ifndef _WIN32
	int woe = (type == FNC_ALL_WOE || type == FNC_FILE_WOE);
#endif

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

	temp = cmds_expand_envvars(dirname);
	free(dirname);
	dirname = temp;

	temp = strrchr(dirname, '/');
	if(temp && type != FNC_FILE_WOE)
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
			(pathcmp(dirname, ".") == 0 && is_unc_root(curr_view->curr_dir)) ||
			(pathcmp(dirname, "/") == 0 && is_unc_path(curr_view->curr_dir)))
	{
		char buf[PATH_MAX];
		if(!is_unc_root(dirname))
			snprintf(buf,
					strchr(curr_view->curr_dir + 2, '/') - curr_view->curr_dir + 1, "%s",
					curr_view->curr_dir);
		else
			strcpy(buf, dirname);

		complete_with_shared(buf, filename);
		free(filename);
		free(dirname);
		return;
	}
	if(is_unc_path(curr_view->curr_dir))
	{
		char buf[PATH_MAX];
		if(is_path_absolute(dirname) && !is_unc_root(curr_view->curr_dir))
			snprintf(buf,
					strchr(curr_view->curr_dir + 2, '/') - curr_view->curr_dir + 2, "%s",
					curr_view->curr_dir);
		else
				snprintf(buf, sizeof(buf), "%s", curr_view->curr_dir);
		strcat(buf, dirname);
		chosp(buf);
		free(dirname);
		dirname = strdup(buf);
	}
#endif

	dir = opendir(dirname);

	if(dir == NULL || my_chdir(dirname) != 0)
	{
		add_completion(filename);
		free(filename);
		free(dirname);
		return;
	}

	filename_len = strlen(filename);
	while((d = readdir(dir)) != NULL)
	{
#ifndef _WIN32
		char *escaped;
#endif

		if(filename[0] == '\0' && d->d_name[0] == '.')
			continue;
		if(pathncmp(d->d_name, filename, filename_len) != 0)
			continue;

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
		else if(pathcmp(dirname, "."))
  	{
			char * tempfile = (char *)NULL;
			int len = strlen(dirname) + strlen(d->d_name) + 1;
			tempfile = malloc(len*sizeof(char));
			if(!tempfile)
			{
				closedir(dir);
				(void)my_chdir(curr_view->curr_dir);
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
		{
			temp = strdup(d->d_name);
		}

		if(isdir)
		{
			char * tempfile = (char *)NULL;
			tempfile = malloc((strlen(d->d_name) + 2) * sizeof(char));
			if(tempfile == NULL)
			{
				closedir(dir);
				(void)my_chdir(curr_view->curr_dir);
				add_completion(filename);
				free(filename);
				free(dirname);
				return;
			}
			snprintf(tempfile, strlen(d->d_name) + 2, "%s/", d->d_name);
			temp = strdup(tempfile);

			free(tempfile);
		}
#ifndef _WIN32
		escaped = woe ? strdup(temp) : escape_filename(temp, 1);
		add_completion(escaped);
		free(escaped);
#else
		add_completion(escape_for_cd(temp));
#endif
		free(temp);
	}

	(void)my_chdir(curr_view->curr_dir);

	completion_group_end();
	if(type != FNC_EXECONLY)
	{
		if(get_completion_count() == 0)
		{
			add_completion(filename);
		}
		else
		{
#ifndef _WIN32
			temp = woe ? strdup(filename) : escape_filename(filename, 1);
			add_completion(temp);
			free(temp);
#else
			add_completion(escape_for_cd(filename));
#endif
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

#ifndef _WIN32
void complete_user_name(const char *str)
{
	struct passwd* pw;
	size_t len;

	len = strlen(str);
	setpwent();
	while((pw = getpwent()) != NULL)
	{
		if(strncmp(pw->pw_name, str, len) == 0)
			add_completion(pw->pw_name);
	}
	completion_group_end();
	add_completion(str);
}

void complete_group_name(const char *str)
{
	struct group* gr;
	size_t len = strlen(str);

	setgrent();
	while((gr = getgrent()) != NULL)
	{
		if(strncmp(gr->gr_name, str, len) == 0)
			add_completion(gr->gr_name);
	}
	completion_group_end();
	add_completion(str);
}
#endif

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
		complete_user_name(colon + 1);
		return colon - str + 1;
	}
#else
	add_completion(str);
	return 0;
#endif
}

static void
complete_filetype(const char *str)
{
	char *filename;
	char *prog_str;
	char *p;
	char *prog_copy;
	char *free_this;
	size_t len = strlen(str);

	filename = get_current_file_name(curr_view);
	prog_str = form_program_list(filename);
	if(prog_str == NULL)
		return;

	p = prog_str;
	while(isspace(*p) || *p == ',')
		p++;

	prog_copy = strdup(p);
	free_this = prog_copy;

	while(prog_copy[0] != '\0')
	{
		char *ptr;
		if((ptr = strchr(prog_copy, ',')) == NULL)
			ptr = prog_copy + strlen(prog_copy);

		while(ptr != NULL && ptr[1] == ',')
			ptr = strchr(ptr + 2, ',');
		if(ptr == NULL)
			break;

		*ptr++ = '\0';

		while(isspace(*prog_copy) || *prog_copy == ',')
			prog_copy++;

		if(strcmp(prog_copy, "*") != 0)
		{
			if((p = strchr(prog_copy, ' ')) != NULL)
				*p = '\0';
			replace_double_comma(prog_copy, 0);
			if(strncmp(prog_copy, str, len) == 0)
				add_completion(prog_copy);
		}
		prog_copy = ptr;
	}

	free(free_this);
	free(prog_str);

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

static void
complete_winrun(const char *str)
{
	static const char *VARIANTS[] = { "^", "$", "%", ".", "," };
	size_t len = strlen(str);
	int i;

	for(i = 0; i < ARRAY_LEN(VARIANTS); i++)
	{
		if(strncmp(str, VARIANTS[i], len) == 0)
			add_completion(VARIANTS[i]);
	}
	completion_group_end();
	add_completion(str);
}

static void
complete_envvar(const char *str)
{
	extern char **environ;
	char **p = environ;
	size_t len = strlen(str);

	while(*p != NULL)
	{
		if(strncmp(*p, str, len) == 0)
		{
			char *equal = strchr(*p, '=');
			*equal = '\0';
			add_completion(*p);
			*equal = '=';
		}
		p++;
	}

	completion_group_end();
	add_completion(str);
}

void
exec_startup_commands(int c, char **v)
{
	static int argc;
	static char **argv;
	int x;

	if(c > 0)
	{
		argc = c;
		argv = v;
	}

	for(x = 1; x < argc; x++)
	{
		if(strcmp(argv[x], "-c") == 0)
		{
			exec_commands(argv[x + 1], curr_view, 0, GET_COMMAND);
			x++;
		}
		else if(argv[x][0] == '+')
		{
			exec_commands(argv[x] + 1, curr_view, 0, GET_COMMAND);
		}
	}
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

static char *
cmds_expand_envvars(const char *str)
{
	char *result = NULL;
	size_t len = 0;
	int prev_slash = 0;
	while(*str != '\0')
	{
		if(!prev_slash && *str == '$' && isalpha(str[1]))
		{
			char name[NAME_MAX];
			const char *p = str + 1;
			char *q = name;
			const char *cq;
			while((isalnum(*p) || *p == '_') && q - name < sizeof(name) - 1)
				*q++ = *p++;
			*q = '\0';

			cq = env_get(name);
			if(cq != NULL)
			{
				size_t old_len = len;
				q = escape_filename(cq, 1);
				len += strlen(q);
				result = realloc(result, len + 1);
				strcpy(result + old_len, q);
				free(q);
				str = p;
			}
			else
			{
				str++;
			}
		}
		else
		{
			if(*str == '\\')
				prev_slash = !prev_slash;
			else
				prev_slash = 0;

			result = realloc(result, len + 1 + 1);
			result[len++] = *str++;
			result[len] = '\0';
		}
	}
	if(result == NULL)
		result = strdup("");
	return result;
}

static void
post(int id)
{
	if(id == COM_GOTO)
		return;
	if((curr_view != NULL && !curr_view->selected_files) || !need_clean_selection)
		return;

	clean_selected_files(curr_view);
	load_saving_pos(curr_view, 1);
	move_to_list_pos(curr_view, curr_view->list_pos);
}

#ifndef TEST
static
#endif
void
select_range(int id, const cmd_info_t *cmd_info)
{
	int x;
	int y = 0;

	/* Both a starting range and an ending range are given. */
	if(cmd_info->begin > -1)
	{
		clean_selected_files(curr_view);

		for(x = cmd_info->begin; x <= cmd_info->end; x++)
		{
			if(pathcmp(curr_view->dir_entry[x].name, "../") == 0 &&
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
	else
	{
		return;
	}

	if(curr_view->selected_files > 0)
		curr_view->user_selection = 0;
}

static void
select_count(const cmd_info_t *cmd_info, int count)
{
	int pos;

	/* Both a starting range and an ending range are given. */
	pos = cmd_info->end;
	if(pos < 0)
		pos = curr_view->list_pos;

	clean_selected_files(curr_view);

	while(count-- > 0 && pos < curr_view->list_rows)
	{
		if(pathcmp(curr_view->dir_entry[pos].name, "../") != 0)
		{
			curr_view->dir_entry[pos].selected = 1;
			curr_view->selected_files++;
		}
		pos++;
	}
}

static int
skip_at_beginning(int id, const char *args)
{
	if(id == COM_WINDO)
	{
		return 0;
	}
	else if(id == COM_WINRUN)
	{
		args = skip_whitespace(args);
		if(*args != '\0')
			return 1;
	}
	return -1;
}

static int
notation_sorter(const void *first, const void *second)
{
	const key_pair_t *paira = (const key_pair_t *)first;
	const key_pair_t *pairb = (const key_pair_t *)second;
	const char *stra = paira->notation;
	const char *strb = pairb->notation;
	return strcasecmp(stra, strb);
}

void
init_commands(void)
{
	int i;

	if(cmds_conf.inner != NULL)
	{
		init_cmds(1, &cmds_conf);
		return;
	}

	/* we get here when init_commands() is called first time */
	init_cmds(1, &cmds_conf);
	add_builtin_commands((const cmd_add_t *)&commands, ARRAY_LEN(commands));

	split_path();
	qsort(key_pairs, ARRAY_LEN(key_pairs), sizeof(*key_pairs), notation_sorter);
	for(i = 0; i < ARRAY_LEN(key_pairs); i++)
		key_pairs[i].len = strlen(key_pairs[i].notation);

	init_variables(&print_func);
}

static void
split_path(void)
{
	const char *path, *p, *q;
	int i;

	path = env_get("PATH");

	if(paths != NULL)
		free_string_array(paths, paths_count);

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
#ifndef _WIN32
		q = strchr(p, ':');
#else
		q = strchr(p, ';');
#endif
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
			if(pathcmp(paths[j], s) == 0)
			{
				free(s);
				i--;
				break;
			}
		}
	}
	while (q[0] != '\0');
	paths_count = i;
}

static void
save_history(const char *line, char **hist, int *num, int *len)
{
	int x;

	if(*len <= 0)
		return;

	/* Don't add empty lines */
	if(line[0] == '\0')
		return;

	/* Don't add :!! or :! to history list */
	if(strcmp(line, "!!") == 0 || strcmp(line, "!") == 0)
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
		free(hist[x]);
		hist[x] = strdup(hist[x - 1]);
		x--;
	}

	free(hist[0]);
	hist[0] = strdup(line);
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

/* Applies all filename modifiers. */
static const char *
apply_mods(const char *path, const char *parent, const char *mod)
{
	static char buf[PATH_MAX];
	int napplied = 0;

	strcpy(buf, path);
	while(*mod != '\0')
	{
		int mod_len;
		const char *p = apply_mod(buf, parent, mod, &mod_len);
		if(p == NULL)
			break;
		strcpy(buf, p);
		mod += mod_len;
		napplied++;
	}

#ifdef _WIN32
	/* this is needed to run something like explorer.exe, which isn't smart enough
	 * to understand forward slashes */
	if(napplied == 0 && pathcmp(cfg.shell, "cmd") != 0)
		to_back_slash(buf);
#endif

	return buf;
}

/* Applies one filename modifiers per call. */
static const char *
apply_mod(const char *path, const char *parent, const char *mod, int *mod_len)
{
	char path_buf[PATH_MAX];
	static char buf[PATH_MAX];

	snprintf(path_buf, sizeof(path_buf), "%s", path);
#ifdef _WIN32
	to_forward_slash(path_buf);
#endif

	*mod_len = 2;
	if(strncmp(mod, ":p", 2) == 0)
		*mod_len += apply_p_mod(path_buf, parent, buf, sizeof(buf));
	else if(strncmp(mod, ":~", 2) == 0)
		*mod_len += apply_tilde_mod(path_buf, buf, sizeof(buf));
	else if(strncmp(mod, ":.", 2) == 0)
		*mod_len += apply_dot_mod(path_buf, buf, sizeof(buf));
	else if(strncmp(mod, ":h", 2) == 0)
		*mod_len += apply_h_mod(path_buf, buf, sizeof(buf));
#ifdef _WIN32
	else if(strncmp(mod, ":u", 2) == 0)
		*mod_len += apply_u_mod(path_buf, buf, sizeof(buf));
#endif
	else if(strncmp(mod, ":t", 2) == 0)
		*mod_len += apply_t_mod(path_buf, buf, sizeof(buf));
	else if(strncmp(mod, ":r", 2) == 0)
		*mod_len += apply_r_mod(path_buf, buf, sizeof(buf));
	else if(strncmp(mod, ":e", 2) == 0)
		*mod_len += apply_e_mod(path_buf, buf, sizeof(buf));
	else if(strncmp(mod, ":s", 2) == 0 || strncmp(mod, ":gs", 3) == 0)
		*mod_len += apply_s_gs_mod(path_buf, mod, buf, sizeof(buf));
	else
		return NULL;

#ifdef _WIN32
	/* this is needed to run something like explorer.exe, which isn't smart enough
	 * to understand forward slashes */
	if(strncmp(mod, ":s", 2) != 0 && strncmp(mod, ":gs", 3) != 0)
	{
		if(pathcmp(cfg.shell, "cmd") != 0)
			to_back_slash(buf);
	}
#endif

	return buf;
}

/* Implementation of :p filename modifier. */
static int
apply_p_mod(const char *path, const char *parent, char *buf, size_t buf_len)
{
	size_t len;
	if(is_path_absolute(path))
	{
		snprintf(buf, buf_len, "%s", path);
		return 0;
	}

	len = snprintf(buf, buf_len, "%s", parent);
	chosp(buf);
	snprintf(buf + len, buf_len - len, "/%s", path);
	return 0;
}

/* Implementation of :~ filename modifier. */
static int
apply_tilde_mod(const char *path, char *buf, size_t buf_len)
{
	size_t home_len = strlen(cfg.home_dir);
	if(pathncmp(path, cfg.home_dir, home_len - 1) != 0)
	{
		snprintf(buf, buf_len, "%s", path);
		return 0;
	}

	snprintf(buf, buf_len, "~%s", path + home_len - 1);
	return 0;
}

/* Implementation of :. filename modifier. */
static int
apply_dot_mod(const char *path, char *buf, size_t buf_len)
{
	size_t len = strlen(curr_view->curr_dir);
	if(pathncmp(path, curr_view->curr_dir, len) != 0 || path[len] == '\0')
		snprintf(buf, buf_len, "%s", path);
	else
		snprintf(buf, buf_len, "%s", path + len + 1);
	return 0;
}

/* Implementation of :h filename modifier. */
static int
apply_h_mod(const char *path, char *buf, size_t buf_len)
{
	char *p = strrchr(path, '/');
	if(p == NULL)
	{
		snprintf(buf, buf_len, ".");
	}
	else
	{
		snprintf(buf, buf_len, "%s", path);
		if(!is_root_dir(path))
		{
			buf[p - path + 1] = '\0';
			if(!is_root_dir(buf))
				buf[p - path] = '\0';
		}
	}
	return 0;
}

#ifdef _WIN32
/* Implementation of :u filename modifier. */
static int
apply_u_mod(const char *path, char *buf, size_t buf_len)
{
	char *p;
	if(!is_unc_path(path))
	{
		DWORD size = buf_len - 2;
		snprintf(buf, buf_len, "//");
		GetComputerNameA(buf + 2, &size);
		return 0;
	}
	snprintf(buf, buf_len, "%s", path);
	p = strchr(path + 2, '/');
	if(p != NULL)
		buf[p - path] = '\0';
	return 0;
}
#endif

/* Implementation of :t filename modifier. */
static int
apply_t_mod(const char *path, char *buf, size_t buf_len)
{
	char *p = strrchr(path, '/');
	snprintf(buf, buf_len, "%s", (p == NULL) ? path : (p + 1));
	return 0;
}

/* Implementation of :r filename modifier. */
static int
apply_r_mod(const char *path, char *buf, size_t buf_len)
{
	char *slash = strrchr(path, '/');
	char *dot = strrchr(path, '.');
	snprintf(buf, buf_len, "%s", path);
	if(dot == NULL || (slash != NULL && dot < slash) || dot == path ||
			dot == slash + 1)
		return 0;
	buf[dot - path] = '\0';
	return 0;
}

/* Implementation of :e filename modifier. */
static int
apply_e_mod(const char *path, char *buf, size_t buf_len)
{
	char *slash = strrchr(path, '/');
	char *dot = strrchr(path, '.');
	if(dot == NULL || (slash != NULL && dot < slash) || dot == path ||
			dot == slash + 1)
		snprintf(buf, buf_len, "%s", "");
	else
		snprintf(buf, buf_len, "%s", dot + 1);
	return 0;
}

/* Implementation of :s and :gs filename modifiers. */
static int
apply_s_gs_mod(const char *path, const char *mod, char *buf, size_t buf_len)
{
	char pattern[256], sub[256];
	int global;
	const char *start = mod;
	char c = (mod[1] == 'g') ? mod++[3] : mod[2];
	const char *t, *p = find_nth_chr(mod, c, 3);
	if(p == NULL)
	{
		snprintf(buf, buf_len, "%s", path);
		return 0;
	}
	t = find_nth_chr(mod, c, 2);
	snprintf(pattern, t - (mod + 3) + 1, "%s", mod + 3);
	snprintf(sub, p - (t + 1) + 1, "%s", t + 1);
	global = (mod[0] == 'g');
	snprintf(buf, buf_len, "%s", substitute_in_name(path, pattern, sub, global));
	return (p + 1) - start - 2;
}

static char *
append_selected_file(FileView *view, char *expanded, int dir_name_len, int pos,
		int quotes, const char *mod)
{
	char buf[PATH_MAX] = "";

	if(dir_name_len != 0)
		strcat(strcpy(buf, view->curr_dir), "/");
	strcat(buf, view->dir_entry[pos].name);
	chosp(buf);

	if(quotes)
	{
		const char *s = enclose_in_dquotes(apply_mods(buf, view->curr_dir, mod));
		expanded = realloc(expanded, strlen(expanded) + strlen(s) + 1 + 1);
		strcat(expanded, s);
	}
	else
	{
		char *temp;

		temp = escape_filename(apply_mods(buf, view->curr_dir, mod), 0);
		expanded = realloc(expanded, strlen(expanded) + strlen(temp) + 1 + 1);
		strcat(expanded, temp);
		free(temp);
	}

	return expanded;
}

#ifndef TEST
static
#endif
char *
append_selected_files(FileView *view, char *expanded, int under_cursor,
		int quotes, const char *mod)
{
	int dir_name_len = 0;
#ifdef _WIN32
	size_t old_len = strlen(expanded);
#endif

	if(view == other_view)
		dir_name_len = strlen(other_view->curr_dir) + 1;

	if(view->selected_files && !under_cursor)
	{
		int y, x = 0;
		for(y = 0; y < view->list_rows; y++)
		{
			if(!view->dir_entry[y].selected)
				continue;

			expanded = append_selected_file(view, expanded, dir_name_len, y, quotes,
					mod);

			if(++x != view->selected_files)
				strcat(expanded, " ");
		}
	}
	else
	{
		expanded = append_selected_file(view, expanded, dir_name_len,
				view->list_pos, quotes, mod);
	}

#ifdef _WIN32
	if(pathcmp(cfg.shell, "cmd") == 0)
		to_back_slash(expanded + old_len);
#endif

	return expanded;
}

static char *
append_to_expanded(char *expanded, const char* str)
{
	char *t;

	t = realloc(expanded, strlen(expanded) + strlen(str) + 1);
	if(t == NULL)
	{
		(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
		free(expanded);
		return NULL;
	}
	strcat(t, str);
	return t;
}

static char *
expand_directory_path(FileView *view, char *expanded, int quotes,
		const char *mod)
{
	char *result;
	if(quotes)
	{
		const char *s = enclose_in_dquotes(apply_mods(view->curr_dir, "/", mod));
		result = append_to_expanded(expanded, s);
	}
	else
	{
		char *escaped;

		escaped = escape_filename(apply_mods(view->curr_dir, "/", mod), 0);
		if(escaped == NULL)
		{
			(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
			free(expanded);
			return NULL;
		}

		result = append_to_expanded(expanded, escaped);
		free(escaped);
	}

#ifdef _WIN32
	if(pathcmp(cfg.shell, "cmd") == 0)
		to_back_slash(result);
#endif

	return result;
}

/* args could be equal NULL
 * The string returned needs to be freed in the calling function */
char *
expand_macros(FileView *view, const char *command, const char *args,
		int *use_menu, int *split)
{
	/* TODO: refactor this function expand_macros() */

	size_t cmd_len;
	char *expanded;
	size_t x;
	int y = 0;
	int len = 0;

	cmd_len = strlen(command);

	expanded = calloc(cmd_len + 1, sizeof(char *));

	for(x = 0; x < cmd_len; x++)
		if(command[x] == '%')
			break;

	strncat(expanded, command, x);
	x++;
	len = strlen(expanded);

	do
	{
		int quotes = 0;
		if(command[x] == '"' && strchr("cCfFbdD", command[x + 1]) != NULL)
		{
			quotes = 1;
			x++;
		}
		switch(command[x])
		{
			case 'a': /* user arguments */
				if(args != NULL)
				{
					char arg_buf[strlen(args) + 2];

					expanded = (char *)realloc(expanded,
							strlen(expanded) + strlen(args) + 3);
					snprintf(arg_buf, sizeof(arg_buf), "%s", args);
					strcat(expanded, arg_buf);
					len = strlen(expanded);
				}
				break;
			case 'b': /* selected files of both dirs */
				expanded = append_selected_files(curr_view, expanded, 0, quotes,
						command + x + 1);
				len = strlen(expanded);
				expanded = realloc(expanded, len + 1 + 1);
				strcat(expanded, " ");
				expanded = append_selected_files(other_view, expanded, 0, quotes,
						command + x + 1);
				len = strlen(expanded);
				break;
			case 'c': /* current dir file under the cursor */
				expanded = append_selected_files(curr_view, expanded, 1, quotes,
						command + x + 1);
				len = strlen(expanded);
				break;
			case 'C': /* other dir file under the cursor */
				expanded = append_selected_files(other_view, expanded, 1, quotes,
						command + x + 1);
				len = strlen(expanded);
				break;
			case 'f': /* current dir selected files */
				expanded = append_selected_files(curr_view, expanded, 0, quotes,
						command + x + 1);
				len = strlen(expanded);
				break;
			case 'F': /* other dir selected files */
				expanded = append_selected_files(other_view, expanded, 0, quotes,
						command + x + 1);
				len = strlen(expanded);
				break;
			case 'd': /* current directory */
				expanded = expand_directory_path(curr_view, expanded, quotes,
						command + x + 1);
				len = strlen(expanded);
				break;
			case 'D': /* other directory */
				expanded = expand_directory_path(other_view, expanded, quotes,
						command + x + 1);
				len = strlen(expanded);
				break;
			case 'm': /* use menu */
				*use_menu = 1;
				break;
			case 'M': /* use menu like with :locate and :find */
				*use_menu = 2;
				break;
			case 'S': /* show command output in the status bar */
				*use_menu = 3;
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
#ifdef _WIN32
				else if(strncmp(command + x, ":u", 2) == 0)
					y += 2;
#endif
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
						y += strlen(command + y);
					x = y;
					continue;
				}
			}
			if(command[x] == '%')
				break;
			if(command[x] != '\0')
				x++;
		}
		assert(x >= y);
		assert(y <= cmd_len);
		expanded = realloc(expanded, len + (x - y) + 1);
		strncat(expanded, command + y, x - y);
		len = strlen(expanded);
		x++;
	}
	while(x < cmd_len);

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
 *  = 0 - do not pause
 *  < 0 - pause on error
 */
int
shellout(const char *command, int pause, int allow_screen)
{
	/* TODO: refactor this big function shellout() */
	size_t len = (command != NULL) ? strlen(command) : 0;
	char buf[cfg.max_args];
	int result;
	int ec;

	if(pause > 0 && len > 1 && command[len - 1] == '&')
		pause = -1;

	if(command != NULL)
	{
		if(allow_screen && cfg.use_screen)
		{
			int bg;
			char *escaped;
			char *ptr = NULL;
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
				{
					snprintf(buf, sizeof(buf),
							"screen -t \"%s\" %s -c '%s" PAUSE_STR "'",
							title + strlen(get_vicmd(&bg)) + 1, escaped_sh, command);
				}
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
				{
					title = strdup("Shell");
				}

				if(pause > 0)
				{
					snprintf(buf, sizeof(buf),
							"screen -t \"%.10s\" %s -c '%s" PAUSE_STR "'", title,
							escaped_sh, command);
				}
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
			{
#ifdef _WIN32
				if(pathcmp(cfg.shell, "cmd") == 0)
					snprintf(buf, sizeof(buf), "%s" PAUSE_STR, command);
				else
#endif
					snprintf(buf, sizeof(buf), "%s; " PAUSE_CMD, command);
			}
			else
			{
				snprintf(buf, sizeof(buf), "%s", command);
			}
		}
	}
	else
	{
		if(allow_screen && cfg.use_screen)
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

	/* Need to use setenv instead of getcwd for a symlink directory */
	env_set("PWD", curr_view->curr_dir);

	ec = my_system(buf);
	result = WEXITSTATUS(ec);

#ifndef _WIN32
	if(result != 0 && pause < 0)
		my_system(PAUSE_CMD);

	/* force views update */
	memset(&lwin.dir_mtime, 0, sizeof(lwin.dir_mtime));
	memset(&rwin.dir_mtime, 0, sizeof(rwin.dir_mtime));
#endif

#ifdef _WIN32
	reset_prog_mode();
	resize_term(cfg.lines, cfg.columns);
#endif
	/* always redraw to handle resizing of terminal */
	if(!curr_stats.auto_redraws)
		curr_stats.need_redraw = 1;

	curs_set(FALSE);

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

	if((buf = malloc(strlen(get_vicmd(bg)) + strlen(files) + 2)) != NULL)
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

static key_pair_t *
find_notation(const char *str)
{
	int l = 0, u = ARRAY_LEN(key_pairs) - 1;
	while(l <= u)
	{
		int i = (l + u)/2;
		int comp = strncasecmp(str, key_pairs[i].notation, key_pairs[i].len);
		if(comp == 0)
			return &key_pairs[i];
		else if(comp < 0)
			u = i - 1;
		else
			l = i + 1;
	}
	return NULL;
}

static wchar_t *
substitute_specs(const char *cmd)
{
	wchar_t *buf, *p;
	size_t len = strlen(cmd) + 1;

	buf = malloc(len*sizeof(wchar_t));
	if(buf == NULL)
		return NULL;

	p = buf;
	while(*cmd != '\0')
	{
		key_pair_t *pair;
		pair = find_notation(cmd);
		if(pair == NULL)
		{
			*p++ = (wchar_t)*cmd++;
		}
		else
		{
			wcscpy(p, pair->key);
			p += wcslen(p);
			cmd += pair->len;
		}
	}
	*p = L'\0';
	assert(p + 1 - buf <= len);

	return buf;
}

static void
print_func(int error, const char *msg, const char *description)
{
	if(print_buf[0] != '\0')
	{
		strncat(print_buf, "\n", sizeof(print_buf) - strlen(print_buf) - 1);
	}
	if(*msg == '\0')
	{
		strncat(print_buf, description, sizeof(print_buf) - strlen(print_buf) - 1);
	}
	else
	{
		snprintf(print_buf, sizeof(print_buf) - strlen(print_buf), "%s: %s", msg,
				description);
	}
}

static void
remove_selection(FileView *view)
{
	if(view->selected_files == 0)
		return;

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
		case CMDS_ERR_NO_BUILTIN_REDEFINE:
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
		case CMDS_ERR_CUSTOM:
			/* error message is posted by command handler */
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
is_in_arg(const char *cmd, const char *pos)
{
	cmd_info_t info;
	int id;

	id = get_cmd_info(cmd, &info);

	if(id == COM_FILTER)
		return (line_pos(cmd, pos, ' ', 1) == 0);
	else if(id == COM_SUBSTITUTE || id == COM_TR)
		return (line_pos(cmd, pos, info.sep, 1) == 0);
	else
		return (line_pos(cmd, pos, ' ', 0) == 0);
}

static int
is_whole_line_command(const char *cmd)
{
	/* TODO: rewrite this using cmds.c */

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
	else if(strncmp(cmd, "win", 3) == 0)
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
		else if((*p == '|' && is_in_arg(cmd, q)) || *p == '\0')
		{
			int ret;

			if(*p != '\0')
				p++;

			while(*cmd == ' ' || *cmd == ':')
				cmd++;
			if(is_whole_line_command(cmd))
			{
				save_msg += exec_command(cmd, view, type) != 0;
				break;
			}

			*q = '\0';
			q = p;

			ret = exec_command(cmd, view, type);
			if(ret < 0)
				save_msg = -1;
			else if(ret > 0)
				save_msg = 1;

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
			return find_npattern(view, view->regexp, type == GET_BSEARCH_PATTERN, 1);
		if(type == GET_VFSEARCH_PATTERN || type == GET_VBSEARCH_PATTERN)
			return find_vpattern(view, view->regexp, type == GET_VBSEARCH_PATTERN);
		if(type == GET_COMMAND)
			return execute_command(view, cmd, 0);
		if(type == GET_VWFSEARCH_PATTERN || type == GET_VWBSEARCH_PATTERN)
			return find_vwpattern(cmd, type == GET_VWBSEARCH_PATTERN);
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
		return find_npattern(view, cmd, type == GET_BSEARCH_PATTERN, 1);
	}
	else if(type == GET_VFSEARCH_PATTERN || type == GET_VBSEARCH_PATTERN)
	{
		strncpy(view->regexp, cmd, sizeof(view->regexp));
		save_search_history(cmd);
		return find_vpattern(view, cmd, type == GET_VBSEARCH_PATTERN);
	}
	else if(type == GET_VWFSEARCH_PATTERN || type == GET_VWBSEARCH_PATTERN)
	{
		return find_vwpattern(cmd, type == GET_VWBSEARCH_PATTERN);
	}
	return 0;
}

void
comm_quit(int write_info, int force)
{
	if(!force)
	{
		job_t *job;
		int bg_count = 0;
#ifndef _WIN32
		sigset_t new_mask;

		sigemptyset(&new_mask);
		sigaddset(&new_mask, SIGCHLD);
		sigprocmask(SIG_BLOCK, &new_mask, NULL);
#endif

		job = jobs;
		while(job != NULL)
		{
			if(job->running && job->pid == -1)
				bg_count++;
			job = job->next;
		}

#ifndef _WIN32
		/* Unblock SIGCHLD signal */
		sigprocmask(SIG_UNBLOCK, &new_mask, NULL);
#endif

		if(bg_count > 0)
		{
			if(!query_user_menu("Warning",
					"Some of backgrounded commands are still working.  Quit?"))
				return;
		}
	}

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

	set_term_title(NULL);
	endwin();
	exit(0);
}

void
comm_only(void)
{
	if(curr_stats.number_of_windows == 1)
		return;

	curr_stats.number_of_windows = 1;
	redraw_window();
}

void
comm_split(int vertical)
{
	SPLIT orient = vertical ? VSPLIT : HSPLIT;
	if(curr_stats.number_of_windows == 2 && curr_stats.split == orient)
		return;

	if(curr_stats.number_of_windows == 2 && curr_stats.splitter_pos > 0)
	{
		if(orient == VSPLIT)
			curr_stats.splitter_pos *= (float)getmaxx(stdscr)/getmaxy(stdscr);
		else
			curr_stats.splitter_pos *= (float)getmaxy(stdscr)/getmaxx(stdscr);
	}

	curr_stats.split = orient;
	curr_stats.number_of_windows = 2;
	redraw_window();
}

static int
goto_cmd(const cmd_info_t *cmd_info)
{
	move_to_list_pos(curr_view, cmd_info->end);
	return 0;
}

static void
output_to_statusbar(const char *cmd)
{
	FILE *file, *err;
	char buf[2048];
	char *lines;
	size_t len;

	if(background_and_capture((char *)cmd, &file, &err) != 0)
	{
		show_error_msgf("Trouble running command", "Unable to run: %s", cmd);
		return;
	}

	lines = NULL;
	len = 0;
	while(fgets(buf, sizeof(buf), file) == buf)
	{
		chomp(buf);
		lines = realloc(lines, len + 1 + strlen(buf) + 1);
		len += sprintf(lines + len, "%s%s", (len == 0) ? "": "\n", buf);
	}

	fclose(file);
	fclose(err);

	status_bar_message((lines == NULL) ? "" : lines);
	free(lines);
}

static int
emark_cmd(const cmd_info_t *cmd_info)
{
	int i;
	char *com = (char *)cmd_info->args;
	char buf[COMMAND_GROUP_INFO_LEN];

	i = skip_whitespace(com) - com;

	if(com[i] == '\0')
		return 0;

	if(cmd_info->usr1 == 3)
	{
		output_to_statusbar(com);
		return 1;
	}
	else if(cmd_info->usr1)
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
		if(shellout(com + i, cmd_info->emark ? 1 : (cfg.fast_run ? 0 : -1), 1)
				== 127 && cfg.fast_run)
		{
			char *buf = fast_run_complete(com + i);
			if(buf == NULL)
				return 1;

			shellout(buf, cmd_info->emark ? 1 : -1, 1);
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
alink_cmd(const cmd_info_t *cmd_info)
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
apropos_cmd(const cmd_info_t *cmd_info)
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
cd_cmd(const cmd_info_t *cmd_info)
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
change_cmd(const cmd_info_t *cmd_info)
{
	enter_change_mode(curr_view);
	need_clean_selection = 0;
	return 0;
}

static int
chmod_cmd(const cmd_info_t *cmd_info)
{
#ifndef _WIN32
	regex_t re;
	int err;
	int i;
#endif

	if(cmd_info->argc == 0)
	{
		enter_attr_mode(curr_view);
		need_clean_selection = 0;
		return 0;
	}

#ifndef _WIN32
	if((err = regcomp(&re, "^([ugoa]*([-+=]([rwxXst]*|[ugo]))+)|([0-7]{3,4})$",
			REG_EXTENDED)) != 0)
	{
		status_bar_errorf("Regexp error: %s", get_regexp_error(err, &re));
		regfree(&re);
		return 1;
	}

	for(i = 0; i < cmd_info->argc; i++)
	{
		if(regexec(&re, cmd_info->argv[i], 0, NULL, 0) == REG_NOMATCH)
		{
			regfree(&re);
			status_bar_errorf("Invalid argument: %s", cmd_info->argv[i]);
			return 1;
		}
	}
	regfree(&re);

	files_chmod(curr_view, cmd_info->args, cmd_info->emark);
#endif
	return 0;
}

#ifndef _WIN32
static int
chown_cmd(const cmd_info_t *cmd_info)
{
	char *colon, *user, *group;
	int u = 0, g = 0;
	uid_t uid;
	gid_t gid;

	if(cmd_info->argc == 0)
	{
		change_owner();
		return 0;
	}

	colon = strchr(cmd_info->argv[0], ':');
	if(colon == NULL)
	{
		user = cmd_info->argv[0];
		group = "";
	}
	else
	{
		*colon = '\0';
		user = cmd_info->argv[0];
		group = colon + 1;
	}
	u = user[0] != '\0';
	g = group[0] != '\0';

	if(u && get_uid(user, &uid) != 0)
	{
		status_bar_errorf("Invalid user name: \"%s\"", user);
		return 1;
	}
	if(g && get_gid(group, &gid) != 0)
	{
		status_bar_errorf("Invalid group name: \"%s\"", group);
		return 1;
	}

	clean_selected_files(curr_view);
	chown_files(u, g, uid, gid);

	return 0;
}
#endif

static int
clone_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->qmark)
	{
		if(cmd_info->argc > 0)
		{
			status_bar_error("No arguments are allowed if you use \"!\"");
			return 1;
		}
		return clone_files(curr_view, NULL, -1, 0, 1) != 0;
	}
	else
	{
		return clone_files(curr_view, cmd_info->argv, cmd_info->argc,
				cmd_info->emark, 1) != 0;
	}
}

static int
cmap_cmd(const cmd_info_t *cmd_info)
{
	return do_map(cmd_info, "Command Line", "cmap", CMDLINE_MODE, 0) != 0;
}

static int
cnoremap_cmd(const cmd_info_t *cmd_info)
{
	return do_map(cmd_info, "Command Line", "cmap", CMDLINE_MODE, 1) != 0;
}

static int
colorscheme_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->qmark)
	{
		status_bar_message(cfg.cs.name);
		return 1;
	}
	else if(cmd_info->argc == 0)
	{
		/* show menu with colorschemes listed */
		show_colorschemes_menu(curr_view);
		return 0;
	}
	else if(find_color_scheme(cmd_info->argv[0]))
	{
		if(cmd_info->argc == 2)
		{
			if(!is_dir(cmd_info->argv[1]))
			{
				status_bar_errorf("%s isn't a directory" , cmd_info->argv[1]);
				return 1;
			}

			assoc_dir(cmd_info->argv[0], cmd_info->argv[1]);
			lwin.color_scheme = check_directory_for_color_scheme(1, lwin.curr_dir);
			rwin.color_scheme = check_directory_for_color_scheme(0, rwin.curr_dir);
			redraw_lists();
			return 0;
		}
		else
		{
			int ret = load_color_scheme(cmd_info->argv[0]);
			lwin.cs = cfg.cs;
			rwin.cs = cfg.cs;
			redraw_lists();
			update_all_windows();
			return ret;
		}
	}
	else
	{
		status_bar_errorf("Cannot find colorscheme %s" , cmd_info->argv[0]);
		return 1;
	}
}

static int
command_cmd(const cmd_info_t *cmd_info)
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
copy_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->qmark)
	{
		if(cmd_info->argc > 0)
		{
			status_bar_error("No arguments are allowed if you use \"?\"");
			return 1;
		}
		if(cmd_info->bg)
			return cpmv_files_bg(curr_view, NULL, -1, 0, cmd_info->emark) != 0;
		else
			return cpmv_files(curr_view, NULL, -1, 0, 0, 0) != 0;
	}
	else if(cmd_info->bg)
	{
		return cpmv_files_bg(curr_view, cmd_info->argv, cmd_info->argc, 0,
				cmd_info->emark) != 0;
	}
	else
	{
		return cpmv_files(curr_view, cmd_info->argv, cmd_info->argc, 0, 0,
				cmd_info->emark) != 0;
	}
}

static int
cunmap_cmd(const cmd_info_t *cmd_info)
{
	return do_unmap(cmd_info->argv[0], CMDLINE_MODE);
}

static int
delete_cmd(const cmd_info_t *cmd_info)
{
	int reg = DEFAULT_REG_NAME;
	int result;

	result = get_reg_and_count(cmd_info, &reg);
	if(result != 0)
		return result;

	if(cmd_info->bg)
		result = delete_file_bg(curr_view, !cmd_info->emark) != 0;
	else
		result = delete_file(curr_view, reg, 0, NULL, !cmd_info->emark) != 0;

	return result;
}

static int
delmarks_cmd(const cmd_info_t *cmd_info)
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
dirs_cmd(const cmd_info_t *cmd_info)
{
	return show_dirstack_menu(curr_view) != 0;
}

static int
edit_cmd(const cmd_info_t *cmd_info)
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
			shellout(buf, -1, 1);
		return 0;
	}
	if(!curr_view->selected_files ||
			!curr_view->dir_entry[curr_view->list_pos].selected)
	{
		char buf[PATH_MAX];
		if(cfg.vim_filter)
			use_vim_plugin(curr_view, cmd_info->argc, cmd_info->argv); /* no return */

		snprintf(buf, sizeof(buf), "%s/%s", curr_view->curr_dir,
				get_current_file_name(curr_view));
		view_file(buf, -1, 1);
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
			shellout(cmd, -1, 1);
		free(cmd);
	}
	return 0;
}

static int
empty_cmd(const cmd_info_t *cmd_info)
{
	empty_trash();
	return 0;
}

static int
exe_cmd(const cmd_info_t *cmd_info)
{
	size_t len = 0;
	char *line = NULL;
	int i;
	int result;

	for(i = 0; i < cmd_info->argc; i++)
	{
		char *p;
		if(line != NULL)
			strcat(line, " ");
		len += 1 + strlen(cmd_info->argv[i]) + 1;
		p = realloc(line, len);
		if(p == NULL)
		{
			(void)show_error_msg("Memory error", "Unable to allocate enough memory");
			break;
		}
		if(line == NULL)
			*p = '\0';
		line = p;
		strcat(line, cmd_info->argv[i]);
	}
	if(line == NULL)
		return 0;

	result = exec_commands(line, curr_view, 0, GET_COMMAND);
	free(line);
	return result != 0;
}

static int
file_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->argc == 0)
	{
		need_clean_selection = 0;
		return show_filetypes_menu(curr_view, cmd_info->bg) != 0;
	}
	else
	{
		if(run_with_filetype(curr_view, cmd_info->argv[0], cmd_info->bg) != 0)
		{
			status_bar_error(
					"Can't find associated program with requested beginning");
			return 1;
		}
		return 0;
	}
}

static int
filetype_cmd(const cmd_info_t *cmd_info)
{
	const char *progs;

	progs = skip_non_whitespace(cmd_info->args);
	progs = skip_whitespace(progs + 1);

	set_programs(cmd_info->argv[0], progs, 0);
	return 0;
}

static int
filextype_cmd(const cmd_info_t *cmd_info)
{
	const char *progs;

	progs = skip_non_whitespace(cmd_info->args);
	progs = skip_whitespace(progs + 1);

	set_programs(cmd_info->argv[0], progs, 1);
	return 0;
}

static int
fileviewer_cmd(const cmd_info_t *cmd_info)
{
	const char *progs;

	progs = skip_non_whitespace(cmd_info->args);
	progs = skip_whitespace(progs + 1);

	set_fileviewer(cmd_info->argv[0], progs);
	return 0;
}

static int
filter_cmd(const cmd_info_t *cmd_info)
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
find_cmd(const cmd_info_t *cmd_info)
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
grep_cmd(const cmd_info_t *cmd_info)
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
help_cmd(const cmd_info_t *cmd_info)
{
	char buf[PATH_MAX];
	int bg;

	if(cfg.use_vim_help)
	{
		if(cmd_info->argc > 0)
		{
#ifndef _WIN32
			char *escaped = escape_filename(cmd_info->args, 0);
			snprintf(buf, sizeof(buf), "%s -c help\\ %s -c only", get_vicmd(&bg),
					escaped);
			free(escaped);
#else
			snprintf(buf, sizeof(buf), "%s -c \"help %s\" -c only", get_vicmd(&bg),
					cmd_info->args);
#endif
		}
		else
		{
			snprintf(buf, sizeof(buf), "%s -c 'help vifm.txt' -c only",
					get_vicmd(&bg));
		}
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
#ifndef _WIN32
		shellout(buf, -1, 1);
#else
	{
		def_prog_mode();
		endwin();
		system("cls");
		system(buf);
		redraw_window();
	}
#endif
	return 0;
}

static int
highlight_cmd(const cmd_info_t *cmd_info)
{
	int i;
	int pos;

	if(cmd_info->argc == 0)
	{
		char buf[256*(MAXNUM_COLOR - 2)] = "";
		for(i = 0; i < MAXNUM_COLOR - 2; i++)
		{
			strcat(buf, get_group_str(i, curr_view->cs.color[i]));
			if(i < MAXNUM_COLOR - 2 - 1)
				strcat(buf, "\n");
		}
		status_bar_message(buf);
		return 1;
	}

	pos = string_array_pos_case(HI_GROUPS, MAXNUM_COLOR - 2, cmd_info->argv[0]);
	if(pos < 0)
	{
		status_bar_errorf("Highlight group not found: %s", cmd_info->argv[0]);
		return 1;
	}

	if(cmd_info->argc == 1)
	{
		status_bar_message(get_group_str(pos, curr_stats.cs->color[pos]));
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
				curr_stats.cs->defaulted = -1;
				return 1;
			}
			curr_stats.cs->color[pos].bg = col;
		}
		else if(strcmp(arg_name, "ctermfg") == 0)
		{
			int col;
			if((col = get_color(equal + 1)) < -1)
			{
				status_bar_errorf("Color name or number not recognized: %s", equal + 1);
				curr_stats.cs->defaulted = -1;
				return 1;
			}
			curr_stats.cs->color[pos].fg = col;
		}
		else if(strcmp(arg_name, "cterm") == 0)
		{
			int attrs;
			if((attrs = get_attrs(equal + 1)) == -1)
			{
				status_bar_errorf("Illegal argument: %s", equal + 1);
				return 1;
			}
			curr_stats.cs->color[pos].attr = attrs;
		}
		else
		{
			status_bar_errorf("Illegal argument: %s", cmd_info->argv[i]);
			return 1;
		}
	}
	init_pair(curr_stats.cs_base + pos, curr_stats.cs->color[pos].fg,
			curr_stats.cs->color[pos].bg);
  curr_stats.need_redraw = 1;
	return 0;
}

static const char *
get_group_str(int group, col_attr_t col)
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
			(curr_stats.load_stage >= 2 && col_num > COLORS)))
		return -2;
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
history_cmd(const cmd_info_t *cmd_info)
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
invert_cmd(const cmd_info_t *cmd_info)
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
jobs_cmd(const cmd_info_t *cmd_info)
{
	return show_jobs_menu(curr_view) != 0;
}

static int
let_cmd(const cmd_info_t *cmd_info)
{
	print_buf[0] = '\0';
	if(let_variable(cmd_info->args) != 0)
	{
		status_bar_error(print_buf);
		return 1;
	}
	else if(print_buf[0] != '\0')
	{
		status_bar_message(print_buf);
	}
	return 0;
}

static int
locate_cmd(const cmd_info_t *cmd_info)
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
ls_cmd(const cmd_info_t *cmd_info)
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
map_cmd(const cmd_info_t *cmd_info)
{
	return map_or_remap(cmd_info, 0);
}

static int
mark_cmd(const cmd_info_t *cmd_info)
{
	int result;
	char *tmp;
	char mark = cmd_info->argv[0][0];

	if(cmd_info->argv[0][1] != '\0')
		return CMDS_ERR_TRAILING_CHARS;

	if(cmd_info->qmark)
	{
		int index = mark2index(mark);
		if(!is_bookmark_empty(index))
		{
			status_bar_errorf("Mark isn't empty: %c", mark);
			return 1;
		}
	}

	if(cmd_info->argc == 1)
	{
		if(cmd_info->end == NOT_DEF)
			return add_bookmark(mark, curr_view->curr_dir,
					curr_view->dir_entry[curr_view->list_pos].name);
		else
			return add_bookmark(mark, curr_view->curr_dir,
					curr_view->dir_entry[cmd_info->end].name);
	}

	tmp = expand_tilde(strdup(cmd_info->argv[1]));
	if(!is_path_absolute(tmp))
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
marks_cmd(const cmd_info_t *cmd_info)
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
messages_cmd(const cmd_info_t *cmd_info)
{
	char *lines;
	size_t len;
	int count;
	int t;

	lines = NULL;
	len = 0;
	count = curr_stats.msg_tail - curr_stats.msg_head;
	if(count < 0)
		count += ARRAY_LEN(curr_stats.msgs);
	t = (curr_stats.msg_head + 1) % ARRAY_LEN(curr_stats.msgs);
	while(count-- > 0)
	{
		const char *msg = curr_stats.msgs[t];
		lines = realloc(lines, len + 1 + strlen(msg) + 1);
		len += sprintf(lines + len, "%s%s", (len == 0) ? "": "\n", msg);
		t = (t + 1) % ARRAY_LEN(curr_stats.msgs);
	}

	if(lines == NULL)
		return 0;

	curr_stats.save_msg_in_list = 0;
	status_bar_message(lines);
	curr_stats.save_msg_in_list = 1;

	free(lines);
	return 1;
}

static int
mkdir_cmd(const cmd_info_t *cmd_info)
{
	make_dirs(curr_view, cmd_info->argv, cmd_info->argc, cmd_info->emark);
	return 1;
}

static int
move_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->qmark)
	{
		if(cmd_info->argc > 0)
		{
			status_bar_error("No arguments are allowed if you use \"!\"");
			return 1;
		}
		if(cmd_info->bg)
			return cpmv_files_bg(curr_view, NULL, -1, 1, cmd_info->emark) != 0;
		else
			return cpmv_files(curr_view, NULL, -1, 1, 0, 0) != 0;
	}
	else if(cmd_info->bg)
	{
		return cpmv_files_bg(curr_view, cmd_info->argv, cmd_info->argc, 1,
				cmd_info->emark) != 0;
	}
	else
	{
		return cpmv_files(curr_view, cmd_info->argv, cmd_info->argc, 1, 0,
				cmd_info->emark) != 0;
	}
}

static int
nmap_cmd(const cmd_info_t *cmd_info)
{
	return do_map(cmd_info, "Normal", "nmap", NORMAL_MODE, 0) != 0;
}

static int
nnoremap_cmd(const cmd_info_t *cmd_info)
{
	return do_map(cmd_info, "Normal", "nmap", NORMAL_MODE, 1) != 0;
}

static int
nohlsearch_cmd(const cmd_info_t *cmd_info)
{
	if(curr_view->selected_files == 0)
		return 0;

	clean_selected_files(curr_view);
	draw_dir_list(curr_view, curr_view->top_line);
	move_to_list_pos(curr_view, curr_view->list_pos);
	return 0;
}

static int
noremap_cmd(const cmd_info_t *cmd_info)
{
	return map_or_remap(cmd_info, 1);
}

static int
map_or_remap(const cmd_info_t *cmd_info, int no_remap)
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
nunmap_cmd(const cmd_info_t *cmd_info)
{
	return do_unmap(cmd_info->argv[0], NORMAL_MODE);
}

static int
only_cmd(const cmd_info_t *cmd_info)
{
	comm_only();
	return 0;
}

static int
popd_cmd(const cmd_info_t *cmd_info)
{
	if(popd() != 0)
	{
		status_bar_message("Directory stack empty");
		return 1;
	}
	return 0;
}

static int
pushd_cmd(const cmd_info_t *cmd_info)
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
pwd_cmd(const cmd_info_t *cmd_info)
{
	status_bar_message(curr_view->curr_dir);
	return 1;
}

static int
registers_cmd(const cmd_info_t *cmd_info)
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
rename_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->emark && cmd_info->argc != 0)
	{
		status_bar_error("No arguments are allowed if you use \"!\"");
		return 1;
	}

	if(cmd_info->emark)
		return rename_files(curr_view, NULL, 0, 1) != 0;
	else if(cmd_info->argc == 0)
		return rename_files(curr_view, NULL, 0, 0) != 0;
	else
		return rename_files(curr_view, cmd_info->argv, cmd_info->argc, 0) != 0;
}

static int
restart_cmd(const cmd_info_t *cmd_info)
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
	curr_stats.dirsize_cache = tree_create(0, 0);

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
	load_def_scheme();

	/* bookmarks */
	p = valid_bookmarks;
	while(*p != '\0')
	{
		int index = mark2index(*p++);
		if(!is_bookmark_empty(index))
			remove_bookmark(index);
	}

	/* variables */
	clear_variables();

	init_variables(&print_func);
	load_default_configuration();
	read_info_file(1);
	save_view_history(&lwin, NULL, NULL, -1);
	save_view_history(&rwin, NULL, NULL, -1);
	load_color_scheme_colors();
	exec_config();
	exec_startup_commands(0, NULL);
	return 0;
}

static int
restore_cmd(const cmd_info_t *cmd_info)
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
		char buf[NAME_MAX];
		if(!curr_view->dir_entry[i].selected)
			continue;

		snprintf(buf, sizeof(buf), "%s", curr_view->dir_entry[i].name);
		chosp(buf);
		if(restore_from_trash(buf) == 0)
			m++;
	}

	load_saving_pos(curr_view, 1);
	status_bar_messagef("Restored %d of %d", m, n);
	return 1;
}

static int
rlink_cmd(const cmd_info_t *cmd_info)
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
screen_cmd(const cmd_info_t *cmd_info)
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
set_cmd(const cmd_info_t *cmd_info)
{
	int result = process_set_args(cmd_info->args);
	return (result < 0) ? CMDS_ERR_CUSTOM : (result != 0);
}

static int
shell_cmd(const cmd_info_t *cmd_info)
{
	const char *sh = env_get("SHELL");
	if(sh == NULL || sh[0] == '\0')
		sh = cfg.shell;
	shellout(sh, 0, 1);
	return 0;
}

static int
sort_cmd(const cmd_info_t *cmd_info)
{
	enter_sort_mode(curr_view);
	need_clean_selection = 0;
	return 0;
}

static int
source_cmd(const cmd_info_t *cmd_info)
{
	int ret = 0;
	char *path = expand_tilde(strdup(cmd_info->argv[0]));
	if(access(path, F_OK) != 0)
	{
		status_bar_errorf("File doesn't exist: %s", cmd_info->argv[0]);
		ret = 1;
	}
	if(access(path, R_OK) != 0)
	{
		status_bar_errorf("File isn't readable: %s", cmd_info->argv[0]);
		ret = 1;
	}
	if(source_file(path) != 0)
	{
		status_bar_errorf("Can't source file: %s", cmd_info->argv[0]);
		ret = 1;
	}
	free(path);
	return ret;
}

static int
split_cmd(const cmd_info_t *cmd_info)
{
	return do_split(cmd_info, 0);
}

static int
substitute_cmd(const cmd_info_t *cmd_info)
{
	static char *last_pattern;
	static char *last_sub;
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

	if(cmd_info->argc >= 1 && cmd_info->argv[0][0] != '\0')
	{
		free(last_pattern);
		last_pattern = strdup(cmd_info->argv[0]);
	}

	if(cmd_info->argc >= 2)
	{
		free(last_sub);
		last_sub = strdup(cmd_info->argv[1]);
	}
	else if(cmd_info->argc == 1)
	{
		free(last_sub);
		last_sub = strdup("");
	}

	if(last_pattern == NULL)
	{
		status_bar_error("No previous pattern");
		return 1;
	}

	return substitute_in_names(curr_view, last_pattern, last_sub, ic, glob) != 0;
}

static int
sync_cmd(const cmd_info_t *cmd_info)
{
	char buf[PATH_MAX];

	snprintf(buf, sizeof(buf), "%s/", curr_view->curr_dir);
	if(cmd_info->argc > 0)
		strcat(buf, cmd_info->argv[0]);

	if(change_directory(other_view, buf) >= 0)
	{
		load_dir_list(other_view, 0);
		wrefresh(other_view->win);
	}
	return 0;
}

static int
touch_cmd(const cmd_info_t *cmd_info)
{
	return make_files(curr_view, cmd_info->argv, cmd_info->argc) != 0;
}

static int
tr_cmd(const cmd_info_t *cmd_info)
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
undolist_cmd(const cmd_info_t *cmd_info)
{
	return show_undolist_menu(curr_view, cmd_info->emark) != 0;
}

static int
unmap_cmd(const cmd_info_t *cmd_info)
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
unlet_cmd(const cmd_info_t *cmd_info)
{
	print_buf[0] = '\0';
	if(unlet_variables(cmd_info->args) != 0 && !cmd_info->emark)
	{
		status_bar_error(print_buf);
		return 1;
	}
	return 0;
}

static int
view_cmd(const cmd_info_t *cmd_info)
{
	if(curr_stats.number_of_windows == 1)
	{
		status_bar_error("Cannot view files in one window mode");
		return 1;
	}
	if(other_view->explore_mode)
	{
		status_bar_error("Other view already is used for file viewing");
		return 1;
	}
	if(curr_stats.view && cmd_info->emark)
		return 0;
	toggle_quick_view();
	return 0;
}

static int
vifm_cmd(const cmd_info_t *cmd_info)
{
	return show_vifm_menu(curr_view) != 0;
}

static int
vmap_cmd(const cmd_info_t *cmd_info)
{
	return do_map(cmd_info, "Visual", "vmap", VISUAL_MODE, 0) != 0;
}

static int
vnoremap_cmd(const cmd_info_t *cmd_info)
{
	return do_map(cmd_info, "Visual", "vmap", VISUAL_MODE, 1) != 0;
}

static int
do_map(const cmd_info_t *cmd_info, const char *map_type,
		const char *map_cmd, int mode, int no_remap)
{
	wchar_t *keys, *mapping;
	char *raw_rhs, *rhs;
	char t;
	int result;

	if(cmd_info->argc <= 1)
	{
		keys = substitute_specs(cmd_info->args);
		show_map_menu(curr_view, map_type, list_cmds(mode), keys);
		free(keys);
		return 0;
	}

	raw_rhs = skip_non_whitespace(cmd_info->args);
	t = *raw_rhs;
	*raw_rhs = '\0';

	rhs = skip_whitespace(raw_rhs + 1);
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
volumes_cmd(const cmd_info_t *cmd_info)
{
	show_volumes_menu(curr_view);
	return 0;
}
#endif

static int
vsplit_cmd(const cmd_info_t *cmd_info)
{
	return do_split(cmd_info, 1);
}

static int
do_split(const cmd_info_t *cmd_info, int vertical)
{
	if(cmd_info->emark && cmd_info->argc != 0)
	{
		status_bar_error("No arguments are allowed if you use \"!\"");
		return 1;
	}

	if(cmd_info->emark)
	{
		if(curr_stats.number_of_windows == 1)
			comm_split(vertical);
		else
			comm_only();
	}
	else
	{
		if(cmd_info->argc == 1)
			cd(other_view, cmd_info->argv[0]);
		comm_split(vertical);
	}
	return 0;
}

static int
vunmap_cmd(const cmd_info_t *cmd_info)
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
windo_cmd(const cmd_info_t *cmd_info)
{
	int result = 0;

	if(cmd_info->argc == 0)
		return 0;

	result += winrun(&lwin, cmd_info->args) != 0;
	result += winrun(&rwin, cmd_info->args) != 0;

	redraw_window();

	return result;
}

static int
winrun_cmd(const cmd_info_t *cmd_info)
{
	int result = 0;
	char *cmd;

	if(cmd_info->argc == 0)
		return 0;

	if(cmd_info->argv[0][1] != '\0' ||
			strchr("^$%.,", cmd_info->argv[0][0]) == NULL)
		return CMDS_ERR_INVALID_ARG;

	if(cmd_info->argc == 1)
		return 0;

	cmd = cmd_info->args + 2;
	switch(cmd_info->argv[0][0])
	{
		case '^':
			result += winrun(&lwin, cmd) != 0;
			break;
		case '$':
			result += winrun(&rwin, cmd) != 0;
			break;
		case '%':
			result += winrun(&lwin, cmd) != 0;
			result += winrun(&rwin, cmd) != 0;
			break;
		case '.':
			result += winrun(curr_view, cmd) != 0;
			break;
		case ',':
			result += winrun(other_view, cmd) != 0;
			break;
	}

	redraw_window();

	return result;
}

static int
winrun(FileView *view, char *cmd)
{
	int result;
	FileView *tmp_curr = curr_view;
	FileView *tmp_other = other_view;

	curr_view = view;
	other_view = (view == tmp_curr) ? tmp_other : tmp_curr;

	load_local_options(curr_view);
	result = exec_commands(cmd, curr_view, 0, GET_COMMAND);

	curr_view = tmp_curr;
	other_view = tmp_other;
	return result;
}

static int
write_cmd(const cmd_info_t *cmd_info)
{
	write_info_file();
	return 0;
}

static int
quit_cmd(const cmd_info_t *cmd_info)
{
	comm_quit(!cmd_info->emark, cmd_info->emark);
	return 0;
}

static int
wq_cmd(const cmd_info_t *cmd_info)
{
	comm_quit(1, cmd_info->emark);
	return 0;
}

static int
yank_cmd(const cmd_info_t *cmd_info)
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
get_reg_and_count(const cmd_info_t *cmd_info, int *reg)
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
usercmd_cmd(const cmd_info_t *cmd_info)
{
	char *expanded_com = NULL;
	int use_menu = 0;
	int split = 0;
	size_t len;
	int external = 1;
	int bg = 0;

	if(strchr(cmd_info->cmd, '%') != NULL)
		expanded_com = expand_macros(curr_view, cmd_info->cmd, cmd_info->args,
				&use_menu, &split);
	else
		expanded_com = strdup(cmd_info->cmd);

	len = strlen(expanded_com);
	while(len > 1 && isspace(expanded_com[len - 1]))
		expanded_com[--len] = '\0';

	if(len > 1)
		bg = expanded_com[len - 1] == '&' && expanded_com[len - 2] == ' ';

	clean_selected_files(curr_view);

	if(expanded_com[0] == ':')
	{
		int sm = exec_commands(expanded_com, curr_view, 0, GET_COMMAND);
		free(expanded_com);
		return sm;
	}
	else if(use_menu == 3)
	{
		output_to_statusbar(expanded_com);
		free(expanded_com);
		return 1;
	}
	else if(use_menu)
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
	else if(expanded_com[0] == '!')
	{
		char *tmp = expanded_com;
		int pause = 0;
		tmp++;
		if(*tmp == '!')
		{
			pause = 1;
			tmp++;
		}
		tmp = skip_whitespace(tmp);

		if(*tmp != '\0' && bg)
		{
			expanded_com[len - 2] = '\0';
			start_background_job(tmp);
		}
		else if(strlen(tmp) > 0)
		{
			shellout(tmp, pause ? 1 : -1, 1);
		}
	}
	else if(expanded_com[0] == '/')
	{
		exec_command(expanded_com + 1, curr_view, GET_FSEARCH_PATTERN);
		external = 0;
		need_clean_selection = 0;
	}
	else if(bg)
	{
		expanded_com[len - 2] = '\0';
		start_background_job(expanded_com);
	}
	else
	{
		shellout(expanded_com, -1, 1);
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
