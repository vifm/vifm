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

#include "commands.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <regex.h>

#include <curses.h>

#include <sys/types.h> /* passwd */
#ifndef _WIN32
#include <sys/wait.h>
#endif

#include <assert.h> /* assert() */
#include <ctype.h> /* isspace() */
#include <errno.h> /* errno */
#include <signal.h>
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* EXIT_SUCCESS system() realloc() free() */
#include <string.h> /* strcat() strchr() strcmp() strcpy() strncpy() strlen() */
#include <time.h>

#include "cfg/config.h"
#include "cfg/info.h"
#include "engine/cmds.h"
#include "engine/completion.h"
#include "engine/keys.h"
#include "engine/options.h"
#include "engine/parsing.h"
#include "engine/text_buffer.h"
#include "engine/var.h"
#include "engine/variables.h"
#include "menus/all.h"
#include "menus/menus.h"
#include "modes/dialogs/attr_dialog.h"
#include "modes/dialogs/change_dialog.h"
#include "modes/dialogs/sort_dialog.h"
#include "modes/cmdline.h"
#include "modes/menu.h"
#include "modes/modes.h"
#include "modes/normal.h"
#include "modes/view.h"
#include "modes/visual.h"
#include "utils/env.h"
#include "utils/file_streams.h"
#include "utils/fs.h"
#include "utils/fs_limits.h"
#include "utils/int_stack.h"
#include "utils/log.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/test_helpers.h"
#include "utils/utils.h"
#include "background.h"
#include "bookmarks.h"
#include "bracket_notation.h"
#include "color_scheme.h"
#include "commands_completion.h"
#include "dir_stack.h"
#include "file_magic.h"
#include "filelist.h"
#include "fileops.h"
#include "filetype.h"
#include "fuse.h"
#include "macros.h"
#include "opt_handlers.h"
#include "path_env.h"
#include "quickview.h"
#include "registers.h"
#include "running.h"
#include "status.h"
#include "term_title.h"
#include "trash.h"
#include "ui.h"
#include "undo.h"

/* Commands without completion. */
enum
{
	COM_FILTER = NO_COMPLETION_BOUNDARY,
	COM_SUBSTITUTE,
	COM_TR,
	COM_IF_STMT,
	COM_CMAP,
	COM_CNOREMAP,
	COM_COMMAND,
	COM_FILETYPE,
	COM_FILEVIEWER,
	COM_FILEXTYPE,
	COM_MAP,
	COM_MMAP,
	COM_MNOREMAP,
	COM_NMAP,
	COM_NNOREMAP,
	COM_NORMAL,
	COM_QMAP,
	COM_QNOREMAP,
	COM_VMAP,
	COM_VNOREMAP,
	COM_NOREMAP,
};

static int swap_range(void);
static int resolve_mark(char mark);
static char * cmds_expand_macros(const char *str, int for_shell, int *usr1,
		int *usr2);
static int setup_extcmd_file(const char path[], const char beginning[],
		int type);
static void prepare_extcmd_file(FILE *fp, const char beginning[], int type);
static char * get_file_first_line(const char path[]);
static void execute_extcmd(const char command[], int type);
static void save_extcmd(const char command[], int type);
static void post(int id);
TSTATIC void select_range(int id, const cmd_info_t *cmd_info);
static int skip_at_beginning(int id, const char *args);
static int is_whole_line_command(const char cmd[]);
static char * skip_command_beginning(const char cmd[]);

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
static int echo_cmd(const cmd_info_t *cmd_info);
static int edit_cmd(const cmd_info_t *cmd_info);
static int else_cmd(const cmd_info_t *cmd_info);
static int empty_cmd(const cmd_info_t *cmd_info);
static int endif_cmd(const cmd_info_t *cmd_info);
static int exe_cmd(const cmd_info_t *cmd_info);
static char * try_eval_arglist(const cmd_info_t *cmd_info);
TSTATIC char * eval_arglist(const char args[], const char **stop_ptr);
static int file_cmd(const cmd_info_t *cmd_info);
static int filetype_cmd(const cmd_info_t *cmd_info);
static int filextype_cmd(const cmd_info_t *cmd_info);
static int add_filetype(const cmd_info_t *cmd_info, int x);
static int fileviewer_cmd(const cmd_info_t *cmd_info);
static int filter_cmd(const cmd_info_t *cmd_info);
static int set_view_filter(FileView *view, const char filter[], int invert);
static const char * get_filter_value(const char filter[]);
static const char * try_compile_regex(const char regex[], int cflags);
static int get_filter_inversion_state(const cmd_info_t *cmd_info);
static int find_cmd(const cmd_info_t *cmd_info);
static int finish_cmd(const cmd_info_t *cmd_info);
static int grep_cmd(const cmd_info_t *cmd_info);
static int help_cmd(const cmd_info_t *cmd_info);
static int highlight_cmd(const cmd_info_t *cmd_info);
static const char *get_group_str(int group, col_attr_t col);
static int get_color(const char str[], int fg, int *attr);
static int get_attrs(const char *text);
static int history_cmd(const cmd_info_t *cmd_info);
static int if_cmd(const cmd_info_t *cmd_info);
static int invert_cmd(const cmd_info_t *cmd_info);
static void print_inversion_state(char state_type);
static void invert_state(char state_type);
static int jobs_cmd(const cmd_info_t *cmd_info);
static int let_cmd(const cmd_info_t *cmd_info);
static int locate_cmd(const cmd_info_t *cmd_info);
static int ls_cmd(const cmd_info_t *cmd_info);
static int map_cmd(const cmd_info_t *cmd_info);
static int mark_cmd(const cmd_info_t *cmd_info);
static int marks_cmd(const cmd_info_t *cmd_info);
static int messages_cmd(const cmd_info_t *cmd_info);
static int mkdir_cmd(const cmd_info_t *cmd_info);
static int mmap_cmd(const cmd_info_t *cmd_info);
static int mnoremap_cmd(const cmd_info_t *cmd_info);
static int move_cmd(const cmd_info_t *cmd_info);
static int cpmv_cmd(const cmd_info_t *cmd_info, int move);
static int munmap_cmd(const cmd_info_t *cmd_info);
static int nmap_cmd(const cmd_info_t *cmd_info);
static int nnoremap_cmd(const cmd_info_t *cmd_info);
static int nohlsearch_cmd(const cmd_info_t *cmd_info);
static int noremap_cmd(const cmd_info_t *cmd_info);
static int map_or_remap(const cmd_info_t *cmd_info, int no_remap);
static int normal_cmd(const cmd_info_t *cmd_info);
static int nunmap_cmd(const cmd_info_t *cmd_info);
static int only_cmd(const cmd_info_t *cmd_info);
static int popd_cmd(const cmd_info_t *cmd_info);
static int pushd_cmd(const cmd_info_t *cmd_info);
static int pwd_cmd(const cmd_info_t *cmd_info);
static int qmap_cmd(const cmd_info_t *cmd_info);
static int qnoremap_cmd(const cmd_info_t *cmd_info);
static int qunmap_cmd(const cmd_info_t *cmd_info);
static int registers_cmd(const cmd_info_t *cmd_info);
static int rename_cmd(const cmd_info_t *cmd_info);
static int restart_cmd(const cmd_info_t *cmd_info);
static int restore_cmd(const cmd_info_t *cmd_info);
static int rlink_cmd(const cmd_info_t *cmd_info);
static int link_cmd(const cmd_info_t *cmd_info, int type);
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
static int do_split(const cmd_info_t *cmd_info, SPLIT orientation);
static int do_map(const cmd_info_t *cmd_info, const char map_type[], int mode,
		int no_remap);
static int vunmap_cmd(const cmd_info_t *cmd_info);
static int do_unmap(const char *keys, int mode);
static int windo_cmd(const cmd_info_t *cmd_info);
static int winrun_cmd(const cmd_info_t *cmd_info);
static int winrun(FileView *view, const char cmd[]);
static int write_cmd(const cmd_info_t *cmd_info);
static int quit_cmd(const cmd_info_t *cmd_info);
static int wq_cmd(const cmd_info_t *cmd_info);
static int yank_cmd(const cmd_info_t *cmd_info);
static int get_reg_and_count(const cmd_info_t *cmd_info, int *reg);
static int usercmd_cmd(const cmd_info_t* cmd_info);
static void output_to_statusbar(const char *cmd);
static void run_in_split(const FileView *view, const char cmd[]);

static const cmd_add_t commands[] = {
	{ .name = "",                 .abbr = NULL,    .emark = 0,  .id = COM_GOTO,        .range = 1,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = goto_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "!",                .abbr = NULL,    .emark = 1,  .id = COM_EXECUTE,     .range = 1,    .bg = 1, .quote = 0, .regexp = 0,
		.handler = emark_cmd,       .qmark = 0,      .expand = 5, .cust_sep = 0,         .min_args = 1, .max_args = NOT_DEF, .select = 1, },
	{ .name = "alink",            .abbr = NULL,    .emark = 1,  .id = COM_ALINK,       .range = 1,    .bg = 0, .quote = 1, .regexp = 0,
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
	{ .name = "clone",            .abbr = NULL,    .emark = 1,  .id = COM_CLONE,       .range = 1,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = clone_cmd,       .qmark = 1,      .expand = 1, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 1, },
	{ .name = "cmap",             .abbr = "cm",    .emark = 0,  .id = COM_CMAP,        .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = cmap_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "cnoremap",         .abbr = "cno",   .emark = 0,  .id = COM_CNOREMAP,    .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = cnoremap_cmd,    .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "colorscheme",      .abbr = "colo",  .emark = 0,  .id = COM_COLORSCHEME, .range = 0,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = colorscheme_cmd, .qmark = 1,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 2,       .select = 0, },
  { .name = "command",          .abbr = "com",   .emark = 1,  .id = COM_COMMAND,     .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
    .handler = command_cmd,     .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "copy",             .abbr = "co",    .emark = 1,  .id = COM_COPY,        .range = 1,    .bg = 1, .quote = 1, .regexp = 0,
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
	{ .name = "echo",             .abbr = "ec",    .emark = 0,  .id = COM_ECHO,        .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = echo_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "edit",             .abbr = "e",     .emark = 0,  .id = COM_EDIT,        .range = 1,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = edit_cmd,        .qmark = 0,      .expand = 1, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 1, },
	{ .name = "else",             .abbr = "el",    .emark = 0,  .id = COM_IF_STMT,     .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = else_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "empty",            .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = empty_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "endif",            .abbr = "en",    .emark = 0,  .id = COM_IF_STMT,     .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = endif_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "execute",          .abbr = "exe",   .emark = 0,  .id = COM_EXE,         .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = exe_cmd,         .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "exit",             .abbr = "exi",   .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = quit_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "file",             .abbr = "f",     .emark = 0,  .id = COM_FILE,        .range = 0,    .bg = 1, .quote = 0, .regexp = 0,
		.handler = file_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "filetype",         .abbr = "filet", .emark = 0,  .id = COM_FILETYPE,    .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = filetype_cmd,    .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 2, .max_args = NOT_DEF, .select = 0, },
	{ .name = "fileviewer",       .abbr = "filev", .emark = 0,  .id = COM_FILEVIEWER,  .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = fileviewer_cmd,  .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 2, .max_args = NOT_DEF, .select = 0, },
	{ .name = "filextype",        .abbr = "filex", .emark = 0,  .id = COM_FILEXTYPE,   .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = filextype_cmd,   .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 2, .max_args = NOT_DEF, .select = 0, },
	{ .name = "filter",           .abbr = NULL,    .emark = 1,  .id = COM_FILTER,      .range = 0,    .bg = 0, .quote = 1, .regexp = 1,
		.handler = filter_cmd,      .qmark = 1,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 1,       .select = 0, },
	{ .name = "find",             .abbr = "fin",   .emark = 0,  .id = COM_FIND,        .range = 1,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = find_cmd,        .qmark = 0,      .expand = 1, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 1, },
	{ .name = "finish",           .abbr = "fini",  .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = finish_cmd,      .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "grep",             .abbr = "gr",    .emark = 1,  .id = COM_GREP,        .range = 1,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = grep_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 1, },
	{ .name = "help",             .abbr = "h",     .emark = 0,  .id = COM_HELP,        .range = 0,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = help_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 1,       .select = 0, },
	{ .name = "highlight",        .abbr = "hi",    .emark = 0,  .id = COM_HIGHLIGHT,   .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = highlight_cmd,   .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 4,       .select = 0, },
	{ .name = "history",          .abbr = "his",   .emark = 0,  .id = COM_HISTORY,     .range = 0,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = history_cmd,     .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 1,       .select = 0, },
	{ .name = "if",               .abbr = NULL,    .emark = 0,  .id = COM_IF_STMT,     .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = if_cmd,          .qmark = 1,      .expand = 0, .cust_sep = 0,         .min_args = 1, .max_args = NOT_DEF, .select = 0, },
	{ .name = "invert",           .abbr = NULL,    .emark = 0,  .id = COM_INVERT,      .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = invert_cmd,      .qmark = 2,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 1,       .select = 0, },
	{ .name = "jobs",             .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = jobs_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "let",              .abbr = NULL,    .emark = 0,  .id = COM_LET,         .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = let_cmd,         .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 1, .max_args = NOT_DEF, .select = 0, },
	{ .name = "locate",           .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = locate_cmd,      .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "ls",               .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = ls_cmd,          .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "map",              .abbr = NULL,    .emark = 1,  .id = COM_MAP,         .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = map_cmd,         .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 2, .max_args = NOT_DEF, .select = 0, },
	{ .name = "mark",             .abbr = "ma",    .emark = 0,  .id = -1,              .range = 1,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = mark_cmd,        .qmark = 2,      .expand = 1, .cust_sep = 0,         .min_args = 1, .max_args = 3,       .select = 0, },
	{ .name = "marks",            .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = marks_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "messages",         .abbr = "mes",   .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = messages_cmd,    .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "mkdir",            .abbr = NULL,    .emark = 1,  .id = COM_MKDIR,       .range = 0,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = mkdir_cmd,       .qmark = 0,      .expand = 1, .cust_sep = 0,         .min_args = 1, .max_args = NOT_DEF, .select = 0, },
	{ .name = "mmap",             .abbr = "mm",    .emark = 0,  .id = COM_MMAP,        .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = mmap_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "mnoremap",         .abbr = "mno",   .emark = 0,  .id = COM_MNOREMAP,    .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = mnoremap_cmd,    .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "move",             .abbr = "m",     .emark = 1,  .id = COM_MOVE,        .range = 1,    .bg = 1, .quote = 1, .regexp = 0,
		.handler = move_cmd,        .qmark = 1,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 1, },
	{ .name = "munmap",           .abbr = "mu",    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = munmap_cmd,      .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 1, .max_args = 1,       .select = 0, },
	{ .name = "nmap",             .abbr = "nm",    .emark = 0,  .id = COM_NMAP,        .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = nmap_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "nnoremap",         .abbr = "nn",    .emark = 0,  .id = COM_NNOREMAP,    .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = nnoremap_cmd,    .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "nohlsearch",       .abbr = "noh",   .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = nohlsearch_cmd,  .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "noremap",          .abbr = "no",    .emark = 0,  .id = COM_NOREMAP,     .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = noremap_cmd,     .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "normal",           .abbr = "norm",  .emark = 1,  .id = COM_NORMAL,      .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = normal_cmd,      .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 1, .max_args = NOT_DEF, .select = 0, },
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
	{ .name = "qmap",             .abbr = "qm",    .emark = 0,  .id = COM_QMAP,        .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = qmap_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "qnoremap",         .abbr = "qno",   .emark = 0,  .id = COM_QNOREMAP,    .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = qnoremap_cmd,    .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "quit",             .abbr = "q",     .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = quit_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "qunmap",           .abbr = "qun",   .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = qunmap_cmd,      .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 1, .max_args = 1,       .select = 0, },
	{ .name = "registers",        .abbr = "reg",   .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = registers_cmd,   .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "rename",           .abbr = NULL,    .emark = 1,  .id = COM_RENAME,      .range = 1,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = rename_cmd,      .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 1, },
	{ .name = "restart",          .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = restart_cmd,     .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "restore",          .abbr = NULL,    .emark = 0,  .id = -1,              .range = 1,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = restore_cmd,     .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 1, },
	{ .name = "rlink",            .abbr = NULL,    .emark = 1,  .id = COM_RLINK,       .range = 1,    .bg = 0, .quote = 1, .regexp = 0,
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
	{ .name = "split",            .abbr = "sp",    .emark = 1,  .id = COM_SPLIT,       .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = split_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 1,       .select = 0, },
	{ .name = "substitute",       .abbr = "s",     .emark = 0,  .id = COM_SUBSTITUTE,  .range = 1,    .bg = 0, .quote = 0, .regexp = 1,
		.handler = substitute_cmd,  .qmark = 0,      .expand = 0, .cust_sep = 1,         .min_args = 0, .max_args = 3,       .select = 1, },
	{ .name = "sync",             .abbr = NULL,    .emark = 1,  .id = COM_SYNC,        .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = sync_cmd,        .qmark = 0,      .expand = 1, .cust_sep = 0,         .min_args = 0, .max_args = 1,       .select = 0, },
	{ .name = "touch",            .abbr = NULL,    .emark = 0,  .id = COM_TOUCH,       .range = 0,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = touch_cmd,       .qmark = 0,      .expand = 1, .cust_sep = 0,         .min_args = 1, .max_args = NOT_DEF, .select = 0, },
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
	{ .name = "vmap",             .abbr = "vm",    .emark = 0,  .id = COM_VMAP,        .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = vmap_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "vnoremap",         .abbr = "vn",    .emark = 0,  .id = COM_VNOREMAP,    .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = vnoremap_cmd,    .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
#ifdef _WIN32
	{ .name = "volumes",          .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = volumes_cmd,     .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
#endif
	{ .name = "vsplit",           .abbr = "vs",    .emark = 1,  .id = COM_VSPLIT,      .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
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
		.handler = usercmd_cmd,     .qmark = 0,      .expand = 1, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 1, },
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
/* Stores condition evaluation result for all nesting if-endif statements. */
static int_stack_t if_levels;

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
			(void)exec_commands(argv[x + 1], curr_view, GET_COMMAND);
			x++;
		}
		else if(argv[x][0] == '+')
		{
			(void)exec_commands(argv[x] + 1, curr_view, GET_COMMAND);
		}
	}
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
cmds_expand_macros(const char *str, int for_shell, int *usr1, int *usr2)
{
	char *result;
	MacroFlags flags = MACRO_NONE;

	result = expand_macros(str, NULL, &flags, for_shell);

	*usr1 = flags;

	return result;
}

char *
cmds_expand_envvars(const char str[])
{
	return expand_envvars(str, 1);
}

void
get_and_execute_command(const char line[], size_t line_pos, int type)
{
	char *const cmd = get_ext_command(line, line_pos, type);
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
get_ext_command(const char beginning[], size_t line_pos, int type)
{
	char cmd_file[PATH_MAX];
	char *cmd = NULL;

	generate_tmp_file_name("vifm.cmdline", cmd_file, sizeof(cmd_file));

	if(setup_extcmd_file(cmd_file, beginning, type) == 0)
	{
		if(view_file(cmd_file, 1, line_pos, 0) == 0)
		{
			cmd = get_file_first_line(cmd_file);
		}
	}
	else
	{
		show_error_msgf("Error Creating Temporary File",
				"Could not create file %s: %s", cmd_file, strerror(errno));
	}

	unlink(cmd_file);
	return cmd;
}

/* Create and fill file for external command prompt.  Returns zero on success,
 * otherwise non-zero is returned and errno contains valid value. */
static int
setup_extcmd_file(const char path[], const char beginning[], int type)
{
	FILE *const fp = fopen(path, "wt");
	if(fp == NULL)
	{
		return 1;
	}
	prepare_extcmd_file(fp, beginning, type);
	fclose(fp);
	return 0;
}

/* Fills the file with history (more recent goes first). */
static void
prepare_extcmd_file(FILE *fp, const char beginning[], int type)
{
	const int is_cmd = (type == GET_COMMAND);
	const int is_prompt = (type == GET_PROMPT_INPUT);
	const hist_t *const hist = is_cmd ? &cfg.cmd_hist :
		(is_prompt ? &cfg.prompt_hist : &cfg.search_hist);
	int i;

	fprintf(fp, "%s\n", beginning);
	for(i = 0; i <= hist->pos; i++)
	{
		fprintf(fp, "%s\n", hist->items[i]);
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

/* Reads the first line of the file specified by the path.  Returns NULL on
 * error or an empty file, otherwise a newly allocated string, which should be
 * freed by the caller, is returned. */
static char *
get_file_first_line(const char path[])
{
	FILE *const fp = fopen(path, "rb");
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
execute_extcmd(const char command[], int type)
{
	if(type == GET_COMMAND)
	{
		curr_stats.save_msg = exec_commands(command, curr_view, type);
	}
	else
	{
		curr_stats.save_msg = exec_command(command, curr_view, type);
	}
}

/* Saves the command to the appropriate history. */
static void
save_extcmd(const char command[], int type)
{
	if(type == GET_COMMAND)
	{
		save_command_history(command);
	}
	else
	{
		save_search_history(command);
	}
}

int
is_history_command(const char command[])
{
	/* Don't add :!! or :! to history list. */
	return strcmp(command, "!!") != 0 && strcmp(command, "!") != 0;
}

static void
post(int id)
{
	if(id != COM_GOTO && curr_view->selected_files > 0 && need_clean_selection)
	{
		clean_selected_files(curr_view);
		load_saving_pos(curr_view, 1);
	}
}

TSTATIC void
select_range(int id, const cmd_info_t *cmd_info)
{
	/* TODO: refactor this function select_range() */

	int x;
	int y = 0;

	/* Both a starting range and an ending range are given. */
	if(cmd_info->begin > -1)
	{
		clean_selected_files(curr_view);

		for(x = cmd_info->begin; x <= cmd_info->end; x++)
		{
			if(is_parent_dir(curr_view->dir_entry[x].name) &&
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
		if(!is_parent_dir(curr_view->dir_entry[pos].name))
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

void
init_commands(void)
{
	if(cmds_conf.inner != NULL)
	{
		init_cmds(1, &cmds_conf);
		return;
	}

	/* we get here when init_commands() is called first time */
	init_cmds(1, &cmds_conf);
	add_builtin_commands((const cmd_add_t *)&commands, ARRAY_LEN(commands));

	init_bracket_notation();
	init_variables();
}

static void
remove_selection(FileView *view)
{
	/* TODO: maybe move this to filelist.c */
	if(view->selected_files == 0)
		return;

	clean_selected_files(view);
	redraw_view(view);
}

/* Returns negative value in case of error */
static int
execute_command(FileView *view, const char command[], int menu)
{
	int id;
	int result;

	if(command == NULL)
	{
		remove_selection(view);
		return 0;
	}

	command = skip_command_beginning(command);

	if(command[0] == '"')
		return 0;

	if(command[0] == '\0' && !menu)
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

	if(!int_stack_is_empty(&if_levels) && !int_stack_get_top(&if_levels) &&
			id != COM_IF_STMT)
	{
		return 0;
	}

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
TSTATIC int
line_pos(const char begin[], const char end[], char sep, int rquoting)
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

int
exec_commands(const char cmd[], FileView *view, int type)
{
	char cmd_copy[strlen(cmd) + 1];
	int save_msg = 0;
	char *p, *q;

	if(*cmd == '\0')
	{
		return exec_command(cmd, view, type);
	}

	strcpy(cmd_copy, cmd);
	cmd = cmd_copy;

	p = cmd_copy;
	q = cmd_copy;
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
			{
				p++;
			}
			else
			{
				*q = '\0';
			}

			cmd = skip_command_beginning(cmd);

			/* For non-menu commands set command-line mode configuration before
			 * calling is_whole_line_command() function, which calls functions of the
			 * cmds module of the engine that use context. */
			if(type != GET_MENU_COMMAND)
			{
				init_cmds(1, &cmds_conf);
			}

			if(is_whole_line_command(cmd))
			{
				save_msg += exec_command(cmd, view, type) != 0;
				break;
			}

			*q = '\0';
			q = p;

			ret = exec_command(cmd, view, type);
			if(ret != 0)
			{
				save_msg = (ret < 0) ? -1 : 1;
			}

			cmd = q;
		}
		else
		{
			*q++ = *p++;
		}
	}

	return save_msg;
}

static int
is_whole_line_command(const char cmd[])
{
	const int cmd_id = get_cmd_id(cmd);
	switch(cmd_id)
	{
		case COM_EXECUTE:
		case COM_CMAP:
		case COM_CNOREMAP:
		case COM_COMMAND:
		case COMMAND_CMD_ID:
		case COM_FILETYPE:
		case COM_FILEVIEWER:
		case COM_FILEXTYPE:
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
		case COM_WINDO:
		case COM_WINRUN:
			return 1;

		default:
			return 0;
	}
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
				line_pos(cmd, q, ' ', starts_with_lit(cmd, "fil")) == 0) || *p == '\0')
		{
			if(*p != '\0')
				p++;

			cmd = skip_command_beginning(cmd);
			if(*cmd == '!' || starts_with_lit(cmd, "com"))
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

/* Skips consecutive whitespace or colon characters at the beginning of the
 * command.  Returns pointer to the first non whitespace and color character. */
static char *
skip_command_beginning(const char cmd[])
{
	while(isspace(*cmd) || *cmd == ':')
	{
		cmd++;
	}
	return (char *)cmd;
}

int
exec_command(const char cmd[], FileView *view, int type)
{
	if(cmd == NULL)
	{
		if(type == GET_FSEARCH_PATTERN || type == GET_BSEARCH_PATTERN)
			return find_npattern(view, view->regexp, type == GET_BSEARCH_PATTERN);
		if(type == GET_VFSEARCH_PATTERN || type == GET_VBSEARCH_PATTERN)
			return find_vpattern(view, view->regexp, type == GET_VBSEARCH_PATTERN);
		if(type == GET_COMMAND)
			return execute_command(view, cmd, 0);
		if(type == GET_VWFSEARCH_PATTERN || type == GET_VWBSEARCH_PATTERN)
			return find_vwpattern(cmd, type == GET_VWBSEARCH_PATTERN);
		if(type == GET_FILTER_PATTERN)
		{
			local_filter_apply(view, "");
			return 0;
		}

		assert(0 && "Received command execution request of unexpected type.");
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
		return find_npattern(view, cmd, type == GET_BSEARCH_PATTERN);
	}
	else if(type == GET_VFSEARCH_PATTERN || type == GET_VBSEARCH_PATTERN)
	{
		strncpy(view->regexp, cmd, sizeof(view->regexp));
		return find_vpattern(view, cmd, type == GET_VBSEARCH_PATTERN);
	}
	else if(type == GET_VWFSEARCH_PATTERN || type == GET_VWBSEARCH_PATTERN)
	{
		return find_vwpattern(cmd, type == GET_VWBSEARCH_PATTERN);
	}
	else if(type == GET_FILTER_PATTERN)
	{
		local_filter_apply(view, cmd);
		return 0;
	}

	assert(0 && "Received command execution request of unknown/unexpected type.");
	return 0;
}

int
commands_block_finished(void)
{
	if(!int_stack_is_empty(&if_levels))
	{
		status_bar_error("Missing :endif");
		int_stack_clear(&if_levels);
		return 1;
	}
	return 0;
}

void
comm_quit(int write_info, int force)
{
	/* TODO: move this to some other unit */
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
		if(fp != NULL)
		{
			fclose(fp);
		}
		else
		{
			LOG_SERROR_MSG(errno, "Can't truncate file: \"%s\"", buf);
		}
	}

#ifdef _WIN32
	system("cls");
#endif

	set_term_title(NULL);
	endwin();
	exit(0);
}

/* Return value of all functions below which name ends with "_cmd" mean:
 *  <0 - one of CMDS_* errors from cmds.h
 *  =0 - nothing was outputted to the status bar, don't need to save its state
 *  <0 - someting was outputted to the status bar, need to save its state */
static int
goto_cmd(const cmd_info_t *cmd_info)
{
	move_to_list_pos(curr_view, cmd_info->end);
	return 0;
}

static int
emark_cmd(const cmd_info_t *cmd_info)
{
	/* TODO: Refactor this function emark_cmd(), see usercmd_cmd() */

	int i;
	int save_msg = 0;
	char *com = (char *)cmd_info->args;
	char buf[COMMAND_GROUP_INFO_LEN];
	MacroFlags flags;

	i = skip_whitespace(com) - com;

	if(com[i] == '\0')
		return 0;

	flags = (MacroFlags)cmd_info->usr1;
	if(flags == MACRO_STATUSBAR_OUTPUT)
	{
		output_to_statusbar(com);
		return 1;
	}
	else if(flags == MACRO_IGNORE)
	{
		output_to_nowhere(com);
		return 0;
	}
	else if(flags == MACRO_MENU_OUTPUT || flags == MACRO_MENU_NAV_OUTPUT)
	{
		const int navigate = flags == MACRO_MENU_NAV_OUTPUT;
		save_msg = show_user_menu(curr_view, com, navigate) != 0;
	}
	else if(flags == MACRO_SPLIT && curr_stats.term_multiplexer != TM_NONE)
	{
		run_in_split(curr_view, com);
	}
	else if(cmd_info->bg)
	{
		start_background_job(com + i, 0);
	}
	else
	{
		clean_selected_files(curr_view);
		if(cfg.fast_run)
		{
			char *buf = fast_run_complete(com + i);
			if(buf == NULL)
				return 1;

			(void)shellout(buf, cmd_info->emark ? 1 : -1, 1);
			free(buf);
		}
		else
		{
			(void)shellout(com + i, cmd_info->emark ? 1 : -1, 1);
		}
	}

	snprintf(buf, sizeof(buf), "in %s: !%s",
			replace_home_part(curr_view->curr_dir), cmd_info->raw_args);
	cmd_group_begin(buf);
	add_operation(OP_USR, strdup(com + i), NULL, "", "");
	cmd_group_end();

	return save_msg;
}

/* Creates symbolic links with absolute paths to files. */
static int
alink_cmd(const cmd_info_t *cmd_info)
{
	return link_cmd(cmd_info, 1);
}

static int
apropos_cmd(const cmd_info_t *cmd_info)
{
	static char *last_args;

	if(cmd_info->argc > 0)
	{
		(void)replace_string(&last_args, cmd_info->args);
	}
	else if(last_args == NULL)
	{
		status_bar_error("Nothing to repeat");
		return 1;
	}

	return show_apropos_menu(curr_view, last_args) != 0;
}

static int
cd_cmd(const cmd_info_t *cmd_info)
{
	int result;
	if(!cfg.auto_ch_pos)
	{
		clean_positions_in_history(curr_view);
		curr_stats.ch_pos = 0;
	}
	if(cmd_info->argc == 0)
	{
		result = cd(curr_view, curr_view->curr_dir, cfg.home_dir);
		if(cmd_info->emark)
			result += cd(other_view, other_view->curr_dir, cfg.home_dir);
	}
	else if(cmd_info->argc == 1)
	{
		result = cd(curr_view, curr_view->curr_dir, cmd_info->argv[0]);
		if(cmd_info->emark)
		{
			if(!is_path_absolute(cmd_info->argv[0]) && cmd_info->argv[0][0] != '~' &&
					strcmp(cmd_info->argv[0], "-") != 0)
			{
				char dir[PATH_MAX];
				snprintf(dir, sizeof(dir), "%s/%s", curr_view->curr_dir,
						cmd_info->argv[0]);
				result += cd(other_view, other_view->curr_dir, dir);
			}
			else if(strcmp(cmd_info->argv[0], "-") == 0)
			{
				result += cd(other_view, other_view->curr_dir, curr_view->curr_dir);
			}
			else
			{
				result += cd(other_view, other_view->curr_dir, cmd_info->argv[0]);
			}
			refresh_view_win(other_view);
		}
	}
	else
	{
		result = cd(curr_view, curr_view->curr_dir, cmd_info->argv[0]);
		if(!is_path_absolute(cmd_info->argv[1]) && cmd_info->argv[1][0] != '~')
		{
			char dir[PATH_MAX];
			snprintf(dir, sizeof(dir), "%s/%s", curr_view->curr_dir,
					cmd_info->argv[1]);
			result += cd(other_view, other_view->curr_dir, dir);
		}
		else
		{
			result += cd(other_view, other_view->curr_dir, cmd_info->argv[1]);
		}
		refresh_view_win(other_view);
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
			break;
		}
	}
	regfree(&re);

	if(i < cmd_info->argc)
	{
		status_bar_errorf("Invalid argument: %s", cmd_info->argv[i]);
		return 1;
	}

	files_chmod(curr_view, cmd_info->args, cmd_info->emark);
#endif
	return 0;
}

#ifndef _WIN32
static int
chown_cmd(const cmd_info_t *cmd_info)
{
	char *colon, *user, *group;
	int u, g;
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
	return do_map(cmd_info, "Command Line", CMDLINE_MODE, 0) != 0;
}

static int
cnoremap_cmd(const cmd_info_t *cmd_info)
{
	return do_map(cmd_info, "Command Line", CMDLINE_MODE, 1) != 0;
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
		/* Show menu with colorschemes listed. */
		return show_colorschemes_menu(curr_view) != 0;
	}
	else if(color_scheme_exists(cmd_info->argv[0]))
	{
		if(cmd_info->argc == 2)
		{
			char *directory = expand_tilde(strdup(cmd_info->argv[1]));
			if(!is_path_absolute(directory))
			{
				if(curr_stats.load_stage < 3)
				{
					status_bar_errorf("The path in :colorscheme command cannot be "
							"relative in startup scripts (%s)", directory);
					free(directory);
					return 1;
				}
				else
				{
					char path[PATH_MAX];
					snprintf(path, sizeof(path), "%s/%s", curr_view->curr_dir, directory);
					(void)replace_string(&directory, path);
				}
			}
			if(!is_dir(directory))
			{
				status_bar_errorf("%s isn't a directory", directory);
				free(directory);
				return 1;
			}

			assoc_dir(cmd_info->argv[0], directory);
			free(directory);

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

/* Copies files. */
static int
copy_cmd(const cmd_info_t *cmd_info)
{
	return cpmv_cmd(cmd_info, 0);
}

static int
cunmap_cmd(const cmd_info_t *cmd_info)
{
	return do_unmap(cmd_info->argv[0], CMDLINE_MODE);
}

/* Processes :[range]delete command followed by "{reg} [{count}]" or
 * "{reg}|{count}" with optional " &". */
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
			const int index = mark2index(*p++);
			if(!is_bookmark_empty(index))
			{
				remove_bookmark(index);
			}
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
			if(!char_is_one_of(valid_bookmarks, cmd_info->argv[i][j]))
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

/* Evaluates arguments as expression and outputs result to statusbar. */
static int
echo_cmd(const cmd_info_t *cmd_info)
{
	char *const eval_result = try_eval_arglist(cmd_info);
	status_bar_message(eval_result);
	free(eval_result);
	return 1;
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
			start_background_job(buf, 0);
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
		(void)view_file(buf, -1, -1, 1);
	}
	else
	{
		int i;

		for(i = 0; i < curr_view->list_rows; i++)
		{
			struct stat st;
			if(curr_view->dir_entry[i].selected == 0)
				continue;
			if(lstat(curr_view->dir_entry[i].name, &st) == 0 &&
					!path_exists(curr_view->dir_entry[i].name))
			{
				show_error_msgf("Access error",
						"Can't access destination of link \"%s\". It might be broken.",
						curr_view->dir_entry[i].name);
				return 0;
			}
		}

		if(cfg.vim_filter)
			use_vim_plugin(curr_view, cmd_info->argc, cmd_info->argv); /* no return */

		if(edit_selection() != 0)
		{
			show_error_msg("Edit error", "Can't edit selection");
		}
	}
	return 0;
}

/* This command designates beginning of the alternative part of if-endif
 * statement. */
static int
else_cmd(const cmd_info_t *cmd_info)
{
	if(int_stack_is_empty(&if_levels))
	{
		status_bar_error(":else without :if");
		return 1;
	}
	int_stack_set_top(&if_levels, !int_stack_get_top(&if_levels));
	return 0;
}

static int
empty_cmd(const cmd_info_t *cmd_info)
{
	empty_trash();
	return 0;
}

/* This command ends conditional block. */
static int
endif_cmd(const cmd_info_t *cmd_info)
{
	if(int_stack_is_empty(&if_levels))
	{
		status_bar_error(":endif without :if");
		return 1;
	}
	int_stack_pop(&if_levels);
	return 0;
}

/* This command composes a string from expressions and runs it as a command. */
static int
exe_cmd(const cmd_info_t *cmd_info)
{
	int result = 1;
	char *const eval_result = try_eval_arglist(cmd_info);
	if(eval_result != NULL)
	{
		result = exec_commands(eval_result, curr_view, GET_COMMAND);
		free(eval_result);
	}
	return result != 0;
}

/* Tries to evaluate a set of expressions and concatenate results with a space.
 * Returns pointer to newly allocated string, which should be freed by caller,
 * or NULL on error. */
static char *
try_eval_arglist(const cmd_info_t *cmd_info)
{
	char *eval_result;
	const char *error_pos = NULL;

	if(cmd_info->argc == 0)
	{
		return NULL;
	}

	text_buffer_clear();
	eval_result = eval_arglist(cmd_info->raw_args, &error_pos);

	if(eval_result == NULL)
	{
		text_buffer_addf("%s: %s", "Invalid expression", error_pos);
		status_bar_errorf(text_buffer_get());
	}

	return eval_result;
}

/* Evaluates a set of expressions and concatenates results with a space.  args
 * can not be empty string.  Returns pointer to newly allocated string, which
 * should be freed by caller, or NULL on error.  stop_ptr will point to the
 * beginning of invalid expression in case of error. */
TSTATIC char *
eval_arglist(const char args[], const char **stop_ptr)
{
	size_t len = 0;
	char *eval_result = NULL;

	assert(args[0] != '\0');

	while(args[0] != '\0')
	{
		var_t result = var_false();
		const ParsingErrors parsing_error = parse(args, &result);
		char *free_this = NULL;
		const char *tmp_result = NULL;
		if(parsing_error == PE_INVALID_EXPRESSION && is_prev_token_whitespace())
		{
			result = get_parsing_result();
			tmp_result = free_this = var_to_string(result);
			args = get_last_parsed_char();
		}
		else if(parsing_error == PE_NO_ERROR)
		{
			tmp_result = free_this = var_to_string(result);
			args = get_last_position();
		}

		if(tmp_result == NULL)
		{
			var_free(result);
			break;
		}

		if(!is_null_or_empty(eval_result))
		{
			eval_result = extend_string(eval_result, " ", &len);
		}
		eval_result = extend_string(eval_result, tmp_result, &len);

		var_free(result);
		free(free_this);
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
	return add_filetype(cmd_info, 0);
}

static int
filextype_cmd(const cmd_info_t *cmd_info)
{
	return add_filetype(cmd_info, 1);
}

static int
add_filetype(const cmd_info_t *cmd_info, int x)
{
	const char *records;

	records = skip_non_whitespace(cmd_info->args);
	records = skip_whitespace(records + 1);

	set_programs(cmd_info->argv[0], records, x,
			curr_stats.env_type == ENVTYPE_EMULATOR_WITH_X);
	return 0;
}

static int
fileviewer_cmd(const cmd_info_t *cmd_info)
{
	const char *records;

	records = skip_non_whitespace(cmd_info->args);
	records = skip_whitespace(records + 1);

	set_fileviewer(cmd_info->argv[0], records);
	return 0;
}

static int
filter_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->qmark)
	{
		const char *const name_state = (curr_view->name_filter.raw[0] == '\0') ?
				" is empty" : ": ";
		const char *const auto_state = (curr_view->auto_filter.raw[0] == '\0') ?
				" is empty" : ": ";
		status_bar_messagef("Name filter%s%s\nAuto filter%s%s", name_state,
				curr_view->name_filter.raw, auto_state, curr_view->auto_filter.raw);
		return 1;
	}
	if(cmd_info->argc == 0)
	{
		if(cmd_info->emark)
		{
			toggle_filter_inversion(curr_view);
		}
		else
		{
			const int invert_filter = get_filter_inversion_state(cmd_info);
			set_view_filter(curr_view, NULL, invert_filter);
		}
		return 0;
	}
	else
	{
		const int invert_filter = get_filter_inversion_state(cmd_info);
		return set_view_filter(curr_view, cmd_info->argv[0], invert_filter) != 0;
	}
}

/* Returns value for filter inversion basing on current configuration and
 * filter command. */
static int
get_filter_inversion_state(const cmd_info_t *cmd_info)
{
	int invert_filter = cfg.filter_inverted_by_default;
	if(cmd_info->emark)
	{
		invert_filter = !invert_filter;
	}
	return invert_filter;
}

/* Tries to update filter of the view rejecting incorrect regular expression.
 * NULL as filter value means filter reset, empty string means using of the last
 * search pattern.  Returns non-zero if message on the statusbar should be
 * saved, otherwise zero is returned. */
static int
set_view_filter(FileView *view, const char filter[], int invert)
{
	const char *error_msg;

	filter = get_filter_value(filter);

	error_msg = try_compile_regex(filter, REG_EXTENDED);
	if(error_msg != NULL)
	{
		status_bar_errorf("Name filter not set: %s", error_msg);
		return 1;
	}

	view->invert = invert;
	(void)filter_set(&view->name_filter, filter);
	(void)filter_clear(&view->auto_filter);
	load_saving_pos(view, 1);
	return 0;
}

/* Returns new value for a filter taking special values of the filter into
 * account.  NULL means filter reset, empty string means using of the last
 * search pattern.  Returns possibly changed filter value, which might be
 * invalidated after adding new search pattern/changing history size. */
static const char *
get_filter_value(const char filter[])
{
	if(filter == NULL)
	{
		filter = "";
	}
	else if(filter[0] == '\0')
	{
		if(!hist_is_empty(&cfg.search_hist))
		{
			filter = cfg.search_hist.items[0];
		}
	}
	return filter;
}

/* Tries to compile given regular expression and specified compile flags.
 * Returns NULL on success, otherwise statically allocated message describing
 * compiling error is returned. */
static const char *
try_compile_regex(const char regex[], int cflags)
{
	const char *error_msg = NULL;

	if(regex[0] != '\0')
	{
		regex_t re;
		const int err = regcomp(&re, regex, cflags);
		if(err != 0)
		{
			error_msg = get_regexp_error(err, &re);
		}
		regfree(&re);
	}

	return error_msg;
}

static int
find_cmd(const cmd_info_t *cmd_info)
{
	static char *last_args;
	static int last_dir;

	if(cmd_info->argc > 0)
	{
		if(cmd_info->argc == 1)
			last_dir = 0;
		else if(is_dir(cmd_info->argv[0]))
			last_dir = 1;
		else
			last_dir = 0;
		(void)replace_string(&last_args, cmd_info->args);
	}
	else if(last_args == NULL)
	{
		status_bar_error("Nothing to repeat");
		return 1;
	}

	return show_find_menu(curr_view, last_dir, last_args) != 0;
}

static int
finish_cmd(const cmd_info_t *cmd_info)
{
	if(curr_stats.sourcing_state != SOURCING_PROCESSING)
	{
		status_bar_error(":finish used outside of a sourced file");
		return 1;
	}

	curr_stats.sourcing_state = SOURCING_FINISHING;
	return 0;
}

static int
grep_cmd(const cmd_info_t *cmd_info)
{
	static char *last_args;
	static int last_invert;
	int inv;

	if(cmd_info->argc > 0)
	{
		(void)replace_string(&last_args, cmd_info->args);
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
			snprintf(buf, sizeof(buf), "%s -c \"help vifm.txt\" -c only",
					get_vicmd(&bg));
		}
	}
	else
	{
#ifndef _WIN32
		char *escaped;
#endif

		if(cmd_info->argc != 0)
		{
			status_bar_error("No arguments are allowed when 'vimhelp' option is off");
			return 1;
		}

		if(!path_exists_at(cfg.config_dir, VIFM_HELP))
		{
			show_error_msgf("No help file", "Can't find \"%s/" VIFM_HELP "\" file",
					cfg.config_dir);
			return 0;
		}

#ifndef _WIN32
		escaped = escape_filename(cfg.config_dir, 0);
		snprintf(buf, sizeof(buf), "%s %s/" VIFM_HELP, get_vicmd(&bg), escaped);
		free(escaped);
#else
		snprintf(buf, sizeof(buf), "%s \"%s/" VIFM_HELP "\"", get_vicmd(&bg),
				cfg.config_dir);
#endif
	}

	if(bg)
	{
		start_background_job(buf, 0);
	}
	else
	{
#ifndef _WIN32
		shellout(buf, -1, 1);
#else
		def_prog_mode();
		endwin();
		system("cls");
		if(system(buf) != EXIT_SUCCESS)
		{
			system("pause");
		}
		update_screen(UT_FULL);
		update_screen(UT_REDRAW);
#endif
	}
	return 0;
}

static int
highlight_cmd(const cmd_info_t *cmd_info)
{
	int i;
	int pos;

	if(cmd_info->argc == 0)
	{
		char msg[256*(MAXNUM_COLOR - 2)];
		size_t msg_len = 0U;
		msg[0] = '\0';
		for(i = 0; i < MAXNUM_COLOR - 2; i++)
		{
			msg_len += snprintf(msg + msg_len, sizeof(msg) - msg_len, "%s%s",
					get_group_str(i, curr_view->cs.color[i]),
					(i < MAXNUM_COLOR - 2 - 1) ? "\n" : "");
		}
		status_bar_message(msg);
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
			if((col = get_color(equal + 1, 0, &curr_stats.cs->color[pos].attr)) < -1)
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
			if((col = get_color(equal + 1, 1, &curr_stats.cs->color[pos].attr)) < -1)
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
			if(curr_stats.env_type == ENVTYPE_LINUX_NATIVE &&
					(attrs & (A_BOLD | A_REVERSE)) == (A_BOLD | A_REVERSE))
			{
				curr_stats.cs->color[pos].attr |= A_BLINK;
			}
		}
		else
		{
			status_bar_errorf("Illegal argument: %s", cmd_info->argv[i]);
			return 1;
		}
	}
	init_pair(curr_stats.cs_base + pos, curr_stats.cs->color[pos].fg,
			curr_stats.cs->color[pos].bg);
	curr_stats.need_update = UT_FULL;
	return 0;
}

/* Composes string representation of highlight group definition. */
static const char *
get_group_str(int group, col_attr_t col)
{
	static char buf[256];

	char fg_buf[16], bg_buf[16];

	color_to_str(col.fg, sizeof(fg_buf), fg_buf);
	color_to_str(col.bg, sizeof(bg_buf), bg_buf);

	snprintf(buf, sizeof(buf), "%-10s cterm=%s ctermfg=%-7s ctermbg=%-7s",
			HI_GROUPS[group], attrs_to_str(col.attr), fg_buf, bg_buf);

	return buf;
}

static int
get_color(const char str[], int fg, int *attr)
{
	int col_pos = string_array_pos_case(COLOR_NAMES, ARRAY_LEN(COLOR_NAMES), str);
	int light_col_pos = string_array_pos_case(LIGHT_COLOR_NAMES,
			ARRAY_LEN(LIGHT_COLOR_NAMES), str);
	int col_num = isdigit(*str) ? atoi(str) : -1;
	if(strcmp(str, "-1") == 0 || strcasecmp(str, "default") == 0 ||
			strcasecmp(str, "none") == 0)
		return -1;
	if(col_pos < 0 && light_col_pos < 0 && (col_num < 0 ||
			(curr_stats.load_stage >= 2 && col_num > COLORS)))
		return -2;

	if(light_col_pos >= 0)
	{
		*attr |= (!fg && curr_stats.env_type == ENVTYPE_LINUX_NATIVE) ?
				A_BLINK : A_BOLD;
		return light_col_pos;
	}
	else if(col_pos >= 0)
	{
		if(!fg && curr_stats.env_type == ENVTYPE_LINUX_NATIVE)
		{
			*attr &= ~A_BLINK;
		}
		return col_pos;
	}
	else
	{
		return col_num;
	}
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
	const char *const type = (cmd_info->argc == 0) ? "." : cmd_info->argv[0];
	const size_t len = strlen(type);

	if(strcmp(type, ":") == 0 || starts_withn("cmd", type, len))
		return show_cmdhistory_menu(curr_view) != 0;
	else if(strcmp(type, "/") == 0 || starts_withn("search", type, len) ||
			starts_withn("fsearch", type, len))
		return show_fsearchhistory_menu(curr_view) != 0;
	else if(strcmp(type, "?") == 0 || starts_withn("bsearch", type, len))
		return show_bsearchhistory_menu(curr_view) != 0;
	else if(strcmp(type, "@") == 0 || starts_withn("input", type, len))
		return show_prompthistory_menu(curr_view) != 0;
	else if(strcmp(type, "=") == 0 || starts_withn("filter", type, MAX(2, len)))
		return show_filterhistory_menu(curr_view) != 0;
	else if(strcmp(type, ".") == 0 || starts_withn("dir", type, len))
		return show_history_menu(curr_view) != 0;
	else
		return CMDS_ERR_TRAILING_CHARS;
}

/* This command starts conditional block. */
static int
if_cmd(const cmd_info_t *cmd_info)
{
	var_t condition;
	text_buffer_clear();
	if(parse(cmd_info->args, &condition) != PE_NO_ERROR)
	{
		text_buffer_addf("%s: %s", "Invalid expression", cmd_info->args);
		status_bar_error(text_buffer_get());
		return 1;
	}
	int_stack_push(&if_levels, var_to_boolean(condition));
	var_free(condition);
	return 0;
}

static int
invert_cmd(const cmd_info_t *cmd_info)
{
	const char *const type_str = (cmd_info->argc == 0) ? "f" : cmd_info->argv[0];
	const char state_type = type_str[0];

	if(type_str[1] != '\0' || !char_is_one_of("fso", type_str[0]))
	{
		return CMDS_ERR_INVALID_ARG;
	}

	if(cmd_info->qmark)
	{
		print_inversion_state(state_type);
		return 1;
	}
	else
	{
		invert_state(state_type);
		return 0;
	}
}

/* Prints inversion state of the feature specified by the state_type
 * argument. */
static void
print_inversion_state(char state_type)
{
	if(state_type == 'f')
	{
		status_bar_messagef("Filter is %sinverted",
				curr_view->invert ? "" : "not ");
	}
	else if(state_type == 's')
	{
		status_bar_message("Selection does not have inversion state");
	}
	else if(state_type == 'o')
	{
		status_bar_messagef("Primary key is sorted in %s order",
				(curr_view->sort[0] > 0) ? "ascending" : "descending");
	}
	else
	{
		assert(0 && "Unexpected state type.");
	}
}

/* Inverts state of the feature specified by the state_type argument. */
static void
invert_state(char state_type)
{
	if(state_type == 'f')
	{
		toggle_filter_inversion(curr_view);
	}
	else if(state_type == 's')
	{
		invert_selection(curr_view);
		redraw_view(curr_view);
		need_clean_selection = 0;
	}
	else if(state_type == 'o')
	{
		invert_sorting_order(curr_view);
		resort_dir_list(1, curr_view);
		redraw_view(curr_view);
	}
	else
	{
		assert(0 && "Unexpected state type.");
	}
}

static int
jobs_cmd(const cmd_info_t *cmd_info)
{
	return show_jobs_menu(curr_view) != 0;
}

static int
let_cmd(const cmd_info_t *cmd_info)
{
	text_buffer_clear();
	if(let_variable(cmd_info->args) != 0)
	{
		status_bar_error(text_buffer_get());
		return 1;
	}
	else if(*text_buffer_get() != '\0')
	{
		status_bar_message(text_buffer_get());
	}
	update_path_env(0);
	return 0;
}

static int
locate_cmd(const cmd_info_t *cmd_info)
{
	static char *last_args;
	if(cmd_info->argc > 0)
	{
		(void)replace_string(&last_args, cmd_info->args);
	}
	else if(last_args == NULL)
	{
		status_bar_error("Nothing to repeat");
		return 1;
	}
	return show_locate_menu(curr_view, last_args) != 0;
}

/* Lists active windows of terminal multiplexer in use, if any. */
static int
ls_cmd(const cmd_info_t *cmd_info)
{
	switch(curr_stats.term_multiplexer)
	{
		case TM_NONE:
			status_bar_message("No terminal multiplexer is in use");
			return 1;
		case TM_SCREEN:
			my_system("screen -X eval windowlist");
			return 0;
		case TM_TMUX:
			if(my_system("tmux choose-window") != EXIT_SUCCESS)
			{
				/* Refresh all windows as failed command outputs message, which can't be
				 * suppressed. */
				update_all_windows();
				/* Fall back to worse way of doing the same for tmux versions < 1.8. */
				my_system("tmux command-prompt choose-window");
			}
			return 0;

		default:
			assert(0 && "Unknown active terminal multiplexer value");
			status_bar_message("Unknown terminal multiplexer is in use");
			return 1;
	}
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
	char *expanded_path;
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

	expanded_path = expand_tilde(strdup(cmd_info->argv[1]));
	if(!is_path_absolute(expanded_path))
	{
		free(expanded_path);
		status_bar_error("Expected full path to the directory");
		return 1;
	}

	if(cmd_info->argc == 2)
	{
		if(cmd_info->end == NOT_DEF || !pane_in_dir(curr_view, expanded_path))
		{
			if(curr_stats.load_stage >= 3 && pane_in_dir(curr_view, expanded_path))
				result = add_bookmark(cmd_info->argv[0][0], expanded_path,
						curr_view->dir_entry[curr_view->list_pos].name);
			else
				result = add_bookmark(cmd_info->argv[0][0], expanded_path, "../");
		}
		else
			result = add_bookmark(cmd_info->argv[0][0], expanded_path,
					curr_view->dir_entry[cmd_info->end].name);
	}
	else
	{
		result = add_bookmark(cmd_info->argv[0][0], expanded_path,
				cmd_info->argv[2]);
	}
	free(expanded_path);
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
	return show_bookmarks_menu(curr_view, buf) != 0;
}

static int
messages_cmd(const cmd_info_t *cmd_info)
{
	/* TODO: move this code to ui.c */
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
		char *new_lines = realloc(lines, len + 1 + strlen(msg) + 1);
		if(new_lines != NULL)
		{
			lines = new_lines;
			len += sprintf(lines + len, "%s%s", (len == 0) ? "": "\n", msg);
			t = (t + 1) % ARRAY_LEN(curr_stats.msgs);
		}
	}

	if(lines == NULL)
		return 0;

	curr_stats.save_msg_in_list = 0;
	curr_stats.allow_sb_msg_truncation = 0;
	status_bar_message(lines);
	curr_stats.allow_sb_msg_truncation = 1;
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
mmap_cmd(const cmd_info_t *cmd_info)
{
	return do_map(cmd_info, "Menu", MENU_MODE, 0) != 0;
}

static int
mnoremap_cmd(const cmd_info_t *cmd_info)
{
	return do_map(cmd_info, "Menu", MENU_MODE, 1) != 0;
}

/* Moves files. */
static int
move_cmd(const cmd_info_t *cmd_info)
{
	return cpmv_cmd(cmd_info, 1);
}

/* Common part of copy and move commands interface implementation. */
static int
cpmv_cmd(const cmd_info_t *cmd_info, int move)
{
	if(cmd_info->qmark)
	{
		if(cmd_info->argc > 0)
		{
			status_bar_error("No arguments are allowed if you use \"?\"");
			return 1;
		}
		if(cmd_info->bg)
			return cpmv_files_bg(curr_view, NULL, -1, move, cmd_info->emark) != 0;
		else
			return cpmv_files(curr_view, NULL, -1, move, 0, 0) != 0;
	}
	else if(cmd_info->bg)
	{
		return cpmv_files_bg(curr_view, cmd_info->argv, cmd_info->argc, move,
				cmd_info->emark) != 0;
	}
	else
	{
		return cpmv_files(curr_view, cmd_info->argv, cmd_info->argc, move, 0,
				cmd_info->emark) != 0;
	}
}

static int
munmap_cmd(const cmd_info_t *cmd_info)
{
	return do_unmap(cmd_info->argv[0], MENU_MODE);
}

static int
nmap_cmd(const cmd_info_t *cmd_info)
{
	return do_map(cmd_info, "Normal", NORMAL_MODE, 0) != 0;
}

static int
nnoremap_cmd(const cmd_info_t *cmd_info)
{
	return do_map(cmd_info, "Normal", NORMAL_MODE, 1) != 0;
}

static int
nohlsearch_cmd(const cmd_info_t *cmd_info)
{
	if(curr_view->selected_files == 0)
		return 0;

	clean_selected_files(curr_view);
	redraw_current_view();
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
		result = do_map(cmd_info, "", CMDLINE_MODE, no_remap);
	}
	else
	{
		result = do_map(cmd_info, "", NORMAL_MODE, no_remap);
		if(result == 0)
			result = do_map(cmd_info, "", VISUAL_MODE, no_remap);
	}
	return result != 0;
}

/* Executes normal mode commands. */
static int
normal_cmd(const cmd_info_t *cmd_info)
{
	wchar_t *wide = to_wide(cmd_info->args);

	if(cmd_info->emark)
	{
		(void)execute_keys_timed_out_no_remap(wide);
	}
	else
	{
		(void)execute_keys_timed_out(wide);
	}

	/* Force leaving command-line mode if the wide contains unfinished ":". */
	if(get_mode() == CMDLINE_MODE)
	{
		(void)execute_keys_timed_out(L"\x03");
	}

	free(wide);
	return 0;
}

static int
nunmap_cmd(const cmd_info_t *cmd_info)
{
	return do_unmap(cmd_info->argv[0], NORMAL_MODE);
}

static int
only_cmd(const cmd_info_t *cmd_info)
{
	only();
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
		show_error_msg("Memory Error", "Unable to allocate enough memory");
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
qmap_cmd(const cmd_info_t *cmd_info)
{
	return do_map(cmd_info, "View", VIEW_MODE, 0) != 0;
}

static int
qnoremap_cmd(const cmd_info_t *cmd_info)
{
	return do_map(cmd_info, "View", VIEW_MODE, 1) != 0;
}

static int
qunmap_cmd(const cmd_info_t *cmd_info)
{
	return do_unmap(cmd_info->argv[0], VIEW_MODE);
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

/* Renames current selection. */
static int
rename_cmd(const cmd_info_t *cmd_info)
{
	return rename_files(curr_view, cmd_info->argv, cmd_info->argc,
			cmd_info->emark) != 0;
}

static int
restart_cmd(const cmd_info_t *cmd_info)
{
	const char *p;
	FileView *tmp_view;

	curr_stats.restart_in_progress = 1;

	/* all user mappings in all modes */
	clear_user_keys();

	/* user defined commands */
	execute_cmd("comclear");

	/* options of current pane */
	reset_options_to_default();
	/* options of other pane */
	tmp_view = curr_view;
	curr_view = other_view;
	load_local_options(other_view);
	reset_options_to_default();
	curr_view = tmp_view;

	/* file types and viewers */
	reset_all_file_associations(curr_stats.env_type == ENVTYPE_EMULATOR_WITH_X);

	/* session status */
	(void)reset_status();

	/* undo list */
	reset_undo_list();

	/* directory history */
	lwin.history_num = 0;
	lwin.history_pos = 0;
	rwin.history_num = 0;
	rwin.history_pos = 0;

	/* All kinds of history. */
	hist_clear(&cfg.cmd_hist);
	hist_clear(&cfg.search_hist);
	hist_clear(&cfg.prompt_hist);
	hist_clear(&cfg.filter_hist);

	/* directory stack */
	clean_stack();

	/* registers */
	clear_registers();

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
	init_variables();
	/* this update is needed as clear_variables() will reset $PATH */
	update_path_env(1);

	reset_views();
	read_info_file(1);
	save_view_history(&lwin, NULL, NULL, -1);
	save_view_history(&rwin, NULL, NULL, -1);
	load_color_scheme_colors();
	source_config();
	exec_startup_commands(0, NULL);

	curr_stats.restart_in_progress = 0;

	return 0;
}

static int
restore_cmd(const cmd_info_t *cmd_info)
{
	/* TODO: move this either to fileops.c or to trash.c */
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

/* Creates symbolic links with relative paths to files. */
static int
rlink_cmd(const cmd_info_t *cmd_info)
{
	return link_cmd(cmd_info, 2);
}

/* Common part of alink and rlink commands interface implementation. */
static int
link_cmd(const cmd_info_t *cmd_info, int type)
{
	if(cmd_info->qmark)
	{
		if(cmd_info->argc > 0)
		{
			status_bar_error("No arguments are allowed if you use \"?\"");
			return 1;
		}
		return cpmv_files(curr_view, NULL, -1, 0, type, 0) != 0;
	}
	else
	{
		return cpmv_files(curr_view, cmd_info->argv, cmd_info->argc, 0, type,
				cmd_info->emark) != 0;
	}
}

/* Shows status of terminal multiplexers support or toggles it. */
static int
screen_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->qmark)
	{
		if(cfg.use_term_multiplexer)
		{
			if(curr_stats.term_multiplexer != TM_NONE)
			{
				status_bar_messagef("Integration with %s is active",
						(curr_stats.term_multiplexer == TM_SCREEN) ? "GNU screen" : "tmux");
			}
			else
			{
				status_bar_message("Integration with terminal multiplexers is enabled "
						"but inactive");
			}
		}
		else
		{
			status_bar_message("Integration with terminal multiplexers is disabled");
		}
		return 1;
	}
	set_use_term_multiplexer(!cfg.use_term_multiplexer);
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
	const char *sh = env_get_def("SHELL", cfg.shell);

	/* Run shell with clean PATH environment variable. */
	load_clean_path_env();
	shellout(sh, 0, 1);
	load_real_path_env();

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
	if(!path_exists(path))
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
	return do_split(cmd_info, HSPLIT);
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
		(void)replace_string(&last_pattern, cmd_info->argv[0]);
	}

	if(cmd_info->argc >= 2)
	{
		(void)replace_string(&last_sub, cmd_info->argv[1]);
	}
	else if(cmd_info->argc == 1)
	{
		(void)replace_string(&last_sub, "");
	}

	if(last_pattern == NULL)
	{
		status_bar_error("No previous pattern");
		return 1;
	}

	return substitute_in_names(curr_view, last_pattern, last_sub, ic, glob) != 0;
}

/* Synchronizes path/cursor position of the other pane with corresponding
 * properties of the current one, possibly including some relative path
 * changes. */
static int
sync_cmd(const cmd_info_t *cmd_info)
{
	char dst_path[PATH_MAX];

	if(cmd_info->emark && cmd_info->argc != 0)
	{
		status_bar_error("No arguments are allowed if you use \"!\"");
		return 1;
	}

	snprintf(dst_path, sizeof(dst_path), "%s/%s", curr_view->curr_dir,
			(cmd_info->argc > 0) ? cmd_info->argv[0] : "");

	if(cd_is_possible(dst_path) && change_directory(other_view, dst_path) >= 0)
	{
		populate_dir_list(other_view, 0);

		if(cmd_info->emark)
		{
			const int offset = (curr_view->list_pos - curr_view->top_line);
			const int shift = (offset*other_view->window_rows)/curr_view->window_rows;
			other_view->top_line = curr_view->list_pos - shift;
			other_view->list_pos = curr_view->list_pos;
			(void)consider_scroll_offset(other_view);

			save_view_history(other_view, NULL, NULL, -1);
		}

		redraw_view(other_view);
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
	text_buffer_clear();
	if(unlet_variables(cmd_info->args) != 0 && !cmd_info->emark)
	{
		status_bar_error(text_buffer_get());
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
	return do_map(cmd_info, "Visual", VISUAL_MODE, 0) != 0;
}

static int
vnoremap_cmd(const cmd_info_t *cmd_info)
{
	return do_map(cmd_info, "Visual", VISUAL_MODE, 1) != 0;
}

static int
do_map(const cmd_info_t *cmd_info, const char map_type[], int mode,
		int no_remap)
{
	wchar_t *keys, *mapping;
	char *raw_rhs, *rhs;
	char t;
	int result;

	if(cmd_info->argc <= 1)
	{
		int save_msg;
		keys = substitute_specs(cmd_info->args);
		save_msg = show_map_menu(curr_view, map_type, list_cmds(mode), keys);
		free(keys);
		return save_msg != 0;
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
		show_error_msg("Mapping Error", "Unable to allocate enough memory");

	return 0;
}

#ifdef _WIN32
static int
volumes_cmd(const cmd_info_t *cmd_info)
{
	return show_volumes_menu(curr_view) != 0;
}
#endif

static int
vsplit_cmd(const cmd_info_t *cmd_info)
{
	return do_split(cmd_info, VSPLIT);
}

static int
do_split(const cmd_info_t *cmd_info, SPLIT orientation)
{
	if(cmd_info->emark && cmd_info->argc != 0)
	{
		status_bar_error("No arguments are allowed if you use \"!\"");
		return 1;
	}

	if(cmd_info->emark)
	{
		if(curr_stats.number_of_windows == 1)
			split_view(orientation);
		else
			only();
	}
	else
	{
		if(cmd_info->argc == 1)
			cd(other_view, curr_view->curr_dir, cmd_info->argv[0]);
		split_view(orientation);
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

	update_screen(UT_FULL);

	return result;
}

static int
winrun_cmd(const cmd_info_t *cmd_info)
{
	int result = 0;
	const char *cmd;

	if(cmd_info->argc == 0)
		return 0;

	if(cmd_info->argv[0][1] != '\0' ||
			!char_is_one_of("^$%.,", cmd_info->argv[0][0]))
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

	update_screen(UT_FULL);

	return result;
}

/* Executes cmd command-line command for a specific view. */
static int
winrun(FileView *view, const char cmd[])
{
	int result;
	FileView *const tmp_curr = curr_view;
	FileView *const tmp_other = other_view;

	curr_view = view;
	other_view = (view == tmp_curr) ? tmp_other : tmp_curr;
	if(curr_view != tmp_curr)
	{
		load_local_options(curr_view);
	}

	result = exec_commands(cmd, curr_view, GET_COMMAND);

	curr_view = tmp_curr;
	other_view = tmp_other;
	if(curr_view != view)
	{
		load_local_options(curr_view);
	}

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

/* Processes :[range]yank command followed by "{reg} [{count}]" or
 * "{reg}|{count}". */
static int
yank_cmd(const cmd_info_t *cmd_info)
{
	int reg = DEFAULT_REG_NAME;
	int result;

	result = get_reg_and_count(cmd_info, &reg);
	if(result == 0)
		result = yank_files(curr_view, reg, 0, NULL) != 0;
	return result;
}

/* Processes arguments of form "{reg} [{count}]" or "{reg}|{count}". May set
 * *reg (so it should be initialized before call) and returns zero on success,
 * cmds unit error code otherwise. */
static int
get_reg_and_count(const cmd_info_t *cmd_info, int *reg)
{
	if(cmd_info->argc == 2)
	{
		int count;

		if(cmd_info->argv[0][1] != '\0')
			return CMDS_ERR_TRAILING_CHARS;
		if(!register_exists(cmd_info->argv[0][0]))
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
			if(!register_exists(cmd_info->argv[0][0]))
				return CMDS_ERR_TRAILING_CHARS;
			*reg = cmd_info->argv[0][0];
		}
	}
	return 0;
}

/* Special handler for user defined commands, which are defined using
 * :command. */
static int
usercmd_cmd(const cmd_info_t *cmd_info)
{
	/* TODO: Refactor this function usercmd_cmd(), see emark_cmd() */

	char *expanded_com = NULL;
	MacroFlags flags;
	size_t len;
	int external = 1;
	int bg;
	int save_msg = 0;

	/* Expand macros in a binded command. */
	expanded_com = expand_macros(cmd_info->cmd, cmd_info->args, &flags,
			get_cmd_id(cmd_info->cmd) == COM_EXECUTE);

	len = trim_right(expanded_com);
	if((bg = ends_with(expanded_com, " &")))
	{
		expanded_com[len - 2] = '\0';
	}

	if(expanded_com[0] == ':')
	{
		int sm = exec_commands(expanded_com, curr_view, GET_COMMAND);
		free(expanded_com);
		return sm != 0;
	}

	clean_selected_files(curr_view);

	if(flags == MACRO_STATUSBAR_OUTPUT)
	{
		output_to_statusbar(expanded_com);
		free(expanded_com);
		return 1;
	}
	else if(flags == MACRO_IGNORE)
	{
		output_to_nowhere(expanded_com);
		free(expanded_com);
		return 0;
	}
	else if(flags == MACRO_MENU_OUTPUT || flags == MACRO_MENU_NAV_OUTPUT)
	{
		const int navigate = flags == MACRO_MENU_NAV_OUTPUT;
		save_msg = show_user_menu(curr_view, expanded_com, navigate) != 0;
	}
	else if(flags == MACRO_SPLIT && curr_stats.term_multiplexer != TM_NONE)
	{
		run_in_split(curr_view, expanded_com);
	}
	else if(starts_with_lit(expanded_com, "filter "))
	{
		const char *filter_val = strchr(expanded_com, ' ') + 1;
		const int invert_filter = cfg.filter_inverted_by_default;
		(void)set_view_filter(curr_view, filter_val, invert_filter);

		external = 0;
	}
	else if(expanded_com[0] == '!')
	{
		char *com_beginning = expanded_com;
		int pause = 0;
		com_beginning++;
		if(*com_beginning == '!')
		{
			pause = 1;
			com_beginning++;
		}
		com_beginning = skip_whitespace(com_beginning);

		if(*com_beginning != '\0' && bg)
		{
			start_background_job(com_beginning, 0);
		}
		else if(strlen(com_beginning) > 0)
		{
			shellout(com_beginning, pause ? 1 : -1, 1);
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
		start_background_job(expanded_com, 0);
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

	return save_msg;
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

/* Runs the cmd in a split window of terminal multiplexer.  Runs shell, if cmd
 * is NULL. */
static void
run_in_split(const FileView *view, const char cmd[])
{
	const char *const cmd_to_run = (cmd == NULL) ? cfg.shell : cmd;

	char *const escaped_cmd = escape_filename(cmd_to_run, 0);

	if(curr_stats.term_multiplexer == TM_TMUX)
	{
		char buf[1024];
		snprintf(buf, sizeof(buf), "tmux split-window %s", escaped_cmd);
		my_system(buf);
	}
	else if(curr_stats.term_multiplexer == TM_SCREEN)
	{
		char buf[1024];
		char *escaped_chdir;

		char *const escaped_dir = escape_filename(view->curr_dir, 0);
		snprintf(buf, sizeof(buf), "chdir %s", escaped_dir);
		free(escaped_dir);

		escaped_chdir = escape_filename(buf, 0);
		snprintf(buf, sizeof(buf), "screen -X eval %s", escaped_chdir);
		free(escaped_chdir);

		my_system(buf);

		snprintf(buf, sizeof(buf), "screen-open-region-with-program %s",
				escaped_cmd);
		my_system(buf);
	}
	else
	{
		assert(0 && "Unexpected active terminal multiplexer value.");
	}

	free(escaped_cmd);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
