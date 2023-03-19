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

#include "visual.h"

#include <curses.h>

#include <assert.h> /* assert() */
#include <stddef.h> /* NULL size_t */
#include <string.h>

#include "../cfg/config.h"
#include "../compat/curses.h"
#include "../engine/keys.h"
#include "../engine/mode.h"
#include "../menus/filetypes_menu.h"
#include "../menus/menus.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../ui/fileview.h"
#include "../ui/statusbar.h"
#include "../ui/ui.h"
#include "../utils/hist.h"
#include "../utils/macros.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/utils.h"
#include "../cmd_core.h"
#include "../filelist.h"
#include "../filtering.h"
#include "../flist_pos.h"
#include "../flist_sel.h"
#include "../fops_misc.h"
#include "../fops_rename.h"
#include "../marks.h"
#include "../registers.h"
#include "../running.h"
#include "../search.h"
#include "../status.h"
#include "dialogs/attr_dialog.h"
#include "cmdline.h"
#include "modes.h"
#include "normal.h"
#include "wk.h"

/* Types of selection amending. */
typedef enum
{
	AT_NONE,   /* Selection amending is not active, almost same as AT_APPEND. */
	AT_APPEND, /* Amend selection by selecting elements in region. */
	AT_REMOVE, /* Amend selection by deselecting elements in region. */
	AT_INVERT, /* Amend selection by inverting selection of elements in region. */
	AT_COUNT   /* Number of selection amending types. */
}
AmendType;

static void backup_selection_flags(view_t *view);
static void restore_selection_flags(view_t *view);
static void cmd_ctrl_a(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_b(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_d(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_e(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_f(key_info_t key_info, keys_info_t *keys_info);
static void page_scroll(int base, int direction);
static void cmd_ctrl_g(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info);
static void cmd_return(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_u(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_x(key_info_t key_info, keys_info_t *keys_info);
static void call_incdec(int count);
static void cmd_ctrl_y(key_info_t key_info, keys_info_t *keys_info);
static void cmd_quote(key_info_t key_info, keys_info_t *keys_info);
static void sug_quote(vle_keys_list_cb cb);
static void cmd_dollar(key_info_t key_info, keys_info_t *keys_info);
static void cmd_percent(key_info_t key_info, keys_info_t *keys_info);
static void cmd_comma(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zero(key_info_t key_info, keys_info_t *keys_info);
static void cmd_colon(key_info_t key_info, keys_info_t *keys_info);
static void cmd_semicolon(key_info_t key_info, keys_info_t *keys_info);
static void cmd_slash(key_info_t key_info, keys_info_t *keys_info);
static void cmd_question(key_info_t key_info, keys_info_t *keys_info);
static void cmd_C(key_info_t key_info, keys_info_t *keys_info);
static void cmd_D(key_info_t key_info, keys_info_t *keys_info);
static void cmd_F(key_info_t key_info, keys_info_t *keys_info);
static void cmd_G(key_info_t key_info, keys_info_t *keys_info);
static void cmd_H(key_info_t key_info, keys_info_t *keys_info);
static void cmd_L(key_info_t key_info, keys_info_t *keys_info);
static void cmd_M(key_info_t key_info, keys_info_t *keys_info);
static void cmd_N(key_info_t key_info, keys_info_t *keys_info);
static void cmd_O(key_info_t key_info, keys_info_t *keys_info);
static void cmd_d(key_info_t key_info, keys_info_t *keys_info);
static void delete(key_info_t key_info, int use_trash);
static void cmd_av(key_info_t key_info, keys_info_t *keys_info);
static void cmd_cl(key_info_t key_info, keys_info_t *keys_info);
static void cmd_cp(key_info_t key_info, keys_info_t *keys_info);
static void cmd_cw(key_info_t key_info, keys_info_t *keys_info);
static void cmd_f(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gA(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ga(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gg(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gl(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gU(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gu(key_info_t key_info, keys_info_t *keys_info);
static void do_gu(int upper);
static void cmd_gv(key_info_t key_info, keys_info_t *keys_info);
static void restore_previous_selection(void);
static void select_first_one(void);
static void cmd_h(key_info_t key_info, keys_info_t *keys_info);
static void cmd_i(key_info_t key_info, keys_info_t *keys_info);
static void cmd_j(key_info_t key_info, keys_info_t *keys_info);
static void cmd_k(key_info_t key_info, keys_info_t *keys_info);
static void go_to_prev(key_info_t key_info, keys_info_t *keys_info, int def,
		int step);
static void cmd_l(key_info_t key_info, keys_info_t *keys_info);
static void go_to_next(key_info_t key_info, keys_info_t *keys_info, int def,
		int step);
static void cmd_m(key_info_t key_info, keys_info_t *keys_info);
static void cmd_q_colon(key_info_t key_info, keys_info_t *keys_info);
static void cmd_q_slash(key_info_t key_info, keys_info_t *keys_info);
static void cmd_q_question(key_info_t key_info, keys_info_t *keys_info);
static void activate_search(int count, int back, int external);
static void cmd_n(key_info_t key_info, keys_info_t *keys_info);
static void search(key_info_t key_info, int backward);
static void cmd_v(key_info_t key_info, keys_info_t *keys_info);
static void change_amend_type(AmendType new_amend_type);
static void cmd_y(key_info_t key_info, keys_info_t *keys_info);
static void accept_and_leave(int save_msg);
static void reject_and_leave(void);
static void leave_clearing_selection(int go_to_top, int save_msg);
static void update_marks(view_t *view);
static void cmd_zd(key_info_t key_info, keys_info_t *keys_info);
static void cmd_zf(key_info_t key_info, keys_info_t *keys_info);
static void cmd_left_paren(key_info_t key_info, keys_info_t *keys_info);
static void cmd_right_paren(key_info_t key_info, keys_info_t *keys_info);
static void cmd_z_k(key_info_t key_info, keys_info_t *keys_info);
static void cmd_z_j(key_info_t key_info, keys_info_t *keys_info);
static void cmd_lb_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_rb_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_lb_d(key_info_t key_info, keys_info_t *keys_info);
static void cmd_rb_d(key_info_t key_info, keys_info_t *keys_info);
static void cmd_lb_s(key_info_t key_info, keys_info_t *keys_info);
static void cmd_rb_s(key_info_t key_info, keys_info_t *keys_info);
static void cmd_lb_z(key_info_t key_info, keys_info_t *keys_info);
static void cmd_rb_z(key_info_t key_info, keys_info_t *keys_info);
static void cmd_left_curly_bracket(key_info_t key_info, keys_info_t *keys_info);
static void cmd_right_curly_bracket(key_info_t key_info,
		keys_info_t *keys_info);
static void find_goto(int ch, int count, int backward);
static void select_up_one(view_t *view, int start_pos);
static void select_down_one(view_t *view, int start_pos);
static void apply_selection(int pos);
static void revert_selection(int pos);
static void goto_pos_force_update(int pos);
static void goto_pos(int pos);
static void update_ui(void);
static int move_pos(int pos);
static void handle_mouse_event(key_info_t key_info, keys_info_t *keys_info);

static view_t *view;
static int start_pos;
static int last_fast_search_char;
static int last_fast_search_backward = -1;
static int upwards_range;
static int search_repeat;

/* Currently active amending submode. */
static AmendType amend_type;

static keys_add_info_t builtin_cmds[] = {
	{WK_C_a,        {{&cmd_ctrl_a}, .descr = "increment number in names"}},
	{WK_C_b,        {{&cmd_ctrl_b}, .descr = "scroll page up"}},
	{WK_C_c,        {{&cmd_ctrl_c}, .descr = "abort visual mode"}},
	{WK_C_d,        {{&cmd_ctrl_d}, .descr = "scroll half-page down"}},
	{WK_C_e,        {{&cmd_ctrl_e}, .descr = "scroll one line down"}},
	{WK_C_f,        {{&cmd_ctrl_f}, .descr = "scroll page down"}},
	{WK_C_g,        {{&cmd_ctrl_g}, .descr = "display file info"}},
	{WK_C_l,        {{&cmd_ctrl_l}, .descr = "redraw"}},
	{WK_CR,         {{&cmd_return}, .descr = "leave preserving selection"}},
	{WK_C_n,        {{&cmd_j},      .descr = "go to item below"}},
	{WK_C_p,        {{&cmd_k},      .descr = "go to item above"}},
	{WK_C_u,        {{&cmd_ctrl_u}, .descr = "scroll half-page up"}},
	{WK_C_x,        {{&cmd_ctrl_x}, .descr = "decrement number in names"}},
	{WK_C_y,        {{&cmd_ctrl_y}, .descr = "scroll one line up"}},
	{WK_ESC,        {{&cmd_ctrl_c}, .descr = "abort visual mode"}},
	{WK_QUOTE,      {{&cmd_quote}, FOLLOWED_BY_MULTIKEY, .descr = "navigate to mark", .suggest = &sug_quote}},
	{WK_CARET,      {{&cmd_zero},      .descr = "go to first column"}},
	{WK_DOLLAR,     {{&cmd_dollar},    .descr = "go to last column"}},
	{WK_PERCENT,    {{&cmd_percent},   .descr = "go to [count]% position"}},
	{WK_COMMA,      {{&cmd_comma},     .descr = "repeat char-search backward"}},
	{WK_ZERO,       {{&cmd_zero},      .descr = "go to first column"}},
	{WK_COLON,      {{&cmd_colon},     .descr = "go to cmdline mode"}},
	{WK_SCOLON,     {{&cmd_semicolon}, .descr = "repeat char-search forward"}},
	{WK_SLASH,      {{&cmd_slash},     .descr = "search forward"}},
	{WK_QM,         {{&cmd_question},  .descr = "search backward"}},
	{WK_C,          {{&cmd_C}, .descr = "clone files"}},
	{WK_D,          {{&cmd_D}, .descr = "remove files permanently"}},
	{WK_F,          {{&cmd_F}, FOLLOWED_BY_MULTIKEY, .descr = "char-search backward"}},
	{WK_G,          {{&cmd_G}, .descr = "go to the last item"}},
	{WK_H,          {{&cmd_H}, .descr = "go to top of viewport"}},
	{WK_L,          {{&cmd_L}, .descr = "go to bottom of viewport"}},
	{WK_M,          {{&cmd_M}, .descr = "go to middle of viewport"}},
	{WK_N,          {{&cmd_N}, .descr = "go to previous search match"}},
	{WK_O,          {{&cmd_O}, .descr = "switch active selection bound"}},
	{WK_U,          {{&cmd_gU}, .descr = "convert to uppercase"}},
	{WK_V,          {{&cmd_v},  .descr = "leave/switch to regular visual mode"}},
	{WK_Y,          {{&cmd_y},  .descr = "yank files"}},
	{WK_a WK_v,     {{&cmd_av}, .descr = "leave/switch to amending visual mode"}},
	{WK_d,          {{&cmd_d},  .descr = "remove files"}},
	{WK_f,          {{&cmd_f}, FOLLOWED_BY_MULTIKEY, .descr = "char-search forward"}},
	{WK_c WK_l,     {{&cmd_cl}, .descr = "change symlink target"}},
	{WK_c WK_p,     {{&cmd_cp}, .descr = "change file permissions/attributes"}},
	{WK_c WK_w,     {{&cmd_cw}, .descr = "rename files"}},
	{WK_g WK_A,     {{&cmd_gA}, .descr = "(re)calculate size"}},
	{WK_g WK_a,     {{&cmd_ga}, .descr = "calculate size"}},
	{WK_g WK_g,     {{&cmd_gg}, .descr = "go to the first item"}},
	{WK_g WK_h,     {{&cmd_h},  .descr = "go to item to the left"}},
	{WK_g WK_j,     {{&cmd_j},  .descr = "go to item below"}},
	{WK_g WK_k,     {{&cmd_k},  .descr = "go to item above"}},
	{WK_g WK_l,     {{&cmd_gl}, .descr = "open selection"}},
	{WK_g WK_U,     {{&cmd_gU}, .descr = "convert to uppercase"}},
	{WK_g WK_u,     {{&cmd_gu}, .descr = "convert to lowercase"}},
	{WK_g WK_v,     {{&cmd_gv}, .descr = "restore previous selection"}},
	{WK_h,          {{&cmd_h},  .descr = "go to item to the left"}},
	{WK_i,          {{&cmd_i},  .descr = "open selection, but don't execute it"}},
	{WK_j,          {{&cmd_j},  .descr = "go to item below"}},
	{WK_k,          {{&cmd_k},  .descr = "go to item above"}},
	{WK_l,          {{&cmd_l},  .descr = "open selection/go to item to the right"}},
	{WK_m,          {{&cmd_m}, FOLLOWED_BY_MULTIKEY, .descr = "set a mark", .suggest = &sug_quote}},
	{WK_n,          {{&cmd_n},  .descr = "go to next search match"}},
	{WK_o,          {{&cmd_O},  .descr = "switch active selection bound"}},
	{WK_q WK_COLON, {{&cmd_q_colon},    .descr = "edit cmdline in editor"}},
	{WK_q WK_SLASH, {{&cmd_q_slash},    .descr = "edit forward search pattern in editor"}},
	{WK_q WK_QM,    {{&cmd_q_question}, .descr = "edit backward search pattern in editor"}},
	{WK_u,          {{&cmd_gu}, .descr = "convert to lowercase"}},
	{WK_v,          {{&cmd_v},  .descr = "leave/switch to regular visual mode"}},
	{WK_y,          {{&cmd_y},  .descr = "yank files"}},
	{WK_z WK_b,     {{&modnorm_zb}, .descr = "push cursor to the bottom"}},
	{WK_z WK_d,     {{&cmd_zd}, .descr = "exclude custom view entry"}},
	{WK_z WK_f,     {{&cmd_zf}, .descr = "add selection to filter"}},
	{WK_z WK_t,     {{&modnorm_zt},      .descr = "push cursor to the top"}},
	{WK_z WK_z,     {{&modnorm_zz},      .descr = "center cursor position"}},
	{WK_LP,         {{&cmd_left_paren},  .descr = "go to previous group of files"}},
	{WK_RP,         {{&cmd_right_paren}, .descr = "go to next group of files"}},
	{WK_z WK_k,     {{&cmd_z_k},  .descr = "go to previous sibling dir"}},
	{WK_z WK_j,     {{&cmd_z_j},  .descr = "go to next sibling dir"}},
	{WK_LB WK_c,    {{&cmd_lb_c}, .descr = "go to previous mismatch"}},
	{WK_RB WK_c,    {{&cmd_rb_c}, .descr = "go to next mismatch"}},
	{WK_LB WK_d,    {{&cmd_lb_d}, .descr = "go to previous dir"}},
	{WK_RB WK_d,    {{&cmd_rb_d}, .descr = "go to next dir"}},
	{WK_LB WK_s,    {{&cmd_lb_s}, .descr = "go to previous selected entry"}},
	{WK_RB WK_s,    {{&cmd_rb_s}, .descr = "go to next selected entry"}},
	{WK_LB WK_z,    {{&cmd_lb_z}, .descr = "go to first sibling"}},
	{WK_RB WK_z,    {{&cmd_rb_z}, .descr = "go to last sibling"}},
	{WK_LCB,        {{&cmd_left_curly_bracket},  .descr = "go to previous file/dir group"}},
	{WK_RCB,        {{&cmd_right_curly_bracket}, .descr = "go to next file/dir group"}},
#ifdef ENABLE_EXTENDED_KEYS
	{{K(KEY_PPAGE)},   {{&cmd_ctrl_b}, .descr = "scroll page up"}},
	{{K(KEY_NPAGE)},   {{&cmd_ctrl_f}, .descr = "scroll page down"}},
	{{K(KEY_DOWN)},    {{&cmd_j},      .descr = "go to item below"}},
	{{K(KEY_UP)},      {{&cmd_k},      .descr = "go to item above"}},
	{{K(KEY_RIGHT)},   {{&cmd_l},      .descr = "open selection/go to item to the right"}},
#endif /* ENABLE_EXTENDED_KEYS */
	{{K(KEY_MOUSE)}, {{&handle_mouse_event}, FOLLOWED_BY_NONE}},
};

void
modvis_init(void)
{
	int ret_code;

	ret_code = vle_keys_add(builtin_cmds, ARRAY_LEN(builtin_cmds), VISUAL_MODE);
	assert(ret_code == 0);

	(void)ret_code;
}

void
modvis_enter(VisualSubmodes sub_mode)
{
	const int ub = marks_find_in_view(curr_view, '<');
	const int lb = marks_find_in_view(curr_view, '>');

	if(sub_mode == VS_RESTORE && (ub < 0 || lb < 0))
	{
		return;
	}

	view = curr_view;
	start_pos = view->list_pos;
	vle_mode_set(VISUAL_MODE, VMT_PRIMARY);

	switch(sub_mode)
	{
		case VS_NORMAL:
			amend_type = AT_NONE;
			flist_sel_stash(view);
			backup_selection_flags(view);
			select_first_one();
			break;
		case VS_RESTORE:
			amend_type = AT_NONE;
			flist_sel_stash(view);
			backup_selection_flags(view);
			restore_previous_selection();
			break;
		case VS_AMEND:
			amend_type = AT_APPEND;
			backup_selection_flags(view);
			select_first_one();
			break;
	}

	redraw_view(view);
}

void
modvis_leave(int save_msg, int goto_top, int clear_selection)
{
	if(goto_top)
	{
		int ub = marks_find_in_view(view, '<');
		if(ub != -1)
			view->list_pos = ub;
	}

	if(clear_selection)
	{
		reset_search_results(view);
		restore_selection_flags(view);
		redraw_view(view);
	}

	curr_stats.save_msg = save_msg;
	if(vle_mode_is(VISUAL_MODE))
	{
		vle_mode_set(NORMAL_MODE, VMT_PRIMARY);
	}
}

/* Stores current values of selected flags of all items in the directory for
 * future use. */
static void
backup_selection_flags(view_t *view)
{
	int i;
	for(i = 0; i < view->list_rows; ++i)
	{
		view->dir_entry[i].was_selected = view->dir_entry[i].selected;
	}
}

/* Restore previous state of selected flags stored by
 * backup_selection_flags(). */
static void
restore_selection_flags(view_t *view)
{
	int i;

	view->selected_files = 0;
	for(i = 0; i < view->list_rows; ++i)
	{
		dir_entry_t *const entry = &view->dir_entry[i];
		entry->selected = entry->was_selected;
		if(entry->selected)
		{
			++view->selected_files;
		}
	}
}

/* Increments first number in names of marked files of the view [count=1]
 * times. */
static void
cmd_ctrl_a(key_info_t key_info, keys_info_t *keys_info)
{
	call_incdec(def_count(key_info.count));
}

static void
cmd_ctrl_b(key_info_t key_info, keys_info_t *keys_info)
{
	if(fpos_can_scroll_back(view))
	{
		page_scroll(fpos_get_last_visible_cell(view), -1);
	}
}

static void
cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info)
{
	reject_and_leave();
}

static void
cmd_ctrl_d(key_info_t key_info, keys_info_t *keys_info)
{
	if(fpos_can_move_down(view))
	{
		goto_pos(fpos_half_scroll(view, 1));
	}
}

static void
cmd_ctrl_e(key_info_t key_info, keys_info_t *keys_info)
{
	if(fpos_can_scroll_fwd(view))
	{
		int new_pos = fpos_adjust_for_scroll_back(view, view->run_size);
		fview_scroll_fwd_by(view, view->run_size);
		goto_pos_force_update(new_pos);
	}
}

static void
cmd_ctrl_f(key_info_t key_info, keys_info_t *keys_info)
{
	if(fpos_can_scroll_fwd(view))
	{
		page_scroll(view->top_line, 1);
	}
}

/* Scrolls pane by one view in both directions. The direction should be 1 or
 * -1. */
static void
page_scroll(int base, int direction)
{
	/* TODO: deduplicate with fpos_scroll_page(). */
	enum { HOR_GAP_SIZE = 2, VER_GAP_SIZE = 1 };
	int offset = fview_is_transposed(view)
	           ? MAX(1, (view->column_count - VER_GAP_SIZE))*view->window_rows
	           : (view->window_rows - HOR_GAP_SIZE)*view->column_count;
	int new_pos = base + direction*offset
	            + view->list_pos%view->run_size - base%view->run_size;
	new_pos = MAX(0, MIN(view->list_rows - 1, new_pos));
	fview_scroll_by(view, direction*offset);
	goto_pos(new_pos);
}

/* Switches amend submodes by round robin scheme. */
static void
cmd_ctrl_g(key_info_t key_info, keys_info_t *keys_info)
{
	if(amend_type != AT_NONE)
	{
		change_amend_type(1 + amend_type%(AT_COUNT - 1));
	}
}

static void
cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info)
{
	update_screen(UT_FULL);
	ui_set_cursor(/*visibility=*/0);
}

static void
cmd_return(key_info_t key_info, keys_info_t *keys_info)
{
	update_marks(view);
	if(vle_mode_is(VISUAL_MODE))
	{
		vle_mode_set(NORMAL_MODE, VMT_PRIMARY);
	}
}

static void
cmd_ctrl_u(key_info_t key_info, keys_info_t *keys_info)
{
	if(fpos_can_move_up(view))
	{
		goto_pos(fpos_half_scroll(view, 0));
	}
}

/* Decrements first number in names of marked files of the view [count=1]
 * times. */
static void
cmd_ctrl_x(key_info_t key_info, keys_info_t *keys_info)
{
	call_incdec(-def_count(key_info.count));
}

/* Increments/decrements first number in names of marked files of the view
 * [count=1] times. */
static void
call_incdec(int count)
{
	flist_set_marking(view, 0);
	int save_msg = fops_incdec(view, count);
	accept_and_leave(save_msg);
}

static void
cmd_ctrl_y(key_info_t key_info, keys_info_t *keys_info)
{
	if(fpos_can_scroll_back(view))
	{
		int new_pos = fpos_adjust_for_scroll_fwd(view, view->run_size);
		fview_scroll_back_by(view, view->run_size);
		goto_pos_force_update(new_pos);
	}
}

/* Clones selection.  Count specifies number of copies of each file or directory
 * to create (one by default). */
static void
cmd_C(key_info_t key_info, keys_info_t *keys_info)
{
	flist_set_marking(view, 0);
	int save_msg = fops_clone(view, NULL, 0, 0, def_count(key_info.count));
	accept_and_leave(save_msg);
}

static void
cmd_D(key_info_t key_info, keys_info_t *keys_info)
{
	delete(key_info, 0);
}

/* Navigates to previous word which starts with specified character. */
static void
cmd_F(key_info_t key_info, keys_info_t *keys_info)
{
	last_fast_search_char = key_info.multi;
	last_fast_search_backward = 1;

	find_goto(key_info.multi, def_count(key_info.count), 1);
}

static void
cmd_G(key_info_t key_info, keys_info_t *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
	{
		key_info.count = view->list_rows;
	}
	goto_pos(key_info.count - 1);
}

/* Move to the first line of window, selecting as we go. */
static void
cmd_H(key_info_t key_info, keys_info_t *keys_info)
{
	goto_pos(fpos_get_top_pos(view));
}

/* Move to the last line of window, selecting as we go. */
static void
cmd_L(key_info_t key_info, keys_info_t *keys_info)
{
	goto_pos(fpos_get_bottom_pos(view));
}

/* Move to middle line of the window, selecting from start position to there. */
static void
cmd_M(key_info_t key_info, keys_info_t *keys_info)
{
	goto_pos(fpos_get_middle_pos(view));
}

static void
cmd_N(key_info_t key_info, keys_info_t *keys_info)
{
	search(key_info, !curr_stats.last_search_backward);
}

static void
cmd_O(key_info_t key_info, keys_info_t *keys_info)
{
	int t = start_pos;
	start_pos = view->list_pos;
	view->list_pos = t;
	update_ui();
}

static void
cmd_quote(key_info_t key_info, keys_info_t *keys_info)
{
	const int pos = marks_find_in_view(view, key_info.multi);
	if(pos >= 0)
	{
		goto_pos(pos);
	}
}

/* Suggests only marks that point into current view. */
static void
sug_quote(vle_keys_list_cb cb)
{
	if(cfg.sug.flags & SF_MARKS)
	{
		marks_suggest(view, cb, 1);
	}
}

/* Move cursor to the last column in ls-view sub-mode selecting or unselecting
 * files while moving. */
static void
cmd_dollar(key_info_t key_info, keys_info_t *keys_info)
{
	if(!fpos_at_last_col(view))
	{
		goto_pos(fpos_line_end(view));
	}
}

static void
cmd_percent(key_info_t key_info, keys_info_t *keys_info)
{
	int line;
	if(key_info.count == NO_COUNT_GIVEN)
		return;
	if(key_info.count > 100)
		return;
	line = (key_info.count * view->list_rows)/100;
	goto_pos(line - 1);
}

/* Continues navigation to word which starts with specified character in
 * opposite direction. */
static void
cmd_comma(key_info_t key_info, keys_info_t *keys_info)
{
	if(last_fast_search_backward != -1)
	{
		find_goto(last_fast_search_char, def_count(key_info.count),
				!last_fast_search_backward);
	}
}

/* Move cursor to the first column in ls-view sub-mode selecting or unselecting
 * files while moving. */
static void
cmd_zero(key_info_t key_info, keys_info_t *keys_info)
{
	if(!fpos_at_first_col(view))
	{
		goto_pos(fpos_line_start(view));
	}
}

/* Switch to command-line mode. */
static void
cmd_colon(key_info_t key_info, keys_info_t *keys_info)
{
	update_marks(view);
	cmds_vars_set_count(key_info.count);
	modcline_enter(CLS_COMMAND, "");
}

/* Continues navigation to word which starts with specified character in initial
 * direction. */
static void
cmd_semicolon(key_info_t key_info, keys_info_t *keys_info)
{
	if(last_fast_search_backward != -1)
	{
		find_goto(last_fast_search_char, def_count(key_info.count),
				last_fast_search_backward);
	}
}

/* Search forward. */
static void
cmd_slash(key_info_t key_info, keys_info_t *keys_info)
{
	activate_search(key_info.count, 0, 0);
}

/* Search backward. */
static void
cmd_question(key_info_t key_info, keys_info_t *keys_info)
{
	activate_search(key_info.count, 1, 0);
}

/* Leaves visual mode if in amending mode, otherwise switches to amending
 * selection mode. */
static void
cmd_av(key_info_t key_info, keys_info_t *keys_info)
{
	if(amend_type != AT_NONE)
	{
		reject_and_leave();
	}
	else
	{
		change_amend_type(AT_APPEND);
	}
}

static void
cmd_d(key_info_t key_info, keys_info_t *keys_info)
{
	delete(key_info, 1);
}

/* Deletes files. */
static void
delete(key_info_t key_info, int use_trash)
{
	flist_set_marking(view, 0);
	if(fops_delete(view, def_reg(key_info.reg), use_trash))
	{
		accept_and_leave(1);
	}
}

/* Navigates to next word which starts with specified character. */
static void
cmd_f(key_info_t key_info, keys_info_t *keys_info)
{
	last_fast_search_char = key_info.multi;
	last_fast_search_backward = 0;

	find_goto(key_info.multi, def_count(key_info.count), 0);
}

/* Calculate size of selected directories ignoring cached sizes. */
static void
cmd_gA(key_info_t key_info, keys_info_t *keys_info)
{
	fops_size_bg(view, 1);
	accept_and_leave(0);
}

/* Calculate size of selected directories taking cached sizes into account. */
static void
cmd_ga(key_info_t key_info, keys_info_t *keys_info)
{
	fops_size_bg(view, 0);
	accept_and_leave(0);
}

/* Change symbolic link(s). */
static void
cmd_cl(key_info_t key_info, keys_info_t *keys_info)
{
	update_marks(view);
	modvis_leave(0, 1, 0);

	flist_set_marking(view, 0);
	curr_stats.save_msg = fops_retarget(view);
}

/* Change file attributes (permissions or properties). */
static void
cmd_cp(key_info_t key_info, keys_info_t *keys_info)
{
	int ub;

	vle_mode_set(NORMAL_MODE, VMT_PRIMARY);
	update_marks(view);
	ub = marks_find_in_view(view, '<');
	if(ub != -1)
	{
		view->list_pos = ub;
	}

	modnorm_cp(view, key_info);
}

/* Renames selected files of the view. */
static void
cmd_cw(key_info_t key_info, keys_info_t *keys_info)
{
	update_marks(view);
	modvis_leave(0, 1, 0);

	flist_set_marking(view, 0);
	(void)fops_rename(view, NULL, 0, 0);
}

static void
cmd_gg(key_info_t key_info, keys_info_t *keys_info)
{
	goto_pos(def_count(key_info.count) - 1);
}

static void
cmd_gl(key_info_t key_info, keys_info_t *keys_info)
{
	update_marks(view);
	modvis_leave(curr_stats.save_msg, 1, 0);
	rn_open(view, FHE_RUN);
	flist_sel_stash(view);
	redraw_view(view);
}

/* Changes letters in filenames to upper case. */
static void
cmd_gU(key_info_t key_info, keys_info_t *keys_info)
{
	do_gu(1);
}

/* Changes letters in filenames to lower case. */
static void
cmd_gu(key_info_t key_info, keys_info_t *keys_info)
{
	do_gu(0);
}

/* Handles gU and gu commands. */
static void
do_gu(int upper)
{
	flist_set_marking(view, 0);
	int save_msg = fops_case(view, upper);
	accept_and_leave(save_msg);
}

/* Restores selection of previous visual mode usage. */
static void
cmd_gv(key_info_t key_info, keys_info_t *keys_info)
{
	restore_previous_selection();
}

/* Restores selection of previous visual mode usage, if it was in current
 * directory of the view. */
static void
restore_previous_selection(void)
{
	int ub = marks_find_in_view(view, '<');
	int lb = marks_find_in_view(view, '>');

	if(ub < 0 || lb < 0)
		return;

	if(ub > lb)
	{
		int t = ub;
		ub = lb;
		lb = t;

		marks_set_special(view, '<', view->dir_entry[ub].origin,
				view->dir_entry[ub].name);
		marks_set_special(view, '>', view->dir_entry[lb].origin,
				view->dir_entry[lb].name);
	}

	flist_sel_drop(view);

	start_pos = ub;
	view->list_pos = ub;

	select_first_one();

	while(view->list_pos < lb)
		select_down_one(view, start_pos);

	if(upwards_range)
	{
		int t = start_pos;
		start_pos = view->list_pos;
		view->list_pos = t;
	}

	update_ui();
}

/* Performs correct selection of item under the cursor. */
static void
select_first_one(void)
{
	apply_selection(view->list_pos);
}

/* Go backwards [count] (one by default) files in ls-like sub-mode. */
static void
cmd_h(key_info_t key_info, keys_info_t *keys_info)
{
	if(!ui_view_displays_columns(view))
	{
		if(fpos_can_move_left(view))
		{
			go_to_prev(key_info, keys_info, 1, fpos_get_hor_step(view));
		}
		return;
	}

	if(get_current_entry(view)->child_pos != 0)
	{
		const dir_entry_t *entry = get_current_entry(view);
		key_info.count = def_count(key_info.count);
		while(key_info.count-- > 0)
		{
			entry -= entry->child_pos;
		}
		key_info.count = 1;
		go_to_prev(key_info, keys_info, 1,
				view->list_pos - entry_to_pos(view, entry));
	}
}

static void
cmd_i(key_info_t key_info, keys_info_t *keys_info)
{
	rn_open(view, FHE_NO_RUN);
	accept_and_leave(curr_stats.save_msg);
}

static void
cmd_j(key_info_t key_info, keys_info_t *keys_info)
{
	if(fpos_can_move_down(view))
	{
		go_to_next(key_info, keys_info, 1, fpos_get_ver_step(view));
	}
}

static void
cmd_k(key_info_t key_info, keys_info_t *keys_info)
{
	if(fpos_can_move_up(view))
	{
		go_to_prev(key_info, keys_info, 1, fpos_get_ver_step(view));
	}
}

/* Moves cursor to one of previous files in the list. */
static void
go_to_prev(key_info_t key_info, keys_info_t *keys_info, int def, int step)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = def;
	key_info.count *= step;
	goto_pos(view->list_pos - key_info.count);
}

static void
cmd_l(key_info_t key_info, keys_info_t *keys_info)
{
	if(ui_view_displays_columns(view))
	{
		cmd_gl(key_info, keys_info);
	}
	else if(fpos_can_move_right(view))
	{
		go_to_next(key_info, keys_info, 1, fpos_get_hor_step(view));
	}
}

/* Moves cursor to one of next files in the list. */
static void
go_to_next(key_info_t key_info, keys_info_t *keys_info, int def, int step)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = def;
	key_info.count *= step;
	goto_pos(view->list_pos + key_info.count);
}

static void
cmd_m(key_info_t key_info, keys_info_t *keys_info)
{
	const dir_entry_t *const curr = get_current_entry(view);
	if(!fentry_is_fake(curr))
	{
		marks_set_user(view, key_info.multi, curr->origin, curr->name);
	}
}

static void
cmd_n(key_info_t key_info, keys_info_t *keys_info)
{
	search(key_info, curr_stats.last_search_backward);
}

static void
search(key_info_t key_info, int backward)
{
	curr_stats.save_msg = search_next(curr_view, backward, /*stash_selection=*/0,
			/*select_matches=*/0, def_count(key_info.count), &goto_pos);
}

/* Runs external editor to get command-line command and then executes it. */
static void
cmd_q_colon(key_info_t key_info, keys_info_t *keys_info)
{
	leave_clearing_selection(0, 0);
	cmds_run_ext("", 0U, CIT_COMMAND);
}

/* Runs external editor to get search pattern and then executes it. */
static void
cmd_q_slash(key_info_t key_info, keys_info_t *keys_info)
{
	activate_search(key_info.count, 0, 1);
}

/* Runs external editor to get search pattern and then executes it. */
static void
cmd_q_question(key_info_t key_info, keys_info_t *keys_info)
{
	activate_search(key_info.count, 1, 1);
}

/* Activates search of different kinds. */
static void
activate_search(int count, int back, int external)
{
	/* TODO: generalize with normal.c:activate_search(). */

	search_repeat = def_count(count);
	curr_stats.last_search_backward = back;
	if(external)
	{
		CmdInputType type = back ? CIT_VBSEARCH_PATTERN : CIT_VFSEARCH_PATTERN;
		cmds_run_ext("", 0U, type);
	}
	else
	{
		const CmdLineSubmode submode = back ? CLS_VBSEARCH : CLS_VFSEARCH;
		modcline_enter(submode, "");
	}
}

/* Leave visual mode if not in amending mode, otherwise switch to normal visual
 * selection. */
static void
cmd_v(key_info_t key_info, keys_info_t *keys_info)
{
	if(amend_type == AT_NONE)
	{
		reject_and_leave();
	}
	else
	{
		change_amend_type(AT_NONE);
	}
}

/* Changes amend type and updates selected files. */
static void
change_amend_type(AmendType new_amend_type)
{
	amend_type = new_amend_type;
	modvis_update();
}

/* Yanks files. */
static void
cmd_y(key_info_t key_info, keys_info_t *keys_info)
{
	flist_set_marking(view, 0);
	int save_msg = fops_yank(view, def_reg(key_info.reg));
	accept_and_leave(save_msg);
}

/* Accepts selected region and leaves visual mode.  This means forgetting
 * selection that existed before. */
static void
accept_and_leave(int save_msg)
{
	int i;
	for(i = 0; i < view->list_rows; ++i)
	{
		view->dir_entry[i].was_selected = 0;
	}
	leave_clearing_selection(1, save_msg);
}

/* Rejects selected region and leaves visual mode.  Previous selection is not
 * touched. */
static void
reject_and_leave(void)
{
	leave_clearing_selection(0, 0);
}

/* Correctly leaves visual mode updating marks, clearing selection and going to
 * the top of selection. */
static void
leave_clearing_selection(int go_to_top, int save_msg)
{
	update_marks(view);
	modvis_leave(save_msg, go_to_top, 1);
}

static void
update_marks(view_t *view)
{
	char start_mark, end_mark;
	const dir_entry_t *start_entry, *end_entry;
	int delta;

	if(start_pos >= view->list_rows)
	{
		start_pos = view->list_rows - 1;
	}

	upwards_range = view->list_pos < start_pos;
	delta = upwards_range ? +1 : -1;

	start_mark = upwards_range ? '<' : '>';
	end_mark = upwards_range ? '>' : '<';

	start_entry = &view->dir_entry[view->list_pos];
	end_entry = &view->dir_entry[start_pos];
	/* Fake entries can't be marked, so skip them on both ends. */
	while(start_entry != end_entry && fentry_is_fake(start_entry))
	{
		start_entry += delta;
	}
	while(start_entry != end_entry && fentry_is_fake(end_entry))
	{
		end_entry -= delta;
	}

	marks_set_special(view, start_mark, start_entry->origin, start_entry->name);
	marks_set_special(view, end_mark, end_entry->origin, end_entry->name);
}

/* Excludes entries from custom view. */
static void
cmd_zd(key_info_t key_info, keys_info_t *keys_info)
{
	flist_custom_exclude(view, key_info.count == 1);
	accept_and_leave(0);
}

/* Filters selected files. */
static void
cmd_zf(key_info_t key_info, keys_info_t *keys_info)
{
	name_filters_add_active(view);
	accept_and_leave(0);
}

/* Moves cursor to the beginning of the previous group of files defined by the
 * primary sorting key. */
static void
cmd_left_paren(key_info_t key_info, keys_info_t *keys_info)
{
	goto_pos(fpos_find_group(view, 0));
}

/* Moves cursor to the beginning of the next group of files defined by the
 * primary sorting key. */
static void
cmd_right_paren(key_info_t key_info, keys_info_t *keys_info)
{
	goto_pos(fpos_find_group(view, 1));
}

/* Go to previous sibling directory entry or do nothing. */
static void
cmd_z_k(key_info_t key_info, keys_info_t *keys_info)
{
	goto_pos(fpos_prev_dir_sibling(view));
}

/* Go to next sibling directory entry or do nothing. */
static void
cmd_z_j(key_info_t key_info, keys_info_t *keys_info)
{
	goto_pos(fpos_next_dir_sibling(view));
}

/* Go to previous mismatched entry or do nothing. */
static void
cmd_lb_c(key_info_t key_info, keys_info_t *keys_info)
{
	goto_pos(fpos_prev_mismatch(view));
}

/* Go to next mismatched entry or do nothing. */
static void
cmd_rb_c(key_info_t key_info, keys_info_t *keys_info)
{
	goto_pos(fpos_next_mismatch(view));
}

/* Go to previous directory entry or do nothing. */
static void
cmd_lb_d(key_info_t key_info, keys_info_t *keys_info)
{
	goto_pos(fpos_prev_dir(view));
}

/* Go to next directory entry or do nothing. */
static void
cmd_rb_d(key_info_t key_info, keys_info_t *keys_info)
{
	goto_pos(fpos_next_dir(view));
}

/* Go to previous selected entry or do nothing. */
static void
cmd_lb_s(key_info_t key_info, keys_info_t *keys_info)
{
	goto_pos(fpos_prev_selected(view));
}

/* Go to next selected entry or do nothing. */
static void
cmd_rb_s(key_info_t key_info, keys_info_t *keys_info)
{
	goto_pos(fpos_next_selected(view));
}

/* Go to first sibling of the current entry. */
static void
cmd_lb_z(key_info_t key_info, keys_info_t *keys_info)
{
	goto_pos(fpos_first_sibling(view));
}

/* Go to last sibling of the current entry. */
static void
cmd_rb_z(key_info_t key_info, keys_info_t *keys_info)
{
	goto_pos(fpos_last_sibling(view));
}

/* Moves cursor to the beginning of the previous group of files defined by them
 * being directory or files. */
static void
cmd_left_curly_bracket(key_info_t key_info, keys_info_t *keys_info)
{
	goto_pos(fpos_find_dir_group(view, 0));
}

/* Moves cursor to the beginning of the next group of files defined by them
 * being directory or files. */
static void
cmd_right_curly_bracket(key_info_t key_info, keys_info_t *keys_info)
{
	goto_pos(fpos_find_dir_group(view, 1));
}

/* Navigates to next/previous file which starts with given character. */
static void
find_goto(int ch, int count, int backward)
{
	int pos = fpos_find_by_ch(view, ch, backward, 1);
	if(pos < 0 || pos == view->list_pos)
	{
		return;
	}

	while(count-- > 0)
	{
		goto_pos(pos);
		pos = fpos_find_by_ch(view, ch, backward, 1);
	}
}

/* move up one position in the window, adding to the selection list */
static void
select_up_one(view_t *view, int start_pos)
{
	view->list_pos--;
	if(view->list_pos < 0)
	{
		view->list_pos = 0;
	}
	else if(view->list_pos < start_pos)
	{
		apply_selection(view->list_pos);
	}
	else if(view->list_pos == start_pos)
	{
		apply_selection(view->list_pos);
		revert_selection(view->list_pos + 1);
	}
	else
	{
		revert_selection(view->list_pos + 1);
	}
}

/* move down one position in the window, adding to the selection list */
static void
select_down_one(view_t *view, int start_pos)
{
	++view->list_pos;

	if(view->list_pos >= view->list_rows)
	{
		view->list_pos = view->list_rows - 1;
	}
	else if(view->list_pos > start_pos)
	{
		apply_selection(view->list_pos);
	}
	else if(view->list_pos == start_pos)
	{
		apply_selection(view->list_pos);
		revert_selection(view->list_pos - 1);
	}
	else
	{
		revert_selection(view->list_pos - 1);
	}
}

/* Applies selection effect to item at specified position. */
static void
apply_selection(int pos)
{
	dir_entry_t *const entry = &view->dir_entry[pos];
	if(!fentry_is_valid(entry))
	{
		return;
	}

	switch(amend_type)
	{
		case AT_NONE:
		case AT_APPEND:
			if(!entry->selected)
			{
				++view->selected_files;
				entry->selected = 1;
			}
			break;
		case AT_REMOVE:
			if(entry->selected)
			{
				--view->selected_files;
				entry->selected = 0;
			}
			break;
		case AT_INVERT:
			if(entry->selected && entry->was_selected)
			{
				--view->selected_files;
			}
			else if(!entry->selected && !entry->was_selected)
			{
				++view->selected_files;
			}
			entry->selected = !entry->was_selected;
			break;

		default:
			assert(0 && "Unexpected amending type.");
			break;
	}
}

/* Reverts selection effect to item at specified position. */
static void
revert_selection(int pos)
{
	dir_entry_t *const entry = &view->dir_entry[pos];
	switch(amend_type)
	{
		case AT_NONE:
		case AT_APPEND:
			if(entry->selected && !entry->was_selected)
			{
				--view->selected_files;
			}
			entry->selected = entry->was_selected;
			break;
		case AT_REMOVE:
			if(!entry->selected && entry->was_selected)
			{
				++view->selected_files;
				entry->selected = 1;
			}
			break;
		case AT_INVERT:
			if(entry->selected && !entry->was_selected)
			{
				--view->selected_files;
			}
			else if(!entry->selected && entry->was_selected)
			{
				++view->selected_files;
			}
			entry->selected = entry->was_selected;
			break;

		default:
			assert(0 && "Unexpected amending type.");
			break;
	}
}

void
modvis_update(void)
{
	const int cursor_pos = view->list_pos;
	view->list_pos = start_pos;

	/* Regardless of the mode we've switched to, start with the original state
	 * of selection to have consistent behaviour on `av` and `vav`. */
	restore_selection_flags(view);

	if(amend_type == AT_NONE)
	{
		/* When not amending, original selection doesn't affect visual selection. */
		flist_sel_stash(view);

		/* All selection flags are reset, so this call actually clears the
		 * backup. */
		backup_selection_flags(view);
	}

	select_first_one();
	move_pos(cursor_pos);

	update_ui();
}

int
modvis_find(view_t *view, const char pattern[], int backward, int print_msg,
		int *found)
{
	return search_find(view, pattern, backward, /*stash_selection=*/0,
			/*select_matches=*/0, search_repeat, &goto_pos, print_msg, found);
}

/* Moves cursor from its current position to specified pos selecting or
 * unselecting files while moving.  Always redraws view. */
static void
goto_pos_force_update(int pos)
{
	(void)move_pos(pos);
	update_ui();
}

/* Moves cursor from its current position to specified pos selecting or
 * unselecting files while moving.  Automatically redraws view if needed. */
static void
goto_pos(int pos)
{
	if(move_pos(pos))
	{
		update_ui();
	}
}

/* Updates elements of the screen after visual-mode specific elements were
 * changed. */
static void
update_ui(void)
{
	fpos_set_pos(view, view->list_pos);
	redraw_view(view);
	ui_ruler_update(view, 1);
}

/* Moves cursor from its current position to specified pos selecting or
 * unselecting files while moving.  Don't call it explicitly, call goto_pos()
 * and goto_pos_force_update() instead.  Returns non-zero if cursor was
 * moved. */
static int
move_pos(int pos)
{
	pos = MIN(view->list_rows - 1, MAX(0, pos));
	if(view->list_pos == pos)
	{
		return 0;
	}

	while(view->list_pos < pos)
		select_down_one(view, start_pos);
	while(view->list_pos > pos)
		select_up_one(view, start_pos);

	return 1;
}

const char *
modvis_describe(void)
{
	static const char *descriptions[] = {
		[AT_NONE]   = "VISUAL",
		[AT_APPEND] = "VISUAL (append)",
		[AT_REMOVE] = "VISUAL (remove)",
		[AT_INVERT] = "VISUAL (invert)",
	};
	ARRAY_GUARD(descriptions, AT_COUNT);

	return descriptions[amend_type];
}

int
modvis_is_amending(void)
{
	return vle_mode_is(VISUAL_MODE) && amend_type != AT_NONE;
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

	if(!wenclose(view->win, e.y, e.x))
	{
		return;
	}

	if(e.bstate & BUTTON1_PRESSED)
	{
		wmouse_trafo(view->win, &e.y, &e.x, FALSE);

		int list_pos = fview_map_coordinates(curr_view, e.x, e.y);
		if(list_pos >= 0)
		{
			int old_pos = curr_view->list_pos;
			goto_pos(list_pos);

			if(curr_view->list_pos == old_pos)
			{
				cmd_gl(key_info, keys_info);
			}
		}
	}
	else if(e.bstate & BUTTON4_PRESSED)
	{
		cmd_ctrl_y(key_info, keys_info);
	}
	else if(e.bstate & (BUTTON2_PRESSED | BUTTON5_PRESSED))
	{
		cmd_ctrl_e(key_info, keys_info);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
