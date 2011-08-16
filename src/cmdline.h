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

#ifndef __CMDLINE_H__
#define __CMDLINE_H__

#include <wchar.h>

enum CmdLineSubModes {
	CMD_SUBMODE,
	MENU_CMD_SUBMODE,
	SEARCH_FORWARD_SUBMODE,
	SEARCH_BACKWARD_SUBMODE,
	MENU_SEARCH_FORWARD_SUBMODE,
	MENU_SEARCH_BACKWARD_SUBMODE,
	VSEARCH_FORWARD_SUBMODE,
	VSEARCH_BACKWARD_SUBMODE,
	PROMPT_SUBMODE,
};

typedef void (*prompt_cb)(const char *renponse);

void init_cmdline_mode(int *key_mode);
void enter_cmdline_mode(enum CmdLineSubModes cl_sub_mode, const wchar_t *cmd,
		void *ptr);
void enter_prompt_mode(const wchar_t *prompt, const char *cmd, prompt_cb cb);
void redraw_cmdline(void);

#ifdef TEST

enum hist { HIST_NONE, HIST_GO, HIST_SEARCH };

struct line_stats
{
	wchar_t *line;            /* the line reading */
	int index;                /* index of the current character */
	int curs_pos;             /* position of the cursor */
	int len;                  /* length of the string */
	int cmd_pos;              /* position in the history */
	wchar_t prompt[320];      /* prompt */
	int prompt_wid;           /* width of prompt */
	int complete_continue;    /* if non-zero, continue the previous completion */
	enum hist history_search; /* HIST_* */
	int hist_search_len;      /* length of history search pattern */
	wchar_t *line_buf;        /* content of line before using history */
};

int line_completion(struct line_stats *stat);

#endif

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
