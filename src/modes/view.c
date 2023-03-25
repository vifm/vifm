/* vifm
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

#include "view.h"

#include <curses.h>

#include <regex.h>
#include <unistd.h> /* usleep() */

#include <assert.h> /* assert() */
#include <limits.h> /* INT_MAX */
#include <stddef.h> /* ptrdiff_t size_t */
#include <string.h> /* memset() strdup() */
#include <stdio.h>  /* snprintf() */
#include <stdlib.h> /* free() */

#include "../cfg/config.h"
#include "../compat/curses.h"
#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "../compat/reallocarray.h"
#include "../engine/keys.h"
#include "../engine/mode.h"
#include "../int/vim.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../ui/colors.h"
#include "../ui/escape.h"
#include "../ui/fileview.h"
#include "../ui/quickview.h"
#include "../ui/statusbar.h"
#include "../ui/ui.h"
#include "../utils/filemon.h"
#include "../utils/fs.h"
#include "../utils/macros.h"
#include "../utils/path.h"
#include "../utils/regexp.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../utils/test_helpers.h"
#include "../utils/utf8.h"
#include "../utils/utils.h"
#include "../filelist.h"
#include "../filetype.h"
#include "../running.h"
#include "../status.h"
#include "../types.h"
#include "../vcache.h"
#include "cmdline.h"
#include "modes.h"
#include "normal.h"
#include "wk.h"

/* Named boolean values of "silent" parameter for better readability. */
enum
{
	NOSILENT, /* Display error message dialog. */
	SILENT,   /* Do not display error message dialog. */
};

/* Describes view state and its properties. */
struct modview_info_t
{
	/* Data of the view. */
	char **lines;     /* List of real lines (owned by vcache unit). */
	int (*widths)[2]; /* (virtual line, screen width) pair per real line. */
	int nlines;       /* Number of real lines. */
	int nlinesv;      /* Number of virtual (possibly wrapped) lines. */
	int line;         /* Current real line number (first visible line). */
	int linev;        /* Current virtual line number. */

	/* Dimensions, units of actions. */
	int win_size; /* Scroll window size. */
	int half_win; /* Height of a "page" (can be changed). */
	int width;    /* Last width used for breaking lines. */

	/* Monitoring of changes for automatic forwarding. */
	int auto_forward;   /* Whether auto forwarding (tail -F) is enabled. */
	filemon_t file_mon; /* File monitor for auto forwarding mode. */

	/* Related to search. */
	regex_t re;               /* Search regular expression. */
	int last_search_backward; /* Value -1 means no search was performed. */
	int search_repeat;        /* Saved count prefix of search commands. */

	/* Viewers. */
	strlist_t viewers;       /* List of viewers of current file. */
	char *ext_viewer;        /* When non-NULL, specifies custom preview command
	                            (no implicit %c). */
	const char *curr_viewer; /* Current viewer (pointer to strings of two previous
	                            fields). */

	/* The rest of the state. */
	view_t *view;     /* File view association with the view. */
	char *filename;   /* Full path to the file being viewed. */
	int detached;     /* Whether view mode was detached. */
	ViewerKind kind;  /* Kind of preview. */
	MacroFlags flags; /* Macro flags for ext_viewer. */
	int wrap;         /* Whether lines are wrapped. */
	int raw;          /* Forced raw preview. */
};

/* View information structure indexes and count. */
enum
{
	VI_LWIN,  /* Index of view information structure for left window. */
	VI_RWIN,  /* Index of view information structure for right window. */
	VI_COUNT, /* Number of view information structures. */
};

static int try_resurrect_detached(const char full_path[], int explore);
static void try_redraw_explore_view(const view_t *view);
static void reset_view_info(modview_info_t *vi);
static void init_view_info(modview_info_t *vi);
static void free_view_info(modview_info_t *vi);
static void redraw(void);
static void calc_vlines(void);
static void calc_vlines_wrapped(modview_info_t *vi);
static void calc_vlines_non_wrapped(modview_info_t *vi);
static void draw(void);
static int get_part(const char line[], int offset, size_t max_len, char part[]);
static void display_error(const char error_msg[]);
static void cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wH(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wJ(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wK(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wL(key_info_t key_info, keys_info_t *keys_info);
static view_t * get_active_view(void);
static void cmd_ctrl_wb(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wh(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wj(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wk(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wl(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wo(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_ws(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wt(key_info_t key_info, keys_info_t *keys_info);
static int is_right_or_bottom(void);
static int is_top_or_left(void);
static void cmd_ctrl_wv(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_ww(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wx(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_wz(key_info_t key_info, keys_info_t *keys_info);
static void cmd_meta_space(key_info_t key_info, keys_info_t *keys_info);
static void cmd_percent(key_info_t key_info, keys_info_t *keys_info);
static void cmd_tab(key_info_t key_info, keys_info_t *keys_info);
static void cmd_slash(key_info_t key_info, keys_info_t *keys_info);
static void pick_vi(int explore);
static void cmd_qmark(key_info_t key_info, keys_info_t *keys_info);
static void cmd_A(key_info_t key_info, keys_info_t *keys_info);
static void cmd_F(key_info_t key_info, keys_info_t *keys_info);
static void cmd_G(key_info_t key_info, keys_info_t *keys_info);
static void cmd_N(key_info_t key_info, keys_info_t *keys_info);
static void cmd_R(key_info_t key_info, keys_info_t *keys_info);
static int load_view_data(modview_info_t *vi, const char action[],
		const char file_to_view[], int silent);
static const char * get_view_data(modview_info_t *vi,
		const char file_to_view[]);
static void pick_current_viewer(modview_info_t *vi);
static void replace_vi(modview_info_t *orig, modview_info_t *new);
static void cmd_a(key_info_t key_info, keys_info_t *keys_info);
static void switch_viewer(modview_info_t *vi, int offset);
static void cmd_b(key_info_t key_info, keys_info_t *keys_info);
static void cmd_d(key_info_t key_info, keys_info_t *keys_info);
static void cmd_f(key_info_t key_info, keys_info_t *keys_info);
static void check_and_set_from_default_win(key_info_t *key_info);
static void set_from_default_win(key_info_t *key_info);
static void cmd_g(key_info_t key_info, keys_info_t *keys_info);
static void cmd_i(key_info_t key_info, keys_info_t *keys_info);
static void cmd_j(key_info_t key_info, keys_info_t *keys_info);
static void cmd_k(key_info_t key_info, keys_info_t *keys_info);
static void cmd_n(key_info_t key_info, keys_info_t *keys_info);
static void goto_search_result(int repeat_count, int inverse_direction);
static void search(int repeat_count, int backward);
static int find_previous(void);
static int find_next(void);
static void cmd_q(key_info_t key_info, keys_info_t *keys_info);
static void cmd_u(key_info_t key_info, keys_info_t *keys_info);
static void update_with_half_win(key_info_t *key_info);
static void cmd_v(key_info_t key_info, keys_info_t *keys_info);
static void cmd_w(key_info_t key_info, keys_info_t *keys_info);
static void cmd_z(key_info_t key_info, keys_info_t *keys_info);
static void update_with_win(key_info_t *key_info);
static int is_trying_the_same_file(void);
static int get_file_to_explore(const view_t *view, char buf[], size_t buf_len);
static int forward_if_changed(modview_info_t *vi);
static int scroll_to_bottom(modview_info_t *vi);
static void reload_view(modview_info_t *vi, int silent);
static void cleanup(modview_info_t *vi);
static modview_info_t * view_info_alloc(void);
static void handle_mouse_event(key_info_t key_info, keys_info_t *keys_info);
TSTATIC int modview_is_raw(modview_info_t *vi);
TSTATIC int modview_is_detached(modview_info_t *vi);
TSTATIC const char * modview_current_viewer(modview_info_t *vi);
TSTATIC int modview_current_line(modview_info_t *vi);
TSTATIC strlist_t modview_lines(modview_info_t *vi);

/* Points to current (for quick view) or last used (for explore mode)
 * modview_info_t structure. */
static modview_info_t *vi;

static keys_add_info_t builtin_cmds[] = {
	{WK_C_b,           {{&cmd_b},      .descr = "scroll page up"}},
	{WK_C_d,           {{&cmd_d},      .descr = "scroll half-page down"}},
	{WK_C_e,           {{&cmd_j},      .descr = "scroll one line down"}},
	{WK_C_f,           {{&cmd_f},      .descr = "scroll page down"}},
	{WK_C_i,           {{&cmd_tab},    .descr = "quit preview/switch pane"}},
	{WK_C_k,           {{&cmd_k},      .descr = "scroll one line up"}},
	{WK_C_l,           {{&cmd_ctrl_l}, .descr = "redraw"}},
	{WK_CR,            {{&cmd_j},      .descr = "scroll one line down"}},
	{WK_C_n,           {{&cmd_j},      .descr = "scroll one line down"}},
	{WK_C_p,           {{&cmd_k},      .descr = "scroll one line up"}},
	{WK_C_r,           {{&cmd_ctrl_l}, .descr = "redraw"}},
	{WK_C_u,           {{&cmd_u},      .descr = "scroll half-page down"}},
	{WK_C_v,           {{&cmd_f},      .descr = "scroll page down"}},
	{WK_C_w WK_H,      {{&cmd_ctrl_wH}, .descr = "move window to the left"}},
	{WK_C_w WK_J,      {{&cmd_ctrl_wJ}, .descr = "move window to the bottom"}},
	{WK_C_w WK_K,      {{&cmd_ctrl_wK}, .descr = "move window to the top"}},
	{WK_C_w WK_L,      {{&cmd_ctrl_wL}, .descr = "move window to the right"}},
	{WK_C_w WK_b,      {{&cmd_ctrl_wb}, .descr = "go to bottom-right window"}},
	{WK_C_w WK_C_h,    {{&cmd_ctrl_wh}, .descr = "go to left window"}},
	{WK_C_w WK_h,      {{&cmd_ctrl_wh}, .descr = "go to left window"}},
	{WK_C_w WK_C_j,    {{&cmd_ctrl_wj}, .descr = "go to bottom window"}},
	{WK_C_w WK_j,      {{&cmd_ctrl_wj}, .descr = "go to bottom window"}},
	{WK_C_w WK_C_k,    {{&cmd_ctrl_wk}, .descr = "go to top window"}},
	{WK_C_w WK_k,      {{&cmd_ctrl_wk}, .descr = "go to top window"}},
	{WK_C_w WK_C_l,    {{&cmd_ctrl_wl}, .descr = "go to right window"}},
	{WK_C_w WK_l,      {{&cmd_ctrl_wl}, .descr = "go to right window"}},
	{WK_C_w WK_C_o,    {{&cmd_ctrl_wo}, .descr = "single-pane mode"}},
	{WK_C_w WK_o,      {{&cmd_ctrl_wo}, .descr = "single-pane mode"}},
	{WK_C_w WK_C_p,    {{&cmd_ctrl_ww}, .descr = "go to other pane"}},
	{WK_C_w WK_p,      {{&cmd_ctrl_ww}, .descr = "go to other pane"}},
	{WK_C_w WK_C_t,    {{&cmd_ctrl_wt}, .descr = "go to top-left window"}},
	{WK_C_w WK_t,      {{&cmd_ctrl_wt}, .descr = "go to top-left window"}},
	{WK_C_w WK_C_w,    {{&cmd_ctrl_ww}, .descr = "go to other pane"}},
	{WK_C_w WK_w,      {{&cmd_ctrl_ww}, .descr = "go to other pane"}},
	{WK_C_w WK_C_x,    {{&cmd_ctrl_wx}, .descr = "exchange panes"}},
	{WK_C_w WK_x,      {{&cmd_ctrl_wx}, .descr = "exchange panes"}},
	{WK_C_w WK_C_z,    {{&cmd_ctrl_wz}, .descr = "exit preview/view modes"}},
	{WK_C_w WK_z,      {{&cmd_ctrl_wz}, .descr = "exit preview/view modes"}},
	{WK_C_w WK_C_s,    {{&cmd_ctrl_ws}, .descr = "horizontal split layout"}},
	{WK_C_w WK_s,      {{&cmd_ctrl_ws}, .descr = "horizontal split layout"}},
	{WK_C_w WK_C_v,    {{&cmd_ctrl_wv}, .descr = "vertical split layout"}},
	{WK_C_w WK_v,      {{&cmd_ctrl_wv}, .descr = "vertical split layout"}},
	{WK_C_w WK_EQUALS, {{&modnorm_ctrl_wequal},   .nim = 1, .descr = "size panes equally"}},
	{WK_C_w WK_LT,     {{&modnorm_ctrl_wless},    .nim = 1, .descr = "decrease pane size by one"}},
	{WK_C_w WK_GT,     {{&modnorm_ctrl_wgreater}, .nim = 1, .descr = "increase pane size by one"}},
	{WK_C_w WK_PLUS,   {{&modnorm_ctrl_wplus},    .nim = 1, .descr = "increase pane size by one"}},
	{WK_C_w WK_MINUS,  {{&modnorm_ctrl_wminus},   .nim = 1, .descr = "decrease pane size by one"}},
	{WK_C_w WK_BAR,    {{&modnorm_ctrl_wpipe},    .nim = 1, .descr = "maximize pane size"}},
	{WK_C_w WK_USCORE, {{&modnorm_ctrl_wpipe},    .nim = 1, .descr = "maximize pane size"}},
	{WK_C_y,           {{&cmd_k},     .descr = "scroll one line up"}},
	{WK_SLASH,         {{&cmd_slash}, .descr = "search forward"}},
	{WK_QM,            {{&cmd_qmark}, .descr = "search backward"}},
	{WK_LT,            {{&cmd_g}, .descr = "scroll to the beginning"}},
	{WK_GT,            {{&cmd_G}, .descr = "scroll to the end"}},
	{WK_SPACE,         {{&cmd_f}, .descr = "scroll page down"}},
	{WK_ALT WK_LT,     {{&cmd_g}, .descr = "scroll to the beginning"}},
	{WK_ALT WK_GT,     {{&cmd_G}, .descr = "scroll to the end"}},
	{WK_ALT WK_SPACE,  {{&cmd_meta_space}, .descr = "scroll down one window"}},
	{WK_PERCENT,       {{&cmd_percent},    .descr = "to [count]% position"}},
	{WK_A,             {{&cmd_A}, .descr = "switch to previous viewer alternative"}},
	{WK_F,             {{&cmd_F}, .descr = "toggle automatic forward scroll"}},
	{WK_G,             {{&cmd_G}, .descr = "scroll to the end"}},
	{WK_N,             {{&cmd_N}, .descr = "go to previous search match"}},
	{WK_Q,             {{&cmd_q}, .descr = "leave view mode"}},
	{WK_R,             {{&cmd_R}, .descr = "reload view contents"}},
	{WK_Z WK_Q,        {{&cmd_q}, .descr = "leave view mode"}},
	{WK_Z WK_Z,        {{&cmd_q}, .descr = "leave view mode"}},
	{WK_a,             {{&cmd_a}, .descr = "switch to next viewer alternative"}},
	{WK_b,             {{&cmd_b}, .descr = "scroll page up"}},
	{WK_d,             {{&cmd_d}, .descr = "scroll half-page down"}},
	{WK_f,             {{&cmd_f}, .descr = "scroll page down"}},
	{WK_g,             {{&cmd_g}, .descr = "scroll to the beginning"}},
	{WK_e,             {{&cmd_j}, .descr = "scroll one line down"}},
	{WK_i,             {{&cmd_i}, .descr = "toggle ignoring viewers (raw mode)"}},
	{WK_j,             {{&cmd_j}, .descr = "scroll one line down"}},
	{WK_k,             {{&cmd_k}, .descr = "scroll one line up"}},
	{WK_n,             {{&cmd_n}, .descr = "go to next search match"}},
	{WK_p,             {{&cmd_percent}, .descr = "to [count]% position"}},
	{WK_q,             {{&cmd_q},       .descr = "leave view mode"}},
	{WK_r,             {{&cmd_ctrl_l},  .descr = "redraw"}},
	{WK_u,             {{&cmd_u},       .descr = "scroll half-page down"}},
	{WK_v,             {{&cmd_v},       .descr = "edit file at current line"}},
	{WK_y,             {{&cmd_k},       .descr = "scroll one line up"}},
	{WK_w,             {{&cmd_w},       .descr = "scroll backward one window"}},
	{WK_z,             {{&cmd_z},       .descr = "scroll forward one window"}},
	{{K(KEY_MOUSE)},   {{&handle_mouse_event}, FOLLOWED_BY_NONE}},
#ifndef __PDCURSES__
	{WK_ALT WK_v,      {{&cmd_b}, .descr = "scroll page up"}},
#else
	{{K(ALT_V)},       {{&cmd_b}, .descr = "scroll page up"}},
#endif
#ifdef ENABLE_EXTENDED_KEYS
	{{WC_C_w, K(KEY_BACKSPACE)}, {{&cmd_ctrl_wh}, .descr = "go to left window"}},
	{{K(KEY_PPAGE)},             {{&cmd_b}, .descr = "scroll page up"}},
	{{K(KEY_NPAGE)},             {{&cmd_f}, .descr = "scroll page down"}},
	{{K(KEY_DOWN)},              {{&cmd_j}, .descr = "scroll one line down"}},
	{{K(KEY_UP)},                {{&cmd_k}, .descr = "scroll one line up"}},
	{{K(KEY_HOME)},              {{&cmd_g}, .descr = "scroll to the beginning"}},
	{{K(KEY_END)},               {{&cmd_G}, .descr = "scroll to the end"}},
	{{K(KEY_BTAB)},              {{&cmd_q}, .descr = "leave view mode"}},
#else
	{WK_ESC L"[Z",               {{&cmd_q}, .descr = "leave view mode"}},
#endif /* ENABLE_EXTENDED_KEYS */
};

void
modview_init(void)
{
	int ret_code;

	ret_code = vle_keys_add(builtin_cmds, ARRAY_LEN(builtin_cmds), VIEW_MODE);
	assert(ret_code == 0);

	(void)ret_code;
}

void
modview_enter(view_t *view, int explore)
{
	char full_path[PATH_MAX + 1];

	if(get_file_to_explore(curr_view, full_path, sizeof(full_path)) != 0)
	{
		show_error_msg("File exploring", "The file cannot be explored");
		return;
	}

	/* Either make use of detached view or prune it. */
	if(try_resurrect_detached(full_path, explore) == 0)
	{
		ui_views_update_titles();
		return;
	}

	pick_vi(explore);

	vi->filename = is_dir(full_path) ? format_str("%s/", full_path)
	                                 : strdup(full_path);
	vi->view = view;

	if(is_null_or_empty(curr_view->preview_prg))
	{
		vi->viewers = ft_get_viewers(vi->filename);
	}
	else
	{
		strlist_t previewer = {};
		previewer.nitems = add_to_string_array(&previewer.items, previewer.nitems,
				curr_view->preview_prg);
		vi->viewers = previewer;
	}

	if(load_view_data(vi, "File exploring", full_path, NOSILENT) != 0)
	{
		reset_view_info(vi);
		return;
	}

	vle_mode_set(VIEW_MODE, VMT_SECONDARY);

	if(explore)
	{
		vi->view = curr_view;
		curr_view->explore_mode = 1;
	}
	else
	{
		vi->view = other_view;
	}

	ui_views_update_titles();
	modview_redraw();
}

void
modview_detached_make(view_t *view, const char cmd[], MacroFlags flags)
{
	char full_path[PATH_MAX + 1];

	if(get_file_to_explore(curr_view, full_path, sizeof(full_path)) != 0)
	{
		show_error_msg("File exploring", "The file cannot be explored");
		return;
	}

	pick_vi(0);
	reset_view_info(vi);

	vi->view = view;
	vi->ext_viewer = strdup(cmd);
	vi->flags = flags;
	vi->filename = strdup(full_path);
	vi->detached = 1;

	if(load_view_data(vi, "File viewing", full_path, NOSILENT) != 0)
	{
		reset_view_info(vi);
		return;
	}

	ui_views_update_titles();
	modview_redraw();
}

/* Either makes use of detached view or prunes it.  Returns zero on success,
 * otherwise non-zero is returned. */
static int
try_resurrect_detached(const char full_path[], int explore)
{
	const int same_file = vi != NULL
	                   && vi->detached
	                   && vi->view == (explore ? curr_view : other_view)
	                   && vi->filename != NULL
	                   && paths_are_equal(vi->filename, full_path);
	if(!same_file)
	{
		return 1;
	}

	if(explore)
	{
		vi->view->explore_mode = 0;
		reset_view_info(vi);
		return 1;
	}
	else
	{
		vle_mode_set(VIEW_MODE, VMT_SECONDARY);
		return 0;
	}
}

void
modview_try_activate(void)
{
	if(curr_view->explore_mode)
	{
		vle_mode_set(VIEW_MODE, VMT_SECONDARY);
		pick_vi(curr_view->explore_mode);
	}
}

void
modview_pre(void)
{
	if(curr_stats.save_msg == 0)
	{
		const char *raw_suffix = (vi->raw ? " (raw)" : "");
		const char *fwd_suffix = (vi->auto_forward ? " (auto forwarding)" : "");
		ui_sb_msgf("-- VIEW --%s%s", raw_suffix, fwd_suffix);
		curr_stats.save_msg = 2;
	}
}

void
modview_post(void)
{
	update_screen(stats_update_fetch());
	modview_ruler_update();
}

void
modview_ruler_update(void)
{
	char rel_pos[32];
	format_position(rel_pos, sizeof(rel_pos), vi->line, vi->nlines,
			vi->view->window_rows);

	char buf[64];
	int curr_line = vi->line + (vi->nlines > 0 ? 1 : 0);
	snprintf(buf, sizeof(buf), "%d-%d %s", curr_line, vi->nlines, rel_pos);

	ui_ruler_set(buf);
}

void
modview_redraw(void)
{
	modview_info_t *saved_vi = vi;

	try_redraw_explore_view(&lwin);
	try_redraw_explore_view(&rwin);

	if(!lwin.explore_mode && !rwin.explore_mode)
	{
		redraw();
	}

	vi = saved_vi;
}

/* Redraws view in explore mode if view is really in explore mode and is visible
 * on the screen. */
static void
try_redraw_explore_view(const view_t *view)
{
	if(view->explore_mode && ui_view_is_visible(view))
	{
		vi = view->vi;
		redraw();
	}
}

void
modview_leave(void)
{
	if(vi->kind != VK_TEXTUAL && vi->view->explore_mode)
	{
		cleanup(vi);
	}

	reset_view_info(vi);

	vle_mode_set(NORMAL_MODE, VMT_PRIMARY);

	if(curr_view->explore_mode)
	{
		curr_view->explore_mode = 0;
		redraw_current_view();
	}
	else
	{
		qv_draw(curr_view);
	}

	ui_view_title_update(curr_view);

	if(curr_view->explore_mode || other_view->explore_mode)
	{
		modview_redraw();
	}
}

void
modview_quit_exploring(view_t *view)
{
	assert(!vle_mode_is(VIEW_MODE) && "Unexpected mode.");
	if(!view->explore_mode)
	{
		return;
	}

	view->explore_mode = 0;

	reset_view_info(view->vi);

	redraw_view(view);
	ui_view_title_update(view);
}

/* Frees and initializes anew modview_info_t structure instance. */
static void
reset_view_info(modview_info_t *vi)
{
	free_view_info(vi);
	init_view_info(vi);
}

/* Initializes modview_info_t structure instance with safe default values. */
static void
init_view_info(modview_info_t *vi)
{
	memset(vi, '\0', sizeof(*vi));
	vi->win_size = -1;
	vi->half_win = -1;
	vi->width = -1;
	vi->last_search_backward = -1;
	vi->search_repeat = NO_COUNT_GIVEN;
}

/* Frees all resources allocated by modview_info_t structure instance. */
static void
free_view_info(modview_info_t *vi)
{
	free_string_array(vi->viewers.items, vi->viewers.nitems);
	free(vi->widths);
	if(vi->last_search_backward != -1)
	{
		regfree(&vi->re);
	}
	free(vi->filename);
	free(vi->ext_viewer);
}

/* Updates line width and redraws the view. */
static void
redraw(void)
{
	ui_view_title_update(vi->view);
	calc_vlines();
	draw();
}

/* Recalculates virtual lines of a view if display options require it. */
static void
calc_vlines(void)
{
	/* Skip the recalculation if window size and wrapping options are the same. */
	if(ui_qv_width(vi->view) == vi->width && vi->wrap == cfg.wrap_quick_view)
	{
		return;
	}

	vi->width = ui_qv_width(vi->view);
	vi->wrap = cfg.wrap_quick_view;

	if(vi->wrap)
	{
		calc_vlines_wrapped(vi);
	}
	else
	{
		calc_vlines_non_wrapped(vi);
	}
}

/* Recalculates virtual lines of a view with line wrapping. */
static void
calc_vlines_wrapped(modview_info_t *vi)
{
	int i;
	vi->nlinesv = 0;
	for(i = 0; i < vi->nlines; i++)
	{
		vi->widths[i][0] = vi->nlinesv++;
		vi->widths[i][1] = utf8_strsw_with_tabs(vi->lines[i], cfg.tab_stop) -
			esc_str_overhead(vi->lines[i]);
		vi->nlinesv += vi->widths[i][1]/vi->width;
	}
}

/* Recalculates virtual lines of a view without line wrapping. */
static void
calc_vlines_non_wrapped(modview_info_t *vi)
{
	int i;
	vi->nlinesv = vi->nlines;
	for(i = 0; i < vi->nlines; i++)
	{
		vi->widths[i][0] = i;
		vi->widths[i][1] = vi->width;
	}
}

static void
draw(void)
{
	int l, vl;
	const int height = ui_qv_height(vi->view);
	const int width = ui_qv_width(vi->view);
	const int max_l = MIN(vi->line + height, vi->nlines);
	const int searched = (vi->last_search_backward != -1);
	esc_state state;

	if(vi->kind != VK_TEXTUAL)
	{
		cleanup(vi);

		(void)get_view_data(vi, vi->filename);

		if(vi->kind == VK_PASS_THROUGH)
		{
			strlist_t list = { .nitems = vi->nlines, .items = vi->lines };
			ui_pass_through(&list, vi->view->win, ui_qv_left(vi->view),
					ui_qv_top(vi->view));
			return;
		}

		/* Proceed and draw output of the previewer instead of returning, even
		 * though it's graphical.  This way it's possible to use an external
		 * previewer that handles both textual and graphical previews. */
	}

	esc_state_init(&state, &cfg.cs.color[WIN_COLOR], COLORS);

	ui_view_erase(vi->view, 1);
	ui_drop_attr(vi->view->win);

	for(vl = 0, l = vi->line; l < max_l && vl < height; ++l)
	{
		int offset = 0;
		int processed = 0;
		char *const line = vi->lines[l];
		char *p = searched ? esc_highlight_pattern(line, &vi->re) : line;
		do
		{
			int printed;
			const int vis = l != vi->line
			             || vl + processed >= vi->linev - vi->widths[vi->line][0];
			offset += esc_print_line(p + offset, vi->view->win, ui_qv_left(vi->view),
					ui_qv_top(vi->view) + vl, width, !vis, !vi->wrap, &state, &printed);
			vl += vis;
			++processed;
		}
		while(vi->wrap && p[offset] != '\0' && vl < height);
		if(searched)
		{
			free(p);
		}
	}
	refresh_view_win(vi->view);

	checked_wmove(vi->view->win, ui_qv_top(vi->view), ui_qv_left(vi->view));
}

int
modview_find(const char pattern[], int backward)
{
	int err;

	if(pattern == NULL)
		return 0;

	if(vi->last_search_backward != -1)
		regfree(&vi->re);
	vi->last_search_backward = -1;
	if((err = regexp_compile(&vi->re, pattern, get_regexp_cflags(pattern))) != 0)
	{
		ui_sb_errf("Invalid pattern: %s", get_regexp_error(err, &vi->re));
		regfree(&vi->re);
		draw();
		return 1;
	}

	vi->last_search_backward = backward;

	search(vi->search_repeat, backward);

	return curr_stats.save_msg;
}

static void
cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info)
{
	modview_redraw();
}

static void
cmd_ctrl_wH(key_info_t key_info, keys_info_t *keys_info)
{
	move_window(get_active_view(), 0, 1);
}

static void
cmd_ctrl_wJ(key_info_t key_info, keys_info_t *keys_info)
{
	move_window(get_active_view(), 1, 0);
}

static void
cmd_ctrl_wK(key_info_t key_info, keys_info_t *keys_info)
{
	move_window(get_active_view(), 1, 1);
}

static void
cmd_ctrl_wL(key_info_t key_info, keys_info_t *keys_info)
{
	move_window(get_active_view(), 0, 0);
}

/* Gets pointer to the currently active view from the view point of the
 * view-mode.  Returns that pointer. */
static view_t *
get_active_view(void)
{
	return (curr_stats.preview.on ? other_view : curr_view);
}

void
modview_panes_swapped(void)
{
	if(curr_stats.preview.explore != NULL)
	{
		curr_stats.preview.explore->view = curr_view;
	}

	if(lwin.vi != NULL)
	{
		lwin.vi->view = &lwin;
	}
	if(rwin.vi != NULL)
	{
		rwin.vi->view = &rwin;
	}
}

/* Go to bottom-right window. */
static void
cmd_ctrl_wb(key_info_t key_info, keys_info_t *keys_info)
{
	if(is_top_or_left())
	{
		cmd_ctrl_ww(key_info, keys_info);
	}
}

static void
cmd_ctrl_wh(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_stats.split == VSPLIT && is_right_or_bottom())
	{
		cmd_ctrl_ww(key_info, keys_info);
	}
}

static void
cmd_ctrl_wj(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_stats.split == HSPLIT && is_top_or_left())
	{
		cmd_ctrl_ww(key_info, keys_info);
	}
}

static void
cmd_ctrl_wk(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_stats.split == HSPLIT && is_right_or_bottom())
	{
		cmd_ctrl_ww(key_info, keys_info);
	}
}

static void
cmd_ctrl_wl(key_info_t key_info, keys_info_t *keys_info)
{
	if(curr_stats.split == VSPLIT && is_top_or_left())
	{
		cmd_ctrl_ww(key_info, keys_info);
	}
}

/* Leave only one (current) pane. */
static void
cmd_ctrl_wo(key_info_t key_info, keys_info_t *keys_info)
{
	if(vi->view->explore_mode)
	{
		only();
	}
	else
	{
		display_error("Can't leave preview window alone on the screen.");
	}
}

static void
cmd_ctrl_ws(key_info_t key_info, keys_info_t *keys_info)
{
	split_view(HSPLIT);
	modview_redraw();
}

static void
cmd_ctrl_wt(key_info_t key_info, keys_info_t *keys_info)
{
	if(is_right_or_bottom())
	{
		cmd_ctrl_ww(key_info, keys_info);
	}
}

/* Checks whether active window is right of bottom one.  Returns non-zero if it
 * is, otherwise zero is returned. */
static int
is_right_or_bottom(void)
{
	return !is_top_or_left();
}

/* Checks whether active window is top of left one.  Returns non-zero if it is,
 * otherwise zero is returned. */
static int
is_top_or_left(void)
{
	const view_t *const top_or_left = curr_view->explore_mode ? &lwin : &rwin;
	return curr_view == top_or_left;
}

static void
cmd_ctrl_wv(key_info_t key_info, keys_info_t *keys_info)
{
	split_view(VSPLIT);
	modview_redraw();
}

static void
cmd_ctrl_ww(key_info_t key_info, keys_info_t *keys_info)
{
	vi->detached = 1;
	vle_mode_set(NORMAL_MODE, VMT_PRIMARY);
	if(curr_view->explore_mode)
	{
		go_to_other_pane();
	}

	ui_views_update_titles();
	if(curr_stats.preview.on)
	{
		qv_draw(curr_view);
	}
}

/* Switches views. */
static void
cmd_ctrl_wx(key_info_t key_info, keys_info_t *keys_info)
{
	vi->detached = 1;
	vle_mode_set(NORMAL_MODE, VMT_PRIMARY);
	switch_panes();
	if(curr_stats.preview.on)
	{
		change_window();
	}
}

/* Quits preview pane or view modes. */
static void
cmd_ctrl_wz(key_info_t key_info, keys_info_t *keys_info)
{
	modview_leave();
	qv_hide();
}

static void
cmd_meta_space(key_info_t key_info, keys_info_t *keys_info)
{
	check_and_set_from_default_win(&key_info);
	key_info.reg = 1;
	cmd_j(key_info, keys_info);
}

static void
cmd_percent(key_info_t key_info, keys_info_t *keys_info)
{
	if(vi->nlines == 0)
	{
		return;
	}

	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 0;
	if(key_info.count > 100)
		key_info.count = 100;

	vi->line = (key_info.count*vi->nlinesv)/100;
	if(vi->line >= vi->nlines)
		vi->line = vi->nlines - 1;
	vi->linev = vi->widths[vi->line][0];
	draw();
}

static void
cmd_tab(key_info_t key_info, keys_info_t *keys_info)
{
	if(!curr_view->explore_mode)
	{
		modview_leave();
		return;
	}

	change_window();
	if(!curr_view->explore_mode)
	{
		vle_mode_set(NORMAL_MODE, VMT_PRIMARY);
	}
	pick_vi(curr_view->explore_mode);

	ui_views_update_titles();
}

/* Updates value of the vi variable and points it to the correct element of the
 * view_info array according to view mode. */
static void
pick_vi(int explore)
{
	modview_info_t **ptr = explore ? &curr_view->vi
	                               : &curr_stats.preview.explore;

	if(*ptr == NULL)
	{
		*ptr = view_info_alloc();
	}

	vi = *ptr;
}

static void
cmd_slash(key_info_t key_info, keys_info_t *keys_info)
{
	vi->search_repeat = key_info.count;
	modcline_enter(CLS_VWFSEARCH, "");
}

static void
cmd_qmark(key_info_t key_info, keys_info_t *keys_info)
{
	vi->search_repeat = key_info.count;
	modcline_enter(CLS_VWBSEARCH, "");
}

/* Switches to the previous viewer. */
static void
cmd_A(key_info_t key_info, keys_info_t *keys_info)
{
	switch_viewer(vi, -1);
}

/* Toggles automatic forwarding of file. */
static void
cmd_F(key_info_t key_info, keys_info_t *keys_info)
{
	vi->auto_forward = !vi->auto_forward;
	if(vi->auto_forward)
	{
		if(forward_if_changed(vi) || scroll_to_bottom(vi))
		{
			draw();
		}
	}
}

/* Either scrolls to specific line number (when specified) or to the bottom of
 * the view. */
static void
cmd_G(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count != NO_COUNT_GIVEN)
	{
		cmd_g(key_info, keys_info);
		return;
	}

	if(scroll_to_bottom(vi))
	{
		draw();
	}
}

static void
cmd_N(key_info_t key_info, keys_info_t *keys_info)
{
	goto_search_result(key_info.count, 1);
}

/* Handles view data reloading key. */
static void
cmd_R(key_info_t key_info, keys_info_t *keys_info)
{
	reload_view(vi, NOSILENT);
}

/* Loads list of strings and related data into modview_info_t structure from
 * specified file.  The action parameter is a title to be used for error
 * messages.  Returns non-zero on error, otherwise zero is returned. */
static int
load_view_data(modview_info_t *vi, const char action[],
		const char file_to_view[], int silent)
{
	const char *error = get_view_data(vi, file_to_view);

	if(error != NULL && silent)
	{
		return 1;
	}

	if(error != NULL)
	{
		show_error_msg(action, error);
		return 1;
	}

	if(vi->nlines == 0)
	{
		vi->widths = NULL;
	}
	else
	{
		vi->widths = reallocarray(NULL, vi->nlines, sizeof(*vi->widths));
		if(vi->widths == NULL)
		{
			vi->lines = NULL;
			vi->nlines = 0;
			show_error_msg(action, "Not enough memory");
			return 1;
		}
	}

	return 0;
}

/* Reads data to be displayed handling error cases.  Returns error message on
 * failure or NULL on success. */
static const char *
get_view_data(modview_info_t *vi, const char file_to_view[])
{
	pick_current_viewer(vi);

	const ViewerKind kind = ft_viewer_kind(vi->curr_viewer);
	view_t *const curr = curr_view;
	curr_view = (curr_stats.preview.on ? curr_view : vi->view);

	const preview_area_t parea = {
		.source = vi->view,
		.view = vi->view,
		.x = ui_qv_left(vi->view),
		.y = ui_qv_top(vi->view),
		.w = ui_qv_width(vi->view),
		.h = ui_qv_height(vi->view),
	};
	curr_stats.preview_hint = &parea;

	const char *error;
	const char *viewer = (vi->raw ? NULL : vi->curr_viewer);

	strlist_t lines;
	if(vi->curr_viewer == vi->ext_viewer)
	{
		/* No macros in this viewer. */
		lines = vcache_lookup(file_to_view, vi->ext_viewer, vi->flags, kind,
				INT_MAX, VC_SYNC, &error);
	}
	else
	{
		MacroFlags flags = MF_NONE;
		char *expanded = (viewer == NULL)
		               ? NULL
		               : qv_expand_viewer(curr_view, viewer, &flags);
		lines = vcache_lookup(file_to_view, expanded, flags, kind, INT_MAX, VC_SYNC,
				&error);
		free(expanded);
	}

	curr_view = curr;
	curr_stats.preview_hint = NULL;

	vi->lines = lines.items;
	vi->nlines = lines.nitems;

	vi->kind = kind;

	return error;
}

/* Makes sure that vi->curr_viewer field has a sensible value. */
static void
pick_current_viewer(modview_info_t *vi)
{
	if(vi->curr_viewer != NULL)
	{
		return;
	}

	if(vi->ext_viewer != NULL)
	{
		vi->curr_viewer = vi->ext_viewer;
	}
	else if(vi->viewers.nitems != 0)
	{
		vi->curr_viewer = vi->viewers.items[0];
	}
}

/* Replaces modview_info_t structure with another one preserving as much as
 * possible. */
static void
replace_vi(modview_info_t *orig, modview_info_t *new)
{
	new->filename = orig->filename;
	orig->filename = NULL;

	new->viewers = orig->viewers;
	orig->viewers.items = NULL;
	orig->viewers.nitems = 0;

	new->curr_viewer = orig->curr_viewer;

	new->ext_viewer = orig->ext_viewer;
	orig->ext_viewer = NULL;

	if(orig->last_search_backward != -1)
	{
		new->last_search_backward = orig->last_search_backward;
		new->re = orig->re;
		orig->last_search_backward = -1;
	}

	new->win_size = orig->win_size;
	new->half_win = orig->half_win;
	new->line = orig->line;
	new->linev = orig->linev;
	new->view = orig->view;
	new->auto_forward = orig->auto_forward;
	new->file_mon = orig->file_mon;

	free_view_info(orig);
	*orig = *new;
}

/* Switches to the next viewer. */
static void
cmd_a(key_info_t key_info, keys_info_t *keys_info)
{
	switch_viewer(vi, 1);
}

/* Switches to the next or previous viewer. */
static void
switch_viewer(modview_info_t *vi, int offset)
{
	assert((offset == 1 || offset == -1) && "Invalid offset.");

	if(vi->raw || vi->viewers.nitems <= 1)
	{
		return;
	}

	int i;
	for(i = 0; i < vi->viewers.nitems; ++i)
	{
		if(vi->viewers.items[i] == vi->curr_viewer)
		{
			break;
		}
	}
	if(i == vi->viewers.nitems)
	{
		return;
	}

	if(ft_viewer_kind(vi->curr_viewer) != VK_TEXTUAL)
	{
		cleanup(vi);
		/* Wait a bit to let terminal emulator do actual refresh (at least some of
		 * them need this). */
		usleep(cfg.graphics_delay);
	}

	i = (i + vi->viewers.nitems + offset)%vi->viewers.nitems;
	vi->curr_viewer = vi->viewers.items[i];
	reload_view(vi, NOSILENT);
}

static void
cmd_b(key_info_t key_info, keys_info_t *keys_info)
{
	check_and_set_from_default_win(&key_info);
	cmd_k(key_info, keys_info);
}

static void
cmd_d(key_info_t key_info, keys_info_t *keys_info)
{
	update_with_half_win(&key_info);
	cmd_j(key_info, keys_info);
}

static void
cmd_f(key_info_t key_info, keys_info_t *keys_info)
{
	check_and_set_from_default_win(&key_info);
	cmd_j(key_info, keys_info);
}

/* Sets key count from scroll window size when count is not specified. */
static void
check_and_set_from_default_win(key_info_t *key_info)
{
	if(key_info->count == NO_COUNT_GIVEN)
	{
		set_from_default_win(key_info);
	}
}

static void
cmd_g(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	key_info.count = MIN(vi->nlinesv - ui_qv_height(vi->view), key_info.count);
	key_info.count = MAX(1, key_info.count);

	if(vi->nlines == 0 || vi->linev == vi->widths[key_info.count - 1][0])
	{
		return;
	}

	vi->line = key_info.count - 1;
	vi->linev = vi->widths[vi->line][0];
	draw();
}

/* Toggles raw mode (ignoring of defined viewers). */
static void
cmd_i(key_info_t key_info, keys_info_t *keys_info)
{
	if(vi->curr_viewer != vi->ext_viewer)
	{
		vi->raw = !vi->raw;
		reload_view(vi, NOSILENT);
	}
}

static void
cmd_j(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.reg == NO_REG_GIVEN)
	{
		if((vi->linev + 1) + ui_qv_height(vi->view) > vi->nlinesv)
			return;
	}
	else
	{
		if(vi->linev + 1 > vi->nlinesv)
			return;
	}

	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;
	if(key_info.reg == NO_REG_GIVEN)
		key_info.count = MIN(key_info.count,
				vi->nlinesv - ui_qv_height(vi->view) - vi->linev);
	else
		key_info.count = MIN(key_info.count, vi->nlinesv - vi->linev - 1);

	while(key_info.count-- > 0)
	{
		const int height = MAX(DIV_ROUND_UP(vi->widths[vi->line][1], vi->width), 1);
		if(vi->linev + 1 >= vi->widths[vi->line][0] + height)
			++vi->line;

		++vi->linev;
	}

	draw();
}

static void
cmd_k(key_info_t key_info, keys_info_t *keys_info)
{
	if(vi->linev == 0)
		return;

	int repeat_count = MIN(def_count(key_info.count), vi->linev);
	while(repeat_count-- > 0)
	{
		if(vi->linev - 1 < vi->widths[vi->line][0])
			--vi->line;

		--vi->linev;
	}

	draw();
}

static void
cmd_n(key_info_t key_info, keys_info_t *keys_info)
{
	goto_search_result(key_info.count, 0);
}

/* Goes to the next/previous match of the last search. */
static void
goto_search_result(int repeat_count, int inverse_direction)
{
	const int backward = inverse_direction ?
		!vi->last_search_backward : vi->last_search_backward;
	search(repeat_count, backward);
}

/* Performs search and navigation to the first match. */
static void
search(int repeat_count, int backward)
{
	if(vi->last_search_backward == -1)
	{
		return;
	}

	if(repeat_count == NO_COUNT_GIVEN)
	{
		repeat_count = 1;
	}

	while(repeat_count-- > 0)
	{
		if(backward ? find_previous() : find_next())
		{
			break;
		}
	}
}

/* Scrolls to the previous search match.  Returns zero on success and non-zero
 * if pattern wasn't found.  Prints a message on search failure. */
static int
find_previous(void)
{
	if(vi->linev == 0)
	{
		draw();
		display_error("Nothing to search");
		return 1;
	}

	char buf[ui_qv_width(vi->view)*4];

	int vl = vi->linev - 1;
	int l = vi->line;

	if(l > 0 && vl < vi->widths[l][0])
	{
		--l;
	}

	int i;
	int offset = 0;
	for(i = 0; l < vi->nlines && i <= vl - vi->widths[l][0]; ++i)
	{
		offset = get_part(vi->lines[l], offset, ui_qv_width(vi->view), buf);
	}

	/* Don't stop until we go above first virtual line of the first line. */
	while(l >= 0 && vl >= 0)
	{
		if(regexec(&vi->re, buf, 0, NULL, 0) == 0)
		{
			vi->linev = vl;
			vi->line = l;
			break;
		}
		if(l > 0 && vl - 1 < vi->widths[l][0])
		{
			--l;
			offset = 0;
			for(i = 0; i <= vl - 1 - vi->widths[l][0]; i++)
				offset = get_part(vi->lines[l], offset, ui_qv_width(vi->view), buf);
		}
		else
			offset = get_part(vi->lines[l], offset, ui_qv_width(vi->view), buf);
		--vl;
	}

	draw();

	if(vi->linev != vl || vi->nlines == 0)
	{
		display_error("Pattern not found");
		return 1;
	}
	return 0;
}

/* Scrolls to the next search match.  Returns zero on success and non-zero if
 * pattern wasn't found.  Prints a message on search failure. */
static int
find_next(void)
{
	char buf[ui_qv_width(vi->view)*4];

	int vl = vi->linev + 1;
	int l = vi->line;

	if(l < vi->nlines - 1 && vl == vi->widths[l + 1][0])
	{
		++l;
	}

	int i;
	int offset = 0;
	for(i = 0; l < vi->nlines && i <= vl - vi->widths[l][0]; ++i)
	{
		offset = get_part(vi->lines[l], offset, ui_qv_width(vi->view), buf);
	}

	while(l < vi->nlines)
	{
		if(regexec(&vi->re, buf, 0, NULL, 0) == 0)
		{
			vi->linev = vl;
			vi->line = l;
			break;
		}
		if(l == vi->nlines - 1)
		{
			break;
		}

		if(vl + 1 >= vi->widths[l + 1][0])
		{
			++l;
			offset = 0;
		}
		offset = get_part(vi->lines[l], offset, ui_qv_width(vi->view), buf);
		++vl;
	}

	draw();

	if(vi->linev != vl || vi->nlines == 0)
	{
		display_error("Pattern not found");
		return 1;
	}
	return 0;
}

/* Extracts part of the line replacing all occurrences of horizontal tabulation
 * character with appropriate number of spaces.  The offset specifies beginning
 * of the part in the line.  The max_len parameter designates the maximum number
 * of screen characters to put into the part.  Returns number of processed items
 * of the line. */
static int
get_part(const char line[], int offset, size_t max_len, char part[])
{
	char *const no_esc = esc_remove(line);
	const char *const begin = no_esc + offset;
	const char *const end = expand_tabulation(begin, max_len, cfg.tab_stop, part);
	const ptrdiff_t processed_chars = end - no_esc;
	free(no_esc);
	return processed_chars;
}

/* Displays the error message in the status bar. */
static void
display_error(const char error_msg[])
{
	ui_sb_err(error_msg);
	curr_stats.save_msg = 1;
}

static void
cmd_q(key_info_t key_info, keys_info_t *keys_info)
{
	modview_leave();
}

static void
cmd_u(key_info_t key_info, keys_info_t *keys_info)
{
	update_with_half_win(&key_info);
	cmd_k(key_info, keys_info);
}

/* Sets key count from half window size when count is not specified, otherwise
 * specified count is stored as new size of the half window size. */
static void
update_with_half_win(key_info_t *key_info)
{
	if(key_info->count == NO_COUNT_GIVEN)
	{
		int height = ui_qv_height(vi->view);
		key_info->count = (vi->half_win > 0)
			? vi->half_win
			: (height == 1 ? 1 : height/2);
	}
	else
	{
		vi->half_win = key_info->count;
	}
}

/* Invokes an editor to edit the current file being viewed.  The command for
 * editing is taken from the 'vicmd'/'vixcmd' option value and extended with
 * middle line number prepended by a plus sign and name of the current file. */
static void
cmd_v(key_info_t key_info, keys_info_t *keys_info)
{
	char path[PATH_MAX + 1];
	get_current_full_path(curr_view, sizeof(path), path);
	(void)vim_view_file(path, 1 + vi->line + ui_qv_height(vi->view)/2, -1, 1);
	/* In some cases two redraw operations are needed, otherwise TUI is not fully
	 * redrawn. */
	update_screen(UT_REDRAW);
	stats_redraw_later();
}

static void
cmd_w(key_info_t key_info, keys_info_t *keys_info)
{
	update_with_win(&key_info);
	cmd_k(key_info, keys_info);
}

static void
cmd_z(key_info_t key_info, keys_info_t *keys_info)
{
	update_with_win(&key_info);
	cmd_j(key_info, keys_info);
}

/* Sets key count from scroll window size when count is not specified, otherwise
 * specified count is stored as new size of the scroll window. */
static void
update_with_win(key_info_t *key_info)
{
	if(key_info->count == NO_COUNT_GIVEN)
	{
		set_from_default_win(key_info);
	}
	else
	{
		vi->win_size = key_info->count;
	}
}

/* Sets key count from scroll window size. */
static void
set_from_default_win(key_info_t *key_info)
{
	if(vi->win_size > 0)
	{
		key_info->count = vi->win_size;
	}
	else
	{
		int height = ui_qv_height(vi->view);
		key_info->count = (height == 1 ? 1 : height - 1);
	}
}

int
modview_detached_draw(void)
{
	pick_vi(0);

	if(vi == NULL || !vi->detached)
	{
		return 0;
	}

	if(is_trying_the_same_file())
	{
		redraw();
		return 1;
	}

	reset_view_info(vi);
	return 0;
}

static int
is_trying_the_same_file(void)
{
	char full_path[PATH_MAX + 1];

	if(get_file_to_explore(curr_view, full_path, sizeof(full_path)) != 0)
	{
		return 0;
	}

	return paths_are_equal(vi->filename, full_path);
}

/* Gets full path to the file that will be explored (the current file of the
 * view).  Returns non-zero if file cannot be explored. */
static int
get_file_to_explore(const view_t *view, char buf[], size_t buf_len)
{
	const dir_entry_t *const curr = get_current_entry(view);
	if(fentry_is_fake(curr))
	{
		return 1;
	}

	qv_get_path_to_explore(curr, buf, buf_len);

	switch(curr->type)
	{
		case FT_CHAR_DEV:
		case FT_BLOCK_DEV:
		case FT_FIFO:
#ifndef _WIN32
		case FT_SOCK:
#endif
			return 1;
		case FT_LINK:
			if(get_link_target_abs(buf, curr->origin, buf, buf_len) != 0)
			{
				return 1;
			}
			return (os_access(buf, R_OK) != 0);

		default:
			return 0;
	}
}

void
modview_check_for_updates(void)
{
	int need_redraw = 0;

	need_redraw += forward_if_changed(curr_stats.preview.explore);
	need_redraw += forward_if_changed(lwin.vi);
	need_redraw += forward_if_changed(rwin.vi);

	if(need_redraw)
	{
		stats_redraw_later();
	}
}

/* Forwards the view if underlying file changed.  Returns non-zero if reload
 * occurred, otherwise zero is returned. */
static int
forward_if_changed(modview_info_t *vi)
{
	filemon_t mon;

	if(vi == NULL || !vi->auto_forward)
	{
		return 0;
	}

	if(filemon_from_file(vi->filename, FMT_MODIFIED, &mon) != 0)
	{
		return 0;
	}

	if(filemon_equal(&mon, &vi->file_mon))
	{
		return 0;
	}

	vi->file_mon = mon;
	reload_view(vi, SILENT);
	return scroll_to_bottom(vi);
}

/* Scrolls view to the bottom if there is any room for that.  Returns non-zero
 * if position was changed, otherwise zero is returned. */
static int
scroll_to_bottom(modview_info_t *vi)
{
	if(vi->linev + 1 + ui_qv_height(vi->view) > vi->nlinesv)
	{
		return 0;
	}

	vi->linev = vi->nlinesv - ui_qv_height(vi->view);
	for(vi->line = 0; vi->line < vi->nlines - 1; ++vi->line)
	{
		if(vi->linev < vi->widths[vi->line + 1][0])
		{
			break;
		}
	}

	return 1;
}

/* Reloads contents of the specified view by rerunning corresponding viewer or
 * just rereading a file. */
static void
reload_view(modview_info_t *vi, int silent)
{
	modview_info_t new_vi;

	init_view_info(&new_vi);
	/* These fields are used in get_view_data(). */
	new_vi.view = vi->view;
	new_vi.curr_viewer = vi->curr_viewer;
	new_vi.ext_viewer = vi->ext_viewer;
	new_vi.viewers = vi->viewers;
	new_vi.raw = vi->raw;

	if(load_view_data(&new_vi, "File exploring reload", vi->filename, silent)
			== 0)
	{
		replace_vi(vi, &new_vi);
		modview_redraw();
	}
}

void
modview_hide_graphics(void)
{
	if(lwin.vi != NULL && lwin.vi->kind != VK_TEXTUAL)
	{
		cleanup(lwin.vi);
	}
	if(rwin.vi != NULL && rwin.vi->kind != VK_TEXTUAL)
	{
		cleanup(rwin.vi);
	}
}

/* Cleans view area, possibly using an external application. */
static void
cleanup(modview_info_t *vi)
{
	const char *viewer = vi->curr_viewer;
	const char *cmd = (viewer != NULL ? ma_get_clear_cmd(viewer) : NULL);
	qv_cleanup(vi->view, cmd);
}

const char *
modview_detached_get_viewer(void)
{
	return (vi == NULL ? NULL : vi->ext_viewer);
}

/* Processes events from the mouse. */
static void
handle_mouse_event(key_info_t key_info, keys_info_t *keys_info)
{
	MEVENT e;
	if(ui_get_mouse(&e) != OK)
	{
		return;
	}

	view_t *selected_view;
	if(ui_wenclose(&lwin, lwin.win, e.x, e.y) ||
			ui_wenclose(&lwin, lwin.title, e.x, e.y))
	{
		selected_view = &lwin;
	}
	else if(ui_wenclose(&rwin, rwin.win, e.x, e.y) ||
			ui_wenclose(&rwin, rwin.title, e.x, e.y))
	{
		selected_view = &rwin;
	}
	else
	{
		return;
	}

	if(selected_view != vi->view)
	{
		if(!vi->view->explore_mode && (e.bstate & BUTTON1_PRESSED))
		{
			cmd_ctrl_ww(key_info, keys_info);
		}
		return;
	}

	if(e.bstate & BUTTON4_PRESSED)
	{
		cmd_k(key_info, keys_info);
	}
	else if(e.bstate & (BUTTON2_PRESSED | BUTTON5_PRESSED))
	{
		cmd_j(key_info, keys_info);
	}
}

/* Allocates and initializes view mode information.  Returns pointer to it. */
static modview_info_t *
view_info_alloc(void)
{
	modview_info_t *const vi = malloc(sizeof(*vi));
	if(vi != NULL)
	{
		init_view_info(vi);
	}
	return vi;
}

void
modview_info_free(modview_info_t *info)
{
	if(info != NULL)
	{
		free_view_info(info);
		free(info);
		if(info == vi)
		{
			vi = NULL;
		}
	}
}

TSTATIC int
modview_is_raw(modview_info_t *vi)
{
	return vi->raw;
}

TSTATIC int
modview_is_detached(modview_info_t *vi)
{
	return vi->detached;
}

TSTATIC const char *
modview_current_viewer(modview_info_t *vi)
{
	return vi->curr_viewer;
}

TSTATIC int
modview_current_line(modview_info_t *vi)
{
	return vi->line;
}

TSTATIC strlist_t
modview_lines(modview_info_t *vi)
{
	strlist_t lines = { .items = vi->lines, .nitems = vi->nlines };
	return lines;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
