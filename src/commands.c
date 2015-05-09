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

#include <sys/stat.h> /* gid_t uid_t */
#include <unistd.h> /* unlink() */

#include <assert.h> /* assert() */
#include <ctype.h> /* isdigit() isspace() */
#include <errno.h> /* errno */
#include <limits.h> /* INT_MIN */
#include <signal.h>
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* EXIT_SUCCESS atoi() free() realloc() */
#include <string.h> /* strcat() strchr() strcmp() strcasecmp() strcpy() strdup()
                       strlen() strrchr() */
#include <wctype.h> /* iswspace() */
#include <wchar.h> /* wcslen() wcsncmp() */

#include "cfg/config.h"
#include "cfg/hist.h"
#include "cfg/info.h"
#include "compat/os.h"
#include "engine/abbrevs.h"
#include "engine/cmds.h"
#include "engine/mode.h"
#include "engine/options.h"
#include "engine/parsing.h"
#include "engine/text_buffer.h"
#include "engine/var.h"
#include "engine/variables.h"
#include "menus/all.h"
#include "modes/dialogs/attr_dialog.h"
#include "modes/dialogs/change_dialog.h"
#include "modes/dialogs/msg_dialog.h"
#include "modes/dialogs/sort_dialog.h"
#include "modes/modes.h"
#include "modes/normal.h"
#include "modes/view.h"
#include "modes/visual.h"
#include "ui/statusbar.h"
#include "ui/ui.h"
#include "utils/env.h"
#include "utils/file_streams.h"
#include "utils/filter.h"
#include "utils/fs.h"
#include "utils/fs_limits.h"
#include "utils/int_stack.h"
#include "utils/log.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/test_helpers.h"
#include "utils/utils.h"
#include "background.h"
#include "bookmarks.h"
#include "bracket_notation.h"
#include "color_manager.h"
#include "color_scheme.h"
#include "colors.h"
#include "commands_completion.h"
#include "dir_stack.h"
#include "filelist.h"
#include "fileops.h"
#include "filetype.h"
#include "fileview.h"
#include "filtering.h"
#include "macros.h"
#include "ops.h"
#include "opt_handlers.h"
#include "path_env.h"
#include "quickview.h"
#include "registers.h"
#include "running.h"
#include "trash.h"
#include "undo.h"
#include "vifm.h"
#include "vim.h"

/* Command scope marker. */
#define SCOPE_GUARD INT_MIN

/* Commands without completion. */
enum
{
	COM_FILTER = NO_COMPLETION_BOUNDARY,
	COM_SUBSTITUTE,
	COM_TR,
	COM_ELSE_STMT,
	COM_ENDIF_STMT,
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
		CmdInputType type);
static void prepare_extcmd_file(FILE *fp, const char beginning[],
		CmdInputType type);
static hist_t * history_by_type(CmdInputType type);
static char * get_file_first_line(const char path[]);
static void execute_extcmd(const char command[], CmdInputType type);
static void save_extcmd(const char command[], CmdInputType type);
static void post(int id);
TSTATIC void select_range(int id, const cmd_info_t *cmd_info);
static int skip_at_beginning(int id, const char args[]);
static int cmd_should_be_processed(int cmd_id);
static int is_out_of_arg(const char cmd[], const char pos[]);
TSTATIC int line_pos(const char begin[], const char end[], char sep,
		int rquoting);
static int is_whole_line_command(const char cmd[]);
static char * skip_command_beginning(const char cmd[]);
static int repeat_command(FileView *view, CmdInputType type);
static int goto_cmd(const cmd_info_t *cmd_info);
static int emark_cmd(const cmd_info_t *cmd_info);
static int alink_cmd(const cmd_info_t *cmd_info);
static int apropos_cmd(const cmd_info_t *cmd_info);
static int cabbrev_cmd(const cmd_info_t *cmd_info);
static int cnoreabbrev_cmd(const cmd_info_t *cmd_info);
static int handle_cabbrevs(const cmd_info_t *cmd_info, int no_remap);
static int list_abbrevs(const char prefix[]);
static int add_cabbrev(const cmd_info_t *cmd_info, int no_remap);
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
static int cquit_cmd(const cmd_info_t *cmd_info);
static int cunabbrev_cmd(const cmd_info_t *cmd_info);
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
static int is_at_scope_bottom(const int_stack_t *scope_stack);
static int exe_cmd(const cmd_info_t *cmd_info);
static char * try_eval_arglist(const cmd_info_t *cmd_info);
TSTATIC char * eval_arglist(const char args[], const char **stop_ptr);
static int file_cmd(const cmd_info_t *cmd_info);
static int filetype_cmd(const cmd_info_t *cmd_info);
static int filextype_cmd(const cmd_info_t *cmd_info);
static int add_filetype(const cmd_info_t *cmd_info, int for_x);
static int fileviewer_cmd(const cmd_info_t *cmd_info);
static int filter_cmd(const cmd_info_t *cmd_info);
static void display_filters_info(const FileView *view);
static char * get_filter_info(const char name[], const filter_t *filter);
static int set_view_filter(FileView *view, const char filter[], int invert,
		int case_sensitive);
static const char * get_filter_value(const char filter[]);
static const char * try_compile_regex(const char regex[], int cflags);
static int get_filter_inversion_state(const cmd_info_t *cmd_info);
static int find_cmd(const cmd_info_t *cmd_info);
static int finish_cmd(const cmd_info_t *cmd_info);
static int grep_cmd(const cmd_info_t *cmd_info);
static int help_cmd(const cmd_info_t *cmd_info);
static int highlight_cmd(const cmd_info_t *cmd_info);
static int highlight_file(const cmd_info_t *cmd_info, int global);
static int is_re_arg(const char arg[]);
static int parse_case_flag(const char flags[], int *case_sensitive);
static void display_file_highlights(const char pattern[], int global);
static int highlight_group(const cmd_info_t *cmd_info);
static const char * get_all_highlights(void);
static const char * get_group_str(int group, const col_attr_t *col);
static const char * get_file_hi_str(const char pattern[], int global,
		int case_sensitive, const col_attr_t *col);
static const char * get_hi_str(const char title[], const col_attr_t *col);
static int parse_and_apply_highlight(const cmd_info_t *cmd_info,
		col_attr_t *color);
static int try_parse_color_name_value(const char str[], int fg,
		col_attr_t *color);
static int parse_color_name_value(const char str[], int fg, int *attr);
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
static int lstrash_cmd(const cmd_info_t *cmd_info);
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
static int redraw_cmd(const cmd_info_t *cmd_info);
static int registers_cmd(const cmd_info_t *cmd_info);
static int rename_cmd(const cmd_info_t *cmd_info);
static int restart_cmd(const cmd_info_t *cmd_info);
static int restore_cmd(const cmd_info_t *cmd_info);
static int rlink_cmd(const cmd_info_t *cmd_info);
static int link_cmd(const cmd_info_t *cmd_info, int absolute);
static int screen_cmd(const cmd_info_t *cmd_info);
static int set_cmd(const cmd_info_t *cmd_info);
static int shell_cmd(const cmd_info_t *cmd_info);
static int sort_cmd(const cmd_info_t *cmd_info);
static int source_cmd(const cmd_info_t *cmd_info);
static int split_cmd(const cmd_info_t *cmd_info);
static int substitute_cmd(const cmd_info_t *cmd_info);
static int sync_cmd(const cmd_info_t *cmd_info);
static int sync_selectively(const cmd_info_t *cmd_info);
static int parse_sync_properties(const cmd_info_t *cmd_info, int *location,
		int *cursor_pos, int *local_options, int *filters);
static void sync_location(const char path[], int sync_cursor_pos,
		int sync_filters);
static void sync_local_opts(void);
static void sync_filters(void);
static int touch_cmd(const cmd_info_t *cmd_info);
static int tr_cmd(const cmd_info_t *cmd_info);
static int trashes_cmd(const cmd_info_t *cmd_info);
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
static int parse_bg_mark(char cmd[]);
static int try_handle_ext_command(const char cmd[], MacroFlags flags,
		int *save_msg);
static void output_to_statusbar(const char *cmd);
static void run_in_split(const FileView *view, const char cmd[]);
static void output_to_custom_flist(FileView *view, const char cmd[], int very);
static void path_handler(const char line[], void *arg);

static const cmd_add_t commands[] = {
	{ .name = "",                 .abbr = NULL,    .emark = 0,  .id = COM_GOTO,        .range = 1,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = goto_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "!",                .abbr = NULL,    .emark = 1,  .id = COM_EXECUTE,     .range = 1,    .bg = 1, .quote = 0, .regexp = 0,
		.handler = emark_cmd,       .qmark = 0,      .expand = 5, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 1, },
	{ .name = "alink",            .abbr = NULL,    .emark = 1,  .id = COM_ALINK,       .range = 1,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = alink_cmd,       .qmark = 1,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 1, },
	{ .name = "apropos",          .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = apropos_cmd,     .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "cabbrev",          .abbr = "ca",    .emark = 0,  .id = COM_CABBR,       .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = cabbrev_cmd,     .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "cnoreabbrev",      .abbr = "cnorea",.emark = 0,  .id = COM_CABBR,       .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = cnoreabbrev_cmd, .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
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
	{ .name = "cquit",            .abbr = "cq",    .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = cquit_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "cunabbrev",        .abbr = "cuna",  .emark = 0,  .id = COM_CABBR,       .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = cunabbrev_cmd,   .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 1, .max_args = NOT_DEF, .select = 0, },
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
	{ .name = "else",             .abbr = "el",    .emark = 0,  .id = COM_ELSE_STMT,   .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = else_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "empty",            .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = empty_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "endif",            .abbr = "en",    .emark = 0,  .id = COM_ENDIF_STMT,  .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = endif_cmd,       .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "execute",          .abbr = "exe",   .emark = 0,  .id = COM_EXE,         .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = exe_cmd,         .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "exit",             .abbr = "exi",   .emark = 1,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = quit_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
	{ .name = "file",             .abbr = "f",     .emark = 0,  .id = COM_FILE,        .range = 0,    .bg = 1, .quote = 0, .regexp = 0,
		.handler = file_cmd,        .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "filetype",         .abbr = "filet", .emark = 0,  .id = COM_FILETYPE,    .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = filetype_cmd,    .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 1, .max_args = NOT_DEF, .select = 0, },
	{ .name = "fileviewer",       .abbr = "filev", .emark = 0,  .id = COM_FILEVIEWER,  .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = fileviewer_cmd,  .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 1, .max_args = NOT_DEF, .select = 0, },
	{ .name = "filextype",        .abbr = "filex", .emark = 0,  .id = COM_FILEXTYPE,   .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = filextype_cmd,   .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 1, .max_args = NOT_DEF, .select = 0, },
	{ .name = "filter",           .abbr = NULL,    .emark = 1,  .id = COM_FILTER,      .range = 0,    .bg = 0, .quote = 1, .regexp = 1,
		.handler = filter_cmd,      .qmark = 1,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 2,       .select = 0, },
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
	{ .name = "lstrash",          .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = lstrash_cmd,     .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
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
	{ .name = "redraw",           .abbr = "redr",  .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = redraw_cmd,      .qmark = 0,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
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
		.handler = sync_cmd,        .qmark = 0,      .expand = 1, .cust_sep = 0,         .min_args = 0, .max_args = NOT_DEF, .select = 0, },
	{ .name = "touch",            .abbr = NULL,    .emark = 0,  .id = COM_TOUCH,       .range = 0,    .bg = 0, .quote = 1, .regexp = 0,
		.handler = touch_cmd,       .qmark = 0,      .expand = 1, .cust_sep = 0,         .min_args = 1, .max_args = NOT_DEF, .select = 0, },
	{ .name = "tr",               .abbr = NULL,    .emark = 0,  .id = COM_TR,          .range = 1,    .bg = 0, .quote = 0, .regexp = 1,
		.handler = tr_cmd,          .qmark = 0,      .expand = 0, .cust_sep = 1,         .min_args = 2, .max_args = 2,       .select = 1, },
	{ .name = "trashes",          .abbr = NULL,    .emark = 0,  .id = -1,              .range = 0,    .bg = 0, .quote = 0, .regexp = 0,
		.handler = trashes_cmd,     .qmark = 1,      .expand = 0, .cust_sep = 0,         .min_args = 0, .max_args = 0,       .select = 0, },
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
	MacroFlags flags = MF_NONE;

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
get_and_execute_command(const char line[], size_t line_pos, CmdInputType type)
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
get_ext_command(const char beginning[], size_t line_pos, CmdInputType type)
{
	char cmd_file[PATH_MAX];
	char *cmd = NULL;

	generate_tmp_file_name("vifm.cmdline", cmd_file, sizeof(cmd_file));

	if(setup_extcmd_file(cmd_file, beginning, type) == 0)
	{
		if(vim_view_file(cmd_file, 1, line_pos, 0) == 0)
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
setup_extcmd_file(const char path[], const char beginning[], CmdInputType type)
{
	FILE *const fp = os_fopen(path, "wt");
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
prepare_extcmd_file(FILE *fp, const char beginning[], CmdInputType type)
{
	const int is_cmd = (type == CIT_COMMAND);
	const hist_t *const hist = history_by_type(type);
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

/* Picks history by command type.  Returns pointer to history. */
static hist_t *
history_by_type(CmdInputType type)
{
	switch(type)
	{
		case CIT_COMMAND:
			return &cfg.cmd_hist;
		case CIT_PROMPT_INPUT:
			return &cfg.prompt_hist;
		case CIT_FILTER_PATTERN:
			return &cfg.filter_hist;

		default:
			return &cfg.search_hist;
	}
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
		curr_stats.save_msg = exec_commands(command, curr_view, type);
	}
	else
	{
		curr_stats.save_msg = exec_command(command, curr_view, type);
	}
}

/* Saves the command to the appropriate history. */
static void
save_extcmd(const char command[], CmdInputType type)
{
	if(type == CIT_COMMAND)
	{
		cfg_save_command_history(command);
	}
	else
	{
		cfg_save_search_history(command);
	}
}

int
is_history_command(const char command[])
{
	/* Don't add :!! or :! to history list. */
	return strcmp(command, "!!") != 0 && strcmp(command, "!") != 0;
}

int
command_accepts_expr(int cmd_id)
{
	return cmd_id == COM_ECHO
	    || cmd_id == COM_EXE
	    || cmd_id == COM_IF_STMT
	    || cmd_id == COM_LET;
}

char *
commands_escape_for_insertion(const char cmd_line[], int pos, const char str[])
{
	const CmdLineLocation ipt = get_cmdline_location(cmd_line, cmd_line + pos);
	switch(ipt)
	{
		case CLL_R_QUOTING:
			/* XXX: Use of filename escape, while special one might be needed. */
		case CLL_OUT_OF_ARG:
		case CLL_NO_QUOTING:
			return escape_filename(str, 0);

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
	if(id != COM_GOTO && curr_view->selected_files > 0 && !keep_view_selection)
	{
		ui_view_reset_selection_and_reload(curr_view);
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

/* Command prefix remover for command parsing unit.  Returns < 0 to do nothing
 * or x to skip command name and x chars. */
static int
skip_at_beginning(int id, const char args[])
{
	if(id == COM_WINDO)
	{
		return 0;
	}
	else if(id == COM_WINRUN)
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
init_commands(void)
{
	if(cmds_conf.inner != NULL)
	{
		init_cmds(1, &cmds_conf);
		return;
	}

	/* We get here when init_commands() is called the first time. */

	init_cmds(1, &cmds_conf);
	add_builtin_commands((const cmd_add_t *)&commands, ARRAY_LEN(commands));

	/* Initialize modules used by this one. */
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

	if(!cmd_should_be_processed(id))
	{
		return 0;
	}

	if(id == USER_CMD_ID)
	{
		char undo_msg[COMMAND_GROUP_INFO_LEN];

		snprintf(undo_msg, sizeof(undo_msg), "in %s: %s",
				replace_home_part(flist_get_dir(view)), command);

		cmd_group_begin(undo_msg);
		cmd_group_end();
	}

	keep_view_selection = 0;
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

	if(!menu && vle_mode_is(NORMAL_MODE))
	{
		remove_selection(view);
	}

	return -1;
}

/* Decides whether next command with id cmd_id should be processed or not,
 * taking state of conditional statements into account.  Returns non-zero if the
 * command should be processed, otherwise zero is returned. */
static int
cmd_should_be_processed(int cmd_id)
{
	static int skipped_nested_if_stmts;

	if(is_at_scope_bottom(&if_levels) || int_stack_get_top(&if_levels) == 1)
	{
		return 1;
	}

	/* Get here only when in false branch of if statement. */

	if(cmd_id == COM_IF_STMT)
	{
		skipped_nested_if_stmts++;
		return 0;
	}
	else if(cmd_id == COM_ELSE_STMT || cmd_id == COM_ENDIF_STMT)
	{
		if(skipped_nested_if_stmts > 0)
		{
			if(cmd_id == COM_ENDIF_STMT)
			{
				skipped_nested_if_stmts--;
			}
			return 0;
		}
		return 1;
	}
	else
	{
		return 0;
	}
}

/* Determines current position in the command line.  Returns:
 *  - 0, if not inside an argument;
 *  - 1, if next character should be skipped (XXX: what does it mean?);
 *  - 2, if inside escaped argument;
 *  - 3, if inside single quoted argument;
 *  - 4, if inside double quoted argument;
 *  - 5, if inside regexp quoted argument. */
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
				else if(*begin == '&' && begin == end - 1)
					state = BEGIN;
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
			case R_QUOTING: return 5;

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
exec_commands(const char cmd[], FileView *view, CmdInputType type)
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
		else if((*p == '|' && is_out_of_arg(cmd, q)) || *p == '\0')
		{
			int whole_line;
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
			if(type != CIT_MENU_COMMAND)
			{
				init_cmds(1, &cmds_conf);
			}
			whole_line = is_whole_line_command(cmd);

			/* Don't break line for whole line commands. */
			if(!whole_line)
			{
				*q = '\0';
				q = p;
			}

			ret = exec_command(cmd, view, type);
			if(ret != 0)
			{
				save_msg = (ret < 0) ? -1 : 1;
			}

			if(whole_line)
			{
				/* Whole line command takes the rest of the string, nothing more to
				 * process. */
				break;
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

/* Checks whether character at given position in the given command-line is
 * outside quoted argument.  Returns non-zero if so, otherwise zero is
 * returned. */
static int
is_out_of_arg(const char cmd[], const char pos[])
{
	return get_cmdline_location(cmd, pos) == CLL_OUT_OF_ARG;
}

CmdLineLocation
get_cmdline_location(const char cmd[], const char pos[])
{
	char separator;
	int regex_quoting;

	cmd_info_t info;
	const int cmd_id = get_cmd_info(cmd, &info);

	switch(cmd_id)
	{
		case COM_FILTER:
			separator = ' ';
			regex_quoting = 1;
			break;
		case COM_SUBSTITUTE:
		case COM_TR:
			separator = info.sep;
			regex_quoting = 1;
			break;

		default:
			separator = ' ';
			regex_quoting = 0;
			break;
	}

	switch(line_pos(cmd, pos, separator, regex_quoting))
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

const char *
find_last_command(const char cmds[])
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
		else if(*p == '\0' || (*p == '|' &&
				line_pos(cmds, q, ' ', starts_with_lit(cmds, "fil")) == 0))
		{
			if(*p != '\0')
			{
				++p;
			}

			cmds = skip_command_beginning(cmds);
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
skip_command_beginning(const char cmd[])
{
	while(isspace(*cmd) || *cmd == ':')
	{
		cmd++;
	}
	return (char *)cmd;
}

int
exec_command(const char cmd[], FileView *view, CmdInputType type)
{
	int menu;
	int backward;

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
			return find_npattern(view, cmd, backward, 1);

		case CIT_VBSEARCH_PATTERN: backward = 1; /* Fall through. */
		case CIT_VFSEARCH_PATTERN:
			return find_vpattern(view, cmd, backward, 1);

		case CIT_VWBSEARCH_PATTERN: backward = 1; /* Fall through. */
		case CIT_VWFSEARCH_PATTERN:
			return find_vwpattern(cmd, backward);

		case CIT_MENU_COMMAND: menu = 1; /* Fall through. */
		case CIT_COMMAND:
			return execute_command(view, cmd, menu);

		case CIT_FILTER_PATTERN:
			local_filter_apply(view, cmd);
			return 0;

		default:
			assert(0 && "Command execution request of unknown/unexpected type.");
			return 0;
	};
}

/* Repeats last command of the specified type.  Returns 0 on success if no
 * message should be saved in the status bar, > 0 to save message on successful
 * execution and < 0 in case of error with error message. */
static int
repeat_command(FileView *view, CmdInputType type)
{
	int backward = 0;
	switch(type)
	{
		case CIT_BSEARCH_PATTERN: backward = 1; /* Fall through. */
		case CIT_FSEARCH_PATTERN:
			return find_npattern(view, cfg_get_last_search_pattern(), backward, 1);

		case CIT_VBSEARCH_PATTERN: backward = 1; /* Fall through. */
		case CIT_VFSEARCH_PATTERN:
			return find_vpattern(view, cfg_get_last_search_pattern(), backward, 1);

		case CIT_VWBSEARCH_PATTERN: backward = 1; /* Fall through. */
		case CIT_VWFSEARCH_PATTERN:
			return find_vwpattern(NULL, backward);

		case CIT_COMMAND:
			return execute_command(view, NULL, 0);

		case CIT_FILTER_PATTERN:
			local_filter_apply(view, "");
			return 0;

		default:
			assert(0 && "Command repetition request of unexpected type.");
			return 0;
	}
}

void
commands_scope_start(void)
{
	(void)int_stack_push(&if_levels, SCOPE_GUARD);
}

int
commands_scope_finish(void)
{
	if(!is_at_scope_bottom(&if_levels))
	{
		status_bar_error("Missing :endif");
		int_stack_pop_seq(&if_levels, SCOPE_GUARD);
		return 1;
	}

	int_stack_pop(&if_levels);
	return 0;
}

/* Return value of all functions below which name ends with "_cmd" mean:
 *  - <0 -- one of CMDS_* errors from cmds.h;
 *  - =0 -- nothing was outputted to the status bar, don't need to save its
 *          state;
 *  - <0 -- someting was outputted to the status bar, need to save its state. */
static int
goto_cmd(const cmd_info_t *cmd_info)
{
	flist_set_pos(curr_view, cmd_info->end);
	return 0;
}

/* Handles :! command, which executes external command via shell. */
static int
emark_cmd(const cmd_info_t *cmd_info)
{
	int save_msg = 0;
	const char *com = cmd_info->args;
	char buf[COMMAND_GROUP_INFO_LEN];
	MacroFlags flags;
	int handled;

	if(cmd_info->argc == 0)
	{
		if(cmd_info->emark)
		{
			const char *const last_cmd = curr_stats.last_cmdline_command;
			if(last_cmd == NULL)
			{
				status_bar_message("No previous command-line command");
				return 1;
			}
			return exec_commands(last_cmd, curr_view, CIT_COMMAND) != 0;
		}
		return CMDS_ERR_TOO_FEW_ARGS;
	}

	com = skip_whitespace(com);
	if(com[0] == '\0')
	{
		return 0;
	}

	flags = (MacroFlags)cmd_info->usr1;
	handled = try_handle_ext_command(com, flags, &save_msg);
	if(handled > 0)
	{
		/* Do nothing. */
	}
	else if(handled < 0)
	{
		return save_msg;
	}
	else if(cmd_info->bg)
	{
		start_background_job(com, 0);
	}
	else
	{
		const int use_term_mux = flags != MF_NO_TERM_MUX;

		clean_selected_files(curr_view);
		if(cfg.fast_run)
		{
			char *const buf = fast_run_complete(com);
			if(buf != NULL)
			{
				(void)shellout(buf, cmd_info->emark ? 1 : -1, use_term_mux);
				free(buf);
			}
		}
		else
		{
			(void)shellout(com, cmd_info->emark ? 1 : -1, use_term_mux);
		}
	}

	snprintf(buf, sizeof(buf), "in %s: !%s",
			replace_home_part(flist_get_dir(curr_view)), cmd_info->raw_args);
	cmd_group_begin(buf);
	add_operation(OP_USR, strdup(com), NULL, "", "");
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

/* Registers command-line mode abbreviation. */
static int
cabbrev_cmd(const cmd_info_t *cmd_info)
{
	return handle_cabbrevs(cmd_info, 0);
}

/* Registers command-line mode abbreviation of noremap kind. */
static int
cnoreabbrev_cmd(const cmd_info_t *cmd_info)
{
	return handle_cabbrevs(cmd_info, 1);
}

/* Handles command-line mode abbreviation of both kinds in one place.  Returns
 * value to be returned by command handler. */
static int
handle_cabbrevs(const cmd_info_t *cmd_info, int no_remap)
{
	if(cmd_info->argc == 0)
	{
		return show_cabbrevs_menu(curr_view) != 0;
	}
	if(cmd_info->argc == 1)
	{
		return list_abbrevs(cmd_info->argv[0]);
	}
	return add_cabbrev(cmd_info, no_remap);
}

/* List command-line mode abbreviations that start with specified prefix.
 * Returns value to be returned by command handler. */
static int
list_abbrevs(const char prefix[])
{
	wchar_t *wide_prefix;
	size_t prefix_len;
	void *state;
	const wchar_t *lhs, *rhs;
	int no_remap;
	vle_textbuf *msg;

	state = NULL;
	if(!vle_abbr_iter(&lhs, &rhs, &no_remap, &state))
	{
		status_bar_message("No abbreviation found");
		return 1;
	}

	msg = vle_tb_create();
	vle_tb_append_line(msg, "Abbreviation -- N -- Replacement");

	wide_prefix = to_wide(prefix);
	prefix_len = wcslen(wide_prefix);

	state = NULL;
	while(vle_abbr_iter(&lhs, &rhs, &no_remap, &state))
	{
		if(wcsncmp(lhs, wide_prefix, prefix_len) == 0)
		{
			char *const descr = describe_abbrev(lhs, rhs, no_remap, 0);
			vle_tb_append_line(msg, descr);
			free(descr);
		}
	}

	status_bar_message(vle_tb_get_data(msg));
	vle_tb_free(msg);

	free(wide_prefix);
	return 1;
}

/* Registers command-line mode abbreviation.  Returns value to be returned by
 * command handler. */
static int
add_cabbrev(const cmd_info_t *cmd_info, int no_remap)
{
	int result;
	wchar_t *subst;
	wchar_t *wargs = to_wide(cmd_info->args);
	wchar_t *rhs = wargs;

	while(cfg_is_word_wchar(*rhs))
	{
		++rhs;
	}
	while(iswspace(*rhs))
	{
		*rhs++ = L'\0';
	}

	subst = substitute_specsw(rhs);
	result = no_remap
	       ? vle_abbr_add_no_remap(wargs, subst)
	       : vle_abbr_add(wargs, subst);
	free(subst);
	free(wargs);

	if(result != 0)
	{
		status_bar_error("Failed to register abbreviation");
	}

	return result;
}

static int
cd_cmd(const cmd_info_t *cmd_info)
{
	int result;

	const char *curr_dir = flist_get_dir(curr_view);
	const char *other_dir = flist_get_dir(other_view);

	if(!cfg.auto_ch_pos)
	{
		clean_positions_in_history(curr_view);
		curr_stats.ch_pos = 0;
	}
	if(cmd_info->argc == 0)
	{
		result = cd(curr_view, curr_dir, cfg.home_dir);
		if(cmd_info->emark)
			result += cd(other_view, other_dir, cfg.home_dir);
	}
	else if(cmd_info->argc == 1)
	{
		result = cd(curr_view, curr_dir, cmd_info->argv[0]);
		if(cmd_info->emark)
		{
			if(!is_path_absolute(cmd_info->argv[0]) && cmd_info->argv[0][0] != '~' &&
					strcmp(cmd_info->argv[0], "-") != 0)
			{
				char dir[PATH_MAX];
				snprintf(dir, sizeof(dir), "%s/%s", curr_dir, cmd_info->argv[0]);
				result += cd(other_view, other_dir, dir);
			}
			else if(strcmp(cmd_info->argv[0], "-") == 0)
			{
				result += cd(other_view, other_dir, curr_dir);
			}
			else
			{
				result += cd(other_view, other_dir, cmd_info->argv[0]);
			}
			refresh_view_win(other_view);
		}
	}
	else
	{
		result = cd(curr_view, curr_dir, cmd_info->argv[0]);
		if(!is_path_absolute(cmd_info->argv[1]) && cmd_info->argv[1][0] != '~')
		{
			char dir[PATH_MAX];
			snprintf(dir, sizeof(dir), "%s/%s", curr_dir, cmd_info->argv[1]);
			result += cd(other_view, other_dir, dir);
		}
		else
		{
			result += cd(other_view, other_dir, cmd_info->argv[1]);
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
	keep_view_selection = 1;
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
		keep_view_selection = 1;
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
	uid_t uid = (uid_t)-1;
	gid_t gid = (gid_t)-1;

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

/* Clones file [count=1] times. */
static int
clone_cmd(const cmd_info_t *cmd_info)
{
	check_marking(curr_view, 0, NULL);

	if(cmd_info->qmark)
	{
		if(cmd_info->argc > 0)
		{
			status_bar_error("No arguments are allowed if you use \"!\"");
			return 1;
		}
		return clone_files(curr_view, NULL, -1, 0, 1) != 0;
	}

	return clone_files(curr_view, cmd_info->argv, cmd_info->argc, cmd_info->emark,
			1) != 0;
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

	if(cmd_info->argc == 0)
	{
		/* Show menu with colorschemes listed. */
		return show_colorschemes_menu(curr_view) != 0;
	}

	if(!color_scheme_exists(cmd_info->argv[0]))
	{
		status_bar_errorf("Cannot find colorscheme %s" , cmd_info->argv[0]);
		return 1;
	}

	if(cmd_info->argc == 2)
	{
		char *directory = expand_tilde(cmd_info->argv[1]);
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

		lwin.local_cs = check_directory_for_color_scheme(1, lwin.curr_dir);
		rwin.local_cs = check_directory_for_color_scheme(0, rwin.curr_dir);
		redraw_lists();
		return 0;
	}
	else
	{
		const int cs_load_result = load_primary_color_scheme(cmd_info->argv[0]);

		assign_color_scheme(&lwin.cs, &cfg.cs);
		assign_color_scheme(&rwin.cs, &cfg.cs);
		redraw_lists();
		update_all_windows();

		return cs_load_result;
	}
}

static int
command_cmd(const cmd_info_t *cmd_info)
{
	char *desc;

	if(cmd_info->argc == 0)
	{
		return show_commands_menu(curr_view) != 0;
	}

	desc = list_udf_content(cmd_info->argv[0]);
	if(desc == NULL)
	{
		status_bar_message("No user-defined commands found");
		return 1;
	}

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

/* Same as :quit, but also aborts directory choosing and mandatory returns
 * non-zero exit code. */
static int
cquit_cmd(const cmd_info_t *cmd_info)
{
	vifm_try_leave(!cmd_info->emark, 1, cmd_info->emark);
	return 0;
}

/* Unregisters command-line abbreviation either by its LHS or RHS. */
static int
cunabbrev_cmd(const cmd_info_t *cmd_info)
{
	wchar_t *const wargs = to_wide(cmd_info->args);
	const int result = vle_abbr_remove(wargs);
	free(wargs);
	if(result != 0)
	{
		status_bar_error("No such abbreviation");
	}
	return 0;
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
	{
		return result;
	}

	check_marking(curr_view, 0, NULL);
	if(cmd_info->bg)
	{
		result = delete_files_bg(curr_view, !cmd_info->emark) != 0;
	}
	else
	{
		result = delete_files(curr_view, reg, !cmd_info->emark) != 0;
	}

	return result;
}

static int
delmarks_cmd(const cmd_info_t *cmd_info)
{
	int i;

	if(cmd_info->emark)
	{
		if(cmd_info->argc == 0)
		{
			clear_all_bookmarks();
			return 0;
		}
		else
		{
			status_bar_error("No arguments are allowed if you use \"!\"");
			return 1;
		}
	}

	if(cmd_info->argc == 0)
	{
		return CMDS_ERR_TOO_FEW_ARGS;
	}

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
			clear_bookmark(cmd_info->argv[i][j]);
		}
	}
	return 0;
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

/* Edits current/selected/specified file(s) in editor. */
static int
edit_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->argc != 0)
	{
		if(stats_file_choose_action_set())
		{
			/* The call below does not return. */
			vifm_choose_files(curr_view, cmd_info->argc, cmd_info->argv);
		}

		vim_edit_files(cmd_info->argc, cmd_info->argv);
		return 0;
	}

	if(!curr_view->selected_files ||
			!curr_view->dir_entry[curr_view->list_pos].selected)
	{
		char file_to_view[PATH_MAX];

		if(stats_file_choose_action_set())
		{
			/* The call below does not return. */
			vifm_choose_files(curr_view, cmd_info->argc, cmd_info->argv);
		}

		get_current_full_path(curr_view, sizeof(file_to_view), file_to_view);
		(void)vim_view_file(file_to_view, -1, -1, 1);
	}
	else
	{
		int i;

		for(i = 0; i < curr_view->list_rows; i++)
		{
			struct stat st;
			if(curr_view->dir_entry[i].selected == 0)
				continue;
			if(os_lstat(curr_view->dir_entry[i].name, &st) == 0 &&
					!path_exists(curr_view->dir_entry[i].name, DEREF))
			{
				show_error_msgf("Access error",
						"Can't access destination of link \"%s\". It might be broken.",
						curr_view->dir_entry[i].name);
				return 0;
			}
		}

		if(stats_file_choose_action_set())
		{
			/* The call below does not return. */
			vifm_choose_files(curr_view, cmd_info->argc, cmd_info->argv);
		}

		if(vim_edit_selection() != 0)
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
	if(is_at_scope_bottom(&if_levels))
	{
		status_bar_error(":else without :if");
		return CMDS_ERR_CUSTOM;
	}
	int_stack_set_top(&if_levels, !int_stack_get_top(&if_levels));
	keep_view_selection = 1;
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
	if(is_at_scope_bottom(&if_levels))
	{
		status_bar_error(":endif without :if");
		return CMDS_ERR_CUSTOM;
	}
	int_stack_pop(&if_levels);
	return 0;
}

/* Checks that bottom of block scope is reached.  Returns non-zero if so,
 * otherwise zero is returned. */
static int
is_at_scope_bottom(const int_stack_t *scope_stack)
{
	return int_stack_is_empty(scope_stack)
	    || int_stack_top_is(scope_stack, SCOPE_GUARD);
}

/* This command composes a string from expressions and runs it as a command. */
static int
exe_cmd(const cmd_info_t *cmd_info)
{
	int result = 1;
	char *const eval_result = try_eval_arglist(cmd_info);
	if(eval_result != NULL)
	{
		result = exec_commands(eval_result, curr_view, CIT_COMMAND);
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

	vle_tb_clear(vle_err);
	eval_result = eval_arglist(cmd_info->raw_args, &error_pos);

	if(eval_result == NULL)
	{
		vle_tb_append_linef(vle_err, "%s: %s", "Invalid expression", error_pos);
		status_bar_error(vle_tb_get_data(vle_err));
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
		char *free_this = NULL;
		const char *tmp_result = NULL;

		var_t result = var_false();
		const ParsingErrors parsing_error = parse(args, &result);
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

/* Displays file handler picking menu. */
static int
file_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->argc == 0)
	{
		keep_view_selection = 1;
		return show_file_menu(curr_view, cmd_info->bg) != 0;
	}

	if(run_with_filetype(curr_view, cmd_info->argv[0], cmd_info->bg) != 0)
	{
		status_bar_error(
				"Can't find associated program with requested beginning");
		return 1;
	}

	return 0;
}

/* Registers non-x file association handler. */
static int
filetype_cmd(const cmd_info_t *cmd_info)
{
	return add_filetype(cmd_info, 0);
}

/* Registers x file association handler. */
static int
filextype_cmd(const cmd_info_t *cmd_info)
{
	return add_filetype(cmd_info, 1);
}

/* Registers x/non-x file association handler.  Single argument form lists
 * currently registered patterns that match specified file name in menu mode.
 * Returns regular *_cmd handler value. */
static int
add_filetype(const cmd_info_t *cmd_info, int for_x)
{
	const char *records;
	int in_x;

	if(cmd_info->argc == 1)
	{
		return show_fileprograms_menu(curr_view, cmd_info->argv[0]) != 0;
	}

	records = vle_cmds_next_arg(cmd_info->args);
	in_x = curr_stats.exec_env_type == EET_EMULATOR_WITH_X;
	ft_set_programs(cmd_info->argv[0], records, for_x, in_x);
	return 0;
}

/* Registers external applications as file viewer scripts for files that match
 * name pattern.  Single argument form lists currently registered patterns that
 * match specified file name in menu mode. */
static int
fileviewer_cmd(const cmd_info_t *cmd_info)
{
	const char *records;

	if(cmd_info->argc == 1)
	{
		return show_fileviewers_menu(curr_view, cmd_info->argv[0]) != 0;
	}

	records = vle_cmds_next_arg(cmd_info->args);
	ft_set_viewers(cmd_info->argv[0], records);
	return 0;
}

static int
filter_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->qmark)
	{
		display_filters_info(curr_view);
		return 1;
	}

	if(cmd_info->argc == 0)
	{
		if(cmd_info->emark)
		{
			toggle_filter_inversion(curr_view);
			return 0;
		}
		else
		{
			const int invert_filter = get_filter_inversion_state(cmd_info);
			return set_view_filter(curr_view, NULL, invert_filter,
					FILTER_DEF_CASE_SENSITIVITY) != 0;
		}
	}
	else
	{
		int invert_filter;
		int case_sensitive = FILTER_DEF_CASE_SENSITIVITY;

		if(cmd_info->argc == 2)
		{
			if(parse_case_flag(cmd_info->argv[1], &case_sensitive) != 0)
			{
				return CMDS_ERR_TRAILING_CHARS;
			}
		}

		invert_filter = get_filter_inversion_state(cmd_info);

		return set_view_filter(curr_view, cmd_info->argv[0], invert_filter,
				case_sensitive) != 0;
	}
}

/* Displays state of all filters on the status bar. */
static void
display_filters_info(const FileView *view)
{
	char *const localf = get_filter_info("Local", &view->local_filter.filter);
	char *const manualf = get_filter_info("Name", &view->manual_filter);
	char *const autof = get_filter_info("Auto", &view->auto_filter);

	status_bar_messagef("Filter -- Flags -- Value\n%s\n%s\n%s", localf, manualf, autof);

	free(localf);
	free(manualf);
	free(autof);
}

/* Composes a description string for given filter.  Returns NULL on out of
 * memory error, otherwise a newly allocated string, which should be freed by
 * the caller, is returned. */
static char *
get_filter_info(const char name[], const filter_t *filter)
{
	const char *flags_str;

	if(filter_is_empty(filter))
	{
		flags_str = "";
	}
	else
	{
		flags_str = (filter->cflags & REG_ICASE) ? "i" : "I";
	}

	return format_str("%-6s    %-5s    %s", name, flags_str, filter->raw);
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
set_view_filter(FileView *view, const char filter[], int invert,
		int case_sensitive)
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
	(void)filter_change(&view->manual_filter, filter, case_sensitive);
	(void)filter_clear(&view->auto_filter);
	ui_view_schedule_reload(view);
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

/* Displays documentation. */
static int
help_cmd(const cmd_info_t *cmd_info)
{
	char cmd[PATH_MAX];
	int bg;

	if(cfg.use_vim_help)
	{
		const char *topic = (cmd_info->argc > 0) ? cmd_info->args : VIFM_VIM_HELP;
		bg = vim_format_help_cmd(topic, cmd, sizeof(cmd));
	}
	else
	{
		if(cmd_info->argc != 0)
		{
			status_bar_error("No arguments are allowed when 'vimhelp' option is off");
			return 1;
		}

		if(!path_exists_at(cfg.config_dir, VIFM_HELP, DEREF))
		{
			show_error_msgf("No help file", "Can't find \"%s/" VIFM_HELP "\" file",
					cfg.config_dir);
			return 0;
		}

		bg = format_help_cmd(cmd, sizeof(cmd));
	}

	if(bg)
	{
		start_background_job(cmd, 0);
	}
	else
	{
		display_help(cmd);
	}
	return 0;
}

/* Handles :highlight command.  There are three forms:
 *  - clear all
 *  - highlight file
 *  - highlight group */
static int
highlight_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->argc == 0)
	{
		status_bar_message(get_all_highlights());
		return 1;
	}

	if(cmd_info->argc == 1 && strcasecmp(cmd_info->argv[0], "clear") == 0)
	{
		reset_color_scheme(curr_stats.cs);

		/* Request full update instead of redraw to force recalculation of mixed
		 * colors like cursor line, which otherwise are not updated. */
		curr_stats.need_update = UT_FULL;
		return 0;
	}

	if(is_re_arg(cmd_info->argv[0]))
	{
		return highlight_file(cmd_info, 0);
	}

	if(surrounded_with(cmd_info->argv[0], '{', '}'))
	{
		return highlight_file(cmd_info, 1);
	}

	return highlight_group(cmd_info);
}

/* Checks whether arg matches regular expression file name pattern.  Returns
 * non-zero if so, otherwise zero is returned. */
static int
is_re_arg(const char arg[])
{
	const char *e = strrchr(arg, '/');
	return arg[0] == '/'                         /* Starts with slash. */
	    && e != NULL && e != arg                 /* Has second slash. */
	    && e - arg > 1                           /* Not empty pattern. */
	    && strspn(e + 1, "iI") == strlen(e + 1); /* Has only correct flags. */
}

/* Handles highlight-file form of :highlight command.  Returns value to be
 * returned by command handler. */
static int
highlight_file(const cmd_info_t *cmd_info, int global)
{
	char pattern[strlen(cmd_info->args) + 1];
	col_attr_t color = { .fg = -1, .bg = -1, .attr = 0, };
	int case_sensitive = 0;
	int result;

	(void)extract_part(cmd_info->args + 1, ' ', pattern);

	if(global)
	{
		/* Cut off trailing '}'. */
		pattern[strlen(pattern) - 1] = '\0';
	}
	else
	{
		char *const flags = strrchr(pattern, '/') + 1;
		if(parse_case_flag(flags, &case_sensitive) != 0)
		{
			return CMDS_ERR_TRAILING_CHARS;
		}

		/* Cut the flags off by replacing slash with null-character. */
		flags[-1] = '\0';
	}

	if(cmd_info->argc == 1)
	{
		display_file_highlights(pattern, global);
		return 1;
	}

	result = parse_and_apply_highlight(cmd_info, &color);
	result += add_file_hi(pattern, global, case_sensitive, &color);

	/* Redraw is enough to update filename specific highlights. */
	curr_stats.need_update = UT_REDRAW;

	return result;
}

/* *case_sensitive should be initialized with default value outside the call.
 * Returns zero on success, otherwise non-zero is returned. */
static int
parse_case_flag(const char flags[], int *case_sensitive)
{
	/* TODO: maybe generalize code with substitute_cmd(). */

	while(*flags != '\0')
	{
		switch(*flags)
		{
			case 'i': *case_sensitive = 0; break;
			case 'I': *case_sensitive = 1; break;

			default:
				return 1;
		}

		++flags;
	}

	return 0;
}

/* Displays information about filename specific highlight on the status bar. */
static void
display_file_highlights(const char pattern[], int global)
{
	int i;

	const col_scheme_t *cs = ui_view_get_cs(curr_view);

	for(i = 0; i < cs->file_hi_count; ++i)
	{
		const file_hi_t *const file_hi = &cs->file_hi[i];

		if(file_hi->global != global)
		{
			continue;
		}

		if(strcasecmp(file_hi->pattern, pattern) == 0)
		{
			break;
		}

		if(global && is_in_str_list(file_hi->pattern, ',', pattern))
		{
			break;
		}
	}

	if(i >= cs->file_hi_count)
	{
		status_bar_errorf("Highlight group not found: %s", pattern);
		return;
	}

	status_bar_message(get_file_hi_str(cs->file_hi[i].pattern,
				cs->file_hi[i].global, cs->file_hi[i].case_sensitive,
				&cs->file_hi[i].hi));
}

/* Handles highlight-group form of :highlight command.  Returns value to be
 * returned by command handler. */
static int
highlight_group(const cmd_info_t *cmd_info)
{
	int result;
	int group_id;
	col_attr_t *color;

	group_id = string_array_pos_case(HI_GROUPS, MAXNUM_COLOR, cmd_info->argv[0]);
	if(group_id < 0)
	{
		status_bar_errorf("Highlight group not found: %s", cmd_info->argv[0]);
		return 1;
	}

	color = &curr_stats.cs->color[group_id];

	if(cmd_info->argc == 1)
	{
		status_bar_message(get_group_str(group_id, color));
		return 1;
	}

	result = parse_and_apply_highlight(cmd_info, color);

	curr_stats.cs->pair[group_id] = colmgr_get_pair(color->fg, color->bg);

	/* Other highlight commands might have finished successfully, so update TUI.
	 * Request full update instead of redraw to force recalculation of mixed
	 * colors like cursor line, which otherwise are not updated. */
	curr_stats.need_update = UT_FULL;

	return result;
}

/* Composes string representation of all highlight group definitions.  Returns
 * pointer to statically allocated buffer. */
static const char *
get_all_highlights(void)
{
	static char msg[256*MAXNUM_COLOR];

	const col_scheme_t *cs = ui_view_get_cs(curr_view);
	size_t msg_len = 0U;
	int i;

	msg[0] = '\0';

	for(i = 0; i < MAXNUM_COLOR; ++i)
	{
		msg_len += snprintf(msg + msg_len, sizeof(msg) - msg_len, "%s%s",
				get_group_str(i, &cs->color[i]), (i < MAXNUM_COLOR - 1) ? "\n" : "");
	}

	if(cs->file_hi_count <= 0)
	{
		return msg;
	}

	msg_len += snprintf(msg + msg_len, sizeof(msg) - msg_len, "\n\n");

	for(i = 0; i < cs->file_hi_count; ++i)
	{
		const file_hi_t *const file_hi = &cs->file_hi[i];
		const char *const line = get_file_hi_str(file_hi->pattern, file_hi->global,
				file_hi->case_sensitive, &file_hi->hi);
		msg_len += snprintf(msg + msg_len, sizeof(msg) - msg_len, "%s%s", line,
				(i < cs->file_hi_count - 1) ? "\n" : "");
	}

	return msg;
}

/* Composes string representation of highlight group definition.  Returns
 * pointer to a statically allocated buffer. */
static const char *
get_group_str(int group, const col_attr_t *col)
{
	return get_hi_str(HI_GROUPS[group], col);
}

/* Composes string representation of filename specific highlight definition.
 * Returns pointer to a statically allocated buffer. */
static const char *
get_file_hi_str(const char pattern[], int global, int case_sensitive,
		const col_attr_t *col)
{
	char title[128];
	const char left = global ? '{' : '/';
	const char right = global ? '}' : '/';
	const char *const case_str = case_sensitive ? "I" : "";

	snprintf(title, sizeof(title), "%c%s%c%s", left, pattern, right, case_str);
	return get_hi_str(title, col);
}

/* Composes string representation of highlight definition.  Returns pointer to a
 * statically allocated buffer. */
static const char *
get_hi_str(const char title[], const col_attr_t *col)
{
	static char buf[256];

	char fg_buf[16], bg_buf[16];

	color_to_str(col->fg, sizeof(fg_buf), fg_buf);
	color_to_str(col->bg, sizeof(bg_buf), bg_buf);

	snprintf(buf, sizeof(buf), "%-10s cterm=%s ctermfg=%-7s ctermbg=%-7s", title,
			attrs_to_str(col->attr), fg_buf, bg_buf);

	return buf;
}

/* Parses arguments of :highlight command.  Returns non-zero in case something
 * was output to the status bar, otherwise zero is returned. */
static int
parse_and_apply_highlight(const cmd_info_t *cmd_info, col_attr_t *color)
{
	int i;

	for(i = 1; i < cmd_info->argc; ++i)
	{
		const char *const arg = cmd_info->argv[i];
		const char *const equal = strchr(arg, '=');
		char arg_name[16];

		if(equal == NULL)
		{
			status_bar_errorf("Missing equal sign in \"%s\"", arg);
			return 1;
		}
		if(equal[1] == '\0')
		{
			status_bar_errorf("Missing argument: %s", arg);
			return 1;
		}

		copy_str(arg_name, MIN(sizeof(arg_name), (size_t)(equal - arg + 1)), arg);

		if(strcmp(arg_name, "ctermbg") == 0)
		{
			if(try_parse_color_name_value(equal + 1, 0, color) != 0)
			{
				return 1;
			}
		}
		else if(strcmp(arg_name, "ctermfg") == 0)
		{
			if(try_parse_color_name_value(equal + 1, 1, color) != 0)
			{
				return 1;
			}
		}
		else if(strcmp(arg_name, "cterm") == 0)
		{
			int attrs;
			if((attrs = get_attrs(equal + 1)) == -1)
			{
				status_bar_errorf("Illegal argument: %s", equal + 1);
				return 1;
			}
			color->attr = attrs;
			if(curr_stats.exec_env_type == EET_LINUX_NATIVE &&
					(attrs & (A_BOLD | A_REVERSE)) == (A_BOLD | A_REVERSE))
			{
				color->attr |= A_BLINK;
			}
		}
		else
		{
			status_bar_errorf("Illegal argument: %s", arg);
			return 1;
		}
	}

	return 0;
}

/* Tries to parse color name value into a number.  Returns non-zero if status
 * bar message should be preserved, otherwise zero is returned. */
static int
try_parse_color_name_value(const char str[], int fg, col_attr_t *color)
{
	col_scheme_t *const cs = curr_stats.cs;
	const int col_num = parse_color_name_value(str, fg, &color->attr);

	if(col_num < -1)
	{
		status_bar_errorf("Color name or number not recognized: %s", str);
		if(cs->state == CSS_LOADING)
		{
			cs->state = CSS_BROKEN;
		}

		return 1;
	}

	if(fg)
	{
		color->fg = col_num;
	}
	else
	{
		color->bg = col_num;
	}

	return 0;
}

/* Parses color string into color number and alters *attr in some cases.
 * Returns value less than -1 to indicate error as -1 is valid return value. */
static int
parse_color_name_value(const char str[], int fg, int *attr)
{
	int col_pos;
	int light_col_pos;
	int col_num;

	if(strcmp(str, "-1") == 0 || strcasecmp(str, "default") == 0 ||
			strcasecmp(str, "none") == 0)
	{
		return -1;
	}

	light_col_pos = string_array_pos_case(LIGHT_COLOR_NAMES,
			ARRAY_LEN(LIGHT_COLOR_NAMES), str);
	if(light_col_pos >= 0)
	{
		*attr |= (!fg && curr_stats.exec_env_type == EET_LINUX_NATIVE) ?
				A_BLINK : A_BOLD;
		return light_col_pos;
	}

	col_pos = string_array_pos_case(XTERM256_COLOR_NAMES,
			ARRAY_LEN(XTERM256_COLOR_NAMES), str);
	if(col_pos >= 0)
	{
		if(!fg && curr_stats.exec_env_type == EET_LINUX_NATIVE)
		{
			*attr &= ~A_BLINK;
		}
		return col_pos;
	}

	col_num = isdigit(*str) ? atoi(str) : -1;
	if(col_num >= 0 && (curr_stats.load_stage < 2 || col_num < COLORS))
	{
		return col_num;
	}

	/* Fail if all possible parsing ways failed. */
	return -2;
}

static int
get_attrs(const char *text)
{
	int result = 0;
	while(*text != '\0')
	{
		const char *const p = until_first(text, ',');
		char buf[64];

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
	else if(strcmp(type, "=") == 0 || starts_withn("filter", type, MAX(2U, len)))
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
	vle_tb_clear(vle_err);
	if(parse(cmd_info->args, &condition) != PE_NO_ERROR)
	{
		vle_tb_append_linef(vle_err, "%s: %s", "Invalid expression",
				cmd_info->args);
		status_bar_error(vle_tb_get_data(vle_err));
		return CMDS_ERR_CUSTOM;
	}
	(void)int_stack_push(&if_levels, var_to_boolean(condition));
	var_free(condition);
	keep_view_selection = 1;
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
		keep_view_selection = 1;
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
	vle_tb_clear(vle_err);
	if(let_variables(cmd_info->args) != 0)
	{
		status_bar_error(vle_tb_get_data(vle_err));
		return 1;
	}
	else if(*vle_tb_get_data(vle_err) != '\0')
	{
		status_bar_message(vle_tb_get_data(vle_err));
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
			(void)vifm_system("screen -X eval windowlist");
			return 0;
		case TM_TMUX:
			if(vifm_system("tmux choose-window") != EXIT_SUCCESS)
			{
				/* Refresh all windows as failed command outputs message, which can't be
				 * suppressed. */
				update_all_windows();
				/* Fall back to worse way of doing the same for tmux versions < 1.8. */
				(void)vifm_system("tmux command-prompt choose-window");
			}
			return 0;

		default:
			assert(0 && "Unknown active terminal multiplexer value");
			status_bar_message("Unknown terminal multiplexer is in use");
			return 1;
	}
}

/* Lists files in trash. */
static int
lstrash_cmd(const cmd_info_t *cmd_info)
{
	return show_trash_menu(curr_view) != 0;
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
	const char *file;
	char mark = cmd_info->argv[0][0];

	if(cmd_info->argv[0][1] != '\0')
		return CMDS_ERR_TRAILING_CHARS;

	if(cmd_info->qmark)
	{
		if(!is_bookmark_empty(mark))
		{
			status_bar_errorf("Mark isn't empty: %c", mark);
			return 1;
		}
	}

	if(cmd_info->argc == 1)
	{
		const int pos = (cmd_info->end == NOT_DEF)
		              ? curr_view->list_pos
		              : cmd_info->end;
		const dir_entry_t *const entry = &curr_view->dir_entry[pos];
		return set_user_bookmark(mark, entry->origin, entry->name);
	}

	expanded_path = expand_tilde(cmd_info->argv[1]);
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
			{
				file = curr_view->dir_entry[curr_view->list_pos].name;
			}
			else
			{
				file = NO_BOOKMARK_FILE;
			}
		}
		else
		{
			file = curr_view->dir_entry[cmd_info->end].name;
		}
	}
	else
	{
		file = cmd_info->argv[2];
	}
	result = set_user_bookmark(mark, expanded_path, file);
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
	const CopyMoveLikeOp op = move ? CMLO_MOVE : CMLO_COPY;

	check_marking(curr_view, 0, NULL);

	if(cmd_info->qmark)
	{
		if(cmd_info->argc > 0)
		{
			status_bar_error("No arguments are allowed if you use \"?\"");
			return 1;
		}

		if(cmd_info->bg)
		{
			return cpmv_files_bg(curr_view, NULL, -1, move, cmd_info->emark) != 0;
		}

		return cpmv_files(curr_view, NULL, -1, op, 0) != 0;
	}

	if(cmd_info->bg)
	{
		return cpmv_files_bg(curr_view, cmd_info->argv, cmd_info->argc, move,
				cmd_info->emark) != 0;
	}

	return cpmv_files(curr_view, cmd_info->argv, cmd_info->argc, op,
			cmd_info->emark) != 0;
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

/* Resets file selection and search highlight. */
static int
nohlsearch_cmd(const cmd_info_t *cmd_info)
{
	ui_view_reset_search_highlight(curr_view);

	if(curr_view->selected_files != 0)
	{
		clean_selected_files(curr_view);
		redraw_current_view();
	}
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
	if(vle_mode_is(CMDLINE_MODE))
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

/* Immediately redraws the screen. */
static int
redraw_cmd(const cmd_info_t *cmd_info)
{
	update_screen(UT_FULL);
	return 0;
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

/* Renames selected files of the current view. */
static int
rename_cmd(const cmd_info_t *cmd_info)
{
	check_marking(curr_view, 0, NULL);
	return rename_files(curr_view, cmd_info->argv, cmd_info->argc,
			cmd_info->emark) != 0;
}

/* Resets internal state and reloads configuration files. */
static int
restart_cmd(const cmd_info_t *cmd_info)
{
	vifm_restart();
	return 0;
}

static int
restore_cmd(const cmd_info_t *cmd_info)
{
	return restore_files(curr_view) != 0;
}

/* Creates symbolic links with relative paths to files. */
static int
rlink_cmd(const cmd_info_t *cmd_info)
{
	return link_cmd(cmd_info, 0);
}

/* Common part of alink and rlink commands interface implementation. */
static int
link_cmd(const cmd_info_t *cmd_info, int absolute)
{
	const CopyMoveLikeOp op = absolute ? CMLO_LINK_ABS : CMLO_LINK_REL;

	check_marking(curr_view, 0, NULL);

	if(cmd_info->qmark)
	{
		if(cmd_info->argc > 0)
		{
			status_bar_error("No arguments are allowed if you use \"?\"");
			return 1;
		}
		return cpmv_files(curr_view, NULL, -1, op, 0) != 0;
	}

	return cpmv_files(curr_view, cmd_info->argv, cmd_info->argc, op,
			cmd_info->emark) != 0;
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
	cfg_set_use_term_multiplexer(!cfg.use_term_multiplexer);
	return 0;
}

static int
set_cmd(const cmd_info_t *cmd_info)
{
	const int result = process_set_args(cmd_info->args);
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
	keep_view_selection = 1;
	return 0;
}

static int
source_cmd(const cmd_info_t *cmd_info)
{
	int ret = 0;
	char *path = expand_tilde(cmd_info->argv[0]);
	if(!path_exists(path, DEREF))
	{
		status_bar_errorf("File doesn't exist: %s", cmd_info->argv[0]);
		ret = 1;
	}
	if(os_access(path, R_OK) != 0)
	{
		status_bar_errorf("File isn't readable: %s", cmd_info->argv[0]);
		ret = 1;
	}
	if(cfg_source_file(path) != 0)
	{
		status_bar_errorf("Error sourcing file: %s", cmd_info->argv[0]);
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

/* :s[ubstitute]/[pat]/[subs]/[flags].  Replaces matches of regular expression
 * in names of files.  Empty pattern is replaced with the latest search pattern.
 * New pattern is saved in search pattern history.  Empty substitution part is
 * replaced with the previously used one. */
static int
substitute_cmd(const cmd_info_t *cmd_info)
{
	/* TODO: Vim preserves these two values across sessions. */
	static char *last_pattern;
	static char *last_sub;

	int ic = 0;
	int glob = cfg.gdefault;

	if(cmd_info->argc == 3)
	{
		/* TODO: maybe extract into a function to generalize code with
		 * filter_cmd(). */
		const char *flags = cmd_info->argv[2];
		while(*flags != '\0')
		{
			switch(*flags)
			{
				case 'i': ic =  1; break;
				case 'I': ic = -1; break;

				case 'g': glob = !glob; break;

				default:
					return CMDS_ERR_TRAILING_CHARS;
			};

			++flags;
		}
	}

	if(cmd_info->argc >= 1)
	{
		if(cmd_info->argv[0][0] == '\0')
		{
			(void)replace_string(&last_pattern, cfg_get_last_search_pattern());
		}
		else
		{
			(void)replace_string(&last_pattern, cmd_info->argv[0]);
			cfg_save_search_history(last_pattern);
		}
	}

	if(cmd_info->argc >= 2)
	{
		(void)replace_string(&last_sub, cmd_info->argv[1]);
	}
	else if(cmd_info->argc == 1)
	{
		(void)replace_string(&last_sub, "");
	}

	if(is_null_or_empty(last_pattern))
	{
		status_bar_error("No previous pattern");
		return 1;
	}

	mark_selected(curr_view);
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
		return sync_selectively(cmd_info);
	}

	if(cmd_info->argc > 1)
	{
		return CMDS_ERR_TRAILING_CHARS;
	}

	snprintf(dst_path, sizeof(dst_path), "%s/%s", flist_get_dir(curr_view),
			(cmd_info->argc > 0) ? cmd_info->argv[0] : "");
	sync_location(dst_path, cmd_info->emark, 0);

	return 0;
}

/* Mirrors requested properties of current view with the other one.  Returns
 * value to be returned by command handler. */
static int
sync_selectively(const cmd_info_t *cmd_info)
{
	int location = 0, cursor_pos = 0, local_options = 0, filters = 0;
	if(parse_sync_properties(cmd_info, &location, &cursor_pos, &local_options,
				&filters) != 0)
	{
		return 1;
	}

	if(filters)
	{
		sync_filters();
	}
	if(location)
	{
		sync_location(flist_get_dir(curr_view), cursor_pos, filters);
	}
	if(local_options)
	{
		sync_local_opts();
	}

	return 0;
}

/* Parses selective view synchronization properties.  Default values for
 * arguments should be set before the call.  Returns zero on success, otherwise
 * non-zero is returned and error message is displayed on the status bar. */
static int
parse_sync_properties(const cmd_info_t *cmd_info, int *location,
		int *cursor_pos, int *local_options, int *filters)
{
	int i;
	for(i = 0; i < cmd_info->argc; ++i)
	{
		const char *const property = cmd_info->argv[i];
		if(strcmp(property, "location") == 0)
		{
			*location = 1;
		}
		else if(strcmp(property, "cursorpos") == 0)
		{
			*cursor_pos = 1;
		}
		else if(strcmp(property, "localopts") == 0)
		{
			*local_options = 1;
		}
		else if(strcmp(property, "filters") == 0)
		{
			*filters = 1;
		}
		else if(strcmp(property, "all") == 0)
		{
			*location = 1;
			*cursor_pos = 1;
			*local_options = 1;
			*filters = 1;
		}
		else
		{
			status_bar_errorf("Unknown selective sync property: %s", property);
			return 1;
		}
	}

	return 0;
}

/* Mirrors location (directory and maybe cursor position plus local filter) of
 * the current view to the other one. */
static void
sync_location(const char path[], int sync_cursor_pos, int sync_filters)
{
	if(!cd_is_possible(path) || change_directory(other_view, path) < 0)
	{
		return;
	}

	/* Normally changing location resets local filter.  Prevent this by
	 * synchronizing it here (after directory changing, but before loading list of
	 * files, hence no extra work). */
	if(sync_filters)
	{
		local_filter_apply(other_view, curr_view->local_filter.filter.raw);
	}

	populate_dir_list(other_view, 0);

	if(sync_cursor_pos)
	{
		const int offset = (curr_view->list_pos - curr_view->top_line);
		const int shift = (offset*other_view->window_rows)/curr_view->window_rows;

		ensure_file_is_selected(other_view, get_current_file_name(curr_view));
		other_view->top_line = MAX(0, curr_view->list_pos - shift);
		(void)consider_scroll_offset(other_view);

		save_view_history(other_view, NULL, NULL, -1);
	}

	ui_view_schedule_redraw(other_view);
}

/* Sets local options of the other view to be equal to options of the current
 * one. */
static void
sync_local_opts(void)
{
	clone_local_options(curr_view, other_view);
	ui_view_schedule_redraw(other_view);
}

/* Sets filters of the other view to be equal to options of the current one. */
static void
sync_filters(void)
{
	other_view->prev_invert = curr_view->prev_invert;
	other_view->invert = curr_view->invert;

	(void)filter_assign(&other_view->local_filter.filter,
			&curr_view->local_filter.filter);
	(void)filter_assign(&other_view->manual_filter, &curr_view->manual_filter);
	(void)filter_assign(&other_view->auto_filter, &curr_view->auto_filter);
	ui_view_schedule_reload(other_view);
}

static int
touch_cmd(const cmd_info_t *cmd_info)
{
	return make_files(curr_view, cmd_info->argv, cmd_info->argc) != 0;
}

/* Replaces letters in names of files according to character mapping. */
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

	mark_selected(curr_view);
	return tr_in_names(curr_view, cmd_info->argv[0], buf) != 0;
}

/* Lists all valid non-empty trash directories in a menu with optional size of
 * each one. */
static int
trashes_cmd(const cmd_info_t *cmd_info)
{
	return show_trashes_menu(curr_view, cmd_info->qmark) != 0;
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
	vle_tb_clear(vle_err);
	if(unlet_variables(cmd_info->args) != 0 && !cmd_info->emark)
	{
		status_bar_error(vle_tb_get_data(vle_err));
		return 1;
	}
	return 0;
}

static int
view_cmd(const cmd_info_t *cmd_info)
{
	if(curr_stats.number_of_windows == 1 && (!curr_stats.view || cmd_info->emark))
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

	raw_rhs = vle_cmds_past_arg(cmd_info->args);
	t = *raw_rhs;
	*raw_rhs = '\0';

	rhs = vle_cmds_at_arg(raw_rhs + 1);
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
			cd(other_view, flist_get_dir(curr_view), cmd_info->argv[0]);
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

	curr_stats.need_update = UT_FULL;

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

	result = exec_commands(cmd, curr_view, CIT_COMMAND);

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

/* Possibly exits vifm normally with or without saving state to vifminfo
 * file. */
static int
quit_cmd(const cmd_info_t *cmd_info)
{
	vifm_try_leave(!cmd_info->emark, 0, cmd_info->emark);
	return 0;
}

static int
wq_cmd(const cmd_info_t *cmd_info)
{
	vifm_try_leave(1, 0, cmd_info->emark);
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
	{
		result = yank_files(curr_view, reg) != 0;
	}

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
	char *expanded_com = NULL;
	MacroFlags flags;
	int external = 1;
	int bg;
	int save_msg = 0;
	int handled;

	/* Expand macros in a binded command. */
	expanded_com = expand_macros(cmd_info->cmd, cmd_info->args, &flags,
			get_cmd_id(cmd_info->cmd) == COM_EXECUTE);

	if(expanded_com[0] == ':')
	{
		int sm = exec_commands(expanded_com, curr_view, CIT_COMMAND);
		free(expanded_com);
		return sm != 0;
	}

	bg = parse_bg_mark(expanded_com);

	clean_selected_files(curr_view);

	handled = try_handle_ext_command(expanded_com, flags, &save_msg);
	if(handled > 0)
	{
		/* Do nothing. */
	}
	else if(handled < 0)
	{
		/* XXX: is it intentional to skip adding such commands to undo list? */
		free(expanded_com);
		return save_msg;
	}
	else if(starts_with_lit(expanded_com, "filter") &&
			char_is_one_of(" !/", expanded_com[6]))
	{
		save_msg = exec_command(expanded_com, curr_view, CIT_COMMAND);
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
			shellout(com_beginning, pause ? 1 : -1, flags != MF_NO_TERM_MUX);
		}
	}
	else if(expanded_com[0] == '/')
	{
		exec_command(expanded_com + 1, curr_view, CIT_FSEARCH_PATTERN);
		external = 0;
		keep_view_selection = 1;
	}
	else if(expanded_com[0] == '=')
	{
		exec_command(expanded_com + 1, curr_view, CIT_FILTER_PATTERN);
		ui_view_schedule_reload(curr_view);
		external = 0;
		keep_view_selection = 1;
	}
	else if(bg)
	{
		start_background_job(expanded_com, 0);
	}
	else
	{
		shellout(expanded_com, -1, flags != MF_NO_TERM_MUX);
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

/* Checks for background mark and trims it from the command.  Returns non-zero
 * if mark is found, and zero otherwise. */
static int
parse_bg_mark(char cmd[])
{
	/* Mark is: space, ampersand, any number of trailing separators. */

	char *const amp = strrchr(cmd, '&');
	if(amp == NULL || amp - 1 < cmd || *vle_cmds_at_arg(amp + 1) != '\0')
	{
		return 0;
	}

	amp[-1] = '\0';
	return 1;
}

/* Handles most of command handling variants.  Returns:
 *  - > 0 -- handled, good to go;
 *  - = 0 -- not handled at all;
 *  - < 0 -- handled, exit. */
static int
try_handle_ext_command(const char cmd[], MacroFlags flags, int *save_msg)
{
	if(flags == MF_STATUSBAR_OUTPUT)
	{
		output_to_statusbar(cmd);
		*save_msg = 1;
		return -1;
	}
	else if(flags == MF_IGNORE)
	{
		output_to_nowhere(cmd);
		*save_msg = 0;
		return -1;
	}
	else if(flags == MF_MENU_OUTPUT || flags == MF_MENU_NAV_OUTPUT)
	{
		const int navigate = flags == MF_MENU_NAV_OUTPUT;
		*save_msg = show_user_menu(curr_view, cmd, navigate) != 0;
	}
	else if(flags == MF_SPLIT && curr_stats.term_multiplexer != TM_NONE)
	{
		run_in_split(curr_view, cmd);
	}
	else if(flags == MF_CUSTOMVIEW_OUTPUT || flags == MF_VERYCUSTOMVIEW_OUTPUT)
	{
		const int very = flags == MF_VERYCUSTOMVIEW_OUTPUT;
		output_to_custom_flist(curr_view, cmd, very);
	}
	else
	{
		return 0;
	}
	return 1;
}

static void
output_to_statusbar(const char *cmd)
{
	FILE *file, *err;
	char buf[2048];
	char *lines;
	size_t len;

	if(background_and_capture((char *)cmd, 1, &file, &err) == (pid_t)-1)
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
		(void)vifm_system(buf);
	}
	else if(curr_stats.term_multiplexer == TM_SCREEN)
	{
		char buf[1024];
		char *escaped_chdir;

		char *const escaped_dir = escape_filename(flist_get_dir(view), 0);
		snprintf(buf, sizeof(buf), "chdir %s", escaped_dir);
		free(escaped_dir);

		escaped_chdir = escape_filename(buf, 0);
		snprintf(buf, sizeof(buf), "screen -X eval %s", escaped_chdir);
		free(escaped_chdir);

		(void)vifm_system(buf);

		snprintf(buf, sizeof(buf), "screen-open-region-with-program %s",
				escaped_cmd);
		(void)vifm_system(buf);
	}
	else
	{
		assert(0 && "Unexpected active terminal multiplexer value.");
	}

	free(escaped_cmd);
}

/* Runs the cmd and parses its output as list of paths to compose custom view.
 * Very custom view implies unsorted list. */
static void
output_to_custom_flist(FileView *view, const char cmd[], int very)
{
	char *title;

	title = format_str("!%s", cmd);
	flist_custom_start(view, title);
	free(title);

	if(process_cmd_output("Loading custom view", cmd, 1, &path_handler,
				view) != 0)
	{
		show_error_msgf("Trouble running command", "Unable to run: %s", cmd);
		return;
	}

	if(very)
	{
		memcpy(&view->custom.sort[0], &view->sort[0], sizeof(view->custom.sort));
		memset(&view->sort[0], SK_NONE, sizeof(view->sort));
	}
	view->custom.unsorted = very;

	(void)flist_custom_finish(view);

	if(very)
	{
		/* As custom view isn't activated until flist_custom_finish() is called,
		 * need to update option separately from view sort array. */
		load_sort_option(view);
	}

	flist_set_pos(view, 0);
}

/* Implements process_cmd_output() callback that loads paths into custom
 * view. */
static void
path_handler(const char line[], void *arg)
{
	FileView *view = arg;
	int line_num;
	char *const path = parse_file_spec(line, &line_num);
	if(path != NULL)
	{
		flist_custom_add(view, path);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
