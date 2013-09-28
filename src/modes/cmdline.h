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

#ifndef VIFM__MODES__CMDLINE_H__
#define VIFM__MODES__CMDLINE_H__

#ifdef TEST
#include "../utils/fs_limits.h"
#endif
#include <wchar.h>

typedef enum
{
	CMD_SUBMODE,
	MENU_CMD_SUBMODE,
	SEARCH_FORWARD_SUBMODE,
	SEARCH_BACKWARD_SUBMODE,
	MENU_SEARCH_FORWARD_SUBMODE,
	MENU_SEARCH_BACKWARD_SUBMODE,
	VSEARCH_FORWARD_SUBMODE,
	VSEARCH_BACKWARD_SUBMODE,
	PROMPT_SUBMODE,
	VIEW_SEARCH_FORWARD_SUBMODE,
	VIEW_SEARCH_BACKWARD_SUBMODE,
	FILTER_SUBMODE,
}
CMD_LINE_SUBMODES;

typedef void (*prompt_cb)(const char *renponse);
typedef int (*complete_cmd_func)(const char *cmd);

void init_cmdline_mode(int *key_mode);

void enter_cmdline_mode(CMD_LINE_SUBMODES cl_sub_mode, const wchar_t *cmd,
		void *ptr);

/* Enters command-line editing mode with prompt submode activated.  cmd
 * specifies initial value, cb - callback called on success, complete -
 * completion function, allow_ee - whether issuing external editor is
 * allowed. */
void enter_prompt_mode(const wchar_t prompt[], const char cmd[], prompt_cb cb,
		complete_cmd_func complete, int allow_ee);

void redraw_cmdline(void);

#ifdef TEST

typedef enum
{
	HIST_NONE,
	HIST_GO,
	HIST_SEARCH
}HIST;

/* Holds state of the command-line editing mode. */
typedef struct
{
	wchar_t *line;            /* the line reading */
	wchar_t *initial_line;    /* initial state of the line */
	int index;                /* index of the current character */
	int curs_pos;             /* position of the cursor */
	int len;                  /* length of the string */
	int cmd_pos;              /* position in the history */
	wchar_t prompt[NAME_MAX]; /* prompt */
	int prompt_wid;           /* width of prompt */
	int complete_continue;    /* if non-zero, continue the previous completion */
	int dot_pos;              /* history position for dot completion, or < 0 */
	size_t dot_index;         /* dot completion line index */
	size_t dot_len;           /* dot completion previous completion len */
	HIST history_search;      /* HIST_* */
	int hist_search_len;      /* length of history search pattern */
	wchar_t *line_buf;        /* content of line before using history */
	int reverse_completion;
	complete_cmd_func complete;
	int search_mode;
	int old_top;              /* for search_mode */
	int old_pos;              /* for search_mode */
	int line_edited;          /* Cache for whether input line changed flag. */
}
line_stats_t;

#endif

#endif /* VIFM__MODES__CMDLINE_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
