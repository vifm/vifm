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

#include "cmd_handlers.h"

#include <regex.h>

#include <curses.h>

#include <sys/stat.h> /* gid_t uid_t */

#include <assert.h> /* assert() */
#include <ctype.h> /* isdigit() */
#include <errno.h> /* errno */
#include <limits.h> /* INT_MAX */
#include <signal.h>
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* EXIT_SUCCESS atoi() free() realloc() */
#include <string.h> /* strchr() strcmp() strcspn() strcasecmp() strcpy()
                       strdup() strerror() strlen() strrchr() strspn() */
#include <wctype.h> /* iswspace() */
#include <wchar.h> /* wcslen() wcsncmp() */

#include "cfg/config.h"
#include "cfg/info.h"
#include "compat/fs_limits.h"
#include "compat/os.h"
#include "engine/abbrevs.h"
#include "engine/autocmds.h"
#include "engine/cmds.h"
#include "engine/keys.h"
#include "engine/mode.h"
#include "engine/options.h"
#include "engine/parsing.h"
#include "engine/text_buffer.h"
#include "engine/var.h"
#include "engine/variables.h"
#include "int/path_env.h"
#include "int/vim.h"
#include "lua/vlua.h"
#include "modes/normal.h"
#include "modes/dialogs/attr_dialog.h"
#include "modes/dialogs/change_dialog.h"
#include "modes/dialogs/msg_dialog.h"
#include "modes/dialogs/sort_dialog.h"
#include "menus/all.h"
#include "menus/menus.h"
#include "modes/modes.h"
#include "modes/wk.h"
#include "ui/color_scheme.h"
#include "ui/colors.h"
#include "ui/fileview.h"
#include "ui/quickview.h"
#include "ui/statusbar.h"
#include "ui/tabs.h"
#include "ui/ui.h"
#include "utils/env.h"
#include "utils/filter.h"
#include "utils/fs.h"
#include "utils/hist.h"
#include "utils/log.h"
#include "utils/matcher.h"
#include "utils/matchers.h"
#include "utils/path.h"
#include "utils/regexp.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/test_helpers.h"
#include "utils/trie.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "background.h"
#include "bmarks.h"
#include "bracket_notation.h"
#include "cmd_completion.h"
#include "cmd_core.h"
#include "compare.h"
#include "dir_stack.h"
#include "filelist.h"
#include "filetype.h"
#include "filtering.h"
#include "flist_hist.h"
#include "flist_pos.h"
#include "flist_sel.h"
#include "fops_cpmv.h"
#include "fops_misc.h"
#include "fops_put.h"
#include "fops_rename.h"
#include "instance.h"
#include "macros.h"
#include "marks.h"
#include "ops.h"
#include "opt_handlers.h"
#include "plugins.h"
#include "registers.h"
#include "running.h"
#include "sort.h"
#include "trash.h"
#include "undo.h"
#include "vifm.h"

static int goto_cmd(const cmd_info_t *cmd_info);
static int emark_cmd(const cmd_info_t *cmd_info);
static int alink_cmd(const cmd_info_t *cmd_info);
static int amap_cmd(const cmd_info_t *cmd_info);
static int anoremap_cmd(const cmd_info_t *cmd_info);
static int apropos_cmd(const cmd_info_t *cmd_info);
static int autocmd_cmd(const cmd_info_t *cmd_info);
static void aucmd_list_cb(const char event[], const char pattern[], int negated,
		const char action[], void *arg);
static void aucmd_action_handler(const char action[], void *arg);
static int aunmap_cmd(const cmd_info_t *cmd_info);
static int bmark_cmd(const cmd_info_t *cmd_info);
static int bmarks_cmd(const cmd_info_t *cmd_info);
static int bmgo_cmd(const cmd_info_t *cmd_info);
static int bmarks_do(const cmd_info_t *cmd_info, int go);
static char * make_tags_list(const cmd_info_t *cmd_info);
static char * args_to_csl(const cmd_info_t *cmd_info);
static int cabbrev_cmd(const cmd_info_t *cmd_info);
static int chistory_cmd(const cmd_info_t *cmd_info);
static int cnoreabbrev_cmd(const cmd_info_t *cmd_info);
static int handle_cabbrevs(const cmd_info_t *cmd_info, int no_remap);
static int list_abbrevs(const char prefix[]);
static int add_cabbrev(const cmd_info_t *cmd_info, int no_remap);
static int cd_cmd(const cmd_info_t *cmd_info);
static int cds_cmd(const cmd_info_t *cmd_info);
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
static int is_colorscheme_assoc_form(const cmd_info_t *cmd_info);
static int assoc_colorscheme(const char name[], const char path[]);
static void set_colorscheme(char *names[], int count);
static int command_cmd(const cmd_info_t *cmd_info);
static int compare_cmd(const cmd_info_t *cmd_info);
static int copen_cmd(const cmd_info_t *cmd_info);
static int parse_compare_properties(const cmd_info_t *cmd_info, CompareType *ct,
		ListType *lt, int *flags);
static int cunmap_cmd(const cmd_info_t *cmd_info);
static int delete_cmd(const cmd_info_t *cmd_info);
static int delmarks_cmd(const cmd_info_t *cmd_info);
static int delbmarks_cmd(const cmd_info_t *cmd_info);
static void remove_bmark(const char path[], const char tags[], time_t timestamp,
		void *arg);
static char * get_bmark_dir(const cmd_info_t *cmd_info);
static char * make_bmark_path(const char path[]);
static int delsession_cmd(const cmd_info_t *cmd_info);
static int dirs_cmd(const cmd_info_t *cmd_info);
static int dmap_cmd(const cmd_info_t *cmd_info);
static int dnoremap_cmd(const cmd_info_t *cmd_info);
static int dialog_map(const cmd_info_t *cmd_info, int no_remap);
static int dunmap_cmd(const cmd_info_t *cmd_info);
static int echo_cmd(const cmd_info_t *cmd_info);
static int edit_cmd(const cmd_info_t *cmd_info);
static int else_cmd(const cmd_info_t *cmd_info);
static int elseif_cmd(const cmd_info_t *cmd_info);
static int empty_cmd(const cmd_info_t *cmd_info);
static int endif_cmd(const cmd_info_t *cmd_info);
static int exe_cmd(const cmd_info_t *cmd_info);
static char * try_eval_args(const cmd_info_t *cmd_info);
static int file_cmd(const cmd_info_t *cmd_info);
static int filetype_cmd(const cmd_info_t *cmd_info);
static int filextype_cmd(const cmd_info_t *cmd_info);
static int fileviewer_cmd(const cmd_info_t *cmd_info);
static int add_assoc(const cmd_info_t *cmd_info, int viewer, int for_x);
static int filter_cmd(const cmd_info_t *cmd_info);
static int update_filter(view_t *view, const cmd_info_t *cmd_info);
static void display_filters_info(const view_t *view);
static char * get_filter_info(const char name[], const filter_t *filter);
static char * get_matcher_info(const char name[], const matcher_t *matcher);
static int set_view_filter(view_t *view, const char filter[],
		const char fallback[], int invert);
static int get_filter_inversion_state(const cmd_info_t *cmd_info);
static int find_cmd(const cmd_info_t *cmd_info);
static int finish_cmd(const cmd_info_t *cmd_info);
static int goto_path_cmd(const cmd_info_t *cmd_info);
static int grep_cmd(const cmd_info_t *cmd_info);
static int help_cmd(const cmd_info_t *cmd_info);
static int hideui_cmd(const cmd_info_t *cmd_info);
static int highlight_cmd(const cmd_info_t *cmd_info);
static int highlight_clear(const cmd_info_t *cmd_info);
static int highlight_column(const cmd_info_t *cmd_info);
static int find_column_id(const char arg[]);
static int highlight_file(const cmd_info_t *cmd_info);
static void display_file_highlights(const matchers_t *matchers);
static int highlight_group(const cmd_info_t *cmd_info);
static const char * get_all_highlights(void);
static const char * get_group_str(int group, const col_attr_t *col);
static const char * get_column_hi_str(int index, const col_attr_t *col);
static const char * get_file_hi_str(const matchers_t *matchers,
		const col_attr_t *col);
static const char * get_hi_str(const char title[], const col_attr_t *col);
static int parse_highlight_attrs(const cmd_info_t *cmd_info,
		col_attr_t *color);
static int try_parse_cterm_color(const char str[], int is_fg,
		col_attr_t *color);
static int try_parse_gui_color(const char str[], int *color);
static int parse_color_name_value(const char str[], int fg, int *attr);
static int is_default_color(const char str[]);
static int get_attrs(const char text[], int *combine_attrs);
static int history_cmd(const cmd_info_t *cmd_info);
static int histnext_cmd(const cmd_info_t *cmd_info);
static int histprev_cmd(const cmd_info_t *cmd_info);
static int if_cmd(const cmd_info_t *cmd_info);
static int eval_if_condition(const cmd_info_t *cmd_info);
static int invert_cmd(const cmd_info_t *cmd_info);
static void print_inversion_state(char state_type);
static void invert_state(char state_type);
static int jobs_cmd(const cmd_info_t *cmd_info);
static int keepsel_cmd(const cmd_info_t *cmd_info);
static int let_cmd(const cmd_info_t *cmd_info);
static int locate_cmd(const cmd_info_t *cmd_info);
static int ls_cmd(const cmd_info_t *cmd_info);
static int lstrash_cmd(const cmd_info_t *cmd_info);
static int map_cmd(const cmd_info_t *cmd_info);
static int mark_cmd(const cmd_info_t *cmd_info);
static int marks_cmd(const cmd_info_t *cmd_info);
#ifndef _WIN32
static int media_cmd(const cmd_info_t *cmd_info);
#endif
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
static int plugin_cmd(const cmd_info_t *cmd_info);
static int plugins_cmd(const cmd_info_t *cmd_info);
static int popd_cmd(const cmd_info_t *cmd_info);
static int pushd_cmd(const cmd_info_t *cmd_info);
static int put_cmd(const cmd_info_t *cmd_info);
static int pwd_cmd(const cmd_info_t *cmd_info);
static int qmap_cmd(const cmd_info_t *cmd_info);
static int qnoremap_cmd(const cmd_info_t *cmd_info);
static int qunmap_cmd(const cmd_info_t *cmd_info);
static int redraw_cmd(const cmd_info_t *cmd_info);
static int regedit_cmd(const cmd_info_t *cmd_info);
static int registers_cmd(const cmd_info_t *cmd_info);
static int regular_cmd(const cmd_info_t *cmd_info);
static int rename_cmd(const cmd_info_t *cmd_info);
static int restart_cmd(const cmd_info_t *cmd_info);
static int restore_cmd(const cmd_info_t *cmd_info);
static int rlink_cmd(const cmd_info_t *cmd_info);
static int link_cmd(const cmd_info_t *cmd_info, int absolute);
static int parse_cpmv_flags(int *argc, char ***argv);
static int screen_cmd(const cmd_info_t *cmd_info);
static int select_cmd(const cmd_info_t *cmd_info);
static int session_cmd(const cmd_info_t *cmd_info);
static int switch_to_a_session(const char session_name[]);
static int restart_into_session(const char session[], int full);
static int set_cmd(const cmd_info_t *cmd_info);
static int setlocal_cmd(const cmd_info_t *cmd_info);
static int setglobal_cmd(const cmd_info_t *cmd_info);
static int shell_cmd(const cmd_info_t *cmd_info);
static int siblnext_cmd(const cmd_info_t *cmd_info);
static int siblprev_cmd(const cmd_info_t *cmd_info);
static int sort_cmd(const cmd_info_t *cmd_info);
static int source_cmd(const cmd_info_t *cmd_info);
static int split_cmd(const cmd_info_t *cmd_info);
static int stop_cmd(const cmd_info_t *cmd_info);
static int substitute_cmd(const cmd_info_t *cmd_info);
static int sync_cmd(const cmd_info_t *cmd_info);
static int sync_selectively(const cmd_info_t *cmd_info);
static int parse_sync_properties(const cmd_info_t *cmd_info, int *location,
		int *cursor_pos, int *local_options, int *filters, int *filelist,
		int *tree);
static void sync_location(const char path[], int cv, int sync_cursor_pos,
		int sync_filters, int tree);
static void sync_local_opts(int defer_slow);
static void sync_filters(void);
static int tabclose_cmd(const cmd_info_t *cmd_info);
static int tabmove_cmd(const cmd_info_t *cmd_info);
static int tabname_cmd(const cmd_info_t *cmd_info);
static int tabnew_cmd(const cmd_info_t *cmd_info);
static int tabnext_cmd(const cmd_info_t *cmd_info);
static int tabonly_cmd(const cmd_info_t *cmd_info);
static int tabprevious_cmd(const cmd_info_t *cmd_info);
static int touch_cmd(const cmd_info_t *cmd_info);
static int get_at(const view_t *view, const cmd_info_t *cmd_info);
static int tr_cmd(const cmd_info_t *cmd_info);
static int trashes_cmd(const cmd_info_t *cmd_info);
static int tree_cmd(const cmd_info_t *cmd_info);
static int parse_tree_properties(const cmd_info_t *cmd_info, int *depth);
static int undolist_cmd(const cmd_info_t *cmd_info);
static int unlet_cmd(const cmd_info_t *cmd_info);
static int unmap_cmd(const cmd_info_t *cmd_info);
static int unselect_cmd(const cmd_info_t *cmd_info);
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
static int parse_map_args(const char **args);
static int vunmap_cmd(const cmd_info_t *cmd_info);
static int do_unmap(const char *keys, int mode);
static int wincmd_cmd(const cmd_info_t *cmd_info);
static int windo_cmd(const cmd_info_t *cmd_info);
static int winrun_cmd(const cmd_info_t *cmd_info);
static int winrun(view_t *view, const char cmd[]);
static int write_cmd(const cmd_info_t *cmd_info);
static int qall_cmd(const cmd_info_t *cmd_info);
static int quit_cmd(const cmd_info_t *cmd_info);
static int wq_cmd(const cmd_info_t *cmd_info);
static int wqall_cmd(const cmd_info_t *cmd_info);
static int yank_cmd(const cmd_info_t *cmd_info);
static int get_reg_and_count(const cmd_info_t *cmd_info, int *reg);
static int get_reg(const char arg[], int *reg);
static int usercmd_cmd(const cmd_info_t* cmd_info);
static int parse_bg_mark(char cmd[]);
TSTATIC void cmds_drop_state(void);

const cmd_add_t cmds_list[] = {
	{ .name = "",                  .abbr = NULL,    .id = COM_GOTO,
	  .descr = "put cursor at specific line",
	  .flags = HAS_RANGE | HAS_COMMENT,
	  .handler = &goto_cmd,        .min_args = 0,   .max_args = 0, },
	{ .name = "!",                 .abbr = NULL,    .id = COM_EXECUTE,
	  .descr = "execute external command",
	  .flags = HAS_EMARK | HAS_RANGE | HAS_BG_FLAG | HAS_MACROS_FOR_SHELL
	         | HAS_SELECTION_SCOPE,
	  .handler = &emark_cmd,       .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "alink",             .abbr = NULL,    .id = COM_ALINK,
	  .descr = "create absolute links",
	  .flags = HAS_EMARK | HAS_RANGE | HAS_QUOTED_ARGS | HAS_COMMENT
	         | HAS_QMARK_NO_ARGS | HAS_SELECTION_SCOPE,
	  .handler = &alink_cmd,       .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "amap",              .abbr = NULL,    .id = COM_AMAP,
	  .descr = "map keys in navigation mode",
	  .flags = HAS_RAW_ARGS,
	  .handler = &amap_cmd,        .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "anoremap",          .abbr = NULL,    .id = COM_ANOREMAP,
	  .descr = "noremap keys in navigation mode",
	  .flags = HAS_RAW_ARGS,
	  .handler = &anoremap_cmd,    .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "apropos",           .abbr = NULL,    .id = -1,
	  .descr = "query apropos results",
	  .flags = 0,
	  .handler = &apropos_cmd,     .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "aunmap",            .abbr = NULL,    .id = -1,
	  .descr = "unmap user keys in navigation mode",
	  .flags = HAS_RAW_ARGS,
	  .handler = &aunmap_cmd,      .min_args = 1,   .max_args = 1, },
	{ .name = "autocmd",           .abbr = "au",    .id = COM_AUTOCMD,
	  .descr = "manage autocommands",
	  .flags = HAS_EMARK | HAS_QUOTED_ARGS,
	  .handler = &autocmd_cmd,     .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "bmark",             .abbr = NULL,    .id = COM_BMARKS,
	  .descr = "set bookmark",
	  .flags = HAS_EMARK | HAS_QUOTED_ARGS | HAS_COMMENT,
	  .handler = &bmark_cmd,       .min_args = 1,   .max_args = NOT_DEF, },
	{ .name = "bmarks",            .abbr = NULL,    .id = COM_BMARKS,
	  .descr = "list bookmarks",
	  .flags = HAS_COMMENT,
	  .handler = &bmarks_cmd,      .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "bmgo",              .abbr = NULL,    .id = COM_BMARKS,
	  .descr = "navigate to a bookmark",
	  .flags = HAS_COMMENT,
	  .handler = &bmgo_cmd,        .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "cabbrev",           .abbr = "ca",    .id = COM_CABBR,
	  .descr = "display/create cmdline abbrevs",
	  .flags = 0,
	  .handler = &cabbrev_cmd,     .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "chistory",          .abbr = "chi",   .id = -1,
	  .descr = "display history of menus",
	  .flags = 0,
	  .handler = &chistory_cmd,    .min_args = 0,   .max_args = 0, },
	{ .name = "cnoreabbrev",       .abbr = "cnorea", .id = COM_CABBR,
	  .descr = "display/create noremap cmdline abbrevs",
	  .flags = 0,
	  .handler = &cnoreabbrev_cmd, .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "cd",                .abbr = NULL,    .id = COM_CD,
	  .descr = "navigate to a directory",
	  .flags = HAS_EMARK | HAS_QUOTED_ARGS | HAS_COMMENT | HAS_MACROS_FOR_CMD
	         | HAS_ENVVARS,
	  .handler = &cd_cmd,          .min_args = 0,   .max_args = 2, },
	{ .name = "cds",               .abbr = NULL,    .id = COM_CDS,
	  .descr = "navigate to path obtained by substitution in current path",
	  .flags = HAS_EMARK | HAS_REGEXP_ARGS | HAS_CUST_SEP,
	  .handler = &cds_cmd,         .min_args = 2,   .max_args = 3, },
	{ .name = "change",            .abbr = "c",     .id = -1,
	  .descr = "change file traits",
	  .flags = HAS_COMMENT,
	  .handler = &change_cmd,      .min_args = 0,   .max_args = 0, },
#ifndef _WIN32
	{ .name = "chmod",             .abbr = NULL,    .id = -1,
	  .descr = "change permissions",
	  .flags = HAS_EMARK | HAS_RANGE | HAS_COMMENT | HAS_SELECTION_SCOPE,
	  .handler = &chmod_cmd,       .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "chown",             .abbr = NULL,    .id = COM_CHOWN,
	  .descr = "change owner/group",
	  .flags = HAS_RANGE | HAS_COMMENT | HAS_SELECTION_SCOPE,
	  .handler = &chown_cmd,       .min_args = 0,   .max_args = 1, },
#else
	{ .name = "chmod",             .abbr = NULL,    .id = -1,
	  .descr = "change attributes",
	  .flags = HAS_EMARK | HAS_RANGE | HAS_COMMENT | HAS_SELECTION_SCOPE,
	  .handler = &chmod_cmd,       .min_args = 0,   .max_args = 0, },
#endif
	{ .name = "clone",             .abbr = NULL,    .id = COM_CLONE,
	  .descr = "clone selection",
	  .flags = HAS_EMARK | HAS_RANGE | HAS_QUOTED_ARGS | HAS_COMMENT
	         | HAS_QMARK_NO_ARGS | HAS_MACROS_FOR_CMD | HAS_SELECTION_SCOPE,
	  .handler = &clone_cmd,       .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "cmap",              .abbr = "cm",    .id = COM_CMAP,
	  .descr = "map keys in cmdline mode",
	  .flags = HAS_RAW_ARGS,
	  .handler = &cmap_cmd,        .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "cnoremap",          .abbr = "cno",   .id = COM_CNOREMAP,
	  .descr = "noremap keys in cmdline mode",
	  .flags = HAS_RAW_ARGS,
	  .handler = &cnoremap_cmd,    .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "colorscheme",       .abbr = "colo",  .id = COM_COLORSCHEME,
	  .descr = "display/select color schemes",
	  .flags = HAS_QUOTED_ARGS | HAS_COMMENT | HAS_QMARK_NO_ARGS,
	  .handler = &colorscheme_cmd, .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "command",           .abbr = "com",   .id = COM_COMMAND,
	  .descr = "display/define :commands",
	  .flags = HAS_EMARK,
	  .handler = &command_cmd,     .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "compare",           .abbr = NULL,    .id = COM_COMPARE,
	  .descr = "compare directories in two panes",
	  .flags = HAS_EMARK | HAS_COMMENT,
	  .handler = &compare_cmd,     .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "copen",             .abbr = "cope",  .id = -1,
	  .descr = "reopen last displayed navigation menu",
	  .flags = HAS_COMMENT,
	  .handler = &copen_cmd,       .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "copy",              .abbr = "co",    .id = COM_COPY,
	  .descr = "copy files",
	  .flags = HAS_EMARK | HAS_RANGE | HAS_BG_FLAG | HAS_QUOTED_ARGS | HAS_COMMENT
	         | HAS_QMARK_NO_ARGS | HAS_SELECTION_SCOPE,
	  .handler = &copy_cmd,        .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "cquit",             .abbr = "cq",    .id = -1,
	  .descr = "quit with error",
	  .flags = HAS_EMARK | HAS_COMMENT,
	  .handler = &cquit_cmd,       .min_args = 0,   .max_args = 0, },
	{ .name = "cunabbrev",         .abbr = "cuna",  .id = COM_CABBR,
	  .descr = "remove cmdline abbrev",
	  .flags = 0,
	  .handler = &cunabbrev_cmd,   .min_args = 1,   .max_args = NOT_DEF, },
	{ .name = "cunmap",            .abbr = "cu",    .id = -1,
	  .descr = "unmap user keys in cmdline mode",
	  .flags = HAS_RAW_ARGS,
	  .handler = &cunmap_cmd,      .min_args = 1,   .max_args = 1, },
	{ .name = "delete",            .abbr = "d",     .id = -1,
	  .descr = "delete files",
	  .flags = HAS_EMARK | HAS_RANGE | HAS_BG_FLAG | HAS_SELECTION_SCOPE,
	  .handler = &delete_cmd,      .min_args = 0,   .max_args = 2, },
	{ .name = "delmarks",          .abbr = "delm",  .id = -1,
	  .descr = "delete marks",
	  .flags = HAS_EMARK | HAS_COMMENT,
	  .handler = &delmarks_cmd,    .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "delbmarks",         .abbr = NULL,    .id = COM_DELBMARKS,
	  .descr = "delete bookmarks",
	  .flags = HAS_EMARK | HAS_COMMENT,
	  .handler = &delbmarks_cmd,   .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "delsession",        .abbr = NULL,    .id = COM_DELSESSION,
	  .descr = "remove a session",
	  .flags = HAS_COMMENT,
	  .handler = &delsession_cmd,  .min_args = 1,   .max_args = 1, },
	{ .name = "display",           .abbr = "di",    .id = -1,
	  .descr = "display registers",
	  .flags = 0,
	  .handler = &registers_cmd,   .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "dirs",              .abbr = NULL,    .id = -1,
	  .descr = "display directory stack",
	  .flags = HAS_COMMENT,
	  .handler = &dirs_cmd,        .min_args = 0,   .max_args = 0, },
	{ .name = "dmap",              .abbr = NULL,    .id = COM_DMAP,
	  .descr = "map keys in dialog modes",
	  .flags = HAS_RAW_ARGS,
	  .handler = &dmap_cmd,        .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "dnoremap",          .abbr = NULL,    .id = COM_DNOREMAP,
	  .descr = "noremap keys in dialog modes",
	  .flags = HAS_RAW_ARGS,
	  .handler = &dnoremap_cmd,    .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "dunmap",            .abbr = NULL,    .id = -1,
	  .descr = "unmap user keys in dialog modes",
	  .flags = HAS_RAW_ARGS,
	  .handler = &dunmap_cmd,      .min_args = 1,   .max_args = 1, },
	{ .name = "echo",             .abbr = "ec",     .id = COM_ECHO,
	  .descr = "eval and print expressions",
	  .flags = 0,
	  .handler = &echo_cmd,        .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "edit",              .abbr = "e",     .id = COM_EDIT,
	  .descr = "edit files",
	  .flags = HAS_RANGE | HAS_QUOTED_ARGS | HAS_COMMENT | HAS_MACROS_FOR_CMD
	         | HAS_SELECTION_SCOPE | HAS_ENVVARS,
	  .handler = &edit_cmd,        .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "else",              .abbr = "el",    .id = COM_ELSE_STMT,
	  .descr = "start alternative control-flow",
	  .flags = HAS_COMMENT,
	  .handler = &else_cmd,        .min_args = 0,   .max_args = 0, },
	/* engine/parsing unit handles comments to resolve parsing ambiguity. */
	{ .name = "elseif",            .abbr = "elsei", .id = COM_ELSEIF_STMT,
	  .descr = "conditional control-flow branching",
	  .flags = 0,
	  .handler = &elseif_cmd,      .min_args = 1,   .max_args = NOT_DEF, },
	{ .name = "empty",             .abbr = NULL,    .id = -1,
	  .descr = "start emptying trashes in background",
	  .flags = HAS_COMMENT,
	  .handler = &empty_cmd,       .min_args = 0,   .max_args = 0, },
	{ .name = "endif",             .abbr = "en",    .id = COM_ENDIF_STMT,
	  .descr = "if-else construction terminator",
	  .flags = HAS_COMMENT,
	  .handler = &endif_cmd,       .min_args = 0,   .max_args = 0, },
	{ .name = "execute",           .abbr = "exe",   .id = COM_EXE,
	  .descr = "execute expressions as :commands",
	  .flags = 0,
	  .handler = &exe_cmd,         .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "exit",              .abbr = "exi",   .id = -1,
	  .descr = "exit the application",
	  .flags = HAS_EMARK | HAS_COMMENT,
	  .handler = &quit_cmd,        .min_args = 0,   .max_args = 0, },
	{ .name = "file",              .abbr = "f",     .id = COM_FILE,
	  .descr = "display/apply file associations",
	  .flags = HAS_BG_FLAG | HAS_COMMENT,
	  .handler = &file_cmd,        .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "filetype",          .abbr = "filet", .id = COM_FILETYPE,
	  .descr = "display/define file associations",
	  .flags = 0,
	  .handler = &filetype_cmd,    .min_args = 1,   .max_args = NOT_DEF, },
	{ .name = "fileviewer",        .abbr = "filev", .id = COM_FILEVIEWER,
	  .descr = "display/define file viewers",
	  .flags = 0,
	  .handler = &fileviewer_cmd,  .min_args = 1,   .max_args = NOT_DEF, },
	{ .name = "filextype",         .abbr = "filex", .id = COM_FILEXTYPE,
	  .descr = "display/define file associations in X",
	  .flags = 0,
	  .handler = &filextype_cmd,   .min_args = 1,   .max_args = NOT_DEF, },
	{ .name = "filter",            .abbr = NULL,    .id = COM_FILTER,
	  .descr = "set/reset file filter",
	  .flags = HAS_EMARK | HAS_REGEXP_ARGS | HAS_QMARK_NO_ARGS,
	  .handler = &filter_cmd,      .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "find",              .abbr = "fin",   .id = COM_FIND,
	  .descr = "query find results",
	  .flags = HAS_RANGE | HAS_QUOTED_ARGS | HAS_MACROS_FOR_CMD
	         | HAS_SELECTION_SCOPE,
	  .handler = &find_cmd,        .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "finish",            .abbr = "fini",  .id = -1,
	  .descr = "stop script processing",
	  .flags = HAS_COMMENT,
	  .handler = &finish_cmd,      .min_args = 0,   .max_args = 0, },
	{ .name = "goto",              .abbr = "go",    .id = COM_GOTO_PATH,
	  .descr = "navigate to specified file/directory",
	  .flags = HAS_ENVVARS | HAS_COMMENT | HAS_MACROS_FOR_CMD | HAS_QUOTED_ARGS,
	  .handler = &goto_path_cmd,   .min_args = 1,   .max_args = 1, },
	{ .name = "grep",              .abbr = "gr",    .id = COM_GREP,
	  .descr = "query grep results",
	  .flags = HAS_EMARK | HAS_RANGE | HAS_SELECTION_SCOPE,
	  .handler = &grep_cmd,        .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "help",              .abbr = "h",     .id = COM_HELP,
	  .descr = "display help",
	  .flags = HAS_QUOTED_ARGS,
	  .handler = &help_cmd,        .min_args = 0,   .max_args = 1, },
	{ .name = "hideui",            .abbr = NULL,    .id = -1,
	  .descr = "hide interface to show previous commands' output",
	  .flags = HAS_COMMENT,
	  .handler = &hideui_cmd,      .min_args = 0,   .max_args = 0, },
	{ .name = "highlight",         .abbr = "hi",    .id = COM_HIGHLIGHT,
	  .descr = "display/define TUI highlighting",
	  .flags = HAS_COMMENT,
	  .handler = &highlight_cmd,   .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "history",           .abbr = "his",   .id = COM_HISTORY,
	  .descr = "display/use history items",
	  .flags = HAS_QUOTED_ARGS | HAS_COMMENT,
	  .handler = &history_cmd,     .min_args = 0,   .max_args = 1, },
	{ .name = "histnext",          .abbr = NULL,    .id = -1,
	  .descr = "go forward through directory history",
	  .flags = HAS_COMMENT,
	  .handler = &histnext_cmd,    .min_args = 0,   .max_args = 0, },
	{ .name = "histprev",          .abbr = NULL,    .id = -1,
	  .descr = "go backward through directory history",
	  .flags = HAS_COMMENT,
	  .handler = &histprev_cmd,    .min_args = 0,   .max_args = 0, },
	/* engine/parsing unit handles comments to resolve parsing ambiguity. */
	{ .name = "if",                .abbr = NULL,    .id = COM_IF_STMT,
	  .descr = "start conditional statement",
	  .flags = 0,
	  .handler = &if_cmd,          .min_args = 1,   .max_args = NOT_DEF, },
	{ .name = "invert",            .abbr = NULL,    .id = COM_INVERT,
	  .descr = "invert filter/selection/sorting",
	  .flags = HAS_COMMENT | HAS_QMARK_WITH_ARGS,
	  .handler = &invert_cmd,      .min_args = 0,   .max_args = 1, },
	{ .name = "jobs",              .abbr = NULL,    .id = -1,
	  .descr = "display active jobs",
	  .flags = HAS_COMMENT,
	  .handler = &jobs_cmd,        .min_args = 0,   .max_args = 0, },
	{ .name = "keepsel",           .abbr = NULL,    .id = COM_KEEPSEL,
	  .descr = "preserve selection during :command by default",
	  .flags = 0,
	  .handler = &keepsel_cmd,     .min_args = 0,   .max_args = NOT_DEF, },
	/* engine/parsing unit handles comments to resolve parsing ambiguity. */
	{ .name = "let",               .abbr = NULL,    .id = COM_LET,
	  .descr = "assign variables",
	  .flags = 0,
	  .handler = &let_cmd,         .min_args = 1,   .max_args = NOT_DEF, },
	{ .name = "locate",            .abbr = NULL,    .id = -1,
	  .descr = "query locate results",
	  .flags = 0,
	  .handler = &locate_cmd,      .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "ls",                .abbr = NULL,    .id = -1,
	  .descr = "list terminal multiplexer windows",
	  .flags = HAS_COMMENT,
	  .handler = &ls_cmd,          .min_args = 0,   .max_args = 0, },
	{ .name = "lstrash",           .abbr = NULL,    .id = -1,
	  .descr = "list files in trashes",
	  .flags = HAS_COMMENT,
	  .handler = &lstrash_cmd,     .min_args = 0,   .max_args = 0, },
	{ .name = "map",               .abbr = NULL,    .id = COM_MAP,
	  .descr = "map keys in normal and visual modes",
	  .flags = HAS_EMARK | HAS_RAW_ARGS,
	  .handler = &map_cmd,         .min_args = 2,   .max_args = NOT_DEF, },
	{ .name = "mark",              .abbr = "ma",    .id = -1,
	  .descr = "set mark",
	  .flags = HAS_RANGE | HAS_QUOTED_ARGS | HAS_COMMENT | HAS_QMARK_WITH_ARGS
	         | HAS_MACROS_FOR_CMD,
	  .handler = &mark_cmd,        .min_args = 1,   .max_args = 3, },
	{ .name = "marks",             .abbr = NULL,    .id = -1,
	  .descr = "display marks",
	  .flags = HAS_COMMENT,
	  .handler = &marks_cmd,       .min_args = 0,   .max_args = NOT_DEF, },
#ifndef _WIN32
	{ .name = "media",             .abbr = NULL,    .id = -1,
	  .descr = "list and manage media devices",
	  .flags = HAS_COMMENT,
	  .handler = &media_cmd,       .min_args = 0,   .max_args = 0, },
#endif
	{ .name = "messages",          .abbr = "mes",   .id = -1,
	  .descr = "display previous status bar messages",
	  .flags = HAS_COMMENT,
	  .handler = &messages_cmd,    .min_args = 0,   .max_args = 0, },
	{ .name = "mkdir",             .abbr = NULL,    .id = COM_MKDIR,
	  .descr = "create directories",
	  .flags = HAS_EMARK | HAS_RANGE | HAS_QUOTED_ARGS | HAS_COMMENT
	         | HAS_MACROS_FOR_CMD,
	  .handler = &mkdir_cmd,       .min_args = 1,   .max_args = NOT_DEF, },
	{ .name = "mmap",              .abbr = "mm",    .id = COM_MMAP,
	  .descr = "map keys in menu mode",
	  .flags = HAS_RAW_ARGS,
	  .handler = &mmap_cmd,        .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "mnoremap",          .abbr = "mno",   .id = COM_MNOREMAP,
	  .descr = "noremap keys in menu mode",
	  .flags = HAS_RAW_ARGS,
	  .handler = &mnoremap_cmd,    .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "move",              .abbr = "m",     .id = COM_MOVE,
	  .descr = "move files",
	  .flags = HAS_EMARK | HAS_RANGE | HAS_BG_FLAG | HAS_QUOTED_ARGS | HAS_COMMENT
	         | HAS_QMARK_NO_ARGS | HAS_SELECTION_SCOPE,
	  .handler = &move_cmd,        .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "munmap",            .abbr = "mu",    .id = -1,
	  .descr = "unmap user keys in menu mode",
	  .flags = HAS_RAW_ARGS,
	  .handler = &munmap_cmd,      .min_args = 1,   .max_args = 1, },
	{ .name = "nmap",              .abbr = "nm",    .id = COM_NMAP,
	  .descr = "map keys in normal mode",
	  .flags = HAS_RAW_ARGS,
	  .handler = &nmap_cmd,        .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "nnoremap",          .abbr = "nn",    .id = COM_NNOREMAP,
	  .descr = "noremap keys in normal mode",
	  .flags = HAS_RAW_ARGS,
	  .handler = &nnoremap_cmd,    .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "nohlsearch",        .abbr = "noh",   .id = -1,
	  .descr = "reset highlighting of search matches",
	  .flags = HAS_COMMENT,
	  .handler = &nohlsearch_cmd,  .min_args = 0,   .max_args = 0, },
	{ .name = "noremap",           .abbr = "no",    .id = COM_NOREMAP,
	  .descr = "noremap keys in normal and visual modes",
	  .flags = HAS_EMARK | HAS_RAW_ARGS,
	  .handler = &noremap_cmd,     .min_args = 2,   .max_args = NOT_DEF, },
	{ .name = "normal",            .abbr = "norm",  .id = COM_NORMAL,
	  .descr = "emulate keys typed in normal mode",
	  .flags = HAS_EMARK,
	  .handler = &normal_cmd,      .min_args = 1,   .max_args = NOT_DEF, },
	{ .name = "nunmap",            .abbr = "nun",   .id = -1,
	  .descr = "unmap user keys in normal mode",
	  .flags = HAS_RAW_ARGS,
	  .handler = &nunmap_cmd,      .min_args = 1,   .max_args = 1, },
	{ .name = "only",              .abbr = "on",    .id = -1,
	  .descr = "switch to single-view mode",
	  .flags = HAS_COMMENT,
	  .handler = &only_cmd,        .min_args = 0,   .max_args = 0, },
	{ .name = "plugin",            .abbr = NULL,    .id = COM_PLUGIN,
	  .descr = "manage plugins",
	  .flags = HAS_COMMENT,
	  .handler = &plugin_cmd,      .min_args = 1,   .max_args = 2, },
	{ .name = "plugins",           .abbr = NULL,    .id = -1,
	  .descr = "display plugins menu",
	  .flags = HAS_COMMENT,
	  .handler = &plugins_cmd,     .min_args = 0,   .max_args = 0, },
	{ .name = "popd",              .abbr = NULL,    .id = -1,
	  .descr = "pop top of directory stack",
	  .flags = HAS_COMMENT,
	  .handler = &popd_cmd,        .min_args = 0,   .max_args = 0, },
	{ .name = "pushd",             .abbr = NULL,    .id = COM_PUSHD,
	  .descr = "push onto directory stack",
	  .flags = HAS_EMARK | HAS_QUOTED_ARGS | HAS_COMMENT | HAS_ENVVARS,
	  .handler = &pushd_cmd,       .min_args = 0,   .max_args = 2, },
	{ .name = "put",               .abbr = "pu",    .id = -1,
	  .descr = "paste files from a register",
	  .flags = HAS_EMARK | HAS_RANGE | HAS_BG_FLAG,
	  .handler = &put_cmd,         .min_args = 0,   .max_args = 1, },
	{ .name = "pwd",               .abbr = "pw",    .id = -1,
	  .descr = "display current location",
	  .flags = HAS_COMMENT,
	  .handler = &pwd_cmd,         .min_args = 0,   .max_args = 0, },
	{ .name = "qall",              .abbr = "qa",    .id = -1,
	  .descr = "close all tabs and exit the application",
	  .flags = HAS_EMARK | HAS_COMMENT,
	  .handler = &qall_cmd,        .min_args = 0,   .max_args = 0, },
	{ .name = "qmap",              .abbr = "qm",    .id = COM_QMAP,
	  .descr = "map keys in preview mode",
	  .flags = HAS_RAW_ARGS,
	  .handler = &qmap_cmd,        .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "qnoremap",          .abbr = "qno",   .id = COM_QNOREMAP,
	  .descr = "noremap keys in preview mode",
	  .flags = HAS_RAW_ARGS,
	  .handler = &qnoremap_cmd,    .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "quit",              .abbr = "q",     .id = -1,
	  .descr = "close a tab or exit the application",
	  .flags = HAS_EMARK | HAS_COMMENT,
	  .handler = &quit_cmd,        .min_args = 0,   .max_args = 0, },
	{ .name = "qunmap",            .abbr = "qun",   .id = -1,
	  .descr = "unmap user keys in preview mode",
	  .flags = HAS_RAW_ARGS,
	  .handler = &qunmap_cmd,      .min_args = 1,   .max_args = 1, },
	{ .name = "redraw",            .abbr = "redr",  .id = -1,
	  .descr = "force screen redraw",
	  .flags = HAS_COMMENT,
	  .handler = &redraw_cmd,      .min_args = 0,   .max_args = 0, },
	{ .name = "regedit",           .abbr = "rege",  .id = -1,
	  .descr = "edit register contents",
	  .flags = HAS_COMMENT | HAS_RAW_ARGS,
	  .handler = &regedit_cmd,     .min_args = 0,   .max_args = 1 },
	{ .name = "registers",         .abbr = "reg",   .id = -1,
	  .descr = "display registers",
	  .flags = 0,
	  .handler = &registers_cmd,   .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "regular",           .abbr = NULL,    .id = -1,
	  .descr = "switch to regular view leaving custom view",
	  .flags = HAS_COMMENT,
	  .handler = &regular_cmd,     .min_args = 0,   .max_args = 0, },
	{ .name = "rename",            .abbr = NULL,    .id = COM_RENAME,
	  .descr = "rename files",
	  .flags = HAS_EMARK | HAS_RANGE | HAS_QUOTED_ARGS | HAS_COMMENT
	         | HAS_SELECTION_SCOPE,
	  .handler = &rename_cmd,      .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "restart",           .abbr = NULL,    .id = -1,
	  .descr = "reset state and reread configuration",
	  .flags = HAS_COMMENT,
	  .handler = &restart_cmd,     .min_args = 0,   .max_args = 1, },
	{ .name = "restore",           .abbr = NULL,    .id = -1,
	  .descr = "restore files from a trash",
	  .flags = HAS_RANGE | HAS_COMMENT | HAS_SELECTION_SCOPE,
	  .handler = &restore_cmd,     .min_args = 0,   .max_args = 0, },
	{ .name = "rlink",             .abbr = NULL,    .id = COM_RLINK,
	  .descr = "create relative links",
	  .flags = HAS_EMARK | HAS_RANGE | HAS_QUOTED_ARGS | HAS_COMMENT
	         | HAS_QMARK_NO_ARGS | HAS_SELECTION_SCOPE,
	  .handler = &rlink_cmd,       .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "screen",            .abbr = NULL,    .id = -1,
	  .descr = "view/toggle terminal multiplexer support",
	  .flags = HAS_COMMENT | HAS_EMARK | HAS_QMARK_NO_ARGS,
	  .handler = &screen_cmd,      .min_args = 0,   .max_args = 0, },
	{ .name = "select",            .abbr = NULL,    .id = COM_SELECT,
	  .descr = "select files matching pattern or range",
	  .flags = HAS_EMARK | HAS_RANGE | HAS_REGEXP_ARGS,
	  .handler = &select_cmd,      .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "session",           .abbr = NULL,    .id = COM_SESSION,
	  .descr = "shows, detaches or switches active session",
	  .flags = HAS_COMMENT | HAS_QMARK_NO_ARGS,
	  .handler = &session_cmd,     .min_args = 0,   .max_args = 1, },
	/* engine/options unit handles comments to resolve parsing ambiguity. */
	{ .name = "set",               .abbr = "se",    .id = COM_SET,
	  .descr = "set global and local options",
	  .flags = 0,
	  .handler = &set_cmd,         .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "setlocal",          .abbr = "setl",  .id = COM_SETLOCAL,
	  .descr = "set local options",
	  .flags = 0,
	  .handler = &setlocal_cmd,    .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "setglobal",         .abbr = "setg",  .id = COM_SET,
	  .descr = "set global options",
	  .flags = 0,
	  .handler = &setglobal_cmd,   .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "shell",             .abbr = "sh",    .id = -1,
	  .descr = "spawn shell",
	  .flags = HAS_EMARK | HAS_COMMENT,
	  .handler = &shell_cmd,       .min_args = 0,   .max_args = 0, },
	{ .name = "siblnext",          .abbr = NULL,    .id = -1,
	  .descr = "navigate to next sibling directory",
	  .flags = HAS_RANGE | HAS_EMARK | HAS_COMMENT,
	  .handler = &siblnext_cmd,    .min_args = 0,   .max_args = 0, },
	{ .name = "siblprev",          .abbr = NULL,    .id = -1,
	  .descr = "navigate to previous sibling directory",
	  .flags = HAS_RANGE | HAS_EMARK | HAS_COMMENT,
	  .handler = &siblprev_cmd,    .min_args = 0,   .max_args = 0, },
	{ .name = "sort",              .abbr = "sor",   .id = -1,
	  .descr = "display sorting dialog",
	  .flags = HAS_COMMENT,
	  .handler = &sort_cmd,        .min_args = 0,   .max_args = 0, },
	{ .name = "source",            .abbr = "so",    .id = COM_SOURCE,
	  .descr = "source file with :commands",
	  .flags = HAS_QUOTED_ARGS | HAS_COMMENT | HAS_ENVVARS,
	  .handler = &source_cmd,      .min_args = 1,   .max_args = 1, },
	{ .name = "split",             .abbr = "sp",    .id = COM_SPLIT,
	  .descr = "horizontal split layout",
	  .flags = HAS_EMARK | HAS_COMMENT,
	  .handler = &split_cmd,       .min_args = 0,   .max_args = 1, },
	{ .name = "stop",              .abbr = "st",    .id = -1,
	  .descr = "suspend the process (same as pressing Ctrl-Z)",
	  .flags = HAS_COMMENT,
	  .handler = &stop_cmd,        .min_args = 0,   .max_args = 0, },
	{ .name = "substitute",        .abbr = "s",     .id = COM_SUBSTITUTE,
	  .descr = "perform substitutions in file names",
	  .flags = HAS_RANGE | HAS_REGEXP_ARGS | HAS_COMMENT | HAS_CUST_SEP
	         | HAS_SELECTION_SCOPE,
	  .handler = &substitute_cmd,  .min_args = 0,   .max_args = 3, },
	{ .name = "sync",              .abbr = NULL,    .id = COM_SYNC,
	  .descr = "synchronize properties of views",
	  .flags = HAS_EMARK | HAS_COMMENT | HAS_MACROS_FOR_CMD,
	  .handler = &sync_cmd,        .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "tabclose",          .abbr = "tabc",  .id = -1,
	  .descr = "close current tab unless it's the only one",
	  .flags = HAS_COMMENT,
	  .handler = &tabclose_cmd,    .min_args = 0,   .max_args = 0, },
	{ .name = "tabmove",           .abbr = "tabm",  .id = -1,
	  .descr = "position current tab after another tab",
	  .flags = HAS_COMMENT,
	  .handler = &tabmove_cmd,     .min_args = 0,   .max_args = 1, },
	{ .name = "tabname",           .abbr = NULL,    .id = -1,
	  .descr = "set name of current tab",
	  .flags = HAS_COMMENT,
	  .handler = &tabname_cmd,     .min_args = 0,   .max_args = 1, },
	{ .name = "tabnew",            .abbr = NULL,    .id = COM_TABNEW,
	  .descr = "make new tab and switch to it",
	  .flags = HAS_QUOTED_ARGS | HAS_ENVVARS | HAS_MACROS_FOR_CMD | HAS_COMMENT,
	  .handler = &tabnew_cmd,      .min_args = 0,   .max_args = 1, },
	{ .name = "tabnext",           .abbr = "tabn",  .id = -1,
	  .descr = "go to next or n-th tab",
	  .flags = HAS_COMMENT,
	  .handler = &tabnext_cmd,     .min_args = 0,   .max_args = 1, },
	{ .name = "tabonly",           .abbr = "tabo",  .id = -1,
	  .descr = "close all tabs but the current one",
	  .flags = HAS_COMMENT,
	  .handler = &tabonly_cmd,     .min_args = 0,   .max_args = 0, },
	{ .name = "tabprevious",       .abbr = "tabp",  .id = -1,
	  .descr = "go to previous or n-th previous tab",
	  .flags = HAS_COMMENT,
	  .handler = &tabprevious_cmd, .min_args = 0,   .max_args = 1, },
	{ .name = "touch",             .abbr = NULL,    .id = COM_TOUCH,
	  .descr = "create files",
	  .flags = HAS_RANGE | HAS_QUOTED_ARGS | HAS_COMMENT | HAS_MACROS_FOR_CMD,
	  .handler = &touch_cmd,       .min_args = 1,   .max_args = NOT_DEF, },
	{ .name = "tr",                .abbr = NULL,    .id = COM_TR,
	  .descr = "replace characters in file names",
	  .flags = HAS_RANGE | HAS_REGEXP_ARGS | HAS_COMMENT | HAS_CUST_SEP
	         | HAS_SELECTION_SCOPE,
	  .handler = &tr_cmd,          .min_args = 2,   .max_args = 2, },
	{ .name = "trashes",           .abbr = NULL,    .id = -1,
	  .descr = "display trash directories",
	  .flags = HAS_COMMENT | HAS_QMARK_NO_ARGS,
	  .handler = &trashes_cmd,     .min_args = 0,   .max_args = 0, },
	{ .name = "tree",              .abbr = NULL,    .id = COM_TREE,
	  .descr = "display filesystem as a tree",
	  .flags = HAS_EMARK | HAS_COMMENT,
	  .handler = &tree_cmd,        .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "undolist",          .abbr = "undol", .id = -1,
	  .descr = "display list of operations",
	  .flags = HAS_EMARK | HAS_COMMENT,
	  .handler = &undolist_cmd,    .min_args = 0,   .max_args = 0, },
	{ .name = "unlet",             .abbr = "unl",   .id = COM_UNLET,
	  .descr = "undefine variable",
	  .flags = HAS_EMARK | HAS_COMMENT,
	  .handler = &unlet_cmd,       .min_args = 1,   .max_args = NOT_DEF, },
	{ .name = "unmap",             .abbr = "unm",   .id = -1,
	  .descr = "unmap user keys in normal and visual modes",
	  .flags = HAS_EMARK | HAS_RAW_ARGS,
	  .handler = &unmap_cmd,       .min_args = 1,   .max_args = 1, },
	{ .name = "unselect",          .abbr = NULL,    .id = COM_SELECT,
	  .descr = "unselect files matching pattern or range",
	  .flags = HAS_RANGE | HAS_REGEXP_ARGS,
	  .handler = &unselect_cmd,    .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "version",           .abbr = "ve",    .id = -1,
	  .descr = "display version information",
	  .flags = HAS_COMMENT,
	  .handler = &vifm_cmd,        .min_args = 0,   .max_args = 0, },
	{ .name = "view",              .abbr = "vie",   .id = -1,
	  .descr = "control visibility of preview",
	  .flags = HAS_EMARK | HAS_COMMENT,
	  .handler = &view_cmd,        .min_args = 0,   .max_args = 0, },
	{ .name = "vifm",              .abbr = NULL,    .id = -1,
	  .descr = "display version information",
	  .flags = HAS_COMMENT,
	  .handler = &vifm_cmd,        .min_args = 0,   .max_args = 0, },
	{ .name = "vmap",              .abbr = "vm",    .id = COM_VMAP,
	  .descr = "map keys in visual mode",
	  .flags = HAS_RAW_ARGS,
	  .handler = &vmap_cmd,        .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "vnoremap",          .abbr = "vn",    .id = COM_VNOREMAP,
	  .descr = "noremap keys in visual mode",
	  .flags = HAS_RAW_ARGS,
	  .handler = &vnoremap_cmd,    .min_args = 0,   .max_args = NOT_DEF, },
#ifdef _WIN32
	{ .name = "volumes",           .abbr = NULL,    .id = -1,
	  .descr = "display list of drives",
	  .flags = HAS_COMMENT,
	  .handler = &volumes_cmd,     .min_args = 0,   .max_args = 0, },
#endif
	{ .name = "vsplit",            .abbr = "vs",    .id = COM_VSPLIT,
	  .descr = "vertical split layout",
	  .flags = HAS_EMARK | HAS_COMMENT,
	  .handler = &vsplit_cmd,      .min_args = 0,   .max_args = 1, },
	{ .name = "vunmap",            .abbr = "vu",    .id = -1,
	  .descr = "unmap user keys in visual mode",
	  .flags = HAS_RAW_ARGS,
	  .handler = &vunmap_cmd,      .min_args = 1,   .max_args = 1, },
	{ .name = "wincmd",            .abbr = "winc",  .id = COM_WINCMD,
	  .descr = "cmdline Ctrl-W substitute",
	  .flags = HAS_RANGE,
	  .handler = &wincmd_cmd,      .min_args = 1,   .max_args = 1, },
	{ .name = "windo",             .abbr = NULL,    .id = COM_WINDO,
	  .descr = "run command for each pane",
	  .flags = 0,
	  .handler = &windo_cmd,       .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "winrun",            .abbr = NULL,    .id = COM_WINRUN,
	  .descr = "run command for specific pane(s)",
	  .flags = 0,
	  .handler = &winrun_cmd,      .min_args = 0,   .max_args = NOT_DEF, },
	{ .name = "write",             .abbr = "w",     .id = -1,
	  .descr = "write vifminfo file",
	  .flags = HAS_COMMENT,
	  .handler = &write_cmd,       .min_args = 0,   .max_args = 0, },
	{ .name = "wq",                .abbr = NULL,    .id = -1,
	  .descr = "close a tab or exit the application",
	  .flags = HAS_EMARK | HAS_COMMENT,
	  .handler = &wq_cmd,          .min_args = 0,   .max_args = 0, },
	{ .name = "wqall",             .abbr = "wqa",   .id = -1,
	  .descr = "exit the application",
	  .flags = HAS_EMARK | HAS_COMMENT,
	  .handler = &wqall_cmd,       .min_args = 0,   .max_args = 0, },
	{ .name = "xall",              .abbr = "xa",    .id = -1,
	  .descr = "exit the application",
	  .flags = HAS_COMMENT,
	  .handler = &qall_cmd,        .min_args = 0,   .max_args = 0, },
	{ .name = "xit",               .abbr = "x",     .id = -1,
	  .descr = "exit the application",
	  .flags = HAS_COMMENT,
	  .handler = &quit_cmd,        .min_args = 0,   .max_args = 0, },
	{ .name = "yank",              .abbr = "y",     .id = -1,
	  .descr = "yank files",
	  .flags = HAS_RANGE | HAS_SELECTION_SCOPE,
	  .handler = &yank_cmd,        .min_args = 0,   .max_args = 2, },

	{ .name = "<USERCMD>",         .abbr = NULL,    .id = -1,
	  .descr = "user-defined command",
	  .flags = HAS_RANGE | HAS_QUOTED_ARGS | HAS_MACROS_FOR_CMD
	         | HAS_SELECTION_SCOPE,
	  .handler = &usercmd_cmd,     .min_args = 0,   .max_args = NOT_DEF, },
};
const size_t cmds_list_size = ARRAY_LEN(cmds_list);

/* Holds global state of command handlers. */
static struct
{
	/* For :find command. */
	struct
	{
		char *last_args;   /* Last arguments passed to the command */
		int includes_path; /* Whether last_args contains path to search in. */
	}
	find;
}
cmds_state;

/* Return value of all functions below which name ends with "_cmd" mean:
 *  - <0 -- one of CMDS_* errors from cmds.h;
 *  - =0 -- nothing was outputted to the status bar, don't need to save its
 *          state;
 *  - >0 -- something was printed to the status bar, need to save its state. */
static int
goto_cmd(const cmd_info_t *cmd_info)
{
	cmds_preserve_selection();
	fpos_set_pos(curr_view, cmd_info->end);
	return 0;
}

/* Handles :! command, which executes external command via shell. */
static int
emark_cmd(const cmd_info_t *cmd_info)
{
	int save_msg = 0;
	const char *com = cmd_info->args;
	char buf[COMMAND_GROUP_INFO_LEN];

	if(cmd_info->argc == 0)
	{
		if(cmd_info->emark)
		{
			const char *const last_cmd = curr_stats.last_cmdline_command;
			if(last_cmd == NULL)
			{
				ui_sb_msg("No previous command-line command");
				return 1;
			}
			return cmds_dispatch(last_cmd, curr_view, CIT_COMMAND) != 0;
		}
		return CMDS_ERR_TOO_FEW_ARGS;
	}

	com = skip_whitespace(com);
	if(com[0] == '\0')
	{
		return 0;
	}

	MacroFlags flags = (MacroFlags)cmd_info->usr1;
	char *title = format_str("!%s", cmd_info->raw_args);
	int handled = rn_ext(curr_view, com, title, flags, cmd_info->bg, &save_msg);
	free(title);

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
		rn_start_bg_command(curr_view, com, flags);
	}
	else
	{
		const int use_term_mux = ma_flags_missing(flags, MF_NO_TERM_MUX);
		const ShellPause pause = (cmd_info->emark ? PAUSE_ALWAYS : PAUSE_ON_ERROR);

		flist_sel_stash(curr_view);

		const char *cmd_to_run = com;
		char *expanded_cmd = NULL;

		if(cfg.fast_run)
		{
			expanded_cmd = fast_run_complete(com);
			if(expanded_cmd != NULL)
			{
				cmd_to_run = expanded_cmd;
			}
		}

		if(ma_flags_present(flags, MF_PIPE_FILE_LIST) ||
				ma_flags_present(flags, MF_PIPE_FILE_LIST_Z))
		{
			(void)rn_pipe(cmd_to_run, curr_view, flags, pause);
		}
		else
		{
			(void)rn_shell(cmd_to_run, pause, use_term_mux, SHELL_BY_USER);
		}

		free(expanded_cmd);
	}

	snprintf(buf, sizeof(buf), "in %s: !%s",
			replace_home_part(flist_get_dir(curr_view)), cmd_info->raw_args);
	un_group_open(buf);
	un_group_add_op(OP_USR, strdup(com), NULL, "", "");
	un_group_close();

	return save_msg;
}

/* Creates symbolic links with absolute paths to files. */
static int
alink_cmd(const cmd_info_t *cmd_info)
{
	return link_cmd(cmd_info, 1);
}

/* Registers a navigation mapping that allows remapping. */
static int
amap_cmd(const cmd_info_t *cmd_info)
{
	return do_map(cmd_info, "Navigation", NAV_MODE, /*no_remap=*/0) != 0;
}

/* Registers a navigation mapping that doesn't allow remapping. */
static int
anoremap_cmd(const cmd_info_t *cmd_info)
{
	return do_map(cmd_info, "Navigation", NAV_MODE, /*no_remap=*/1) != 0;
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
		ui_sb_err("Nothing to repeat");
		return CMDS_ERR_CUSTOM;
	}

	return show_apropos_menu(curr_view, last_args) != 0;
}

/* Adds/lists/removes autocommands. */
static int
autocmd_cmd(const cmd_info_t *cmd_info)
{
	enum { ADDITION, LISTING, REMOVAL } type = cmd_info->emark
	                                         ? REMOVAL : (cmd_info->argc < 3)
	                                         ? LISTING : ADDITION;

	const char *event = NULL;
	const char *patterns = NULL;
	const char *action;

	/* Check usage. */
	if(cmd_info->emark && cmd_info->argc > 2)
	{
		return CMDS_ERR_TRAILING_CHARS;
	}

	/* Parse event and patterns. */
	if(cmd_info->argc > 0)
	{
		if(type == ADDITION || strcmp(cmd_info->argv[0], "*") != 0)
		{
			event = cmd_info->argv[0];
		}
		if(cmd_info->argc > 1)
		{
			patterns = cmd_info->argv[1];
		}
	}

	/* Check validity of event. */
	if(event != NULL)
	{
		/* Non-const for is_in_string_array_case(). */
		static char *events[] = { "DirEnter" };
		if(!is_in_string_array_case(events, ARRAY_LEN(events), event))
		{
			ui_sb_errf("No such event: %s", event);
			return CMDS_ERR_CUSTOM;
		}
	}

	if(type == REMOVAL)
	{
		vle_aucmd_remove(event, patterns);
		return 0;
	}

	if(type == LISTING)
	{
		vle_textbuf *const msg = vle_tb_create();
		vle_aucmd_list(event, patterns, &aucmd_list_cb, msg);
		ui_sb_msg(vle_tb_get_data(msg));
		vle_tb_free(msg);
		return 1;
	}

	/* Addition. */

	action = &cmd_info->args[cmd_info->argvp[2][0]];

	if(vle_aucmd_on_execute(event, patterns, action, &aucmd_action_handler) != 0)
	{
		ui_sb_err("Failed to register autocommand");
		return CMDS_ERR_CUSTOM;
	}

	return 0;
}

/* Implementation of autocommand action. */
static void
aucmd_action_handler(const char action[], void *arg)
{
	view_t *view = arg;
	view_t *tmp_curr, *tmp_other;
	ui_view_pick(view, &tmp_curr, &tmp_other);

	char *saved_cwd = save_cwd();
	(void)vifm_chdir(flist_get_dir(view));

	const int prev_global_local_settings = curr_stats.global_local_settings;
	curr_stats.global_local_settings = 0;

	(void)cmds_dispatch(action, view, CIT_COMMAND);

	curr_stats.global_local_settings = prev_global_local_settings;

	restore_cwd(saved_cwd);

	ui_view_unpick(view, tmp_curr, tmp_other);
}

/* Handler of list callback for autocommands. */
static void
aucmd_list_cb(const char event[], const char pattern[], int negated,
		const char action[], void *arg)
{
	vle_textbuf *msg = arg;
	const char *fmt = (strlen(pattern) <= 10)
	                ? "%-10s %s%-10s %s"
	                : "%-10s %s%-10s\n                      %s";

	vle_tb_append_linef(msg, fmt, event, negated ? "!" : "", pattern, action);
}

/* Unregisters a navigation mapping. */
static int
aunmap_cmd(const cmd_info_t *cmd_info)
{
	return do_unmap(cmd_info->argv[0], NAV_MODE);
}

/* Marks directory with set of tags. */
static int
bmark_cmd(const cmd_info_t *cmd_info)
{
	char *const tags = make_tags_list(cmd_info);
	char *const path = get_bmark_dir(cmd_info);
	const int err = (tags == NULL || bmarks_set(path, tags) != 0);
	if(err && tags != NULL)
	{
		ui_sb_err("Failed to add bookmark");
	}
	free(path);
	free(tags);
	return (err ? CMDS_ERR_CUSTOM : 0);
}

/* Lists either all bookmarks or those matching specified tags. */
static int
bmarks_cmd(const cmd_info_t *cmd_info)
{
	return bmarks_do(cmd_info, 0);
}

/* When there are more than 1 match acts like :bmarks, otherwise navigates to
 * single match immediately. */
static int
bmgo_cmd(const cmd_info_t *cmd_info)
{
	return bmarks_do(cmd_info, 1);
}

/* Runs bookmarks menu in either view or go mode (different only on single
 * match). */
static int
bmarks_do(const cmd_info_t *cmd_info, int go)
{
	char *const tags = args_to_csl(cmd_info);
	const int result = (show_bmarks_menu(curr_view, tags, go) != 0);
	free(tags);
	return result;
}

/* Converts command arguments into comma-separated list of tags.  Returns newly
 * allocated string or NULL on error or invalid tag name (in which case an error
 * is printed on the status bar). */
static char *
make_tags_list(const cmd_info_t *cmd_info)
{
	int i;

	if(cmd_info->emark && cmd_info->argc == 1)
	{
		ui_sb_err("Too few arguments");
		return NULL;
	}

	for(i = cmd_info->emark ? 1 : 0; i < cmd_info->argc; ++i)
	{
		if(strpbrk(cmd_info->argv[i], ", \t") != NULL)
		{
			ui_sb_errf("Tags can't include comma or whitespace: %s",
					cmd_info->argv[i]);
			return NULL;
		}
	}

	return args_to_csl(cmd_info);
}

/* Makes comma-separated list from command line arguments.  Returns newly
 * allocated string or NULL on absence of arguments. */
static char *
args_to_csl(const cmd_info_t *cmd_info)
{
	int i;
	char *tags = NULL;
	size_t len = 0U;

	if(cmd_info->argc == 0)
	{
		return NULL;
	}

	i = cmd_info->emark ? 1 : 0;
	strappend(&tags, &len, cmd_info->argv[i]);
	for(++i; i < cmd_info->argc; ++i)
	{
		strappendch(&tags, &len, ',');
		strappend(&tags, &len, cmd_info->argv[i]);
	}

	return tags;
}

/* Registers command-line mode abbreviation. */
static int
cabbrev_cmd(const cmd_info_t *cmd_info)
{
	return handle_cabbrevs(cmd_info, 0);
}

/* Displays list of remembered menus. */
static int
chistory_cmd(const cmd_info_t *cmd_info)
{
	return show_chistory_menu(curr_view) != 0;
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
	size_t prefix_len;
	void *state;
	const wchar_t *lhs, *rhs;
	int no_remap;
	vle_textbuf *msg;
	int seen_match;

	wchar_t *wide_prefix = to_wide(prefix);
	if(wide_prefix == NULL)
	{
		show_error_msgf("Abbrevs Error", "Failed to convert to wide string: %s",
				prefix);
		return 0;
	}

	state = NULL;
	if(!vle_abbr_iter(&lhs, &rhs, &no_remap, &state))
	{
		ui_sb_msg("No abbreviation found");
		return 1;
	}

	msg = vle_tb_create();
	vle_tb_append_line(msg, "Abbreviation -- N -- Replacement");

	prefix_len = wcslen(wide_prefix);

	seen_match = 0;
	state = NULL;
	while(vle_abbr_iter(&lhs, &rhs, &no_remap, &state))
	{
		if(wcsncmp(lhs, wide_prefix, prefix_len) == 0)
		{
			char *const descr = describe_abbrev(lhs, rhs, no_remap, 0);
			vle_tb_append_line(msg, descr);
			free(descr);
			seen_match = 1;
		}
	}

	if(seen_match)
	{
		ui_sb_msg(vle_tb_get_data(msg));
	}
	else
	{
		ui_sb_msg("No abbreviation found");
	}
	vle_tb_free(msg);

	free(wide_prefix);
	return 1;
}

/* Registers command-line mode abbreviation.  Returns value to be returned by
 * command handler. */
static int
add_cabbrev(const cmd_info_t *cmd_info, int no_remap)
{
	wchar_t *subst;
	wchar_t *wargs = to_wide(cmd_info->args);
	wchar_t *rhs = wargs;

	if(wargs == NULL)
	{
		show_error_msgf("Abbrevs Error", "Failed to convert to wide string: %s",
				cmd_info->args);
		return 0;
	}

	while(cfg_is_word_wchar(*rhs))
	{
		++rhs;
	}
	while(iswspace(*rhs))
	{
		*rhs++ = L'\0';
	}

	subst = substitute_specsw(rhs);
	int err = no_remap
	        ? vle_abbr_add_no_remap(wargs, subst)
	        : vle_abbr_add(wargs, subst);
	free(subst);
	free(wargs);

	if(err != 0)
	{
		ui_sb_err("Failed to register abbreviation");
		return CMDS_ERR_CUSTOM;
	}

	return 0;
}

/* Changes location of a view or both views.  Handle multiple configurations of
 * the command (with/without !, none/one/two arguments). */
static int
cd_cmd(const cmd_info_t *cmd_info)
{
	int result;

	char *const curr_dir = strdup(flist_get_dir(curr_view));
	char *const other_dir = strdup(flist_get_dir(other_view));

	if(!cfg.auto_ch_pos)
	{
		flist_hist_clear(curr_view);
		curr_stats.ch_pos = 0;
	}

	if(cmd_info->argc == 0)
	{
		result = cd(curr_view, curr_dir, cfg.home_dir);
		if(result == 0 && cmd_info->emark)
		{
			result += cd(other_view, other_dir, cfg.home_dir);
		}
	}
	else if(cmd_info->argc == 1)
	{
		result = cd(curr_view, curr_dir, cmd_info->argv[0]);
		if(cmd_info->emark)
		{
			if(!is_path_absolute(cmd_info->argv[0]) && cmd_info->argv[0][0] != '~' &&
					strcmp(cmd_info->argv[0], "-") != 0)
			{
				char dir[PATH_MAX + 1];
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
			char dir[PATH_MAX + 1];
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
	{
		curr_stats.ch_pos = 1;
	}

	free(curr_dir);
	free(other_dir);

	return result;
}

/* Performs substitution on current path and navigates to the result. */
static int
cds_cmd(const cmd_info_t *cmd_info)
{
	int case_sensitive = !regexp_should_ignore_case(cmd_info->argv[0]);
	if(cmd_info->argc > 2)
	{
		cmd_info->argv[2][strcspn(cmd_info->argv[2], " \t")] = '\0';
		if(parse_case_flag(cmd_info->argv[2], &case_sensitive) != 0)
		{
			ui_sb_errf("Failed to parse flags: %s", cmd_info->argv[2]);
			return CMDS_ERR_CUSTOM;
		}
	}

	const char *const curr_dir = flist_get_dir(curr_view);
	char *const new_path = strdup(regexp_replace(curr_dir, cmd_info->argv[0],
				cmd_info->argv[1], 0, !case_sensitive));
	if(new_path == NULL)
	{
		return CMDS_ERR_NO_MEM;
	}

	if(strcmp(curr_dir, new_path) == 0)
	{
		free(new_path);
		ui_sb_msgf("No \"%s\" found in CWD", cmd_info->argv[0]);
		return 1;
	}

	int result = cd(curr_view, curr_dir, new_path);
	if(result == 0 && cmd_info->emark)
	{
		result += cd(other_view, curr_dir, new_path);
	}

	free(new_path);

	return result;
}

static int
change_cmd(const cmd_info_t *cmd_info)
{
	enter_change_mode(curr_view);
	cmds_preserve_selection();
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
		cmds_preserve_selection();
		return 0;
	}

#ifndef _WIN32
	if((err = regcomp(&re, "^([ugoa]*([-+=]([rwxXst]*|[ugo]))+)|([0-7]{3,4})$",
			REG_EXTENDED)) != 0)
	{
		ui_sb_errf("Regexp error: %s", get_regexp_error(err, &re));
		regfree(&re);
		return CMDS_ERR_CUSTOM;
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
		ui_sb_errf("Invalid argument: %s", cmd_info->argv[i]);
		return CMDS_ERR_CUSTOM;
	}

	flist_set_marking(curr_view, 0);
	files_chmod(curr_view, cmd_info->args, cmd_info->emark);

	/* Reload metadata because attribute change might not be detected. */
	ui_view_schedule_reload(curr_view);
	ui_view_schedule_reload(other_view);
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
		fops_chuser();
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
		ui_sb_errf("Invalid user name: \"%s\"", user);
		return CMDS_ERR_CUSTOM;
	}
	if(g && get_gid(group, &gid) != 0)
	{
		ui_sb_errf("Invalid group name: \"%s\"", group);
		return CMDS_ERR_CUSTOM;
	}

	flist_set_marking(curr_view, 0);
	return fops_chown(u, g, uid, gid) != 0;
}
#endif

/* Clones file [count=1] times. */
static int
clone_cmd(const cmd_info_t *cmd_info)
{
	flist_set_marking(curr_view, 0);

	if(cmd_info->qmark)
	{
		if(cmd_info->argc > 0)
		{
			ui_sb_err("No arguments are allowed if you use \"?\"");
			return CMDS_ERR_CUSTOM;
		}
		return fops_clone(curr_view, NULL, -1, 0, 1) != 0;
	}

	return fops_clone(curr_view, cmd_info->argv, cmd_info->argc, cmd_info->emark,
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

/* Displays colorscheme menu, current colorscheme, associates colorscheme with a
 * subtree or sets primary colorscheme. */
static int
colorscheme_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->qmark)
	{
		ui_sb_msg(cfg.cs.name);
		return 1;
	}

	if(cmd_info->argc == 0)
	{
		/* Show menu with colorschemes listed. */
		return show_colorschemes_menu(curr_view) != 0;
	}

	const int assoc_form = is_colorscheme_assoc_form(cmd_info);
	if((cmd_info->argc == 1 || assoc_form) && !cs_exists(cmd_info->argv[0]))
	{
		ui_sb_errf("Cannot find colorscheme %s" , cmd_info->argv[0]);
		return CMDS_ERR_CUSTOM;
	}

	if(assoc_form)
	{
		return assoc_colorscheme(cmd_info->argv[0], cmd_info->argv[1]);
	}

	set_colorscheme(cmd_info->argv, cmd_info->argc);
	return 0;
}

/* Checks whether :colorscheme was invoked to associate a color scheme with a
 * directory.  Returns non-zero if so, otherwise zero is returned. */
static int
is_colorscheme_assoc_form(const cmd_info_t *cmd_info)
{
	if(cmd_info->argc != 2 || cs_exists(cmd_info->argv[1]))
	{
		return 0;
	}

	/* The path in :colorscheme command cannot be relative in startup scripts. */
	if(curr_stats.load_stage < 3)
	{
		char *path = expand_tilde(cmd_info->argv[1]);
		int is_abs_path = is_path_absolute(path);
		free(path);
		return is_abs_path;
	}
	return 1;
}

/* Associates colorscheme with a subtree.  Returns value to be returned by
 * command handler. */
static int
assoc_colorscheme(const char name[], const char path[])
{
	char path_buf[PATH_MAX + 1];
	char *directory = expand_tilde(path);
	if(!is_path_absolute(directory))
	{
		snprintf(path_buf, sizeof(path_buf), "%s/%s", flist_get_dir(curr_view),
				directory);
		(void)replace_string(&directory, path_buf);
	}
	canonicalize_path(directory, path_buf, sizeof(path_buf));
	(void)replace_string(&directory, path_buf);

	if(!is_dir(directory))
	{
		ui_sb_errf("%s isn't a directory", directory);
		free(directory);
		return CMDS_ERR_CUSTOM;
	}

	cs_assoc_dir(name, directory);
	free(directory);

	lwin.local_cs = cs_load_local(1, lwin.curr_dir);
	rwin.local_cs = cs_load_local(0, rwin.curr_dir);
	redraw_lists();
	return 0;
}

/* Sets primary colorscheme. */
static void
set_colorscheme(char *names[], int count)
{
	if(count == 1)
	{
		cs_load_primary(names[0]);
	}
	else
	{
		cs_load_primary_list(names, count);
	}

	if(!lwin.local_cs)
	{
		cs_assign(&lwin.cs, &cfg.cs);
	}
	if(!rwin.local_cs)
	{
		cs_assign(&rwin.cs, &cfg.cs);
	}
	redraw_lists();
	update_all_windows();
}

static int
command_cmd(const cmd_info_t *cmd_info)
{
	char *desc;

	if(cmd_info->argc == 0)
	{
		cmds_preserve_selection();
		return show_commands_menu(curr_view) != 0;
	}

	desc = vle_cmds_print_udcs(cmd_info->argv[0]);
	if(desc == NULL)
	{
		ui_sb_msg("No user-defined commands found");
		return 1;
	}

	ui_sb_msg(desc);
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
	if(wargs == NULL)
	{
		show_error_msgf("Abbrevs Error", "Failed to convert to wide string: %s",
				cmd_info->args);
		return 0;
	}

	const int err = vle_abbr_remove(wargs);
	free(wargs);

	if(err != 0)
	{
		ui_sb_errf("No such abbreviation: %s", cmd_info->args);
		return CMDS_ERR_CUSTOM;
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

	flist_set_marking(curr_view, 0);
	if(cmd_info->bg)
	{
		result = fops_delete_bg(curr_view, !cmd_info->emark) != 0;
	}
	else
	{
		result = fops_delete(curr_view, reg, !cmd_info->emark) != 0;
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
			marks_clear_all();
			return 0;
		}
		else
		{
			ui_sb_err("No arguments are allowed if you use \"!\"");
			return CMDS_ERR_CUSTOM;
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
			if(!char_is_one_of(marks_all, cmd_info->argv[i][j]))
			{
				return CMDS_ERR_INVALID_ARG;
			}
		}
	}

	for(i = 0; i < cmd_info->argc; i++)
	{
		int j;
		for(j = 0; cmd_info->argv[i][j] != '\0'; j++)
		{
			marks_clear_one(curr_view, cmd_info->argv[i][j]);
		}
	}
	return 0;
}

/* Removes bookmarks. */
static int
delbmarks_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->emark)
	{
		int i;

		/* Remove all bookmarks. */
		if(cmd_info->argc == 0)
		{
			bmarks_clear();
			return 0;
		}

		/* Remove bookmarks from listed paths. */
		for(i = 0; i < cmd_info->argc; ++i)
		{
			char *const path = make_bmark_path(cmd_info->argv[i]);
			bmarks_remove(path);
			free(path);
		}
	}
	else if(cmd_info->argc == 0)
	{
		/* Remove bookmarks from current directory. */
		char *const path = get_bmark_dir(cmd_info);
		bmarks_remove(path);
		free(path);
	}
	else
	{
		/* Remove set of bookmarks that include all of the specified tags. */
		char *const tags = make_tags_list(cmd_info);
		if(tags == NULL)
		{
			return CMDS_ERR_CUSTOM;
		}

		bmarks_find(tags, &remove_bmark, NULL);
		free(tags);
	}
	return 0;
}

/* bmarks_find() callback that removes bookmarks. */
static void
remove_bmark(const char path[], const char tags[], time_t timestamp, void *arg)
{
	/* It's safe to remove bookmark in the callback. */
	bmarks_remove(path);
}

/* Formats path for a bookmark in a unified way for several commands.  Returns
 * newly allocated string with the path. */
static char *
get_bmark_dir(const cmd_info_t *cmd_info)
{
	const char *const cwd = flist_get_dir(curr_view);

	if(cmd_info->emark)
	{
		return make_bmark_path(cmd_info->argv[0]);
	}

	return is_root_dir(cwd) ? strdup(cwd) : format_str("%s/", cwd);
}

/* Prepares path for a bookmark.  Returns newly allocated string. */
static char *
make_bmark_path(const char path[])
{
	char *ret;
	const char *const cwd = flist_get_dir(curr_view);
	char *const expanded = replace_tilde(ma_expand_single(path));

	if(is_path_absolute(expanded))
	{
		return expanded;
	}

	ret = format_str("%s%s%s", cwd, is_root_dir(cwd) ? "" : "/", expanded);
	free(expanded);
	return ret;
}

/* Compares files in one or two panes to produce their diff or lists of
 * duplicates or unique files. */
static int
compare_cmd(const cmd_info_t *cmd_info)
{
	int is_toggling = cmd_info->emark;
	if(is_toggling && !cv_compare(curr_view->custom.type))
	{
		ui_sb_err("Toggling requires active compare view");
		return CMDS_ERR_CUSTOM;
	}

	CompareType ct = CT_CONTENTS;
	ListType lt = LT_ALL;
	int flags = (is_toggling ? CF_NONE : CF_GROUP_PATHS);
	if(parse_compare_properties(cmd_info, &ct, &lt, &flags) != 0)
	{
		return CMDS_ERR_CUSTOM;
	}

	if(is_toggling)
	{
		struct cv_data_t *cv = &curr_view->custom;
		return (compare_two_panes(cv->diff_cmp_type, cv->diff_list_type,
					cv->diff_cmp_flags ^ flags) != 0);
	}

	if((flags & CF_SHOW) == 0)
	{
		flags |= CF_SHOW;
	}

	return (flags & CF_SINGLE_PANE)
	     ? (compare_one_pane(curr_view, ct, lt, flags) != 0)
	     : (compare_two_panes(ct, lt, flags) != 0);
}

/* Opens menu with contents of the last displayed menu with navigation to files
 * by default, if any. */
static int
copen_cmd(const cmd_info_t *cmd_info)
{
	return menus_unstash(curr_view) != 0;
}

/* Parses comparison properties.  Default values for arguments should be set
 * before the call.  Returns zero on success, otherwise non-zero is returned and
 * error message is displayed on the status bar. */
static int
parse_compare_properties(const cmd_info_t *cmd_info, CompareType *ct,
		ListType *lt, int *flags)
{
	int i;
	for(i = 0; i < cmd_info->argc; ++i)
	{
		const char *const property = cmd_info->argv[i];

		if(cmd_info->emark && !starts_with_lit(property, "show"))
		{
			ui_sb_errf("Unexpected property for toggling: %s", property);
			return CMDS_ERR_CUSTOM;
		}

		if     (strcmp(property, "byname") == 0)     *ct = CT_NAME;
		else if(strcmp(property, "bysize") == 0)     *ct = CT_SIZE;
		else if(strcmp(property, "bycontents") == 0) *ct = CT_CONTENTS;

		else if(strcmp(property, "listall") == 0)    *lt = LT_ALL;
		else if(strcmp(property, "listunique") == 0) *lt = LT_UNIQUE;
		else if(strcmp(property, "listdups") == 0)   *lt = LT_DUPS;

		else if(strcmp(property, "ofboth") == 0)     *flags &= ~CF_SINGLE_PANE;
		else if(strcmp(property, "ofone") == 0)      *flags |= CF_SINGLE_PANE;

		else if(strcmp(property, "groupids") == 0)   *flags &= ~CF_GROUP_PATHS;
		else if(strcmp(property, "grouppaths") == 0) *flags |= CF_GROUP_PATHS;

		else if(strcmp(property, "skipempty") == 0)  *flags |= CF_SKIP_EMPTY;

		else if(strcmp(property, "showidentical") == 0)
			*flags |= CF_SHOW_IDENTICAL;
		else if(strcmp(property, "showdifferent") == 0)
			*flags |= CF_SHOW_DIFFERENT;
		else if(strcmp(property, "showuniqueleft") == 0)
			*flags |= CF_SHOW_UNIQUE_LEFT;
		else if(strcmp(property, "showuniqueright") == 0)
			*flags |= CF_SHOW_UNIQUE_RIGHT;

		else if(strcmp(property, "withicase") == 0)
		{
			*flags &= ~CF_RESPECT_CASE;
			*flags |= CF_IGNORE_CASE;
		}
		else if(strcmp(property, "withrcase") == 0)
		{
			*flags &= ~CF_IGNORE_CASE;
			*flags |= CF_RESPECT_CASE;
		}

		else
		{
			ui_sb_errf("Unknown comparison property: %s", property);
			return CMDS_ERR_CUSTOM;
		}
	}

	return 0;
}

/* Deletes a session. */
static int
delsession_cmd(const cmd_info_t *cmd_info)
{
	const char *session_name = cmd_info->argv[0];

	if(!sessions_exists(session_name))
	{
		ui_sb_errf("No stored sessions with such name: %s", session_name);
		return CMDS_ERR_CUSTOM;
	}

	if(sessions_remove(session_name) != 0)
	{
		ui_sb_errf("Failed to delete a session: %s", session_name);
		return CMDS_ERR_CUSTOM;
	}

	return 0;
}

static int
dirs_cmd(const cmd_info_t *cmd_info)
{
	return show_dirstack_menu(curr_view) != 0;
}

/* Maps key sequence in dialogs expanding mappings in RHS. */
static int
dmap_cmd(const cmd_info_t *cmd_info)
{
	return dialog_map(cmd_info, 0);
}

/* Maps key sequence in dialogs not expanding mappings in RHS. */
static int
dnoremap_cmd(const cmd_info_t *cmd_info)
{
	return dialog_map(cmd_info, 1);
}

/* Implementation of :dmap and :dnoremap.  Returns cmds unit friendly code. */
static int
dialog_map(const cmd_info_t *cmd_info, int no_remap)
{
	int result;
	if(cmd_info->argc <= 1)
	{
		result = do_map(cmd_info, "Dialog", SORT_MODE, no_remap);
	}
	else
	{
		result = do_map(cmd_info, "", SORT_MODE, no_remap);
		result = result == 0 ? do_map(cmd_info, "", ATTR_MODE, no_remap) : result;
		result = result == 0 ? do_map(cmd_info, "", CHANGE_MODE, no_remap) : result;
		result = result == 0
		       ? do_map(cmd_info, "", FILE_INFO_MODE, no_remap)
		       : result;
	}
	return result != 0;
}

/* Unmaps key sequence in dialogs. */
static int
dunmap_cmd(const cmd_info_t *cmd_info)
{
	const char *lhs = cmd_info->argv[0];
	int result = do_unmap(lhs, SORT_MODE);
	result = result == 0 ? do_unmap(lhs, ATTR_MODE) : result;
	result = result == 0 ? do_unmap(lhs, CHANGE_MODE) : result;
	result = result == 0 ? do_unmap(lhs, FILE_INFO_MODE) : result;
	return result != 0;
}

/* Evaluates arguments as expression and outputs result to status bar. */
static int
echo_cmd(const cmd_info_t *cmd_info)
{
	char *const eval_result = try_eval_args(cmd_info);
	if(eval_result == NULL)
	{
		return CMDS_ERR_CUSTOM;
	}

	ui_sb_msg(eval_result);
	free(eval_result);
	return 1;
}

/* Edits current/selected/specified file(s) in editor. */
static int
edit_cmd(const cmd_info_t *cmd_info)
{
	flist_set_marking(curr_view, 1);

	if(cmd_info->argc != 0)
	{
		if(stats_file_choose_action_set())
		{
			/* The call below does not return. */
			vifm_choose_files(curr_view, cmd_info->argc, cmd_info->argv);
		}

		(void)vim_edit_files(cmd_info->argc, cmd_info->argv);
		return 0;
	}

	dir_entry_t *entry = NULL;
	while(iter_marked_entries(curr_view, &entry))
	{
		char full_path[PATH_MAX + 1];
		get_full_path_of(entry, sizeof(full_path), full_path);

		if(!path_exists(full_path, DEREF) && path_exists(full_path, NODEREF))
		{
			show_error_msgf("Access error",
					"Can't access destination of link \"%s\". It might be broken.",
					full_path);
			return 0;
		}
	}

	if(stats_file_choose_action_set())
	{
		/* Reuse marking second time. */
		curr_view->pending_marking = 1;

		/* The call below does not return. */
		vifm_choose_files(curr_view, 0, NULL);
	}

	if(vim_edit_marking(curr_view) != 0)
	{
		show_error_msg("Edit error", "Can't edit selection");
	}
	return 0;
}

/* This command designates beginning of the alternative part of if-endif
 * statement. */
static int
else_cmd(const cmd_info_t *cmd_info)
{
	if(cmds_scoped_else() != 0)
	{
		ui_sb_err("Misplaced :else");
		return CMDS_ERR_CUSTOM;
	}
	return 0;
}

/* This command designates beginning of the alternative branch of if-endif
 * statement with its own condition. */
static int
elseif_cmd(const cmd_info_t *cmd_info)
{
	const int x = eval_if_condition(cmd_info);
	if(x < 0)
	{
		return CMDS_ERR_CUSTOM;
	}

	if(cmds_scoped_elseif(x) != 0)
	{
		ui_sb_err("Misplaced :elseif");
		return CMDS_ERR_CUSTOM;
	}

	return 0;
}

/* Starts process of emptying all trashes in background. */
static int
empty_cmd(const cmd_info_t *cmd_info)
{
	trash_empty_all();
	return 0;
}

/* This command ends conditional block. */
static int
endif_cmd(const cmd_info_t *cmd_info)
{
	if(cmds_scoped_endif() != 0)
	{
		ui_sb_err(":endif without :if");
		return CMDS_ERR_CUSTOM;
	}
	return 0;
}

/* This command composes a string from expressions and runs it as a command. */
static int
exe_cmd(const cmd_info_t *cmd_info)
{
	int result = 1;
	char *const eval_result = try_eval_args(cmd_info);
	if(eval_result != NULL)
	{
		result = cmds_dispatch(eval_result, curr_view, CIT_COMMAND);
		free(eval_result);
	}
	return result != 0;
}

/* Tries to evaluate a set of expressions and concatenate results with a space.
 * Returns pointer to newly allocated string, which should be freed by caller,
 * or NULL on error. */
static char *
try_eval_args(const cmd_info_t *cmd_info)
{
	char *eval_result;
	const char *error_pos = NULL;

	if(cmd_info->argc == 0)
	{
		return strdup("");
	}

	vle_tb_clear(vle_err);
	eval_result = cmds_eval_args(cmd_info->raw_args, &error_pos);

	if(eval_result == NULL)
	{
		vle_tb_append_linef(vle_err, "%s: %s", "Invalid expression", error_pos);
		ui_sb_err(vle_tb_get_data(vle_err));
	}

	return eval_result;
}

/* Displays file handler picking menu. */
static int
file_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->argc == 0)
	{
		cmds_preserve_selection();
		return show_file_menu(curr_view, cmd_info->bg) != 0;
	}

	if(rn_open_with_match(curr_view, cmd_info->argv[0], cmd_info->bg) != 0)
	{
		ui_sb_err("Can't find associated program with requested beginning");
		return CMDS_ERR_CUSTOM;
	}

	return 0;
}

/* Registers non-x file association handler. */
static int
filetype_cmd(const cmd_info_t *cmd_info)
{
	return add_assoc(cmd_info, 0, 0);
}

/* Registers x file association handler. */
static int
filextype_cmd(const cmd_info_t *cmd_info)
{
	return add_assoc(cmd_info, 0, 1);
}

/* Registers external applications as file viewer scripts for files that match
 * name pattern.  Single argument form lists currently registered patterns that
 * match specified file name in menu mode. */
static int
fileviewer_cmd(const cmd_info_t *cmd_info)
{
	return add_assoc(cmd_info, 1, 0);
}

/* Registers x/non-x or viewer file association handler.  Single argument form
 * lists currently registered patterns that match specified file name in menu
 * mode.  Returns regular *_cmd handler value. */
static int
add_assoc(const cmd_info_t *cmd_info, int viewer, int for_x)
{
	char **matchers;
	int nmatchers;
	int i;
	const int in_x = (curr_stats.exec_env_type == EET_EMULATOR_WITH_X);
	const char *const records = vle_cmds_next_arg(cmd_info->args);

	if(cmd_info->argc == 1)
	{
		return viewer
		     ? (show_fileviewers_menu(curr_view, cmd_info->argv[0]) != 0)
		     : (show_fileprograms_menu(curr_view, cmd_info->argv[0]) != 0);
	}

	matchers = matchers_list(cmd_info->argv[0], &nmatchers);
	if(matchers == NULL)
	{
		return CMDS_ERR_NO_MEM;
	}

	for(i = 0; i < nmatchers; ++i)
	{
		char *error;
		matchers_t *const ms = matchers_alloc(matchers[i], 0, 1, "", &error);
		if(ms == NULL)
		{
			ui_sb_errf("Wrong pattern (%s): %s", matchers[i], error);
			free(error);
			free_string_array(matchers, nmatchers);
			return CMDS_ERR_CUSTOM;
		}

		if(viewer)
		{
			ft_set_viewers(ms, records);
		}
		else
		{
			ft_set_programs(ms, records, for_x, in_x);
		}
	}

	free_string_array(matchers, nmatchers);
	return 0;
}

/* Sets/displays/clears filters. */
static int
filter_cmd(const cmd_info_t *cmd_info)
{
	int ret;

	if(cmd_info->qmark)
	{
		display_filters_info(curr_view);
		return 1;
	}

	ret = update_filter(curr_view, cmd_info);
	if(curr_stats.global_local_settings)
	{
		int i;
		tab_info_t tab_info;
		for(i = 0; tabs_enum_all(i, &tab_info); ++i)
		{
			if(tab_info.view != curr_view)
			{
				ret |= update_filter(tab_info.view, cmd_info);
			}
		}
	}

	return ret;
}

/* Updates filters of the view. */
static int
update_filter(view_t *view, const cmd_info_t *cmd_info)
{
	const char *fallback = hists_search_last();

	if(cmd_info->argc == 0)
	{
		if(cmd_info->emark)
		{
			filters_invert(view);
			return 0;
		}

		/* When no arguments are provided, we don't want to fall back to last
		 * history entry. */
		fallback = "";
	}

	return set_view_filter(view, cmd_info->args, fallback,
			get_filter_inversion_state(cmd_info)) != 0;
}

/* Displays state of all filters on the status bar. */
static void
display_filters_info(const view_t *view)
{
	char *const localf = get_filter_info("Local", &view->local_filter.filter);
	char *const manualf = get_matcher_info("Explicit", view->manual_filter);
	char *const autof = get_filter_info("Implicit", &view->auto_filter);

	ui_sb_msgf("  Filter -- Flags -- Value\n%s\n%s\n%s", localf, manualf, autof);

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

	return format_str("%-8s    %-5s    %s", name, flags_str, filter->raw);
}

/* Composes description string for given matcher.  Returns NULL on out of
 * memory error, otherwise a newly allocated string, which should be freed by
 * the caller, is returned. */
static char *
get_matcher_info(const char name[], const matcher_t *matcher)
{
	const char *const flags = matcher_is_empty(matcher) ? "" : "---->";
	const char *const value = matcher_get_expr(matcher);
	return format_str("%-8s    %-5s    %s", name, flags, value);
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
 * On empty pattern fallback is used.  Returns non-zero if message on the
 * status bar should be saved, otherwise zero is returned. */
static int
set_view_filter(view_t *view, const char filter[], const char fallback[],
		int invert)
{
	char *error;
	matcher_t *const matcher = matcher_alloc(filter, FILTER_DEF_CASE_SENSITIVITY,
			0, fallback, &error);
	if(matcher == NULL)
	{
		ui_sb_errf("Name filter not set: %s", error);
		free(error);
		return CMDS_ERR_CUSTOM;
	}

	view->invert = invert;
	matcher_free(view->manual_filter);
	view->manual_filter = matcher;
	(void)filter_clear(&view->auto_filter);
	ui_view_schedule_reload(view);
	return 0;
}

/* Looks for files matching pattern. */
static int
find_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->argc > 0)
	{
		if(cmd_info->argc == 1)
			cmds_state.find.includes_path = 0;
		else if(is_dir(cmd_info->argv[0]))
			cmds_state.find.includes_path = 1;
		else
			cmds_state.find.includes_path = 0;

		(void)replace_string(&cmds_state.find.last_args, cmd_info->args);
	}
	else if(cmds_state.find.last_args == NULL)
	{
		ui_sb_err("Nothing to repeat");
		return CMDS_ERR_CUSTOM;
	}

	return show_find_menu(curr_view, cmds_state.find.includes_path,
			cmds_state.find.last_args) != 0;
}

static int
finish_cmd(const cmd_info_t *cmd_info)
{
	if(curr_stats.sourcing_state != SOURCING_PROCESSING)
	{
		ui_sb_err(":finish used outside of a sourced file");
		return CMDS_ERR_CUSTOM;
	}

	curr_stats.sourcing_state = SOURCING_FINISHING;
	cmds_scope_escape();
	return 0;
}

/* Changes view to have specified file/directory under the cursor. */
static int
goto_path_cmd(const cmd_info_t *cmd_info)
{
	char abs_path[PATH_MAX + 1];
	char *fname;

	char *const expanded = expand_tilde(cmd_info->argv[0]);
	to_canonic_path(expanded, flist_get_dir(curr_view), abs_path,
			sizeof(abs_path));
	free(expanded);

	if(is_root_dir(abs_path))
	{
		ui_sb_errf("Can't navigate to root directory: %s", abs_path);
		return CMDS_ERR_CUSTOM;
	}

	if(!path_exists(abs_path, NODEREF))
	{
		ui_sb_errf("Path doesn't exist: %s", abs_path);
		return CMDS_ERR_CUSTOM;
	}

	fname = strdup(get_last_path_component(abs_path));
	remove_last_path_component(abs_path);
	navigate_to_file(curr_view, abs_path, fname, 0);
	free(fname);
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
		ui_sb_err("Nothing to repeat");
		return CMDS_ERR_CUSTOM;
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
	char cmd[PATH_MAX + 1];
	int bg;

	const char *vi_cmd = cfg_get_vicmd(&bg);
	const int use_handler = vlua_handler_cmd(curr_stats.vlua, vi_cmd);

	if(cfg.use_vim_help)
	{
		const char *topic = (cmd_info->argc > 0) ? cmd_info->args : VIFM_VIM_HELP;

		if(use_handler)
		{
			if(vlua_open_help(curr_stats.vlua, vi_cmd, topic) != 0)
			{
				show_error_msgf(":help", "Failed to display help for %s via handler",
						topic);
			}
			return 0;
		}

		bg = vim_format_help_cmd(topic, cmd, sizeof(cmd));
	}
	else
	{
		if(cmd_info->argc != 0)
		{
			ui_sb_err("No arguments are allowed when 'vimhelp' option is off");
			return CMDS_ERR_CUSTOM;
		}

		char help_file[PATH_MAX + 1];
		build_path(help_file, sizeof(help_file), cfg.config_dir, VIFM_HELP);

		if(use_handler)
		{
			if(vlua_edit_one(curr_stats.vlua, vi_cmd, help_file, -1, -1, 0) != 0)
			{
				show_error_msg(":help", "Failed to open help file via handler");
			}
			return 0;
		}

		if(!path_exists(help_file, DEREF))
		{
			show_error_msgf("No help file", "Can't find \"%s\" file", help_file);
			return 0;
		}

		bg = format_help_cmd(cmd, sizeof(cmd));
	}

	if(bg)
	{
		bg_run_external(cmd, 0, SHELL_BY_APP, NULL);
	}
	else
	{
		display_help(cmd);
	}
	return 0;
}

/* Hides interface to show previous commands' output. */
static int
hideui_cmd(const cmd_info_t *cmd_info)
{
	ui_pause();
	return 0;
}

/* Handles :highlight command.  There are several forms:
 *  - highlight clear [file|column:name]
 *  - highlight file
 *  - highlight group
 *  - highlight column:name */
static int
highlight_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->argc == 0)
	{
		ui_sb_msg(get_all_highlights());
		return 1;
	}

	if(strcasecmp(cmd_info->argv[0], "clear") == 0)
	{
		return highlight_clear(cmd_info);
	}

	if(starts_with_lit(cmd_info->argv[0], "column:"))
	{
		return highlight_column(cmd_info);
	}
	if(matchers_is_expr(cmd_info->argv[0]))
	{
		return highlight_file(cmd_info);
	}
	return highlight_group(cmd_info);
}

/* Handles clear form of :highlight command.  Returns value to be returned by
 * command handler. */
static int
highlight_clear(const cmd_info_t *cmd_info)
{
	if(cmd_info->argc == 2)
	{
		if(starts_with_lit(cmd_info->argv[1], "column:"))
		{
			int column_idx = find_column_id(cmd_info->argv[1]);
			if(column_idx < 0)
			{
				return CMDS_ERR_CUSTOM;
			}

			if(!cs_del_column_hi(curr_stats.cs, column_idx))
			{
				ui_sb_errf("No such group: %s", cmd_info->argv[1]);
				return CMDS_ERR_CUSTOM;
			}
		}
		else if(!cs_del_file_hi(cmd_info->argv[1]))
		{
			ui_sb_errf("No such group: %s", cmd_info->argv[1]);
			return CMDS_ERR_CUSTOM;
		}

		ui_invalidate_cs(curr_stats.cs);

		/* Redraw to update filename specific highlights. */
		stats_redraw_later();

		return 0;
	}

	if(cmd_info->argc == 1)
	{
		cs_reset(curr_stats.cs);
		ui_invalidate_cs(curr_stats.cs);

		/* Request full update instead of redraw to force recalculation of mixed
		 * colors like cursor line, which otherwise are not updated. */
		stats_refresh_later();
		return 0;
	}

	return CMDS_ERR_TRAILING_CHARS;
}

/* Handles highlight-column form of :highlight command.  Returns value to be
 * returned by command handler. */
static int
highlight_column(const cmd_info_t *cmd_info)
{
	int column_idx = find_column_id(cmd_info->argv[0]);
	if(column_idx < 0)
	{
		return 1;
	}

	/* XXX: start with an existing value, if present? */
	col_attr_t color = { .fg = -1, .bg = -1, .attr = 0, };
	int result = parse_highlight_attrs(cmd_info, &color);
	if(result != 0)
	{
		return result;
	}

	if(cmd_info->argc == 1)
	{
		const col_attr_t *col = cs_get_column_hi(curr_stats.cs, column_idx);
		if(col != NULL)
		{
			ui_sb_msg(get_column_hi_str(column_idx, col));
			return 1;
		}
		return 0;
	}

	if(cs_set_column_hi(curr_stats.cs, column_idx, &color) != 0)
	{
		ui_sb_err("Failed to add/update column color");
		return 1;
	}

	/* Redraw to update columns' look. */
	stats_redraw_later();

	return 0;
}

/* Looks up column id by column:name argument.  Returns the id or -1 on failure
 * and prints error message to the status bar. */
static int
find_column_id(const char arg[])
{
	(void)skip_prefix(&arg, "column:");

	int column_idx = ui_map_column_name(arg);
	if(column_idx < 0)
	{
		ui_sb_errf("No such column: %s", arg);
	}
	return column_idx;
}

/* Handles highlight-file form of :highlight command.  Returns value to be
 * returned by command handler. */
static int
highlight_file(const cmd_info_t *cmd_info)
{
	char pattern[strlen(cmd_info->args) + 1];
	/* XXX: start with an existing value, if present? */
	col_attr_t color = { .fg = -1, .bg = -1, .attr = 0, };
	int result;
	matchers_t *matchers;
	char *error;

	(void)extract_part(cmd_info->args, " \t", pattern);

	matchers = matchers_alloc(pattern, 0, 1, "", &error);
	if(matchers == NULL)
	{
		ui_sb_errf("Pattern error: %s", error);
		free(error);
		return CMDS_ERR_CUSTOM;
	}

	if(cmd_info->argc == 1)
	{
		display_file_highlights(matchers);
		matchers_free(matchers);
		return 1;
	}

	result = parse_highlight_attrs(cmd_info, &color);
	if(result != 0)
	{
		matchers_free(matchers);
		return result;
	}

	cs_add_file_hi(matchers, &color);
	/* We don't need to invalidate anything on startup and while loading a color
	 * scheme. */
	if(curr_stats.load_stage > 1 && curr_stats.cs->state != CSS_LOADING)
	{
		ui_invalidate_cs(curr_stats.cs);
	}

	/* Redraw to update filename specific highlights. */
	stats_redraw_later();

	return result;
}

/* Displays information about filename specific highlight on the status bar. */
static void
display_file_highlights(const matchers_t *matchers)
{
	int i;

	const col_scheme_t *cs = ui_view_get_cs(curr_view);

	for(i = 0; i < cs->file_hi_count; ++i)
	{
		if(matchers_includes(cs->file_hi[i].matchers, matchers))
		{
			break;
		}
	}

	if(i >= cs->file_hi_count)
	{
		ui_sb_errf("Highlight group not found: %s", matchers_get_expr(matchers));
		return;
	}

	ui_sb_msg(get_file_hi_str(cs->file_hi[i].matchers, &cs->file_hi[i].hi));
}

/* Handles highlight-group form of :highlight command.  Returns value to be
 * returned by command handler. */
static int
highlight_group(const cmd_info_t *cmd_info)
{
	int result;
	int group_id;
	col_attr_t tmp_color;
	col_attr_t *color;

	group_id = string_array_pos_case(HI_GROUPS, MAXNUM_COLOR, cmd_info->argv[0]);
	if(group_id < 0)
	{
		ui_sb_errf("Highlight group not found: %s", cmd_info->argv[0]);
		return CMDS_ERR_CUSTOM;
	}

	color = &curr_stats.cs->color[group_id];

	if(cmd_info->argc == 1)
	{
		ui_sb_msg(get_group_str(group_id, color));
		return 1;
	}

	tmp_color = *color;
	result = parse_highlight_attrs(cmd_info, &tmp_color);
	if(result != 0)
	{
		return result;
	}

	*color = tmp_color;
	curr_stats.cs->pair[group_id] = cs_load_color(color);

	/* Other highlight commands might have finished successfully, so update TUI.
	 * Request full update instead of redraw to force recalculation of mixed
	 * colors like cursor line, which otherwise are not updated. */
	stats_refresh_later();

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
		sstrappend(msg, &msg_len, sizeof(msg), get_group_str(i, &cs->color[i]));
		sstrappendch(msg, &msg_len, sizeof(msg), '\n');
	}

	int has_column_hi = 0;
	for(i = 0; i < cs->column_hi_count; ++i)
	{
		const col_attr_t *col = cs_get_column_hi(cs, i);
		if(col != NULL)
		{
			has_column_hi = 1;

			/* Different order is due to the fact that non-zero cs->column_hi_count
			 * doesn't necessarily imply that cs_get_column_hi() will return
			 * non-NULL even once. */
			sstrappendch(msg, &msg_len, sizeof(msg), '\n');
			sstrappend(msg, &msg_len, sizeof(msg), get_column_hi_str(i, col));
		}
	}

	if(cs->file_hi_count > 0)
	{
		sstrappend(msg, &msg_len, sizeof(msg), (has_column_hi ? "\n\n" : "\n"));
	}

	for(i = 0; i < cs->file_hi_count; ++i)
	{
		const file_hi_t *const file_hi = &cs->file_hi[i];
		const char *const line = get_file_hi_str(file_hi->matchers, &file_hi->hi);
		sstrappend(msg, &msg_len, sizeof(msg), line);
		sstrappendch(msg, &msg_len, sizeof(msg), '\n');
	}

	if(msg_len > 0 && msg[msg_len - 1] == '\n')
	{
		msg[msg_len - 1] = '\0';
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

/* Composes string representation of column highlight definition.  Returns
 * pointer to a statically allocated buffer. */
static const char *
get_column_hi_str(int index, const col_attr_t *col)
{
	char name[16];
	snprintf(name, sizeof(name), "column:%s", sort_enum[index]);
	return get_hi_str(name, col);
}

/* Composes string representation of filename specific highlight definition.
 * Returns pointer to a statically allocated buffer. */
static const char *
get_file_hi_str(const matchers_t *matchers, const col_attr_t *col)
{
	return get_hi_str(matchers_get_expr(matchers), col);
}

/* Composes string representation of highlight definition.  Returns pointer to a
 * statically allocated buffer. */
static const char *
get_hi_str(const char title[], const col_attr_t *col)
{
	static char buf[512];
	static char gui_buf[2*sizeof(buf)];

	char fg_buf[16], bg_buf[16];

	cs_color_to_str(col->fg, sizeof(fg_buf), fg_buf, /*is_gui=*/0);
	cs_color_to_str(col->bg, sizeof(bg_buf), bg_buf, /*is_gui=*/0);

	snprintf(buf, sizeof(buf), "%-10s cterm=%s ctermfg=%-7s ctermbg=%s",
			title, cs_attrs_to_str(col, /*gui_part=*/0), fg_buf, bg_buf);

	if(!col->gui_set)
	{
		return buf;
	}

	cs_color_to_str(col->gui_fg, sizeof(fg_buf), fg_buf, /*is_gui=*/1);
	cs_color_to_str(col->gui_bg, sizeof(bg_buf), bg_buf, /*is_gui=*/1);

	snprintf(gui_buf, sizeof(gui_buf), "%s\n%*s gui=%-6s guifg=%-9s guibg=%s",
			buf, (int)MAX(utf8_strsw(title), 10U), "",
			cs_attrs_to_str(col, /*gui_part=*/1), fg_buf, bg_buf);

	return gui_buf;
}

/* Parses arguments of :highlight command.  Returns non-zero in case of error
 * and prints a message on the status bar, on success zero is returned. */
static int
parse_highlight_attrs(const cmd_info_t *cmd_info, col_attr_t *color)
{
	int i;

	for(i = 1; i < cmd_info->argc; ++i)
	{
		const char *const arg = cmd_info->argv[i];
		const char *const equal = strchr(arg, '=');
		char arg_name[16];

		if(equal == NULL)
		{
			ui_sb_errf("Missing equal sign in \"%s\"", arg);
			return CMDS_ERR_CUSTOM;
		}
		if(equal[1] == '\0')
		{
			ui_sb_errf("Missing argument: %s", arg);
			return CMDS_ERR_CUSTOM;
		}

		copy_str(arg_name, MIN(sizeof(arg_name), (size_t)(equal - arg + 1)), arg);

		if(strcmp(arg_name, "ctermbg") == 0)
		{
			if(try_parse_cterm_color(equal + 1, 0, color) != 0)
			{
				/* Color not supported by the current terminal is not a hard error. */
				return 1;
			}
		}
		else if(strcmp(arg_name, "ctermfg") == 0)
		{
			if(try_parse_cterm_color(equal + 1, 1, color) != 0)
			{
				/* Color not supported by the current terminal is not a hard error. */
				return 1;
			}
		}
		else if(strcmp(arg_name, "guibg") == 0)
		{
			int value;
			if(try_parse_gui_color(equal + 1, &value) != 0)
			{
				/* Color not supported by the current terminal is not a hard error. */
				return 1;
			}

			cs_color_enable_gui(color);
			color->gui_bg = value;
		}
		else if(strcmp(arg_name, "guifg") == 0)
		{
			int value;
			if(try_parse_gui_color(equal + 1, &value) != 0)
			{
				/* Color not supported by the current terminal is not a hard error. */
				return 1;
			}

			cs_color_enable_gui(color);
			color->gui_fg = value;
		}
		else if(strcmp(arg_name, "cterm") == 0 || strcmp(arg_name, "gui") == 0)
		{
			int attrs;
			int combine_attrs;
			if((attrs = get_attrs(equal + 1, &combine_attrs)) == -1)
			{
				ui_sb_errf("Illegal argument: %s", equal + 1);
				return CMDS_ERR_CUSTOM;
			}

			if(strcmp(arg_name, "cterm") == 0)
			{
				color->attr = attrs;
				color->combine_attrs = combine_attrs;

				if(curr_stats.exec_env_type == EET_LINUX_NATIVE &&
						(attrs & (A_BOLD | A_REVERSE)) == (A_BOLD | A_REVERSE))
				{
					color->attr |= A_BLINK;
				}
			}
			else
			{
				cs_color_enable_gui(color);
				color->gui_attr = attrs;
				color->combine_gui_attrs = combine_attrs;
			}
		}
		else
		{
			ui_sb_errf("Illegal argument: %s", arg);
			return CMDS_ERR_CUSTOM;
		}
	}

	return 0;
}

/* Tries to parse color number or color name.  Returns non-zero if status bar
 * message should be preserved, otherwise zero is returned. */
static int
try_parse_cterm_color(const char str[], int is_fg, col_attr_t *color)
{
	col_scheme_t *const cs = curr_stats.cs;
	const int col_num = parse_color_name_value(str, is_fg, &color->attr);

	if(col_num < -1)
	{
		ui_sb_errf("Color name or number not recognized: %s", str);
		if(cs->state == CSS_LOADING)
		{
			cs->state = CSS_BROKEN;
		}

		return 1;
	}

	if(is_fg)
	{
		color->fg = col_num;
	}
	else
	{
		color->bg = col_num;
	}

	return 0;
}

/* Tries to parse a direct color.  Returns non-zero if status bar message should
 * be preserved, otherwise zero is returned. */
static int
try_parse_gui_color(const char str[], int *color)
{
	const char *hex_digits = "0123456789abcdefABCDEF";

	if(is_default_color(str))
	{
		*color = -1;
		return 0;
	}

	*color = string_array_pos_case(XTERM256_COLOR_NAMES,
			ARRAY_LEN(XTERM256_COLOR_NAMES), str);
	if(*color >= 0 && *color < 8)
	{
		return 0;
	}

	if(str[0] != '#' || strlen(str) != 7 || strspn(str + 1, hex_digits) != 6)
	{
		ui_sb_errf("Unrecognized color value format: %s", str);
		if(curr_stats.cs->state == CSS_LOADING)
		{
			curr_stats.cs->state = CSS_BROKEN;
		}
		return 1;
	}

	unsigned int value;
	(void)sscanf(str, "#%x", &value);

	*color = value;
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

	if(is_default_color(str))
	{
		return -1;
	}

	light_col_pos = string_array_pos_case(LIGHT_COLOR_NAMES,
			ARRAY_LEN(LIGHT_COLOR_NAMES), str);
	if(light_col_pos >= 0 && COLORS < 16)
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
	if(col_num >= 0 && col_num < COLORS)
	{
		return col_num;
	}

	/* Fail if all possible parsing ways failed. */
	return -2;
}

/* Checks whether a string signifies a default color.  Returns non-zero if so,
 * otherwise zero is returned. */
static int
is_default_color(const char str[])
{
	return (strcmp(str, "-1") == 0)
	    || (strcasecmp(str, "default") == 0)
	    || (strcasecmp(str, "none") == 0);
}

/* Parses comma-separated list of attributes.  Returns parsed result or -1 on
 * error.  *combine_attrs is always assigned to. */
static int
get_attrs(const char text[], int *combine_attrs)
{
#ifdef HAVE_A_ITALIC_DECL
	const int italic_attr = A_ITALIC;
#else
	/* If A_ITALIC is missing (it's an extension), use A_REVERSE instead. */
	const int italic_attr = A_REVERSE;
#endif

	*combine_attrs = 0;

	int result = 0;
	while(*text != '\0')
	{
		const char *const p = until_first(text, ',');
		char buf[64];

		copy_str(buf, p - text + 1, text);
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
		else if(strcasecmp(buf, "italic") == 0)
			result |= italic_attr;
		else if(strcasecmp(buf, "none") == 0)
			result = 0;
		else if(strcasecmp(buf, "combine") == 0)
			*combine_attrs = 1;
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
	else if(starts_withn("exprreg", type, len))
		return show_exprreghistory_menu(curr_view) != 0;
	else if(starts_withn("mcmd", type, len))
		return show_menucmdhistory_menu(curr_view) != 0;
	else if(strcmp(type, ".") == 0 || starts_withn("dir", type, len))
		return show_history_menu(curr_view) != 0;
	else
		return CMDS_ERR_TRAILING_CHARS;
}

/* Goes forward though directory history. */
static int
histnext_cmd(const cmd_info_t *cmd_info)
{
	flist_hist_go_forward(curr_view);
	return 0;
}

/* Goes backward though directory history. */
static int
histprev_cmd(const cmd_info_t *cmd_info)
{
	flist_hist_go_back(curr_view);
	return 0;
}

/* This command starts conditional block. */
static int
if_cmd(const cmd_info_t *cmd_info)
{
	const int x = eval_if_condition(cmd_info);
	if(x < 0)
	{
		return CMDS_ERR_CUSTOM;
	}

	cmds_scoped_if(x);
	return 0;
}

/* Evaluates condition for if-endif statement.  Returns negative number on
 * error, zero for expression that's evaluated to false and positive number for
 * true expressions. */
static int
eval_if_condition(const cmd_info_t *cmd_info)
{
	vle_tb_clear(vle_err);

	int result;

	parsing_result_t parsing_result =
		vle_parser_eval(cmd_info->args, /*interactive=*/1);
	if(parsing_result.error != PE_NO_ERROR)
	{
		vle_tb_append_linef(vle_err, "%s: %s", "Invalid expression",
				cmd_info->args);
		ui_sb_err(vle_tb_get_data(vle_err));
		result = CMDS_ERR_CUSTOM;
	}
	else
	{
		result = var_to_bool(parsing_result.value);
	}

	var_free(parsing_result.value);
	return result;
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
		ui_sb_msgf("Filter is %sinverted", curr_view->invert ? "" : "not ");
	}
	else if(state_type == 's')
	{
		ui_sb_msg("Selection does not have inversion state");
	}
	else if(state_type == 'o')
	{
		ui_sb_msgf("Primary key is sorted in %s order",
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
		filters_invert(curr_view);
	}
	else if(state_type == 's')
	{
		flist_sel_invert(curr_view);
		redraw_view(curr_view);
		cmds_preserve_selection();
	}
	else if(state_type == 'o')
	{
		invert_sorting_order(curr_view);
		resort_dir_list(1, curr_view);
		load_sort_option(curr_view);
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

/* Change default from resetting selection at the end of a command to keeping
 * it. */
static int
keepsel_cmd(const cmd_info_t *cmd_info)
{
	return cmds_exec(cmd_info->args, curr_view, /*menu=*/0, /*keep_sel=*/1);
}

static int
let_cmd(const cmd_info_t *cmd_info)
{
	vle_tb_clear(vle_err);
	if(let_variables(cmd_info->args) != 0)
	{
		ui_sb_err(vle_tb_get_data(vle_err));
		return CMDS_ERR_CUSTOM;
	}
	else if(*vle_tb_get_data(vle_err) != '\0')
	{
		ui_sb_msg(vle_tb_get_data(vle_err));
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
		ui_sb_err("Nothing to repeat");
		return CMDS_ERR_CUSTOM;
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
			ui_sb_msg("No terminal multiplexer is in use");
			return 1;
		case TM_SCREEN:
			(void)vifm_system("screen -X eval windowlist", SHELL_BY_APP);
			return 0;
		case TM_TMUX:
			if(vifm_system("tmux choose-window", SHELL_BY_APP) != EXIT_SUCCESS)
			{
				/* Refresh all windows as failed command outputs message, which can't be
				 * suppressed. */
				update_all_windows();
				/* Fall back to worse way of doing the same for tmux versions < 1.8. */
				(void)vifm_system("tmux command-prompt choose-window", SHELL_BY_APP);
			}
			return 0;

		default:
			assert(0 && "Unknown active terminal multiplexer value");
			ui_sb_msg("Unknown terminal multiplexer is in use");
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
		if(!marks_is_empty(curr_view, mark))
		{
			ui_sb_errf("Mark isn't empty: %c", mark);
			return CMDS_ERR_CUSTOM;
		}
	}

	if(cmd_info->argc == 1)
	{
		const int pos = (cmd_info->end == NOT_DEF)
		              ? curr_view->list_pos
		              : cmd_info->end;
		const dir_entry_t *const entry = &curr_view->dir_entry[pos];
		return marks_set_user(curr_view, mark, entry->origin, entry->name);
	}

	expanded_path = expand_tilde(cmd_info->argv[1]);
	if(!is_path_absolute(expanded_path))
	{
		free(expanded_path);
		ui_sb_err("Expected full path to the directory");
		return CMDS_ERR_CUSTOM;
	}

	if(cmd_info->argc == 2)
	{
		if(cmd_info->end == NOT_DEF || !pane_in_dir(curr_view, expanded_path))
		{
			if(curr_stats.load_stage >= 3 && pane_in_dir(curr_view, expanded_path))
			{
				file = get_current_file_name(curr_view);
			}
			else
			{
				file = NO_MARK_FILE;
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
	result = marks_set_user(curr_view, mark, expanded_path, file);
	free(expanded_path);

	return result;
}

/* Displays all or some of marks. */
static int
marks_cmd(const cmd_info_t *cmd_info)
{
	char buf[256];
	int i, j;

	if(cmd_info->argc == 0)
	{
		return show_marks_menu(curr_view, marks_all) != 0;
	}

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
	return show_marks_menu(curr_view, buf) != 0;
}

#ifndef _WIN32

/* Shows a menu for managing media. */
static int
media_cmd(const cmd_info_t *cmd_info)
{
	return (show_media_menu(curr_view) != 0);
}

#endif

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
	ui_sb_msg(lines);
	curr_stats.allow_sb_msg_truncation = 1;
	curr_stats.save_msg_in_list = 1;

	free(lines);
	return 1;
}

/* :mkdir creates sub-directory.  :mkdir! creates chain of directories.  In
 * second case both relative and absolute paths are allowed. */
static int
mkdir_cmd(const cmd_info_t *cmd_info)
{
	const int at = get_at(curr_view, cmd_info);
	return fops_mkdirs(curr_view, at, cmd_info->argv, cmd_info->argc,
			cmd_info->emark) != 0;
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

	flist_set_marking(curr_view, 0);

	int argc = cmd_info->argc;
	char **argv = cmd_info->argv;
	int flags = parse_cpmv_flags(&argc, &argv);
	if(flags < 0)
	{
		return CMDS_ERR_CUSTOM;
	}

	flags |= (cmd_info->emark ? CMLF_FORCE : CMLF_NONE);

	if(cmd_info->qmark)
	{
		if(argc > 0)
		{
			ui_sb_err("No positional arguments are allowed if you use \"?\"");
			return CMDS_ERR_CUSTOM;
		}

		if(cmd_info->bg)
		{
			return fops_cpmv_bg(curr_view, NULL, -1, move, flags) != 0;
		}

		return fops_cpmv(curr_view, NULL, -1, op, flags) != 0;
	}

	if(cmd_info->bg)
	{
		return fops_cpmv_bg(curr_view, argv, argc, move, flags) != 0;
	}

	return fops_cpmv(curr_view, argv, argc, op, flags) != 0;
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
	flist_sel_stash_if_nonempty(curr_view);
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
	wchar_t *const wide = to_wide(cmd_info->args);
	if(wide == NULL)
	{
		show_error_msgf("Command Error", "Failed to convert to wide string: %s",
				cmd_info->args);
		return 0;
	}

	vle_mode_t old_primary_mode = vle_mode_get_primary();
	vle_mode_t old_current_mode = vle_mode_get();
	vle_mode_set(NORMAL_MODE, VMT_PRIMARY);

	if(cmd_info->emark)
	{
		(void)vle_keys_exec_timed_out_no_remap(wide);
	}
	else
	{
		(void)vle_keys_exec_timed_out(wide);
	}
	free(wide);

	/* Force leaving command-line mode if the input contains unfinished ":". */
	if(modes_is_cmdline_like())
	{
		(void)vle_keys_exec_timed_out(WK_C_c);
	}

	/* Don't restore the mode if input sequence led to a mode change as it was
	 * probably the intention of the user. */
	if(vle_mode_is(NORMAL_MODE))
	{
		/* Relative order of the calls is important. */
		vle_mode_set(old_primary_mode, VMT_PRIMARY);
		vle_mode_set(old_current_mode, VMT_SECONDARY);
	}

	cmds_preserve_selection();
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

/* Manages plugins. */
static int
plugin_cmd(const cmd_info_t *cmd_info)
{
	if(strcmp(cmd_info->argv[0], "load") == 0)
	{
		if(cmd_info->argc != 1)
		{
			return CMDS_ERR_TRAILING_CHARS;
		}

		plugs_load(curr_stats.plugs, curr_stats.plugins_dirs);
		return 0;
	}

	if(cmd_info->argc != 2)
	{
		return CMDS_ERR_TOO_FEW_ARGS;
	}

	if(strcmp(cmd_info->argv[0], "blacklist") == 0)
	{
		plugs_blacklist(curr_stats.plugs, cmd_info->argv[1]);
		return 0;
	}
	if(strcmp(cmd_info->argv[0], "whitelist") == 0)
	{
		plugs_whitelist(curr_stats.plugs, cmd_info->argv[1]);
		return 0;
	}

	ui_sb_errf("Unknown subcommand: %s", cmd_info->argv[0]);
	return CMDS_ERR_CUSTOM;
}

/* Displays plugins menu. */
static int
plugins_cmd(const cmd_info_t *cmd_info)
{
	return (show_plugins_menu(curr_view) != 0);
}

static int
popd_cmd(const cmd_info_t *cmd_info)
{
	if(dir_stack_pop() != 0)
	{
		ui_sb_msg("Directory stack empty");
		return 1;
	}
	return 0;
}

static int
pushd_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->argc == 0)
	{
		if(dir_stack_swap() != 0)
		{
			ui_sb_err("No other directories");
			return CMDS_ERR_CUSTOM;
		}
		return 0;
	}
	if(dir_stack_push_current() != 0)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return 0;
	}
	cd_cmd(cmd_info);
	return 0;
}

/* Puts files from the register (default register unless otherwise specified)
 * into current directory.  Can operate in background. */
static int
put_cmd(const cmd_info_t *cmd_info)
{
	int reg = DEFAULT_REG_NAME;
	const int at = get_at(curr_view, cmd_info);

	if(cmd_info->argc == 1)
	{
		const int error = get_reg(cmd_info->argv[0], &reg);
		if(error != 0)
		{
			return error;
		}
	}

	if(cmd_info->bg)
	{
		return fops_put_bg(curr_view, at, reg, cmd_info->emark) != 0;
	}

	return fops_put(curr_view, at, reg, cmd_info->emark) != 0;
}

static int
pwd_cmd(const cmd_info_t *cmd_info)
{
	ui_sb_msg(flist_get_dir(curr_view));
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

/* Opens external editor to edit specified register contents. */
static int
regedit_cmd(const cmd_info_t *cmd_info)
{
	int reg_name = DEFAULT_REG_NAME;
	if(cmd_info->argc > 0)
	{
		if(strlen(cmd_info->argv[0]) != 1)
		{
			ui_sb_errf("Invalid argument: %s", cmd_info->argv[0]);
			return CMDS_ERR_CUSTOM;
		}
		reg_name = tolower(cmd_info->argv[0][0]);
	}
	if(reg_name == BLACKHOLE_REG_NAME)
	{
		ui_sb_err("Cannot modify blackhole register.");
		return CMDS_ERR_CUSTOM;
	}

	regs_sync_from_shared_memory();
	const reg_t *reg = regs_find(reg_name);
	if(reg == NULL)
	{
		ui_sb_err("Register with given name does not exist.");
		return CMDS_ERR_CUSTOM;
	}

	char tmp_fname[PATH_MAX + 1];
	FILE *file = make_file_in_tmp("vifm.regedit", 0600, /*auto_delete=*/0,
			tmp_fname, sizeof(tmp_fname));
	if(file == NULL)
	{
		ui_sb_err("Couldn't write register content into external file.");
		return CMDS_ERR_CUSTOM;
	}

	write_lines_to_file(file, reg->files, reg->nfiles);
	fclose(file);

	/* Forking is disabled because in some cases process can return value
	 * before any changes will be written and editor will be closed. */
	const int edit_result = vim_view_file(tmp_fname, 1, 1, /*allow_forking=*/0);
	if(edit_result != 0)
	{
		ui_sb_err("Register content edition went unsuccessful.");
		return CMDS_ERR_CUSTOM;
	}

	int read_lines;
	char **edited_content = read_file_of_lines(tmp_fname, &read_lines);
	int error = errno;
	unlink(tmp_fname);
	if(edited_content == NULL)
	{
		ui_sb_errf("Couldn't read edited register's content: %s", strerror(error));
		return CMDS_ERR_CUSTOM;
	}

	/* Normalize paths.  Not done in regs_set() as it doesn't need to know about
	 * current directory. */
	int i;
	const char *base_dir = flist_get_dir(curr_view);
	for(i = 0; i < read_lines; ++i)
	{
		char canonic[PATH_MAX + 1];
		to_canonic_path(edited_content[i], base_dir, canonic, sizeof(canonic));
		replace_string(&edited_content[i], canonic);
	}

	regs_set(reg_name, edited_content, read_lines);
	free_string_array(edited_content, read_lines);

	regs_sync_to_shared_memory();
	return 0;
}

/* Displays menu listing contents of registers (all or just specified ones). */
static int
registers_cmd(const cmd_info_t *cmd_info)
{
	char reg_names[256];
	int i, j;

	if(cmd_info->argc == 0)
	{
		return show_register_menu(curr_view, valid_registers) != 0;
	}

	/* Format list of unique register names. */
	j = 0;
	reg_names[0] = '\0';
	for(i = 0; i < cmd_info->argc; ++i)
	{
		const char *p = cmd_info->argv[i];
		while(*p != '\0')
		{
			if(strchr(reg_names, *p) == NULL)
			{
				reg_names[j++] = *p;
				reg_names[j] = '\0';
			}
			++p;
		}
	}

	return show_register_menu(curr_view, reg_names) != 0;
}

/* Switches to regular view leaving custom view. */
static int
regular_cmd(const cmd_info_t *cmd_info)
{
	if(flist_custom_active(curr_view))
	{
		rn_leave(curr_view, 1);
	}
	return 0;
}

/* Renames selected files of the current view. */
static int
rename_cmd(const cmd_info_t *cmd_info)
{
	flist_set_marking(curr_view, 0);
	return fops_rename(curr_view, cmd_info->argv, cmd_info->argc,
			cmd_info->emark) != 0;
}

/* Resets internal state and reloads configuration files. */
static int
restart_cmd(const cmd_info_t *cmd_info)
{
	int full = 0;
	if(cmd_info->argc == 1)
	{
		if(strcmp(cmd_info->argv[0], "full") != 0)
		{
			ui_sb_errf("Unexpected argument: %s", cmd_info->argv[0]);
			return CMDS_ERR_CUSTOM;
		}
		full = 1;
	}

	(void)restart_into_session(cfg.session, full);
	return 0;
}

static int
restore_cmd(const cmd_info_t *cmd_info)
{
	flist_set_marking(curr_view, 0);
	return fops_restore(curr_view) != 0;
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

	int argc = cmd_info->argc;
	char **argv = cmd_info->argv;
	int flags = parse_cpmv_flags(&argc, &argv);
	if(flags < 0)
	{
		return CMDS_ERR_CUSTOM;
	}

	flags |= (cmd_info->emark ? CMLF_FORCE : CMLF_NONE);

	flist_set_marking(curr_view, 0);

	if(cmd_info->qmark)
	{
		if(argc > 0)
		{
			ui_sb_err("No positional arguments are allowed if you use \"?\"");
			return CMDS_ERR_CUSTOM;
		}
		return fops_cpmv(curr_view, NULL, -1, op, flags) != 0;
	}

	return fops_cpmv(curr_view, argv, argc, op, flags) != 0;
}

/* Parses leading copy/move options and adjusts argc/argv to exclude them.
 * Returns -1 on parsing error, otherwise combination of CMLF_* values is
 * returned. */
static int
parse_cpmv_flags(int *argc, char ***argv)
{
	int flags = 0;

	int i;
	for(i = 0; i < *argc; ++i)
	{
		if(argv[0][i][0] != '-')
		{
			/* Implicit end of options. */
			break;
		}
		if(strcmp(argv[0][i], "--") == 0)
		{
			++i;
			/* Explicit end of options. */
			break;
		}

		if(strcmp(argv[0][i], "-skip") == 0)
		{
			flags |= CMLF_SKIP;
		}
		else
		{
			ui_sb_errf("Unrecognized :command option: %s", argv[0][i]);
			return -1;
		}
	}

	*argc -= i;
	*argv += i;

	return flags;
}

/* Shows status of terminal multiplexers support, sets it or toggles it. */
static int
screen_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->qmark)
	{
		if(cfg.use_term_multiplexer)
		{
			if(curr_stats.term_multiplexer != TM_NONE)
			{
				ui_sb_msgf("Integration with %s is active",
						(curr_stats.term_multiplexer == TM_SCREEN) ? "GNU screen" : "tmux");
			}
			else
			{
				ui_sb_msg("Integration with terminal multiplexers is enabled but "
						"inactive");
			}
		}
		else
		{
			ui_sb_msg("Integration with terminal multiplexers is disabled");
		}
		return 1;
	}

	if(cmd_info->emark)
	{
		cfg_set_use_term_multiplexer(1);
	}
	else
	{
		cfg_set_use_term_multiplexer(!cfg.use_term_multiplexer);
	}

	return 0;
}

/* Selects files that match passed in expression or range. */
static int
select_cmd(const cmd_info_t *cmd_info)
{
	int error;
	cmds_preserve_selection();

	/* If no arguments are passed, select the range. */
	if(cmd_info->argc == 0)
	{
		/* Append to previous selection unless ! is specified. */
		if(cmd_info->emark)
		{
			flist_sel_drop(curr_view);
		}

		flist_sel_by_range(curr_view, cmd_info->begin, cmd_info->end, 1);
		return 0;
	}

	if(cmd_info->begin != NOT_DEF)
	{
		ui_sb_err("Either range or argument should be supplied.");
		error = 1;
	}
	else if(cmd_info->args[0] == '!' && !char_is_one_of("/{", cmd_info->args[1]))
	{
		error = flist_sel_by_filter(curr_view, cmd_info->args + 1, cmd_info->emark,
				1);
	}
	else
	{
		error = flist_sel_by_pattern(curr_view, cmd_info->args, cmd_info->emark, 1);
	}

	return (error ? CMDS_ERR_CUSTOM : 0);
}

/* Displays current session, detaches from a session or switches to a (possibly
 * new) session. */
static int
session_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->qmark)
	{
		if(sessions_active())
		{
			ui_sb_msgf("Active session: %s", sessions_current());
		}
		else
		{
			ui_sb_msg("No active session");
		}
		return 1;
	}

	if(cmd_info->argc == 0)
	{
		char *current = strdup(sessions_current());
		if(sessions_stop() == 0)
		{
			ui_sb_msgf("Detached from session without saving: %s", current);
			put_string(&curr_stats.last_session, current);
			return 1;
		}
		ui_sb_msg("No active session");
		free(current);
		return 1;
	}

	const char *session_name = cmd_info->argv[0];
	if(contains_slash(session_name))
	{
		ui_sb_err("Session name can't include path separators");
		return CMDS_ERR_CUSTOM;
	}

	if(strcmp(session_name, "-") == 0)
	{
		if(is_null_or_empty(curr_stats.last_session))
		{
			ui_sb_err("No previous session");
			return CMDS_ERR_CUSTOM;
		}
		if(!sessions_exists(curr_stats.last_session))
		{
			ui_sb_err("Previous session doesn't exist");
			return CMDS_ERR_CUSTOM;
		}

		session_name = curr_stats.last_session;
	}

	char *old_current_session = NULL;
	update_string(&old_current_session, sessions_current());

	if(switch_to_a_session(session_name) == 0)
	{
		update_string(&curr_stats.last_session, old_current_session);
	}

	free(old_current_session);
	return 1;
}

/* Performs switch to a session by its name.  Always prints status bar message.
 * Returns zero on success, otherwise non-zero is returned. */
static int
switch_to_a_session(const char session_name[])
{
	if(sessions_active())
	{
		if(sessions_current_is(session_name))
		{
			ui_sb_msgf("Already active session: %s", session_name);
			return 1;
		}

		state_store();
	}

	if(sessions_create(session_name) == 0)
	{
		ui_sb_msgf("Switched to a new session: %s", sessions_current());
		return 0;
	}

	if(restart_into_session(session_name, 0) != 0)
	{
		if(sessions_active())
		{
			ui_sb_errf("Session switching has failed, active session: %s",
					sessions_current());
		}
		else
		{
			ui_sb_err("Session switching has failed, no active session");
		}
		return CMDS_ERR_CUSTOM;
	}

	ui_sb_msgf("Loaded session: %s", sessions_current());
	return 0;
}

/* Performs restart and optional (re)loading of a session.  Returns zero on
 * success, otherwise non-zero is returned. */
static int
restart_into_session(const char session[], int full)
{
	instance_start_restart();

	if(full || session != NULL)
	{
		tabs_reinit();
	}

	int result;
	if(session == NULL)
	{
		state_load(!full);
		result = 0;
	}
	else
	{
		result = sessions_load(session);
	}

	instance_finish_restart();
	return result;
}

/* Updates/displays global and local options. */
static int
set_cmd(const cmd_info_t *cmd_info)
{
	const int result = process_set_args(cmd_info->args, 1, 1);
	return (result < 0) ? CMDS_ERR_CUSTOM : (result != 0);
}

/* Updates/displays only global options. */
static int
setglobal_cmd(const cmd_info_t *cmd_info)
{
	const int result = process_set_args(cmd_info->args, 1, 0);
	return (result < 0) ? CMDS_ERR_CUSTOM : (result != 0);
}

/* Updates/displays only local options. */
static int
setlocal_cmd(const cmd_info_t *cmd_info)
{
	const int result = process_set_args(cmd_info->args, 0, 1);
	return (result < 0) ? CMDS_ERR_CUSTOM : (result != 0);
}

static int
shell_cmd(const cmd_info_t *cmd_info)
{
	rn_shell(NULL, PAUSE_NEVER, cmd_info->emark ? 0 : 1, SHELL_BY_APP);
	return 0;
}

/* Navigates to [count]th next sibling directory. */
static int
siblnext_cmd(const cmd_info_t *cmd_info)
{
	const int count = (cmd_info->count == NOT_DEF ? 1 : cmd_info->count);
	return (go_to_sibling_dir(curr_view, count, cmd_info->emark) != 0);
}

/* Navigates to [count]th previous sibling directory. */
static int
siblprev_cmd(const cmd_info_t *cmd_info)
{
	const int count = (cmd_info->count == NOT_DEF ? 1 : cmd_info->count);
	return (go_to_sibling_dir(curr_view, -count, cmd_info->emark) != 0);
}

static int
sort_cmd(const cmd_info_t *cmd_info)
{
	enter_sort_mode(curr_view);
	cmds_preserve_selection();
	return 0;
}

static int
source_cmd(const cmd_info_t *cmd_info)
{
	int ret = 0;
	char *path = expand_tilde(cmd_info->argv[0]);

	if(!path_exists(path, DEREF))
	{
		ui_sb_errf("Sourced file doesn't exist: %s", cmd_info->argv[0]);
		ret = CMDS_ERR_CUSTOM;
	}
	else if(os_access(path, R_OK) != 0)
	{
		ui_sb_errf("Sourced file isn't readable: %s", cmd_info->argv[0]);
		ret = CMDS_ERR_CUSTOM;
	}
	else if(cfg_source_file(path) != 0)
	{
		ui_sb_errf("Error sourcing file: %s", cmd_info->argv[0]);
		ret = CMDS_ERR_CUSTOM;
	}

	free(path);
	return ret;
}

static int
split_cmd(const cmd_info_t *cmd_info)
{
	return do_split(cmd_info, HSPLIT);
}

/* Stops the process by send itself SIGSTOP. */
static int
stop_cmd(const cmd_info_t *cmd_info)
{
	instance_stop();
	return 0;
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
		 * parse_case_flag(). */
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
			(void)replace_string(&last_pattern, hists_search_last());
		}
		else
		{
			(void)replace_string(&last_pattern, cmd_info->argv[0]);
			hists_search_save(last_pattern);
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
		ui_sb_err("No previous pattern");
		return CMDS_ERR_CUSTOM;
	}

	flist_set_marking(curr_view, 0);
	return fops_subst(curr_view, last_pattern, last_sub, ic, glob) != 0;
}

/* Synchronizes path/cursor position of the other pane with corresponding
 * properties of the current one, possibly including some relative path
 * changes. */
static int
sync_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->emark && cmd_info->argc != 0)
	{
		return sync_selectively(cmd_info);
	}

	if(cmd_info->argc > 1)
	{
		return CMDS_ERR_TRAILING_CHARS;
	}

	if(cmd_info->argc > 0)
	{
		char dst_path[PATH_MAX + 1];
		to_canonic_path(cmd_info->argv[0], flist_get_dir(curr_view), dst_path,
				sizeof(dst_path));
		sync_location(dst_path, 0, cmd_info->emark, 0, 0);
	}
	else
	{
		sync_location(flist_get_dir(curr_view), 0, cmd_info->emark, 0, 0);
	}

	return 0;
}

/* Mirrors requested properties of current view with the other one.  Returns
 * value to be returned by command handler. */
static int
sync_selectively(const cmd_info_t *cmd_info)
{
	int location = 0, cursor_pos = 0, local_options = 0, filters = 0,
			filelist = 0, tree = 0;
	if(parse_sync_properties(cmd_info, &location, &cursor_pos, &local_options,
				&filters, &filelist, &tree) != 0)
	{
		return CMDS_ERR_CUSTOM;
	}

	if(!cv_tree(curr_view->custom.type))
	{
		tree = 0;
	}

	if(local_options)
	{
		sync_local_opts(location);
	}
	if(location)
	{
		filelist = filelist && flist_custom_active(curr_view);
		sync_location(flist_get_dir(curr_view), filelist, cursor_pos, filters,
				tree);
	}
	if(filters)
	{
		sync_filters();
	}

	return 0;
}

/* Parses selective view synchronization properties.  Default values for
 * arguments should be set before the call.  Returns zero on success, otherwise
 * non-zero is returned and error message is displayed on the status bar. */
static int
parse_sync_properties(const cmd_info_t *cmd_info, int *location,
		int *cursor_pos, int *local_options, int *filters, int *filelist, int *tree)
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
		else if(strcmp(property, "filelist") == 0)
		{
			*location = 1;
			*filelist = 1;
		}
		else if(strcmp(property, "tree") == 0)
		{
			*location = 1;
			*tree = 1;
		}
		else if(strcmp(property, "all") == 0)
		{
			*location = 1;
			*cursor_pos = 1;
			*local_options = 1;
			*filters = 1;
			*filelist = 1;
			*tree = 1;
		}
		else
		{
			ui_sb_errf("Unknown selective sync property: %s", property);
			return 1;
		}
	}

	return 0;
}

/* Mirrors location (directory and maybe cursor position plus local filter) of
 * the current view to the other one. */
static void
sync_location(const char path[], int cv, int sync_cursor_pos, int sync_filters,
		int tree)
{
	if(!cd_is_possible(path) || change_directory(other_view, path) < 0)
	{
		return;
	}

	if(tree)
	{
		/* Normally changing location resets local filter.  Prevent this by
		 * synchronizing it here (after directory changing, but before loading list
		 * of files, hence no extra work). */
		if(sync_filters)
		{
			local_filter_apply(other_view, curr_view->local_filter.filter.raw);
		}

		(void)flist_clone_tree(other_view, curr_view);
	}
	else if(cv)
	{
		flist_custom_clone(other_view, curr_view, 0);
		if(sync_filters)
		{
			local_filter_apply(other_view, curr_view->local_filter.filter.raw);
			replace_dir_entries(other_view, &other_view->custom.entries,
					&other_view->custom.entry_count, other_view->dir_entry,
					other_view->list_rows);
		}
	}
	else
	{
		/* Normally changing location resets local filter.  Prevent this by
		 * synchronizing it here (after directory changing, but before loading list
		 * of files, hence no extra work). */
		if(sync_filters)
		{
			local_filter_apply(other_view, curr_view->local_filter.filter.raw);
		}

		(void)populate_dir_list(other_view, 0);
	}

	if(sync_cursor_pos)
	{
		if(flist_custom_active(curr_view) && !flist_custom_active(other_view))
		{
			flist_hist_lookup(other_view, curr_view);
		}
		else
		{
			char curr_file_path[PATH_MAX + 1];

			const int offset = (curr_view->list_pos - curr_view->top_line);
			const int shift = (offset*other_view->window_rows)/curr_view->window_rows;

			get_current_full_path(curr_view, sizeof(curr_file_path), curr_file_path);
			break_atr(curr_file_path, '/');
			navigate_to_file(other_view, curr_file_path,
					get_current_file_name(curr_view), 1);

			other_view->top_line = MAX(0, curr_view->list_pos - shift);
			(void)fview_enforce_scroll_offset(other_view);
		}

		flist_hist_save(other_view);
	}

	ui_view_schedule_redraw(other_view);
}

/* Sets local options of the other view to be equal to options of the current
 * one. */
static void
sync_local_opts(int defer_slow)
{
	clone_local_options(curr_view, other_view, defer_slow);
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
	matcher_free(other_view->manual_filter);
	other_view->manual_filter = matcher_clone(curr_view->manual_filter);
	(void)filter_assign(&other_view->auto_filter, &curr_view->auto_filter);
	ui_view_schedule_reload(other_view);
}

/* Closes current tab unless it's the last one. */
static int
tabclose_cmd(const cmd_info_t *cmd_info)
{
	tabs_close();
	return 0;
}

/* Moves current tab to a different position. */
static int
tabmove_cmd(const cmd_info_t *cmd_info)
{
	int where_to;

	if(cmd_info->argc == 0 || strcmp(cmd_info->argv[0], "$") == 0)
	{
		where_to = tabs_count(curr_view);
	}
	else if(!read_int(cmd_info->argv[0], &where_to))
	{
		return CMDS_ERR_INVALID_ARG;
	}

	tabs_move(curr_view, where_to);
	ui_views_update_titles();
	return 0;
}

/* Sets, changes or resets name of current tab. */
static int
tabname_cmd(const cmd_info_t *cmd_info)
{
	tabs_rename(curr_view, cmd_info->argc == 0 ? NULL : cmd_info->argv[0]);
	ui_views_update_titles();
	return 0;
}

/* Creates a new tab.  Takes optional path for the new tab. */
static int
tabnew_cmd(const cmd_info_t *cmd_info)
{
	if(cfg.pane_tabs && curr_view->custom.type == CV_DIFF)
	{
		ui_sb_err("Switching tab of single pane would drop comparison");
		return CMDS_ERR_CUSTOM;
	}

	const char *path = NULL;
	char canonic_dir[PATH_MAX + 1];

	if(cmd_info->argc > 0)
	{
		char dir[PATH_MAX + 1];

		int updir;
		flist_pick_cd_path(curr_view, flist_get_dir(curr_view), cmd_info->argv[0],
				&updir, dir, sizeof(dir));
		if(updir)
		{
			copy_str(dir, sizeof(dir), "..");
		}

		to_canonic_path(dir, flist_get_dir(curr_view), canonic_dir,
				sizeof(canonic_dir));

		if(!cd_is_possible(canonic_dir))
		{
			return 0;
		}

		path = canonic_dir;
	}

	if(tabs_new(NULL, path) != 0)
	{
		ui_sb_err("Failed to open a new tab");
		return CMDS_ERR_CUSTOM;
	}
	return 0;
}

/* Switches either to the next tab or to tab specified by its number in the only
 * optional parameter. */
static int
tabnext_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->argc == 0)
	{
		tabs_next(1);
		return 0;
	}

	int n;
	if(!read_int(cmd_info->argv[0], &n) || n <= 0 || n > tabs_count(curr_view))
	{
		return CMDS_ERR_INVALID_ARG;
	}

	tabs_goto(n - 1);
	return 0;
}

/* Closes all tabs but the current one. */
static int
tabonly_cmd(const cmd_info_t *cmd_info)
{
	tabs_only(curr_view);
	stats_redraw_later();
	return 0;
}

/* Switches either to the previous tab or to n-th previous tab, where n is
 * specified by the only optional parameter. */
static int
tabprevious_cmd(const cmd_info_t *cmd_info)
{
	if(cmd_info->argc == 0)
	{
		tabs_previous(1);
		return 0;
	}

	int n;
	if(!read_int(cmd_info->argv[0], &n) || n <= 0)
	{
		return CMDS_ERR_INVALID_ARG;
	}

	tabs_previous(n);
	return 0;
}

/* Creates files. */
static int
touch_cmd(const cmd_info_t *cmd_info)
{
	const int at = get_at(curr_view, cmd_info);
	return fops_mkfiles(curr_view, at, cmd_info->argv, cmd_info->argc) != 0;
}

/* Gets destination position based on range.  Returns the position. */
static int
get_at(const view_t *view, const cmd_info_t *cmd_info)
{
	return (cmd_info->end == NOT_DEF) ? view->list_pos : cmd_info->end;
}

/* Replaces letters in names of files according to character mapping. */
static int
tr_cmd(const cmd_info_t *cmd_info)
{
	char buf[strlen(cmd_info->argv[0]) + 1];
	size_t pl, sl;

	if(cmd_info->argv[0][0] == '\0' || cmd_info->argv[1][0] == '\0')
	{
		ui_sb_err("Empty argument");
		return CMDS_ERR_CUSTOM;
	}

	pl = strlen(cmd_info->argv[0]);
	sl = strlen(cmd_info->argv[1]);
	strcpy(buf, cmd_info->argv[1]);
	if(pl < sl)
	{
		ui_sb_err("Second argument cannot be longer");
		return CMDS_ERR_CUSTOM;
	}
	else if(pl > sl)
	{
		while(sl < pl)
		{
			buf[sl] = buf[sl - 1];
			++sl;
		}
		buf[sl] = '\0';
	}

	flist_set_marking(curr_view, 0);
	return fops_tr(curr_view, cmd_info->argv[0], buf) != 0;
}

/* Lists all valid non-empty trash directories in a menu with optional size of
 * each one. */
static int
trashes_cmd(const cmd_info_t *cmd_info)
{
	return show_trashes_menu(curr_view, cmd_info->qmark) != 0;
}

/* Convert view into a tree with root at current location. */
static int
tree_cmd(const cmd_info_t *cmd_info)
{
	int in_tree = flist_custom_active(curr_view)
	           && cv_tree(curr_view->custom.type);

	if(cmd_info->emark && in_tree)
	{
		rn_leave(curr_view, 1);
		return 0;
	}

	int depth;
	if(parse_tree_properties(cmd_info, &depth) != 0)
	{
			return CMDS_ERR_CUSTOM;
	}

	(void)flist_load_tree(curr_view, flist_get_dir(curr_view), depth);
	return 0;
}

/* Parses properties of atree.  Default values are set by the function.  Returns
 * zero on success, otherwise non-zero is returned and error message is
 * displayed on the status bar. */
static int
parse_tree_properties(const cmd_info_t *cmd_info, int *depth)
{
	*depth = INT_MAX;

	int i;
	for(i = 0; i < cmd_info->argc; ++i)
	{
		const char *arg = cmd_info->argv[i];
		if(skip_prefix(&arg, "depth="))
		{
			char *endptr;
			const long value = strtol(arg, &endptr, 10);
			if(*endptr != '\0' || value < 1)
			{
				ui_sb_errf("Invalid depth: %s", arg);
				return CMDS_ERR_CUSTOM;
			}

			*depth = value - 1;
		}
		else
		{
			ui_sb_errf("Invalid argument: %s", arg);
			return CMDS_ERR_CUSTOM;
		}
	}

	return 0;
}

static int
undolist_cmd(const cmd_info_t *cmd_info)
{
	return show_undolist_menu(curr_view, cmd_info->emark) != 0;
}

static int
unlet_cmd(const cmd_info_t *cmd_info)
{
	vle_tb_clear(vle_err);
	if(unlet_variables(cmd_info->args) != 0 && !cmd_info->emark)
	{
		ui_sb_err(vle_tb_get_data(vle_err));
		return CMDS_ERR_CUSTOM;
	}
	return 0;
}

/* Unmaps keys in normal and visual or in command-line mode. */
static int
unmap_cmd(const cmd_info_t *cmd_info)
{
	wchar_t *subst = substitute_specs(cmd_info->argv[0]);
	if(subst == NULL)
	{
		show_error_msgf("Unmapping Error", "Failed to convert to wide string: %s",
				cmd_info->argv[0]);
		return 0;
	}

	int result;
	if(cmd_info->emark)
	{
		result = (vle_keys_user_remove(subst, CMDLINE_MODE) != 0);
	}
	else if(!vle_keys_user_exists(subst, NORMAL_MODE))
	{
		ui_sb_err("No such mapping in normal mode");
		result = -1;
	}
	else if(!vle_keys_user_exists(subst, VISUAL_MODE))
	{
		ui_sb_err("No such mapping in visual mode");
		result = -2;
	}
	else
	{
		result = (vle_keys_user_remove(subst, NORMAL_MODE) != 0);
		result += (vle_keys_user_remove(subst, VISUAL_MODE) != 0);
	}
	free(subst);

	if(result > 0)
	{
		ui_sb_err("Error while unmapping keys");
	}
	return (result != 0 ? CMDS_ERR_CUSTOM : 0);
}

/* Unselects files that match passed in expression or range. */
static int
unselect_cmd(const cmd_info_t *cmd_info)
{
	int error;
	cmds_preserve_selection();

	/* If no arguments are passed, unselect the range. */
	if(cmd_info->argc == 0)
	{
		flist_sel_by_range(curr_view, cmd_info->begin, cmd_info->end, 0);
		return 0;
	}

	if(cmd_info->begin != NOT_DEF)
	{
		ui_sb_err("Either range or argument should be supplied.");
		error = 1;
	}
	else if(cmd_info->args[0] == '!' && !char_is_one_of("/{", cmd_info->args[1]))
	{
		error = flist_sel_by_filter(curr_view, cmd_info->args + 1, cmd_info->emark,
				0);
	}
	else
	{
		error = flist_sel_by_pattern(curr_view, cmd_info->args, 0, 0);
	}

	return (error ? CMDS_ERR_CUSTOM : 0);
}

static int
view_cmd(const cmd_info_t *cmd_info)
{
	cmds_preserve_selection();

	if((!curr_stats.preview.on || cmd_info->emark) && !qv_can_show())
	{
		return CMDS_ERR_CUSTOM;
	}
	if(curr_stats.preview.on && cmd_info->emark)
	{
		return 0;
	}
	qv_toggle();
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

/* Maps keys in the specified mode. */
static int
do_map(const cmd_info_t *cmd_info, const char map_type[], int mode,
		int no_remap)
{
	wchar_t *keys, *mapping;
	char *raw_rhs, *rhs;
	char t;

	if(cmd_info->argc <= 1)
	{
		keys = substitute_specs(cmd_info->args);
		if(keys != NULL)
		{
			int save_msg = show_map_menu(curr_view, map_type, mode, keys);
			free(keys);
			return save_msg != 0;
		}
		show_error_msgf("Mapping Error", "Failed to convert to wide string: %s",
				cmd_info->args);
		return 0;
	}

	const char *args = cmd_info->args;
	const int flags = (no_remap ? KEYS_FLAG_NOREMAP : KEYS_FLAG_NONE)
	                | parse_map_args(&args);

	raw_rhs = vle_cmds_past_arg(args);
	t = *raw_rhs;
	*raw_rhs = '\0';

	int error = 0;
	rhs = vle_cmds_at_arg(raw_rhs + 1);
	keys = substitute_specs(args);
	mapping = substitute_specs(rhs);
	if(keys != NULL && mapping != NULL)
	{
		error = vle_keys_user_add(keys, mapping, mode, flags);
	}
	else
	{
		show_error_msgf("Mapping Error", "Failed to convert to wide string: %s",
				cmd_info->args);
	}
	free(mapping);
	free(keys);

	*raw_rhs = t;

	if(error)
		show_error_msg("Mapping Error", "Unable to allocate enough memory");

	return 0;
}

/* Parses <*> :*map arguments removing them from the line.  Returns flags
 * collected. */
static int
parse_map_args(const char **args)
{
	int flags = 0;
	do
	{
		if(skip_prefix(args, "<silent>"))
		{
			flags |= KEYS_FLAG_SILENT;
		}
		else if(skip_prefix(args, "<wait>"))
		{
			flags |= KEYS_FLAG_WAIT;
		}
		else
		{
			break;
		}
		*args = skip_whitespace(*args);
	}
	while(1);
	return flags;
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
		ui_sb_err("No arguments are allowed if you use \"!\"");
		return CMDS_ERR_CUSTOM;
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

/* Unmaps keys for the specified mode.  Returns zero on success, otherwise
 * non-zero is returned and message is printed on the status bar. */
static int
do_unmap(const char keys[], int mode)
{
	wchar_t *subst = substitute_specs(keys);
	if(subst == NULL)
	{
		show_error_msgf("Unmapping Error", "Failed to convert to wide string: %s",
				keys);
		return 0;
	}

	int result = vle_keys_user_remove(subst, mode);
	free(subst);

	if(result != 0)
	{
		ui_sb_err("No such mapping");
		return CMDS_ERR_CUSTOM;
	}
	return 0;
}

/* Executes Ctrl-W [count] {arg} normal mode command. */
static int
wincmd_cmd(const cmd_info_t *cmd_info)
{
	int count;
	char *cmd;
	wchar_t *wcmd;

	if(cmd_info->count != NOT_DEF && cmd_info->count < 0)
	{
		return CMDS_ERR_INVALID_RANGE;
	}
	if(cmd_info->args[0] == '\0' || cmd_info->args[1] != '\0')
	{
		return CMDS_ERR_INVALID_ARG;
	}

	count = (cmd_info->count <= 1) ? 1 : cmd_info->count;
	cmd = format_str("%c%d%s", NC_C_w, count, cmd_info->args);

	wcmd = to_wide(cmd);
	if(wcmd == NULL)
	{
		show_error_msgf("Command Error", "Failed to convert to wide string: %s",
				cmd);
		free(cmd);
		return 0;
	}
	free(cmd);

	(void)vle_keys_exec_timed_out_no_remap(wcmd);
	free(wcmd);
	return 0;
}

/* Prefix that execute same command-line command for both panes. */
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

	stats_refresh_later();

	return result;
}

/* Executes cmd command-line command for a specific view. */
static int
winrun(view_t *view, const char cmd[])
{
	const int prev_global_local_settings = curr_stats.global_local_settings;
	int result;
	view_t *tmp_curr, *tmp_other;

	ui_view_pick(view, &tmp_curr, &tmp_other);

	/* :winrun and :windo should be able to set settings separately for each
	 * window. */
	curr_stats.global_local_settings = 0;
	result = cmds_dispatch(cmd, curr_view, CIT_COMMAND);
	curr_stats.global_local_settings = prev_global_local_settings;

	ui_view_unpick(view, tmp_curr, tmp_other);

	return result;
}

static int
write_cmd(const cmd_info_t *cmd_info)
{
	state_store();
	return 0;
}

/* Possibly exits vifm normally with or without saving state to vifminfo
 * file. */
static int
qall_cmd(const cmd_info_t *cmd_info)
{
	vifm_try_leave(!cmd_info->emark, 0, cmd_info->emark);
	return 0;
}

/* Possibly exits vifm normally with or without saving state to vifminfo file or
 * closes a tab. */
static int
quit_cmd(const cmd_info_t *cmd_info)
{
	ui_quit(!cmd_info->emark, cmd_info->emark);
	return 0;
}

/* Possibly exits the application saving vifminfo file or closes a tab. */
static int
wq_cmd(const cmd_info_t *cmd_info)
{
	ui_quit(1, cmd_info->emark);
	return 0;
}

/* Possibly exits the application saving vifminfo file. */
static int
wqall_cmd(const cmd_info_t *cmd_info)
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
		flist_set_marking(curr_view, 0);
		result = fops_yank(curr_view, reg) != 0;
	}

	return result;
}

/* Processes arguments of form "{reg} [{count}]" or "{reg}|{count}".  On success
 * *reg is set (so it should be initialized before the call) and zero is
 * returned, otherwise cmds unit error code is returned. */
static int
get_reg_and_count(const cmd_info_t *cmd_info, int *reg)
{
	if(cmd_info->argc == 2)
	{
		int count;
		int error;

		error = get_reg(cmd_info->argv[0], reg);
		if(error != 0)
		{
			return error;
		}

		if(!isdigit(cmd_info->argv[1][0]))
			return CMDS_ERR_TRAILING_CHARS;

		count = atoi(cmd_info->argv[1]);
		if(count == 0)
		{
			ui_sb_err("Count argument can't be zero");
			return CMDS_ERR_CUSTOM;
		}
		flist_sel_count(curr_view, cmd_info->end, count);
	}
	else if(cmd_info->argc == 1)
	{
		if(isdigit(cmd_info->argv[0][0]))
		{
			int count = atoi(cmd_info->argv[0]);
			if(count == 0)
			{
				ui_sb_err("Count argument can't be zero");
				return CMDS_ERR_CUSTOM;
			}
			flist_sel_count(curr_view, cmd_info->end, count);
		}
		else
		{
			return get_reg(cmd_info->argv[0], reg);
		}
	}
	return 0;
}

/* Processes argument as register name.  On success *reg is set (so it should be
 * initialized before the call) and zero is returned, otherwise cmds unit error
 * code is returned. */
static int
get_reg(const char arg[], int *reg)
{
	if(arg[1] != '\0')
	{
		return CMDS_ERR_TRAILING_CHARS;
	}
	if(!regs_exists(arg[0]))
	{
		return CMDS_ERR_TRAILING_CHARS;
	}

	*reg = arg[0];
	return 0;
}

/* Special handler for user defined commands, which are defined using
 * :command. */
static int
usercmd_cmd(const cmd_info_t *cmd_info)
{
	char *expanded_com;
	MacroFlags flags;
	int external = 1;
	int bg;
	int save_msg = 0;

	MacroExpandReason mer = MER_OP;
	if(vle_cmds_identify(cmd_info->user_action) == COM_EXECUTE)
	{
		mer = MER_SHELL_OP;
	}

	/* Expand macros in a bound command. */
	expanded_com = ma_expand(cmd_info->user_action, cmd_info->args, &flags, mer);

	if(expanded_com[0] == ':')
	{
		cmds_scope_start();

		int sm = cmds_dispatch(expanded_com, curr_view, CIT_COMMAND);
		free(expanded_com);

		if(cmds_scope_finish() != 0)
		{
			ui_sb_err("Unmatched if-else-endif");
			return CMDS_ERR_CUSTOM;
		}

		return sm != 0;
	}

	bg = parse_bg_mark(expanded_com);

	flist_sel_stash(curr_view);

	char *title = format_str(":%s%s%s", cmd_info->user_cmd,
			(cmd_info->raw_args[0] == '\0' ? "" : " "), cmd_info->raw_args);
	int handled = rn_ext(curr_view, expanded_com, title, flags, bg, &save_msg);
	free(title);

	const int use_term_multiplexer = ma_flags_missing(flags, MF_NO_TERM_MUX);

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
		save_msg = cmds_dispatch1(expanded_com, curr_view, CIT_COMMAND);
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
			bg_run_external(com_beginning, 0, SHELL_BY_USER, NULL);
		}
		else if(strlen(com_beginning) > 0)
		{
			rn_shell(com_beginning, pause ? PAUSE_ALWAYS : PAUSE_ON_ERROR,
					use_term_multiplexer, SHELL_BY_USER);
		}
	}
	else if(expanded_com[0] == '/')
	{
		modnorm_set_search_attrs(/*count=*/1, /*last_search_backward=*/0);
		cmds_dispatch1(expanded_com + 1, curr_view, CIT_FSEARCH_PATTERN);
		cmds_preserve_selection();
		external = 0;
	}
	else if(expanded_com[0] == '=')
	{
		cmds_dispatch1(expanded_com + 1, curr_view, CIT_FILTER_PATTERN);
		ui_view_schedule_reload(curr_view);
		cmds_preserve_selection();
		external = 0;
	}
	else if(bg)
	{
		rn_start_bg_command(curr_view, expanded_com, flags);
	}
	else if(ma_flags_present(flags, MF_PIPE_FILE_LIST) ||
			ma_flags_present(flags, MF_PIPE_FILE_LIST_Z))
	{
		(void)rn_pipe(expanded_com, curr_view, flags, PAUSE_ON_ERROR);
	}
	else
	{
		(void)rn_shell(expanded_com, PAUSE_ON_ERROR, use_term_multiplexer,
				SHELL_BY_USER);
	}

	if(external)
	{
		un_group_reopen_last();
		un_group_add_op(OP_USR, strdup(expanded_com), NULL, "", "");
		un_group_close();
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

TSTATIC void
cmds_drop_state(void)
{
	update_string(&cmds_state.find.last_args, NULL);
	cmds_state.find.includes_path = 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
