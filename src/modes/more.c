/* vifm
 * Copyright (C) 2015 xaizek.
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

#include "more.h"

#include <curses.h>

#include <assert.h> /* assert() */
#include <limits.h> /* INT_MAX */
#include <stddef.h> /* NULL size_t */
#include <string.h> /* strdup() */

#include "../cfg/config.h"
#include "../compat/curses.h"
#include "../compat/reallocarray.h"
#include "../engine/keys.h"
#include "../engine/mode.h"
#include "../menus/menus.h"
#include "../ui/ui.h"
#include "../utils/macros.h"
#include "../utils/str.h"
#include "../utils/utf8.h"
#include "cmdline.h"
#include "modes.h"
#include "wk.h"

/* Provide more readable definitions of key codes. */

static void calc_vlines_wrapped(void);
static void leave_more_mode(void);
static const char * get_text_beginning(void);
static void draw_all(const char text[]);
static void cmd_leave(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info);
static void cmd_colon(key_info_t key_info, keys_info_t *keys_info);
static void cmd_bottom(key_info_t key_info, keys_info_t *keys_info);
static void cmd_top(key_info_t key_info, keys_info_t *keys_info);
static void cmd_down_line(key_info_t key_info, keys_info_t *keys_info);
static void cmd_up_line(key_info_t key_info, keys_info_t *keys_info);
static void cmd_down_screen(key_info_t key_info, keys_info_t *keys_info);
static void cmd_up_screen(key_info_t key_info, keys_info_t *keys_info);
static void cmd_down_page(key_info_t key_info, keys_info_t *keys_info);
static void cmd_up_page(key_info_t key_info, keys_info_t *keys_info);
static void goto_vline(int line);
static void goto_vline_below(int by);
static void goto_vline_above(int by);

/* Text displayed by the mode. */
static char *text;
/* (first virtual line, screen width, offset in text) triples per real line. */
static int (*data)[3];

/* Number of virtual lines. */
static int nvlines;

/* Current real line number. */
static int curr_line;
/* Current virtual line number. */
static int curr_vline;

/* Width of the area text is printed on. */
static int viewport_width;
/* Height of the area text is printed on. */
static int viewport_height;

/* List of builtin keys. */
static keys_add_info_t builtin_keys[] = {
	{WK_C_c,   {{&cmd_leave},     .descr = "leave more mode"}},
	{WK_C_j,   {{&cmd_down_line}, .descr = "scroll one line down"}},
	{WK_C_l,   {{&cmd_ctrl_l},    .descr = "redraw"}},

	{WK_COLON, {{&cmd_colon},       .descr = "switch to cmdline mode"}},
	{WK_ESC,   {{&cmd_leave},       .descr = "leave more mode"}},
	{WK_CR,    {{&cmd_leave},       .descr = "leave more mode"}},
	{WK_SPACE, {{&cmd_down_screen}, .descr = "scroll one screen down"}},

	{WK_G,     {{&cmd_bottom},      .descr = "scroll to the end"}},
	{WK_b,     {{&cmd_up_screen},   .descr = "scroll one screen up"}},
	{WK_d,     {{&cmd_down_page},   .descr = "scroll page down"}},
	{WK_f,     {{&cmd_down_screen}, .descr = "scroll one screen down"}},
	{WK_g,     {{&cmd_top},         .descr = "scroll to the beginning"}},
	{WK_j,     {{&cmd_down_line},   .descr = "scroll one line down"}},
	{WK_k,     {{&cmd_up_line},     .descr = "scroll one line up"}},
	{WK_q,     {{&cmd_leave},       .descr = "leave more mode"}},
	{WK_u,     {{&cmd_up_page},     .descr = "scroll page up"}},

#ifdef ENABLE_EXTENDED_KEYS
	{{K(KEY_BACKSPACE)}, {{&cmd_up_line},     .descr = "scroll one line up"}},
	{{K(KEY_DOWN)},      {{&cmd_down_line},   .descr = "scroll one line down"}},
	{{K(KEY_UP)},        {{&cmd_up_line},     .descr = "scroll one line up"}},
	{{K(KEY_HOME)},      {{&cmd_top},         .descr = "scroll to the beginning"}},
	{{K(KEY_END)},       {{&cmd_bottom},      .descr = "scroll to the end"}},
	{{K(KEY_NPAGE)},     {{&cmd_down_screen}, .descr = "scroll one screen down"}},
	{{K(KEY_PPAGE)},     {{&cmd_up_screen},   .descr = "scroll one screen up"}},
#endif /* ENABLE_EXTENDED_KEYS */
};

void
modmore_init(void)
{
	int ret_code;

	ret_code = vle_keys_add(builtin_keys, ARRAY_LEN(builtin_keys), MORE_MODE);
	assert(ret_code == 0 && "Failed to initialize more mode keys.");

	(void)ret_code;
}

void
modmore_enter(const char txt[])
{
	text = strdup(txt);
	curr_line = 0;
	curr_vline = 0;

	vle_mode_set(MORE_MODE, VMT_PRIMARY);
	ui_hide_graphics();

	modmore_redraw();
}

void
modmore_abort(void)
{
	leave_more_mode();
}

/* Recalculates virtual lines of a view with line wrapping. */
static void
calc_vlines_wrapped(void)
{
	const char *p;
	char *q;

	int i;
	const int nlines = count_lines(text, INT_MAX);

	data = reallocarray(NULL, nlines, sizeof(*data));

	nvlines = 0;

	p = text;
	q = text - 1;

	for(i = 0; i < nlines; ++i)
	{
		char saved_char;
		q = until_first(q + 1, '\n');
		saved_char = *q;
		*q = '\0';

		data[i][0] = nvlines++;
		data[i][1] = utf8_strsw_with_tabs(p, cfg.tab_stop);
		data[i][2] = p - text;
		nvlines += data[i][1]/viewport_width;

		*q = saved_char;
		p = q + 1;
	}
}

/* Quits the mode restoring previously active one. */
static void
leave_more_mode(void)
{
	update_string(&text, NULL);
	free(data);
	data = NULL;

	vle_mode_set(NORMAL_MODE, VMT_PRIMARY);
	stats_redraw_later();
}

void
modmore_redraw(void)
{
	if(resize_for_menu_like() != 0)
	{
		return;
	}
	wresize(status_bar, 1, getmaxx(stdscr));

	viewport_width = getmaxx(menu_win);
	viewport_height = getmaxy(menu_win);
	calc_vlines_wrapped();
	goto_vline(curr_vline);

	draw_all(get_text_beginning());
	checked_wmove(menu_win, 0, 0);
}

/* Retrieves beginning of the text that should be displayed.  Returns the
 * beginning. */
static const char *
get_text_beginning(void)
{
	int skipped = 0;
	const char *text_piece = text + data[curr_line][2];
	/* Skip invisible virtual lines (those that are above current one). */
	while(skipped < curr_vline - data[curr_line][0])
	{
		text_piece += utf8_strsnlen(text_piece, viewport_width);
		++skipped;
	}
	return text_piece;
}

/* Draws all components of the mode onto the screen. */
static void
draw_all(const char text[])
{
	/* Setup correct attributes for the windows. */
	ui_set_attr(menu_win, &cfg.cs.color[WIN_COLOR], cfg.cs.pair[WIN_COLOR]);
	ui_set_attr(status_bar, &cfg.cs.color[CMD_LINE_COLOR],
			cfg.cs.pair[CMD_LINE_COLOR]);

	/* Clean up everything. */
	werase(menu_win);
	werase(status_bar);

	/* Draw the text. */
	checked_wmove(menu_win, 0, 0);
	wprint(menu_win, text);

	/* Draw status line. */
	checked_wmove(status_bar, 0, 0);
	mvwprintw(status_bar, 0, 0, "-- More -- %d-%d/%d", curr_vline + 1,
			MIN(nvlines, curr_vline + viewport_height), nvlines);

	/* Inform curses of the changes. */
	wnoutrefresh(menu_win);
	wnoutrefresh(status_bar);

	/* Apply all changes. */
	doupdate();
}

/* Leaves the mode. */
static void
cmd_leave(key_info_t key_info, keys_info_t *keys_info)
{
	leave_more_mode();
}

/* Redraws the mode. */
static void
cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info)
{
	modmore_redraw();
}

/* Switches to command-line mode. */
static void
cmd_colon(key_info_t key_info, keys_info_t *keys_info)
{
	leave_more_mode();
	modcline_enter(CLS_COMMAND, "");
}

/* Navigate to the bottom. */
static void
cmd_bottom(key_info_t key_info, keys_info_t *keys_info)
{
	goto_vline(INT_MAX);
}

/* Navigate to the top. */
static void
cmd_top(key_info_t key_info, keys_info_t *keys_info)
{
	goto_vline(0);
}

/* Go one line below. */
static void
cmd_down_line(key_info_t key_info, keys_info_t *keys_info)
{
	goto_vline(curr_vline + 1);
}

/* Go one line above. */
static void
cmd_up_line(key_info_t key_info, keys_info_t *keys_info)
{
	goto_vline(curr_vline - 1);
}

/* Go one screen below. */
static void
cmd_down_screen(key_info_t key_info, keys_info_t *keys_info)
{
	goto_vline(curr_vline + viewport_height);
}

/* Go one screen above. */
static void
cmd_up_screen(key_info_t key_info, keys_info_t *keys_info)
{
	goto_vline(curr_vline - viewport_height);
}

/* Go one page (half of the screen) below. */
static void
cmd_down_page(key_info_t key_info, keys_info_t *keys_info)
{
	goto_vline(curr_vline + viewport_height/2);
}

/* Go one page (half of the screen) above. */
static void
cmd_up_page(key_info_t key_info, keys_info_t *keys_info)
{
	goto_vline(curr_vline - viewport_height/2);
}

/* Navigates to the specified virtual line taking care of values that are out of
 * range. */
static void
goto_vline(int line)
{
	const int max_vline = nvlines - viewport_height;

	if(line > max_vline)
	{
		line = max_vline;
	}
	if(line < 0)
	{
		line = 0;
	}

	if(curr_vline == line)
	{
		return;
	}

	if(line > curr_vline)
	{
		goto_vline_below(line - curr_vline);
	}
	else
	{
		goto_vline_above(curr_vline - line);
	}

	modmore_redraw();
}

/* Navigates by virtual lines below. */
static void
goto_vline_below(int by)
{
	while(by-- > 0)
	{
		const int height = MAX(DIV_ROUND_UP(data[curr_line][1], viewport_width), 1);
		if(curr_vline + 1 >= data[curr_line][0] + height)
		{
			++curr_line;
		}

		++curr_vline;
	}
}

/* Navigates by virtual lines above. */
static void
goto_vline_above(int by)
{
	while(by-- > 0)
	{
		if(curr_vline - 1 < data[curr_line][0])
		{
			--curr_line;
		}

		--curr_vline;
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
