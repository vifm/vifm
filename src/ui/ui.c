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

/* Save wrefresh identifier behind the macro before it's poisoned in the
 * header. */
#define use_wrefresh wrefresh

#include "ui.h"

#include <curses.h> /* mvwin() werase() */

#ifndef _WIN32
#include <sys/ioctl.h>
#include <termios.h> /* struct winsize */
#endif
#include <unistd.h>

#include <assert.h> /* assert() */
#include <ctype.h> /* isdigit() */
#include <errno.h> /* errno */
#include <stddef.h> /* NULL size_t wchar_t */
#include <stdint.h> /* uint64_t */
#include <stdlib.h> /* abs() free() */
#include <stdio.h> /* snprintf() vsnprintf() */
#include <string.h> /* memset() strcat() strcmp() strcpy() strdup() strlen() */
#include <wchar.h> /* wint_t wcslen() */

#include "../cfg/config.h"
#include "../cfg/info.h"
#include "../compat/curses.h"
#include "../compat/fs_limits.h"
#include "../compat/pthread.h"
#include "../engine/mode.h"
#include "../int/term_title.h"
#include "../lua/vlua.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../modes/modes.h"
#include "../modes/view.h"
#include "../modes/wk.h"
#include "../utils/darray.h"
#include "../utils/fs.h"
#include "../utils/log.h"
#include "../utils/macros.h"
#include "../utils/matchers.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../utils/utf8.h"
#include "../utils/utils.h"
#include "../compare.h"
#include "../event_loop.h"
#include "../filelist.h"
#include "../flist_sel.h"
#include "../macros.h"
#include "../opt_handlers.h"
#include "../status.h"
#include "../vifm.h"
#include "private/statusline.h"
#include "cancellation.h"
#include "color_manager.h"
#include "color_scheme.h"
#include "colored_line.h"
#include "colors.h"
#include "fileview.h"
#include "quickview.h"
#include "statusbar.h"
#include "statusline.h"
#include "tabs.h"

/* List of formatted tab labels with some extra information. */
typedef struct
{
	cline_t *labels; /* List of labels (might miss some tabs). */
	int count;       /* Number of labels. */
	int skipped;     /* Number of leading tabs skipped due to lack of space. */
	int current;     /* Index of the current tab (does not take skipped count into
	                    account). */
}
tab_line_info_t;

/* Information for formatting tab title. */
typedef struct
{
	char *name;               /* Tab name. */
	char *num;                /* Tab number. */
	char *escaped_view_title; /* Auto-formatted title. */
	char *escaped_path;       /* Current path (could be a file path). */
	char *cv_title;           /* Prefix of custom view. */
	int tree;                 /* Whether in tree mode. */
	int current;              /* Whether it's a current tab. */
}
tab_title_info_t;

/* Type of path transformation function for format_view_title(). */
typedef char * (*path_func)(const char[]);

/* Window configured to do not wait for input.  Never appears on the screen,
 * sometimes used for reading input. */
static WINDOW *no_delay_window;
/* Window configured to wait for input indefinitely.  Never appears on the
 * screen, sometimes used for reading input. */
static WINDOW *inf_delay_window;

/* Pieces of top line for the left/top view. */
static WINDOW *ltop_line1; /* On top of left border. */
static WINDOW *ltop_line2; /* Middle of right border in horizontal view. */
/* On top of middle border. */
static WINDOW *mtop_line;
/* Pieces of top line for the right/bottom view. */
static WINDOW *rtop_line1; /* On top of right border. */
static WINDOW *rtop_line2; /* Middle of right border in horizontal view. */

unsigned int ui_next_view_id = 3;

view_t lwin = { .id = 1 };
view_t rwin = { .id = 2 };

view_t *other_view;
view_t *curr_view;

WINDOW *status_bar;
WINDOW *stat_win;
WINDOW *job_bar;
WINDOW *ruler_win;
WINDOW *input_win;
WINDOW *menu_win;
WINDOW *sort_win;
WINDOW *change_win;
WINDOW *error_win;
WINDOW *tab_line;

static WINDOW *lborder;
static WINDOW *mborder;
static WINDOW *rborder;

static int init_pair_wrapper(int pair, int fg, int bg);
static int pair_content_wrapper(int pair, int *fg, int *bg);
static int pair_in_use(int pair);
static void move_pair(int from, int to);
static void create_windows(void);
static void update_geometry(void);
static int update_start(UpdateType update_kind);
static void update_finish(void);
static void adjust_splitter(int screen_w, int screen_h);
static int get_working_area_height(void);
static void clear_border(WINDOW *border);
static int middle_border_is_visible(void);
static void update_views(int reload);
static void reload_lists(void);
static void reload_list(view_t *view);
static void update_view(view_t *view);
static void update_window_lazy(WINDOW *win);
static void update_term_size(void);
static void update_statusbar_layout(void);
static int are_statusbar_widgets_visible(void);
static int get_ruler_width(view_t *view);
static char * expand_ruler_macros(view_t *view, const char format[]);
static void switch_panes_content(void);
static void set_splitter(int pos);
static FileType ui_view_entry_target_type(const dir_entry_t *entry);
static void refresh_bottom_lines(void);
static char * path_identity(const char path[]);
static int view_shows_tabline(const view_t *view);
static int get_tabline_height(void);
static void print_tabline(WINDOW *win, view_t *view, col_attr_t base_col,
		path_func pf);
static tab_line_info_t format_tab_labels(view_t *view, int max_width,
		path_func pf);
static void compute_avg_width(int *avg_width, int *spare_width,
		int min_widths[], int max_width, view_t *view, path_func pf);
TSTATIC cline_t make_tab_title(const tab_title_info_t *title_info);
TSTATIC tab_title_info_t make_tab_title_info(const tab_info_t *tab_info,
		path_func pf, int tab_num, int current_tab);
TSTATIC void dispose_tab_title_info(tab_title_info_t *title_info);
static cline_t format_tab_part(const char fmt[],
		const tab_title_info_t *title_info);
static char * format_view_title(const view_t *view, path_func pf);
static void print_view_title(const view_t *view, int active_view, char title[]);
static col_attr_t fixup_titles_attributes(const view_t *view, int active_view);
static int is_in_miller_view(const view_t *view);
static int is_forced_list_mode(const view_t *view);

/* List of macros that are expanded in the ruler. */
static const char RULER_MACROS[] = "-xlLPS%[]";

void
ui_ruler_update(view_t *view, int lazy_redraw)
{
	char *expanded;

	if(!are_statusbar_widgets_visible())
	{
		/* Do nothing, especially don't update layout because it might be a custom
		 * layout at the moment. */
		return;
	}

	update_statusbar_layout();

	expanded = expand_ruler_macros(view, cfg.ruler_format);
	expanded = break_in_two(expanded, getmaxx(ruler_win), "%=");

	ui_ruler_set(expanded);
	if(!lazy_redraw)
	{
		ui_refresh_win(ruler_win);
	}

	free(expanded);
}

void
ui_ruler_set(const char val[])
{
	const int x = getmaxx(ruler_win) - strlen(val);

	werase(ruler_win);
	mvwaddstr(ruler_win, 0, MAX(x, 0), val);
	wnoutrefresh(ruler_win);
}

int
setup_ncurses_interface(void)
{
	int screen_x, screen_y;

	initscr();
	noecho();
	nonl();
	raw();

	ui_set_cursor(/*visibility=*/0);

	getmaxyx(stdscr, screen_y, screen_x);
	/* Screen is too small to be useful. */
	if(screen_y < MIN_TERM_HEIGHT || screen_x < MIN_TERM_WIDTH)
	{
		vifm_finish("Terminal is too small to run vifm.");
	}

	if(!has_colors())
	{
		vifm_finish("Vifm requires a console that can support color.");
	}

	start_color();
	use_default_colors();

	const colmgr_conf_t colmgr_conf = {
		.max_color_pairs = COLOR_PAIRS,
		.max_colors = COLORS,
		.init_pair = &init_pair_wrapper,
		.pair_content = &pair_content_wrapper,
		.pair_in_use = &pair_in_use,
		.move_pair = &move_pair,
	};
	colmgr_init(&colmgr_conf);

	cs_load_defaults();

	create_windows();

	cfg.tab_stop = TABSIZE;

#ifdef ENABLE_EXTENDED_KEYS
	keypad(status_bar, TRUE);
#endif /* ENABLE_EXTENDED_KEYS */

#if defined(NCURSES_EXT_FUNCS) && NCURSES_EXT_FUNCS >= 20081102
#ifdef HAVE_SET_ESCDELAY_FUNC
	/* Use ncurses specific function to make delay after pressing escape key
	 * unnoticeable.  Used to be zero, but in some corner cases multiple bytes
	 * composing a functional key code might be handled to the application with a
	 * delay. */
	set_escdelay(5);
#endif
#endif

#if defined(NCURSES_VERSION) && defined(ENABLE_EXTENDED_KEYS)
	/* Disable 'unsupported' extended keys so that esc-codes will be
	 * received instead of unnamed extended keycode values (> KEY_MAX).
	 * These keys can then be mapped with esc-codes in vifmrc.
	 * An example could be <c-down>:
	 *     qnoremap <esc>[1;5B j
	 * as without this change <c-down> can return a keycode value of 531
	 * (terminfo name kDN5), which is larger than KEY_MAX and has no
	 * pre-defined curses key name. */
	int i;
	for(i = KEY_MAX; i < 2000; ++i)
	{
		keyok(i, FALSE);
	}
#endif

#ifdef HAVE_EXTENDED_COLORS
	curr_stats.direct_color = (COLORS == 0x1000000);
#endif

	ui_resize_all();

	return 1;
}

/* Calls init_pair() from libcurses. */
static int
init_pair_wrapper(int pair, int fg, int bg)
{
#ifdef HAVE_EXTENDED_COLORS
	return init_extended_pair(pair, fg, bg);
#else
	return init_pair(pair, fg, bg);
#endif
}

/* Calls pair_content() from libcurses. */
static int
pair_content_wrapper(int pair, int *fg, int *bg)
{
#ifdef HAVE_EXTENDED_COLORS
	return extended_pair_content(pair, fg, bg);
#else
	short fg_short, bg_short;

	const int result = pair_content(pair, &fg_short, &bg_short);

	*fg = fg_short;
	*bg = bg_short;

	return result;
#endif
}

/* Checks whether pair is being used at the moment.  Returns non-zero if so and
 * zero otherwise. */
static int
pair_in_use(int pair)
{
	int i;

	for(i = 0; i < MAXNUM_COLOR; ++i)
	{
		if(cfg.cs.pair[i] == pair || lwin.cs.pair[i] == pair ||
				rwin.cs.pair[i] == pair)
		{
			return 1;
		}
	}

	return 0;
}

/* Substitutes old pair number with the new one. */
static void
move_pair(int from, int to)
{
	int i;
	for(i = 0; i < MAXNUM_COLOR; ++i)
	{
		if(cfg.cs.pair[i] == from)
		{
			cfg.cs.pair[i] = to;
		}
		if(lwin.cs.pair[i] == from)
		{
			lwin.cs.pair[i] = to;
		}
		if(rwin.cs.pair[i] == from)
		{
			rwin.cs.pair[i] = to;
		}
	}
}

void
ui_set_mouse_active(int active)
{
	if(vifm_testing())
	{
		return;
	}

	if(active)
	{
		mousemask(ALL_MOUSE_EVENTS, /*oldmask=*/NULL);
		mouseinterval(0);
	}
	else
	{
		mousemask(/*newmask=*/0, /*oldmask=*/NULL);
	}
}

/* Initializes all WINDOW variables by calling newwin() to create ncurses
 * windows and configures hardware cursor. */
static void
create_windows(void)
{
	no_delay_window = newwin(1, 1, 0, 0);
	wtimeout(no_delay_window, 0);

	inf_delay_window = newwin(1, 1, 0, 0);
	wtimeout(inf_delay_window, -1);

	/* These refreshes prevent curses from drawing these windows on first read
	 * from them.  It seems that this way they get updated early and this keeps
	 * them from being drawn again. */
	wnoutrefresh(no_delay_window);
	wnoutrefresh(inf_delay_window);

	menu_win = newwin(1, 1, 0, 0);
	sort_win = newwin(1, 1, 0, 0);
	change_win = newwin(1, 1, 0, 0);
	error_win = newwin(1, 1, 0, 0);

	lborder = newwin(1, 1, 0, 0);

	lwin.title = newwin(1, 1, 0, 0);
	lwin.win = newwin(1, 1, 0, 0);

	mborder = newwin(1, 1, 0, 0);

	ltop_line1 = newwin(1, 1, 0, 0);
	ltop_line2 = newwin(1, 1, 0, 0);

	mtop_line = newwin(1, 1, 0, 0);
	tab_line = newwin(1, 1, 0, 0);

	rtop_line1 = newwin(1, 1, 0, 0);
	rtop_line2 = newwin(1, 1, 0, 0);

	rwin.title = newwin(1, 1, 0, 0);
	rwin.win = newwin(1, 1, 0, 0);

	rborder = newwin(1, 1, 0, 0);

	stat_win = newwin(1, 1, 0, 0);
	job_bar = newwin(1, 1, 0, 0);
	status_bar = newwin(1, 1, 0, 0);
	ruler_win = newwin(1, 1, 0, 0);
	input_win = newwin(1, 1, 0, 0);

	leaveok(menu_win, FALSE);
	leaveok(sort_win, FALSE);
	leaveok(change_win, FALSE);
	leaveok(error_win, FALSE);
	leaveok(lwin.win, FALSE);
	leaveok(rwin.win, FALSE);
	leaveok(status_bar, FALSE);

	leaveok(lborder, TRUE);
	leaveok(lwin.title, TRUE);
	leaveok(mborder, TRUE);
	leaveok(ltop_line1, TRUE);
	leaveok(ltop_line2, TRUE);
	leaveok(mtop_line, TRUE);
	leaveok(tab_line, TRUE);
	leaveok(rtop_line1, TRUE);
	leaveok(rtop_line2, TRUE);
	leaveok(rwin.title, TRUE);
	leaveok(rborder, TRUE);
	leaveok(stat_win, TRUE);
	leaveok(job_bar, TRUE);
	leaveok(ruler_win, TRUE);
	leaveok(input_win, TRUE);
}

void
ui_update_term_state(void)
{
	int screen_x, screen_y;

	update_term_size();
	getmaxyx(stdscr, screen_y, screen_x);
	(void)stats_update_term_state(screen_x, screen_y);
}

int
ui_char_pressed(wint_t c)
{
	if(curr_stats.load_stage < 2)
	{
		return 0;
	}

	ui_cancellation_push_off();
	wint_t pressed;

	while(1)
	{
		pressed = L'\0';
		/* Query single character in non-blocking mode. */
		if(compat_wget_wch(no_delay_window, &pressed) == ERR)
		{
			break;
		}

		if(c != NC_C_c && pressed == NC_C_c)
		{
			ui_cancellation_request();
		}

		if(pressed == c)
		{
			break;
		}
	}

	ui_cancellation_pop();

	return (pressed == c);
}

void
ui_drain_input(void)
{
	wint_t c;
	while(compat_wget_wch(no_delay_window, &c) != ERR)
	{
		/* Discard input. */
	}
}

static void
correct_size(view_t *view)
{
	getmaxyx(view->win, view->window_rows, view->window_cols);
	fview_update_geometry(view);
}

/* Updates TUI elements sizes and coordinates for single window
 * configuration. */
static void
only_layout(view_t *view, int screen_x)
{
	const int pos_correction = cfg.side_borders_visible ? 1 : 0;
	const int size_correction = cfg.side_borders_visible ? -2 : 0;
	const int y = get_tabline_height();

	wresize(view->title, 1, screen_x + size_correction);
	mvwin(view->title, y, pos_correction);

	mvwin(ltop_line1, y, 0);
	mvwin(ltop_line2, y, 0);

	mvwin(rtop_line1, y, screen_x - 1);
	mvwin(rtop_line2, y, screen_x - 1);

	wresize(tab_line, 1, screen_x);
	mvwin(tab_line, 0, 0);

	wresize(view->win, get_working_area_height(), screen_x + size_correction);
	mvwin(view->win, y + 1, pos_correction);
}

/* Updates TUI elements sizes and coordinates for vertical configuration of
 * panes: left one and right one. */
static void
vertical_layout(int screen_x)
{
	const int pos_correction = cfg.side_borders_visible ? 1 : 0;
	const int size_correction = cfg.side_borders_visible ? -1 : 0;
	const int border_height = get_working_area_height();
	const int y = get_tabline_height();

	int splitter_pos;
	if(curr_stats.splitter_pos < 0)
		splitter_pos = screen_x/2 - 1 + screen_x%2;
	else
		splitter_pos = curr_stats.splitter_pos;

	int splitter_width = (cfg.flexible_splitter ? 2 - screen_x%2 : 1);
	if(splitter_pos < 4)
		splitter_pos = 4;
	if(splitter_pos > screen_x - 4 - splitter_width)
		splitter_pos = screen_x - 4 - splitter_width;
	if(curr_stats.splitter_pos >= 0)
		stats_set_splitter_pos(splitter_pos);

	wresize(lwin.title, 1, splitter_pos + size_correction);
	mvwin(lwin.title, y, pos_correction);

	wresize(lwin.win, border_height, splitter_pos + size_correction);
	mvwin(lwin.win, y + 1, pos_correction);

	wresize(mborder, border_height, splitter_width);
	mvwin(mborder, y + 1, splitter_pos);

	mvwin(ltop_line1, y, 0);
	mvwin(ltop_line2, y, 0);

	wresize(tab_line, 1, screen_x);
	mvwin(tab_line, 0, 0);

	wresize(mtop_line, 1, splitter_width);
	mvwin(mtop_line, y, splitter_pos);

	mvwin(rtop_line1, y, screen_x - 1);
	mvwin(rtop_line2, y, screen_x - 1);

	wresize(rwin.title, 1,
			screen_x - (splitter_pos + splitter_width) + size_correction);
	mvwin(rwin.title, y, splitter_pos + splitter_width);

	wresize(rwin.win, border_height,
			screen_x - (splitter_pos + splitter_width) + size_correction);
	mvwin(rwin.win, y + 1, splitter_pos + splitter_width);
}

/* Updates TUI elements sizes and coordinates for horizontal configuration of
 * panes: top one and bottom one. */
static void
horizontal_layout(int screen_x, int screen_y)
{
	const int pos_correction = cfg.side_borders_visible ? 1 : 0;
	const int size_correction = cfg.side_borders_visible ? -2 : 0;
	const int y = get_tabline_height();

	int splitter_pos;
	if(curr_stats.splitter_pos < 0)
		splitter_pos = screen_y/2 - 1;
	else
		splitter_pos = curr_stats.splitter_pos;
	int splitter_height = (middle_border_is_visible() ? 1 : 0);
	if(splitter_pos < 2)
		splitter_pos = 2;
	if(splitter_pos > get_working_area_height() - 1 - splitter_height)
		splitter_pos = get_working_area_height() - 1 - splitter_height;
	if(curr_stats.splitter_pos >= 0)
		stats_set_splitter_pos(splitter_pos);

	wresize(lwin.title, 1, screen_x + size_correction);
	mvwin(lwin.title, y, pos_correction);

	wresize(lwin.win, splitter_pos - (y + 1), screen_x + size_correction);
	mvwin(lwin.win, y + 1, pos_correction);

	wresize(mborder, splitter_height, screen_x);
	mvwin(mborder, splitter_pos, 0);

	wresize(rwin.title, 1, screen_x + size_correction);
	mvwin(rwin.title, splitter_pos + splitter_height, pos_correction);

	wresize(rwin.win,
			get_working_area_height() - splitter_pos - splitter_height + y,
			screen_x + size_correction);
	mvwin(rwin.win, splitter_pos + splitter_height + 1, pos_correction);

	mvwin(ltop_line1, y, 0);
	mvwin(ltop_line2, splitter_pos, 0);

	wresize(tab_line, 1, screen_x);
	mvwin(tab_line, 0, 0);

	mvwin(rtop_line1, y, screen_x - 1);
	mvwin(rtop_line2, splitter_pos, screen_x - 1);

	wresize(lborder, screen_y - 1, 1);
	mvwin(lborder, y, 0);

	wresize(rborder, screen_y - 1, 1);
	mvwin(rborder, y, screen_x - 1);

	/* Unused in this layout. */
	wresize(mtop_line, 1, 1);
	mvwin(mtop_line, 0, 0);
}

/* Calculates height available for main area that contains file lists.  Returns
 * the height. */
static int
get_working_area_height(void)
{
	return getmaxy(stdscr)          /* Total available height. */
	     - 1                        /* Top line. */
	     - ui_stat_height()         /* Status line. */
	     - ui_stat_job_bar_height() /* Job bar. */
	     - 1                        /* Status bar line. */
	     - get_tabline_height();    /* Tab line. */
}

/* Updates internal data structures to reflect actual terminal geometry. */
static void
update_geometry(void)
{
	int screen_x, screen_y;

	update_term_size();

	getmaxyx(stdscr, screen_y, screen_x);
	cfg.lines = screen_y;
	cfg.columns = screen_x;

	LOG_INFO_MSG("New geometry: %dx%d", screen_x, screen_y);

	if(curr_stats.initial_lines == INT_MIN)
	{
		curr_stats.initial_lines = screen_y;
		curr_stats.initial_columns = screen_x;
	}

	load_geometry();
}

void
ui_quit(int write_info, int force)
{
	if(tabs_quit_on_close())
	{
		vifm_try_leave(write_info, 0, force);
	}
	else
	{
		tabs_close();
	}
}

int
cv_unsorted(CVType type)
{
	return type == CV_VERY || cv_compare(type);
}

int
cv_compare(CVType type)
{
	return type == CV_COMPARE || type == CV_DIFF;
}

int
cv_tree(CVType type)
{
	return type == CV_TREE || type == CV_CUSTOM_TREE;
}

const char *
cv_describe(CVType type)
{
	switch(type)
	{
		case CV_REGULAR:
			return "custom";

		case CV_VERY:
			return "very-custom";

		case CV_CUSTOM_TREE:
		case CV_TREE:
			return "tree";

		case CV_DIFF:
		case CV_COMPARE:
			return "compare";
	}

	return "UNKNOWN";
}

void
update_screen(UpdateType update_kind)
{
	if(update_start(update_kind))
	{
		update_all_windows();
		update_finish();
	}
}

void
ui_redraw_as_background(void)
{
	if(update_start(UT_REDRAW))
	{
		update_finish();
	}
}

/* Most of the update logic.  Everything that's done before updating windows.
 * Returns non-zero if update was carried out until the end, otherwise zero is
 * returned. */
static int
update_start(UpdateType update_kind)
{
	if(curr_stats.load_stage < 2 || update_kind == UT_NONE)
	{
		return 0;
	}

	ui_resize_all();

	if(curr_stats.restart_in_progress)
	{
		return 0;
	}

	update_attributes();

	if(curr_stats.term_state != TS_NORMAL)
	{
		return 0;
	}

	qv_ui_updated();

	update_views(update_kind == UT_FULL);
	/* Redraw message dialog over updated panes.  It's not very nice to do it
	 * here, but for sure better then blocking pane updates by checking for
	 * message mode. */
	if(vle_mode_is(MSG_MODE))
	{
		redraw_msg_dialog(0);
	}

	ui_stat_update(curr_view, 0);

	if(curr_stats.save_msg == 0 && !ui_sb_multiline())
	{
		modes_statusbar_update();

		if(vle_mode_is(VIEW_MODE))
		{
			modview_ruler_update();
		}
		else
		{
			ui_ruler_update(curr_view, 1);
		}
	}

	if(curr_stats.save_msg == 0)
	{
		ui_sb_clear();
	}
	else
	{
		ui_sb_msg(NULL);
	}

	if(vle_mode_is(VIEW_MODE) ||
			(curr_stats.number_of_windows == 2 && other_view->explore_mode))
	{
		modview_redraw();
	}

	return 1;
}

/* Post-windows-update part of an update. */
static void
update_finish(void)
{
	if(!curr_view->explore_mode)
	{
		fview_cursor_redraw(curr_view);
	}

	update_input_buf();
	ui_stat_job_bar_redraw();
}

void
ui_resize_all(void)
{
	update_geometry();

	int screen_w, screen_h;
	getmaxyx(stdscr, screen_h, screen_w);
	LOG_INFO_MSG("screen_h = %d; screen_w = %d", screen_h, screen_w);

	if(stats_update_term_state(screen_w, screen_h) != TS_NORMAL)
	{
		return;
	}

	adjust_splitter(screen_w, screen_h);

	wresize(stdscr, screen_h, screen_w);
	wresize(menu_win, screen_h - 1, screen_w);

	int border_h = get_working_area_height();
	int border_y = 1 + get_tabline_height();

	/* TODO: ideally we shouldn't set any colors here (why do we do it?). */
	ui_set_bg(lborder, &cfg.cs.color[BORDER_COLOR], cfg.cs.pair[BORDER_COLOR]);
	wresize(lborder, border_h, 1);
	mvwin(lborder, border_y, 0);

	ui_set_bg(rborder, &cfg.cs.color[BORDER_COLOR], cfg.cs.pair[BORDER_COLOR]);
	wresize(rborder, border_h, 1);
	mvwin(rborder, border_y, screen_w - 1);

	/* These need a resize at least after terminal size was zero or they grow and
	 * produce bad looking effect. */
	wresize(ltop_line1, 1, 1);
	wresize(ltop_line2, 1, 1);
	wresize(rtop_line1, 1, 1);
	wresize(rtop_line2, 1, 1);

	if(curr_stats.number_of_windows == 1)
	{
		only_layout(&lwin, screen_w);
		only_layout(&rwin, screen_w);
	}
	else
	{
		if(curr_stats.split == HSPLIT)
			horizontal_layout(screen_w, screen_h);
		else
			vertical_layout(screen_w);
	}

	correct_size(&lwin);
	correct_size(&rwin);

	wresize(stat_win, ui_stat_height(), screen_w);
	(void)ui_stat_reposition(1, 0);

	wresize(job_bar, 1, screen_w);

	update_statusbar_layout();

	ui_set_cursor(/*visibility=*/0);
}

/* Adjusts splitter position after screen resize. */
static void
adjust_splitter(int screen_w, int screen_h)
{
	static int prev_w = -1, prev_h = -1;

	if(curr_stats.splitter_pos < 0)
	{
		prev_w = -1;
		prev_h = -1;
		return;
	}

	if(prev_w != screen_w || prev_h != screen_h)
	{
		stats_set_splitter_ratio(curr_stats.splitter_ratio);
		prev_w = screen_w;
		prev_h = screen_h;
	}
}

/* Clears border, possibly by filling it with a pattern (depends on
 * configuration). */
static void
clear_border(WINDOW *border)
{
	int i;
	int height, width;

	werase(border);

	getmaxyx(border, height, width);
	if(height > width)
	{
		/* Vertical split */
		if(strcmp(cfg.vborder_filler, " ") == 0 || *cfg.vborder_filler == '\0')
		{
			return;
		}
		for(i = 0; i < height; ++i)
		{
			mvwaddstr(border, i, 0, cfg.vborder_filler);
		}
	}
	else
	{
		/* Horizontal split */
		if(strcmp(cfg.hborder_filler, " ") == 0 || *cfg.hborder_filler == '\0')
		{
			return;
		}
		for(i = 0; i < width; ++i)
		{
			mvwaddstr(border, 0, i, cfg.hborder_filler);
		}
	}

	wnoutrefresh(border);
}

/* Checks whether mborder should be displayed/updated.  Returns non-zero if so,
 * otherwise zero is returned. */
static int
middle_border_is_visible(void)
{
	if(curr_stats.number_of_windows != 2)
	{
		return 0;
	}

	if(curr_stats.split == VSPLIT)
	{
		/* Always visible for vertical split. */
		return 1;
	}

	/* Optionally visible for horizontal split. */
	return (strlen(cfg.hborder_filler) != 0);
}

/* Updates (redraws or reloads) views. */
static void
update_views(int reload)
{
	if(reload)
		reload_lists();
	else
		redraw_lists();
}

/* Reloads file lists for both views. */
static void
reload_lists(void)
{
	reload_list(curr_view);

	if(curr_stats.number_of_windows == 2)
	{
		ui_view_title_update(other_view);
		if(curr_stats.preview.on)
		{
			qv_draw(curr_view);
		}
		else if(!other_view->explore_mode)
		{
			reload_list(other_view);
		}
	}
}

/* Reloads view handling special case of loading it for the first time during
 * startup. */
static void
reload_list(view_t *view)
{
	if(curr_stats.load_stage < 3)
	{
		const int keep_position = cfg_ch_pos_on(CHPOS_STARTUP)
		                        ? 0
		                        : !is_dir_list_loaded(view);
		load_dir_list(view, keep_position);
		return;
	}

	load_saving_pos(view);
}

void
change_window(void)
{
	swap_view_roles();

	load_view_options(curr_view);

	if(window_shows_dirlist(other_view))
	{
		const col_scheme_t *cs = ui_view_get_cs(other_view);
		if(cs_is_color_set(&cs->color[OTHER_WIN_COLOR]))
		{
			draw_dir_list(other_view);
		}
		else
		{
			fview_draw_inactive_cursor(other_view);
		}
	}

	if(curr_stats.preview.on && !is_dir_list_loaded(curr_view))
	{
		/* This view hasn't been loaded since startup yet, do it now. */
		navigate_to(curr_view, curr_view->curr_dir);
	}
	else
	{
		/* Change working directory, so that %c macro and other cwd-sensitive things
		 * work as expected. */
		(void)vifm_chdir(flist_get_dir(curr_view));
	}

	if(window_shows_dirlist(&lwin) && window_shows_dirlist(&rwin))
	{
		fview_cursor_redraw(curr_view);
		ui_views_update_titles();
	}
	else
	{
		stats_redraw_later();
	}
}

void
swap_view_roles(void)
{
	view_t *const tmp = curr_view;
	curr_view = other_view;
	other_view = tmp;
}

void
update_all_windows(void)
{
	if(curr_stats.load_stage >= 2)
	{
		touch_all_windows();
		doupdate();
	}
}

void
touch_all_windows(void)
{
	if(curr_stats.load_stage < 2)
	{
		return;
	}

	if(!modes_is_menu_like())
	{
		update_window_lazy(tab_line);

		if(middle_border_is_visible())
		{
			update_window_lazy(mborder);
			update_window_lazy(mtop_line);
		}

		if(curr_stats.number_of_windows == 1)
		{
			/* In one window view. */
			update_view(curr_view);
		}
		else
		{
			/* Two pane View. */
			update_view(&lwin);
			update_view(&rwin);
		}

		if(cfg.side_borders_visible)
		{
			/* This needs to be updated before things like status bar and status line
			 * to account for cases when they hide the top line. */
			update_window_lazy(ltop_line1);
			update_window_lazy(ltop_line2);
			update_window_lazy(rtop_line1);
			update_window_lazy(rtop_line2);

			update_window_lazy(lborder);
			update_window_lazy(rborder);
		}

		if(cfg.display_statusline)
		{
			update_window_lazy(stat_win);
		}

		if(ui_stat_job_bar_height() != 0)
		{
			update_window_lazy(job_bar);
		}
	}

	update_window_lazy(ruler_win);
	update_window_lazy(input_win);
	update_window_lazy(status_bar);

	if(vle_mode_is(MSG_MODE))
	{
		/* Location of message dialog can change, so it's better to let it position
		 * itself or multiple copies appear on the screen. */
		redraw_msg_dialog(1);
	}
}

/* Updates all parts of file view. */
static void
update_view(view_t *view)
{
	update_window_lazy(view->title);

	/* If view displays graphics, we don't want to update it or the image will be
	 * lost. */
	if(!view->explore_mode && !view->displays_graphics &&
			!(curr_stats.preview.on && view == other_view))
	{
		update_window_lazy(view->win);
	}
}

/* Tell curses to internally mark window as changed. */
static void
update_window_lazy(WINDOW *win)
{
	touchwin(win);
	/* Tell curses that it shouldn't assume that screen isn't messed up in any
	 * way. */
	redrawwin(win);
	wnoutrefresh(win);
}

void
update_input_bar(const wchar_t *str)
{
	if(!curr_stats.use_input_bar)
		return;

	if(wcslen(str) > (size_t)getmaxx(input_win))
	{
		str += wcslen(str) - getmaxx(input_win);
	}

	werase(input_win);
	compat_waddwstr(input_win, str);
	ui_refresh_win(input_win);
}

void
clear_num_window(void)
{
	if(curr_stats.use_input_bar)
	{
		werase(input_win);
		ui_refresh_win(input_win);
	}
}

void
show_progress(const char msg[], int period)
{
	static char marks[] = { '|', '/', '-', '\\' };
	static int mark = 0;
	static int count = 1;
	static int total = 0;

	/* Do nothing if UI is not functional. */
	if(curr_stats.load_stage < 1)
	{
		return;
	}

	/* Reset state. */
	if(period == 0)
	{
		count = 1;
		total = 0;
		return;
	}

	/* Advance state. */
	++count;
	++total;

	/* Skip intermediate updates to do not hammer UI with refreshes. */
	if(abs(period) != 1 && count%abs(period) != 1)
	{
		return;
	}
	count = 1;

	/* Assume that period equal to or less than one means that message already
	 * contains counter (maybe along with total number) or doesn't need one. */
	if(period <= 1)
	{
		ui_sb_quick_msgf("%s %c", msg, marks[mark]);
	}
	else
	{
		ui_sb_quick_msgf("%s %c %d", msg, marks[mark], total);
	}

	/* Pick next mark character. */
	mark = (mark + 1) % sizeof(marks);
}

void
redraw_lists(void)
{
	if(curr_stats.load_stage == 0)
	{
		return;
	}

	redraw_current_view();
	if(curr_stats.number_of_windows == 2)
	{
		if(curr_stats.preview.on)
		{
			qv_draw(curr_view);
			refresh_view_win(other_view);
		}
		else if(!other_view->explore_mode)
		{
			fview_cursor_redraw(other_view);
			draw_dir_list(other_view);
			refresh_view_win(other_view);
		}
	}
}

void
update_attributes(void)
{
	if(curr_stats.load_stage < 2)
		return;

	if(cfg.side_borders_visible)
	{
		ui_set_bg(lborder, &cfg.cs.color[BORDER_COLOR], cfg.cs.pair[BORDER_COLOR]);
		clear_border(lborder);
		ui_set_bg(rborder, &cfg.cs.color[BORDER_COLOR], cfg.cs.pair[BORDER_COLOR]);
		clear_border(rborder);

		ui_set_bg(ltop_line1, &cfg.cs.color[TOP_LINE_COLOR],
				cfg.cs.pair[TOP_LINE_COLOR]);
		werase(ltop_line1);

		ui_set_bg(ltop_line2, &cfg.cs.color[TOP_LINE_COLOR],
				cfg.cs.pair[TOP_LINE_COLOR]);
		werase(ltop_line2);

		ui_set_bg(rtop_line1, &cfg.cs.color[TOP_LINE_COLOR],
				cfg.cs.pair[TOP_LINE_COLOR]);
		werase(rtop_line1);

		ui_set_bg(rtop_line2, &cfg.cs.color[TOP_LINE_COLOR],
				cfg.cs.pair[TOP_LINE_COLOR]);
		werase(rtop_line2);
	}

	if(middle_border_is_visible())
	{
		ui_set_bg(mtop_line, &cfg.cs.color[TOP_LINE_COLOR],
				cfg.cs.pair[TOP_LINE_COLOR]);
		werase(mtop_line);
		/* For some reason this wnoutrefresh is needed to make this window appear
		 * at the same time along with others during startup.  Might be an
		 * indication that all windows need a refresh, but that need is masked. */
		wnoutrefresh(mtop_line);

		ui_set_bg(mborder, &cfg.cs.color[BORDER_COLOR], cfg.cs.pair[BORDER_COLOR]);
		clear_border(mborder);
	}

	ui_set_bg(tab_line, &cfg.cs.color[TAB_LINE_COLOR],
			cfg.cs.pair[TAB_LINE_COLOR]);
	werase(tab_line);

	ui_set_bg(stat_win, &cfg.cs.color[STATUS_LINE_COLOR],
			cfg.cs.pair[STATUS_LINE_COLOR]);

	ui_set_bg(job_bar, &cfg.cs.color[JOB_LINE_COLOR],
			cfg.cs.pair[JOB_LINE_COLOR]);

	ui_set_bg(menu_win, &cfg.cs.color[WIN_COLOR], cfg.cs.pair[WIN_COLOR]);
	ui_set_bg(sort_win, &cfg.cs.color[WIN_COLOR], cfg.cs.pair[WIN_COLOR]);
	ui_set_bg(change_win, &cfg.cs.color[WIN_COLOR], cfg.cs.pair[WIN_COLOR]);
	ui_set_bg(error_win, &cfg.cs.color[WIN_COLOR], cfg.cs.pair[WIN_COLOR]);

	ui_set_bg(status_bar, &cfg.cs.color[CMD_LINE_COLOR],
			cfg.cs.pair[CMD_LINE_COLOR]);
	ui_set_bg(ruler_win, &cfg.cs.color[CMD_LINE_COLOR],
			cfg.cs.pair[CMD_LINE_COLOR]);
	ui_set_bg(input_win, &cfg.cs.color[CMD_LINE_COLOR],
			cfg.cs.pair[CMD_LINE_COLOR]);
}

void
ui_refresh_win(WINDOW *win)
{
	if(!stats_silenced_ui())
	{
		use_wrefresh(win);
	}
}

void
wprint(WINDOW *win, const char str[])
{
#ifndef _WIN32
	waddstr(win, str);
#else
	wchar_t *t = to_wide(str);
	if(t == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	compat_waddwstr(win, t);
	free(t);
#endif
}

void
wprinta(WINDOW *win, const char str[], const cchar_t *line_attrs,
		int attrs_xors)
{
	attr_t attrs;
	short color_pair;
	wchar_t wch[getcchar(line_attrs, NULL, &attrs, &color_pair, NULL)];
	getcchar(line_attrs, wch, &attrs, &color_pair, NULL);

	col_attr_t col = {
		.attr = attrs ^ attrs_xors,
		.gui_attr = attrs ^ attrs_xors
	};
	ui_set_attr(win, &col, color_pair);
	wprint(win, str);
	wnoutrefresh(win);
}

int
resize_for_menu_like(void)
{
	int screen_x, screen_y;

	ui_update_term_state();
	if(curr_stats.term_state == TS_TOO_SMALL)
	{
		return 1;
	}

	update_term_size();
	flushinp(); /* without it we will get strange character on input */
	getmaxyx(stdscr, screen_y, screen_x);

	werase(stdscr);
	werase(status_bar);
	werase(ruler_win);

	wresize(menu_win, screen_y - 1, screen_x);

	update_statusbar_layout();

	ui_refresh_win(status_bar);
	ui_refresh_win(ruler_win);
	ui_refresh_win(input_win);

	return 0;
}

void
ui_setup_for_menu_like(void)
{
	if(curr_stats.load_stage > 0)
	{
		scrollok(menu_win, FALSE);
		ui_set_cursor(/*visibility=*/0);
		werase(menu_win);
		werase(status_bar);
		werase(ruler_win);
		ui_refresh_win(status_bar);
		ui_refresh_win(ruler_win);
	}
}

/* Query terminal size from the "device" and pass it to curses library. */
static void
update_term_size(void)
{
	if(curr_stats.load_stage < 1)
	{
		/* No terminal in tests. */
		return;
	}

#ifndef _WIN32
	struct winsize ws = { .ws_col = -1, .ws_row = -1 };

	if(ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1)
	{
		LOG_SERROR_MSG(errno, "Failed to query terminal size.");
		vifm_finish("Terminal error.");
	}
	/* Allow 0 as GNU Hurd returns this, is_term_resized() returns false then, but
	 * everything works. */
	if(ws.ws_row == (typeof(ws.ws_row))-1 || ws.ws_col == (typeof(ws.ws_col))-1)
	{
		LOG_INFO_MSG("ws.ws_row = %d; ws.ws_col = %d", ws.ws_row, ws.ws_col);
		vifm_finish("Terminal is unable to run vifm.");
	}

	if(is_term_resized(ws.ws_row, ws.ws_col))
	{
		resizeterm(ws.ws_row, ws.ws_col);
	}
#elif defined(__PDCURSES__)
	if(is_termresized())
	{
		/* This resizes vifm to window size. */
		resize_term(0, 0);
	}
#endif
}

/* Re-layouts windows located on status bar (status bar itself, input and the
 * ruler). */
static void
update_statusbar_layout(void)
{
	int screen_x, screen_y;

	int ruler_width;
	int fields_pos;

	getmaxyx(stdscr, screen_y, screen_x);

	ruler_width = get_ruler_width(curr_view);
	fields_pos = screen_x - (INPUT_WIN_WIDTH + ruler_width);

	wresize(ruler_win, 1, ruler_width);
	mvwin(ruler_win, screen_y - 1, fields_pos + INPUT_WIN_WIDTH);

	wresize(input_win, 1, INPUT_WIN_WIDTH);
	mvwin(input_win, screen_y - 1, fields_pos);

	/* We might be in command-line mode in which case we shouldn't change visible
	 * parts of the layout. */
	if(are_statusbar_widgets_visible())
	{
		wresize(status_bar, 1, fields_pos);
		mvwin(status_bar, screen_y - 1, 0);

		wnoutrefresh(ruler_win);
		wnoutrefresh(input_win);
	}
}

/* Checks whether ruler and input bar are visible.  Returns non-zero if so, zero
 * is returned otherwise. */
static int
are_statusbar_widgets_visible(void)
{
	return !ui_sb_multiline() && !ui_sb_locked();
}

/* Gets "recommended" width for the ruler.  Returns the width. */
static int
get_ruler_width(view_t *view)
{
	char *expanded;
	int len;
	int list_pos;

	/* Size must correspond to the "worst case" of the last list item. */
	list_pos = view->list_pos;
	view->list_pos = (view->list_rows == 0) ? 0 : (view->list_rows - 1);

	expanded = expand_ruler_macros(view, cfg.ruler_format);
	len = strlen(expanded);
	free(expanded);

	view->list_pos = list_pos;

	return MAX(POS_WIN_MIN_WIDTH, len);
}

/* Expands view macros to be displayed on the ruler line according to the format
 * string.  Returns newly allocated string, which should be freed by the caller,
 * or NULL if there is not enough memory. */
static char *
expand_ruler_macros(view_t *view, const char format[])
{
	return expand_view_macros(view, format, RULER_MACROS);
}

void
refresh_view_win(view_t *view)
{
	if(curr_stats.restart_in_progress)
	{
		return;
	}

	ui_refresh_win(view->win);
	refresh_bottom_lines();
}

void
move_window(view_t *view, int horizontally, int first)
{
	const SPLIT split_type = horizontally ? HSPLIT : VSPLIT;
	const view_t *const desired_view = first ? &lwin : &rwin;
	split_view(split_type);
	if(view != desired_view)
	{
		/* Switch two panes saving current windows as the active one (left/top or
		 * right/bottom). */
		switch_panes_content();
		go_to_other_pane();
		tabs_switch_panes();
	}
}

void
switch_panes(void)
{
	switch_panes_content();
	modview_try_activate();
}

void
ui_view_pick(view_t *view, view_t **old_curr, view_t **old_other)
{
	*old_curr = curr_view;
	*old_other = other_view;

	curr_view = view;
	other_view = (view == *old_curr) ? *old_other : *old_curr;
	if(curr_view != *old_curr)
	{
		load_view_options(curr_view);
	}
}

void
ui_view_unpick(view_t *view, view_t *old_curr, view_t *old_other)
{
	if(curr_view != view)
	{
		/* Do nothing if view roles were switched from outside. */
		return;
	}

	curr_view = old_curr;
	other_view = old_other;
	if(curr_view != view)
	{
		load_view_options(curr_view);
	}
}

/* Switches panes content. */
static void
switch_panes_content(void)
{
	ui_swap_view_data(&lwin, &rwin);

	flist_update_origins(&lwin);
	flist_update_origins(&rwin);

	modview_panes_swapped();

	stats_redraw_later();
}

void
ui_swap_view_data(view_t *left, view_t *right)
{
	view_t tmp_view;
	WINDOW *tmp;
	int t;

	/* Data in some fields doesn't need to be swapped.  Swap it beforehand so that
	 * swapping structures will put it back. */

	tmp = left->win;
	left->win = right->win;
	right->win = tmp;

	t = left->window_rows;
	left->window_rows = right->window_rows;
	right->window_rows = t;

	t = left->window_cols;
	left->window_cols = right->window_cols;
	right->window_cols = t;

	tmp = left->title;
	left->title = right->title;
	right->title = tmp;

	/* Swap these fields so they reflect updated layout. */

	t = left->custom.diff_stats.unique_left;
	left->custom.diff_stats.unique_left = left->custom.diff_stats.unique_right;
	left->custom.diff_stats.unique_right = t;

	t = right->custom.diff_stats.unique_left;
	right->custom.diff_stats.unique_left = right->custom.diff_stats.unique_right;
	right->custom.diff_stats.unique_right = t;

	const int unique_lr = (CF_SHOW_UNIQUE_LEFT | CF_SHOW_UNIQUE_RIGHT);
	if((left->custom.diff_cmp_flags & unique_lr) == CF_SHOW_UNIQUE_LEFT ||
			(left->custom.diff_cmp_flags & unique_lr) == CF_SHOW_UNIQUE_RIGHT)
	{
		left->custom.diff_cmp_flags ^= unique_lr;
		right->custom.diff_cmp_flags ^= unique_lr;
	}

	tmp_view = *left;
	*left = *right;
	*right = tmp_view;
}

void
go_to_other_pane(void)
{
	change_window();
	modview_try_activate();
}

void
split_view(SPLIT orientation)
{
	if(curr_stats.number_of_windows == 2 && curr_stats.split == orientation)
		return;

	curr_stats.split = orientation;
	curr_stats.number_of_windows = 2;

	if(curr_stats.splitter_pos > 0)
	{
		stats_set_splitter_ratio(curr_stats.splitter_ratio);
	}

	stats_redraw_later();
}

void
only(void)
{
	if(curr_stats.number_of_windows != 1)
	{
		curr_stats.number_of_windows = 1;
		update_screen(UT_REDRAW);
	}
}

void
move_splitter(int by, int fact)
{
	int pos = curr_stats.splitter_pos;

	/* Determine exact splitter position if it's centered at the moment. */
	if(curr_stats.splitter_pos < 0)
	{
		if(curr_stats.split == HSPLIT)
		{
			pos = getmaxy(stdscr)/2 - 1;
		}
		else
		{
			pos = getmaxx(stdscr)/2 - 1 + getmaxx(stdscr)%2;
		}
	}

	set_splitter(pos + fact*by);
}

void
ui_remember_scroll_offset(void)
{
	const int rwin_pos = rwin.top_line/rwin.column_count;
	const int lwin_pos = lwin.top_line/lwin.column_count;
	curr_stats.scroll_bind_off = rwin_pos - lwin_pos;
}

void
ui_view_resize(view_t *view, int to)
{
	int pos;

	if(curr_stats.split == HSPLIT)
	{
		const int height = get_working_area_height();
		pos = (view == &lwin) ? (1 + to) : (height - to);
	}
	else
	{
		const int width = getmaxx(stdscr) - 1;
		pos = (view == &lwin) ? to : (width - to);
	}

	set_splitter(pos);
}

/* Sets splitter position making sure it isn't negative value which has special
 * meaning. */
static void
set_splitter(int pos)
{
	stats_set_splitter_pos(pos < 0 ? 0 : pos);
}

void
format_entry_name(const dir_entry_t *entry, NameFormat fmt, size_t buf_len,
		char buf[])
{
	if(fmt == NF_NONE)
	{
		char *escaped = escape_unreadable(entry->name);
		copy_str(buf, buf_len, escaped);
		free(escaped);
		return;
	}

	char tmp_buf[strlen(entry->name) + 1];
	const char *name = entry->name;

	if(fmt == NF_ROOT)
	{
		int root_len;
		const char *ext_pos;

		copy_str(tmp_buf, sizeof(tmp_buf), entry->name);
		split_ext(tmp_buf, &root_len, &ext_pos);
		name = tmp_buf;
	}

	char *escaped = escape_unreadable(name);

	const char *prefix, *suffix;
	ui_get_decors(entry, &prefix, &suffix);
	snprintf(buf, buf_len, "%s%s%s", prefix,
			(is_root_dir(escaped) && suffix[0] == '/') ? "" : escaped, suffix);

	free(escaped);
}

void
ui_get_decors(const dir_entry_t *entry, const char **prefix,
		const char **suffix)
{
	/* The check of actual file type can be relatively slow in some cases, so make
	 * sure we do it only when needed and at most once. */
	FileType type = FT_UNK;

	if(entry->name_dec_num == -1)
	{
		/* Find a match and cache the result. */

		((dir_entry_t *)entry)->name_dec_num = 0;
		if(cfg.name_dec_count != 0)
		{
			char full_path[PATH_MAX + 1];
			int i;

			get_full_path_of(entry, sizeof(full_path) - 1U, full_path);
			type = ui_view_entry_target_type(entry);
			if(type == FT_DIR)
			{
				strcat(full_path, "/");
			}

			for(i = 0; i < cfg.name_dec_count; ++i)
			{
				const file_dec_t *const file_dec = &cfg.name_decs[i];
				if(matchers_match(file_dec->matchers, full_path))
				{
					((dir_entry_t *)entry)->name_dec_num = i + 1;
					break;
				}
			}
		}
	}

	if(entry->name_dec_num == 0)
	{
		type = (type == FT_UNK) ? ui_view_entry_target_type(entry) : type;
		*prefix = cfg.type_decs[type][DECORATION_PREFIX];
		*suffix = cfg.type_decs[type][DECORATION_SUFFIX];
	}
	else
	{
		assert(entry->name_dec_num - 1 >= 0 && "Wrong index.");
		assert(entry->name_dec_num - 1 < cfg.name_dec_count && "Wrong index.");

		*prefix = cfg.name_decs[entry->name_dec_num - 1].prefix;
		*suffix = cfg.name_decs[entry->name_dec_num - 1].suffix;
	}
}

void
ui_view_reset_decor_cache(const view_t *view)
{
	int i;

	for(i = 0; i < view->list_rows; ++i)
	{
		view->dir_entry[i].name_dec_num = -1;
	}

	for(i = 0; i < view->left_column.entries.nentries; ++i)
	{
		view->left_column.entries.entries[i].name_dec_num = -1;
	}

	for(i = 0; i < view->right_column.entries.nentries; ++i)
	{
		view->right_column.entries.entries[i].name_dec_num = -1;
	}
}

/* Gets real type of file view entry.  Returns type of entry, resolving symbolic
 * link if needed. */
static FileType
ui_view_entry_target_type(const dir_entry_t *entry)
{
	if(entry->type == FT_LINK)
	{
		char *const full_path = format_str("%s/%s", entry->origin, entry->name);
		const FileType type = (get_symlink_type(full_path) != SLT_UNKNOWN)
		                    ? FT_DIR
		                    : FT_LINK;
		free(full_path);
		return type;
	}

	return entry->type;
}

void
checked_wmove(WINDOW *win, int y, int x)
{
	if(wmove(win, y, x) == ERR)
	{
		LOG_INFO_MSG("Error moving cursor on a window to (x=%d, y=%d).", x, y);
	}
}

void
ui_set_cursor(int visibility)
{
	/* PDCurses crashes if curs_set() is called for uninitialized library. */
	if(!vifm_testing())
	{
		(void)curs_set(visibility);
	}
}

int
ui_get_mouse(MEVENT *event)
{
	int ret = getmouse(event);
	if(ret != OK)
	{
		return ret;
	}

	/* Positions after 222 can become negative due to a combination of protocol
	 * limitations and implementation.  This workaround can extend the range at
	 * least a bit when SGR 1006 isn't available. */
	if(event->x < 0)
	{
		event->x &= 0xff;
	}
	if(event->y < 0)
	{
		event->y &= 0xff;
	}

	int mask = M_ALL_MODES;
	switch(vle_mode_get())
	{
		case NORMAL_MODE:  mask |= M_NORMAL_MODE; break;
		case NAV_MODE:
		case CMDLINE_MODE: mask |= M_CMDLINE_MODE; break;
		case VISUAL_MODE:  mask |= M_VISUAL_MODE; break;
		case MENU_MODE:    mask |= M_MENU_MODE; break;
		case VIEW_MODE:    mask |= M_VIEW_MODE; break;

		default:           mask = 0; break;
	}

	return (cfg.mouse & mask ? OK : ERR);
}

WINDOW *
ui_get_tab_line_win(const view_t *view)
{
	return (view_shows_tabline(view) ? view->title : tab_line);
}

int
ui_map_tab_line(view_t *view, int x)
{
	if(!is_null_or_empty(cfg.tab_line))
	{
		/* No mouse mapping for custom tabline. */
		return -1;
	}

	path_func pf = cfg.shorten_title_paths ? &replace_home_part : &path_identity;

	const int max_width = getmaxx(ui_get_tab_line_win(view));
	tab_line_info_t info = format_tab_labels(view, max_width, pf);

	int tab_idx = -1;

	int i;
	for(i = 0; i < info.count; ++i)
	{
		if(tab_idx == -1 && x < (int)info.labels[i].attrs_len)
		{
			tab_idx = info.skipped + i;
		}
		x -= info.labels[i].attrs_len;

		cline_dispose(&info.labels[i]);
	}
	free(info.labels);

	return tab_idx;
}

int
ui_wenclose(const view_t *view, WINDOW *win, int x, int y)
{
	return (ui_view_is_visible(view) && wenclose(win, y, x));
}

void
ui_display_too_small_term_msg(void)
{
	touchwin(stdscr);
	ui_refresh_win(stdscr);

	mvwin(status_bar, 0, 0);
	wresize(status_bar, getmaxy(stdscr), getmaxx(stdscr));
	werase(status_bar);
	waddstr(status_bar, "Terminal is too small for vifm");
	touchwin(status_bar);
	ui_refresh_win(status_bar);
}

void
ui_view_win_changed(view_t *view)
{
	wnoutrefresh(view->win);
	refresh_bottom_lines();
}

/* Makes sure that status line and status bar are drawn over view window after
 * view refresh. */
static void
refresh_bottom_lines(void)
{
	/* Use getmaxy(...) instead of multiline_status_bar to handle command line
	 * mode, which doesn't use this module to show multiline messages. */
	if(cfg.display_statusline && getmaxy(status_bar) > 1)
	{
		touchwin(stat_win);
		wnoutrefresh(stat_win);
		touchwin(status_bar);
		wnoutrefresh(status_bar);
	}
}

void
ui_view_reset_selection_and_reload(view_t *view)
{
	flist_sel_stash(view);
	load_saving_pos(view);
}

void
ui_view_reset_search_highlight(view_t *view)
{
	if(view->matches != 0)
	{
		view->matches = 0;
		ui_view_schedule_redraw(view);
	}
}

void
ui_views_reload_visible_filelists(void)
{
	if(curr_stats.preview.on)
	{
		load_saving_pos(curr_view);
	}
	else
	{
		ui_views_reload_filelists();
	}
}

void
ui_views_reload_filelists(void)
{
	load_saving_pos(curr_view);
	load_saving_pos(other_view);
}

void
ui_views_update_titles(void)
{
	ui_view_title_update(&lwin);
	ui_view_title_update(&rwin);
}

void
ui_view_title_update(view_t *view)
{
	const int gen_view = vle_mode_is(VIEW_MODE) && !curr_view->explore_mode;
	view_t *selected = gen_view ? other_view : curr_view;
	path_func pf = cfg.shorten_title_paths ? &replace_home_part : &path_identity;
	col_attr_t title_col;

	if(curr_stats.load_stage < 2 || !ui_view_is_visible(view))
	{
		return;
	}

	if(view == selected && !stats_silenced_ui())
	{
		char *const term_title = format_view_title(view, pf);
		term_title_update(term_title);
		free(term_title);
	}

	title_col = fixup_titles_attributes(view, view == selected);

	if(view_shows_tabline(view))
	{
		print_tabline(view->title, view, title_col, pf);
	}
	else
	{
		char *const title = format_view_title(view, pf);
		print_view_title(view, view == selected, title);
		wnoutrefresh(view->title);
		free(title);
	}

	if(view == curr_view && get_tabline_height() > 0)
	{
		print_tabline(tab_line, view, cfg.cs.color[TAB_LINE_COLOR], pf);
	}
}

/* Identity path function.  Returns its argument. */
static char *
path_identity(const char path[])
{
	return (char *)path;
}

/* Checks whether view displays list of pane tabs at the moment.  Returns
 * non-zero if so, otherwise zero is returned. */
static int
view_shows_tabline(const view_t *view)
{
	return cfg.pane_tabs
	    && !(curr_stats.preview.on && view == other_view)
	    && cfg.show_tab_line != STL_NEVER
	    && !(cfg.show_tab_line == STL_MULTIPLE && tabs_count(view) == 1);
}

/* Retrieves height of the tab line in lines.  Returns the height. */
static int
get_tabline_height(void)
{
	if(cfg.pane_tabs || cfg.show_tab_line == STL_NEVER)
	{
		return 0;
	}

	return (cfg.show_tab_line == STL_MULTIPLE && tabs_count(curr_view) == 1)
	     ? 0
	     : 1;
}

/* Prints title of the tab on specified curses window. */
static void
print_tabline(WINDOW *win, view_t *view, col_attr_t base_col, path_func pf)
{
	const int max_width = (vifm_testing() ? cfg.columns : getmaxx(win));

	ui_set_bg(win, &base_col, -1);
	werase(win);
	checked_wmove(win, 0, 0);

	if(is_null_or_empty(cfg.tab_line))
	{
		tab_line_info_t info = format_tab_labels(view, max_width, pf);

		int i;
		for(i = 0; i < info.count; ++i)
		{
			col_attr_t col = base_col;
			if(i == info.current - info.skipped)
			{
				cs_mix_colors(&col, &cfg.cs.color[TAB_LINE_SEL_COLOR]);
			}

			cline_print(&info.labels[i], win, &col);
			cline_dispose(&info.labels[i]);
		}
		free(info.labels);
	}
	else
	{
		char *fmt;
		if(vlua_handler_cmd(curr_stats.vlua, cfg.tab_line))
		{
			int other = (view == other_view);
			fmt = vlua_make_tab_line(curr_stats.vlua, cfg.tab_line, other, max_width);
		}
		else
		{
			fmt = strdup(cfg.tab_line);
		}

		if(fmt != NULL)
		{
			cline_t title = ma_expand_colored_custom(fmt, /*nmacros=*/0,
					/*macros=*/NULL, MA_OPT);
			free(fmt);

			cline_print(&title, win, &base_col);
			cline_dispose(&title);
		}
	}

	wnoutrefresh(win);
}

/* Computes layout of the tab line and formats tab labels.  Returns list of tab
 * labels along with supplementary information. */
static tab_line_info_t
format_tab_labels(view_t *view, int max_width, path_func pf)
{
	int i;
	int tab_count = tabs_count(view);
	int width_used = 0;

	int avg_width, spare_width;
	int min_widths[tab_count];
	compute_avg_width(&avg_width, &spare_width, min_widths, max_width, view, pf);

	int min_width = 0;
	for(i = 0; i < tab_count; ++i)
	{
		min_width += min_widths[i];
	}

	cline_t *tab_labels = NULL;
	DA_INSTANCE(tab_labels);

	int current_tab = -1;
	int skipped_tabs = 0;
	tab_info_t tab_info;
	for(i = 0; tabs_get(view, i, &tab_info) && width_used < max_width; ++i)
	{
		int current = (tab_info.view == view);
		if(current)
		{
			current_tab = i;
		}
		else if(current_tab == -1 && min_width > max_width)
		{
			/* Skip a tab that precedes the current one because it doesn't fit in. */
			min_width -= min_widths[i];
			spare_width = max_width - min_width;
			++skipped_tabs;
			continue;
		}

		tab_title_info_t title_info = make_tab_title_info(&tab_info, pf, i,
				current);
		cline_t prefix = format_tab_part(cfg.tab_prefix, &title_info);
		cline_t title = make_tab_title(&title_info);
		cline_t suffix = format_tab_part(cfg.tab_suffix, &title_info);
		dispose_tab_title_info(&title_info);

		const int width_needed = title.attrs_len;
		const int extra_width = prefix.attrs_len + suffix.attrs_len;
		int width = max_width;

		if(!current)
		{
			width = tab_info.last ? (max_width - width_used)
			                      : MIN(avg_width, extra_width + width_needed);
		}

		if(width < width_needed + extra_width)
		{
			if(spare_width > 0)
			{
				width += 1;
				--spare_width;
			}

			cline_left_ellipsis(&title, MAX(0, width - extra_width),
					curr_stats.ellipsis);
		}

		int real_width = prefix.attrs_len + title.attrs_len + suffix.attrs_len;

		if(width < real_width && max_width - width_used >= real_width)
		{
			width = real_width;
		}
		if(width >= real_width)
		{
			cline_t *tab_label = DA_EXTEND(tab_labels);
			if(tab_label != NULL)
			{
				*tab_label = prefix;
				cline_append(tab_label, &title);
				cline_append(tab_label, &suffix);
				DA_COMMIT(tab_labels);
			}
		}

		width_used += real_width;
	}

	tab_line_info_t result = {
		.labels = tab_labels,
		.count = DA_SIZE(tab_labels),
		.skipped = skipped_tabs,
		.current = current_tab,
	};
	return result;
}

/* Computes average width of tab tips as well as number of spare character
 * positions. */
static void
compute_avg_width(int *avg_width, int *spare_width, int min_widths[],
		int max_width, view_t *view, path_func pf)
{
	int left = max_width;
	int widths[tabs_count(view)];
	int i;
	tab_info_t tab_info;

	*avg_width = 0;
	*spare_width = 0;

	for(i = 0; tabs_get(view, i, &tab_info); ++i)
	{
		int current = (tab_info.view == view);

		tab_title_info_t title_info = make_tab_title_info(&tab_info, pf, i,
				current);
		cline_t prefix = format_tab_part(cfg.tab_prefix, &title_info);
		cline_t title = make_tab_title(&title_info);
		cline_t suffix = format_tab_part(cfg.tab_suffix, &title_info);
		dispose_tab_title_info(&title_info);

		widths[i] = prefix.attrs_len + title.attrs_len + suffix.attrs_len;
		min_widths[i] = prefix.attrs_len + suffix.attrs_len;

		cline_dispose(&prefix);
		cline_dispose(&title);
		cline_dispose(&suffix);

		if(current)
		{
			min_widths[i] = MIN(max_width, widths[i]);

			if(tabs_count(view) != 1)
			{
				int tab_count = tabs_count(view);

				left = MAX(max_width - widths[i], 0);
				*avg_width = left/(tab_count - 1);
				*spare_width = left%(tab_count - 1);
			}
		}
	}

	int new_avg_width = *avg_width;
	do
	{
		int well_used_width = 0;
		int truncated_count = 0;
		*avg_width = new_avg_width;
		for(i = 0; tabs_get(view, i, &tab_info); ++i)
		{
			if(tab_info.view != view)
			{
				if(widths[i] <= *avg_width)
				{
					well_used_width += widths[i];
				}
				else
				{
					++truncated_count;
				}
			}
		}
		if(truncated_count == 0)
		{
			break;
		}
		new_avg_width = (left - well_used_width)/truncated_count;
		*spare_width = (left - well_used_width)%truncated_count;
	}
	while(new_avg_width != *avg_width);
}

/* Gets title of the tab.  Returns colored line. */
TSTATIC cline_t
make_tab_title(const tab_title_info_t *title_info)
{
	if(!is_null_or_empty(cfg.tab_label))
	{
		return format_tab_part(cfg.tab_label, title_info);
	}

	cline_t result = cline_make();

	if(is_null_or_empty(title_info->name))
	{
		replace_string(&result.line, title_info->escaped_view_title);
	}
	else
	{
		replace_string(&result.line, title_info->name);
	}

	result.line_len = strlen(result.line);
	cline_finish(&result);
	return result;
}

/* Produces and stores information needed to format tab title.  Returns the
 * information. */
TSTATIC tab_title_info_t
make_tab_title_info(const tab_info_t *tab_info, path_func pf, int tab_num,
		int current_tab)
{
	view_t *view = tab_info->view;

	if(cfg.tail_tab_line_paths)
	{
		/* We can just do the replacement, because shortening home part doesn't make
		 * sense for a single path entry. */
		pf = &get_last_path_component;
	}

	char path[PATH_MAX + 1];
	if(view->explore_mode)
	{
		get_current_full_path(view, sizeof(path), path);
	}
	else
	{
		copy_str(path, sizeof(path), flist_get_dir(view));
	}

	tab_title_info_t result = {
		.current = current_tab,
	};

	const char *custom_title = "";
	if(flist_custom_active(view))
	{
		custom_title = view->custom.title;
		result.tree = cv_tree(view->custom.type);
	}

	result.name = escape_unreadable(tab_info->name == NULL ? "" : tab_info->name);
	result.escaped_path = escape_unreadable(path);
	result.cv_title = escape_unreadable(custom_title);

	char *view_title = format_view_title(view, pf);
	result.escaped_view_title = escape_unreadable(view_title);
	free(view_title);

	result.num = format_str("%d", tab_num + 1);

	return result;
}

/* Frees information used to format tab title. */
TSTATIC void
dispose_tab_title_info(tab_title_info_t *title_info)
{
	free(title_info->num);
	free(title_info->escaped_view_title);
	free(title_info->cv_title);
	free(title_info->escaped_path);
	free(title_info->name);
}

/* Formats part of a tab label.  Returns colored line. */
static cline_t
format_tab_part(const char fmt[], const tab_title_info_t *title_info)
{
	custom_macro_t macros[] = {
		{ .letter = 'n', .value = title_info->name },
		{ .letter = 'N', .value = title_info->num },
		{ .letter = 't', .value = title_info->escaped_view_title },
		{ .letter = 'p', .value = title_info->escaped_path,
			.expand_mods = 1, .parent = "/" },
		{ .letter = 'c', .value = title_info->cv_title },

		{ .letter = 'C', .value = (title_info->current ? "*" : ""),
		  .flag = 1},
		{ .letter = 'T', .value = (title_info->tree ? "*" : ""),
			.flag = 1 },
	};

	cline_t title = ma_expand_colored_custom(fmt, ARRAY_LEN(macros), macros,
			MA_OPT);

	return title;
}

/* Formats title for the view.  The pf function will be applied to full paths.
 * Returns newly allocated string, which should be freed by the caller, or NULL
 * if there is not enough memory. */
static char *
format_view_title(const view_t *view, path_func pf)
{
	char *unescaped_title;

	if(view->explore_mode)
	{
		char full_path[PATH_MAX + 1];
		get_current_full_path(view, sizeof(full_path), full_path);
		unescaped_title = strdup(pf(full_path));
	}
	else if(curr_stats.preview.on && view == other_view)
	{
		const char *const viewer = modview_detached_get_viewer();
		if(viewer != NULL)
		{
			unescaped_title = format_str("Command: %s", viewer);
		}
		else
		{
			unescaped_title = format_str("File: %s",
					get_current_file_name(curr_view));
		}
	}
	else if(flist_custom_active(view))
	{
		const char *tree_mark = (cv_tree(view->custom.type) ? "[tree]" : "");
		const char *path = pf(view->custom.orig_dir);
		if(view->custom.title[0] == '\0')
		{
			unescaped_title = format_str("%s @ %s", tree_mark, path);
		}
		else
		{
			unescaped_title = format_str("%s[%s] @ %s", tree_mark, view->custom.title,
					path);
		}
	}
	else
	{
		unescaped_title = strdup(pf(view->curr_dir));
	}

	char *escaped_title = escape_unreadable(unescaped_title);
	free(unescaped_title);
	return escaped_title;
}

/* Prints view title (which can be changed for printing).  Takes care of setting
 * correct attributes. */
static void
print_view_title(const view_t *view, int active_view, char title[])
{
	char *ellipsis;

	const size_t title_width = getmaxx(view->title);
	if(title_width == (size_t)-1)
	{
		return;
	}

	werase(view->title);

	if (cfg.ellipsis_position != 0)
	{
		ellipsis = cfg.ellipsis_position < 0
		         ? left_ellipsis(title, title_width, curr_stats.ellipsis)
		         : right_ellipsis(title, title_width, curr_stats.ellipsis);
	}
	else
	{
		ellipsis = active_view
		         ? left_ellipsis(title, title_width, curr_stats.ellipsis)
		         : right_ellipsis(title, title_width, curr_stats.ellipsis);
	}

	wprint(view->title, ellipsis);
	free(ellipsis);
}

/* Updates attributes for view titles and top line.  Returns base color used for
 * the title. */
static col_attr_t
fixup_titles_attributes(const view_t *view, int active_view)
{
	col_attr_t col = cfg.cs.color[TOP_LINE_COLOR];

	if(view->title == NULL)
	{
		/* Workaround for tests. */
		return col;
	}

	if(active_view)
	{
		cs_mix_colors(&col, &cfg.cs.color[TOP_LINE_SEL_COLOR]);

		ui_set_bg(view->title, &col, -1);
		ui_set_attr(view->title, &col, -1);
	}
	else
	{
		ui_set_bg(view->title, &col, cfg.cs.pair[TOP_LINE_COLOR]);
		ui_set_attr(view->title, &col, cfg.cs.pair[TOP_LINE_COLOR]);

		ui_set_bg(mtop_line, &col, cfg.cs.pair[TOP_LINE_COLOR]);
		werase(mtop_line);
	}

	return col;
}

int
ui_view_sort_list_contains(const signed char sort[SK_COUNT], char key)
{
	int i = -1;
	while(++i < SK_COUNT)
	{
		const int sort_key = abs(sort[i]);
		if(sort_key > SK_LAST)
		{
			return 0;
		}
		else if(sort_key == key)
		{
			return 1;
		}
	}
	return 0;
}

void
ui_view_sort_list_ensure_well_formed(view_t *view, signed char sort_keys[])
{
	int found_name_key = 0;
	int i = -1;

	while(++i < SK_COUNT)
	{
		const int sort_key = abs(sort_keys[i]);
		if(sort_key > SK_LAST)
		{
			break;
		}
		else if(sort_key == SK_BY_NAME || sort_key == SK_BY_INAME)
		{
			found_name_key = 1;
		}
	}

	if(!found_name_key && i < SK_COUNT &&
			(!flist_custom_active(view) ||
			 (sort_keys == view->sort && !ui_view_unsorted(view))))
	{
		sort_keys[i++] = SK_DEFAULT;
	}

	if(i < SK_COUNT)
	{
		memset(&sort_keys[i], SK_NONE, SK_COUNT - i);
	}
}

signed char *
ui_view_sort_list_get(const view_t *view, const signed char sort[])
{
	return (flist_custom_active(view) && ui_view_unsorted(view))
	     ? (signed char *)view->custom.sort
	     : (signed char *)sort;
}

int
ui_view_displays_numbers(const view_t *view)
{
	return view->num_type != NT_NONE && ui_view_displays_columns(view);
}

int
ui_view_is_visible(const view_t *view)
{
	return curr_stats.number_of_windows == 2 || curr_view == view;
}

int
ui_view_displays_columns(const view_t *view)
{
	return !view->ls_view
	    || is_in_miller_view(view)
	    || is_forced_list_mode(view);
}

int
ui_view_available_width(const view_t *view)
{
	const int correction = cfg.extra_padding ? -2 : 0;
	return view->window_cols + correction
	     - ui_view_left_reserved(view) - ui_view_right_reserved(view);
}

int
ui_view_left_reserved(const view_t *view)
{
	const int total = view->miller_ratios[0] + view->miller_ratios[1]
	                + view->miller_ratios[2];
	return is_in_miller_view(view)
	     ? (view->window_cols*view->miller_ratios[0])/total : 0;
}

int
ui_view_right_reserved(const view_t *view)
{
	dir_entry_t *const entry = get_current_entry(view);

	if(!is_in_miller_view(view) || is_parent_dir(entry->name))
	{
		return 0;
	}

	if(view->miller_preview != MP_ALL &&
			fentry_is_dir(entry) != (view->miller_preview == MP_DIRS))
	{
		return 0;
	}

	const int total = view->miller_ratios[0] + view->miller_ratios[1]
	                + view->miller_ratios[2];
	return (view->window_cols*view->miller_ratios[2])/total;
}

/* Whether miller columns should be displayed.  Returns non-zero if so,
 * otherwise zero is returned. */
static int
is_in_miller_view(const view_t *view)
{
	return view->miller_view
	    && !flist_custom_active(view);
}

/* Whether view must display straight single-column file list.  Returns non-zero
 * if so, otherwise zero is returned. */
static int
is_forced_list_mode(const view_t *view)
{
	return flist_custom_active(view)
	    && (cv_tree(view->custom.type) || cv_compare(view->custom.type));
}

int
ui_qv_left(const view_t *view)
{
	const int with_margin = (!curr_stats.preview.clearing && cfg.extra_padding);
	return with_margin ? 1 : 0;
}

int
ui_qv_top(const view_t *view)
{
	const int with_margin = (!curr_stats.preview.clearing && cfg.extra_padding);
	return with_margin ? 1 : 0;
}

int
ui_qv_height(const view_t *view)
{
	const int with_margin = (!curr_stats.preview.clearing && cfg.extra_padding);
	return with_margin ? view->window_rows - 2 : view->window_rows;
}

int
ui_qv_width(const view_t *view)
{
	const int with_margin = (!curr_stats.preview.clearing && cfg.extra_padding);
	return with_margin ? view->window_cols - 2 : view->window_cols;
}

void
ui_qv_cleanup_if_needed(void)
{
	if(curr_stats.preview.on && curr_stats.preview.kind != VK_TEXTUAL)
	{
		qv_cleanup(other_view, curr_stats.preview.cleanup_cmd);
	}
}

void
ui_hide_graphics(void)
{
	ui_qv_cleanup_if_needed();
	modview_hide_graphics();
}

void
ui_invalidate_cs(const col_scheme_t *cs)
{
	int i;
	tab_info_t tab_info;
	for(i = 0; tabs_enum_all(i, &tab_info); ++i)
	{
		if(ui_view_get_cs(tab_info.view) == cs)
		{
			fview_reset_cs(tab_info.view);
		}
	}
}

const col_scheme_t *
ui_view_get_cs(const view_t *view)
{
	return view->local_cs ? &view->cs : &cfg.cs;
}

void
ui_view_erase(view_t *view, int use_global_cs)
{
	const col_scheme_t *cs = (use_global_cs ? &cfg.cs : ui_view_get_cs(view));
	col_attr_t col = ui_get_win_color(view, cs);
	ui_set_bg(view->win, &col, -1);
	werase(view->win);
}

col_attr_t
ui_get_win_color(const view_t *view, const col_scheme_t *cs)
{
	col_attr_t col = cs->color[WIN_COLOR];
	if(view == other_view && window_shows_dirlist(view))
	{
		cs_mix_colors(&col, &cs->color[OTHER_WIN_COLOR]);
	}
	return col;
}

int
ui_view_unsorted(const view_t *view)
{
	return cv_unsorted(view->custom.type);
}

void
ui_shutdown(void)
{
	if(curr_stats.load_stage >= 0 && !vifm_testing() && !isendwin())
	{
		ui_qv_cleanup_if_needed();
		modview_hide_graphics();
		def_prog_mode();
		endwin();
	}
}

void
ui_pause(void)
{
	/* Show previous screen state. */
	ui_shutdown();
	/* Yet restore program mode to read input without waiting for Enter. */
	reset_prog_mode();
	/* Refresh the window, because otherwise curses redraws the screen on call to
	 * `compat_wget_wch()` (why does it do this?). */
	wnoutrefresh(inf_delay_window);

	/* Ignore window resize. */
	wint_t pressed;
	do
	{
		/* Nothing. */
	}
	while(compat_wget_wch(inf_delay_window, &pressed) != ERR &&
			pressed == KEY_RESIZE);

	/* Redraw UI to account for all things including graphical preview. */
	stats_redraw_later();
}

void
ui_set_bg(WINDOW *win, const col_attr_t *col, int pair)
{
	if(curr_stats.load_stage >= 1)
	{
		const cchar_t bg = cs_color_to_cchar(col, pair);
		wbkgrndset(win, &bg);
	}
}

void
ui_set_attr(WINDOW *win, const col_attr_t *col, int pair)
{
	/* win can be NULL in tests. */
	if(curr_stats.load_stage < 1 || win == NULL)
	{
		return;
	}

	if(pair < 0)
	{
		pair = cs_load_color(col);
	}

	/* Compiler complains about unused result of comma operator, because
	 * wattr_set() is a macro and it uses comma to evaluate multiple expresions.
	 * So cast result to void. */
	(void)wattr_set(win, cs_color_get_attr(col), pair, NULL);
}

void
ui_drop_attr(WINDOW *win)
{
	if(curr_stats.load_stage >= 1)
	{
		wattrset(win, 0);
	}
}

void
ui_pass_through(const strlist_t *lines, WINDOW *win, int x, int y)
{
	/* Position hardware cursor on the screen. */
	checked_wmove(win, y, x);
	use_wrefresh(win);

	int i;
	for(i = 0; i < lines->nitems; ++i)
	{
		puts(lines->items[i]);
	}

	/* Make curses synchronize its idea about where cursor is with the terminal
	 * state. */
	touchwin(status_bar);
	checked_wmove(status_bar, 0, 0);
	use_wrefresh(status_bar);
}

void
ui_view_schedule_redraw(view_t *view)
{
	pthread_mutex_lock(view->timestamps_mutex);
	view->need_redraw = 1;
	pthread_mutex_unlock(view->timestamps_mutex);
}

void
ui_view_schedule_reload(view_t *view)
{
	pthread_mutex_lock(view->timestamps_mutex);
	view->need_reload = 1;
	pthread_mutex_unlock(view->timestamps_mutex);
}

void
ui_view_redrawn(view_t *view)
{
	pthread_mutex_lock(view->timestamps_mutex);
	view->need_redraw = 0;
	pthread_mutex_unlock(view->timestamps_mutex);
}

UiUpdateEvent
ui_view_query_scheduled_event(view_t *view)
{
	UiUpdateEvent event;

	pthread_mutex_lock(view->timestamps_mutex);

	if(view->need_reload)
	{
		event = UUE_RELOAD;
	}
	else if(view->need_redraw)
	{
		event = UUE_REDRAW;
	}
	else
	{
		event = UUE_NONE;
	}

	view->need_redraw = 0;
	view->need_reload = 0;

	pthread_mutex_unlock(view->timestamps_mutex);

	return event;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
