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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifndef __STATUS_H__
#define __STATUS_H__

#include <sys/stat.h>

#include <inttypes.h>
#include <limits.h>

#include "../config.h"
#include "tree.h"

#if !defined(NAME_MAX) && defined(_WIN32)
#include <io.h>
#define NAME_MAX FILENAME_MAX
#endif

#include "color_scheme.h"

enum Split
{
	HSPLIT,
	VSPLIT,
};

typedef struct
{
	int need_redraw;
	int startup_redraw_pending;
	int last_char;
	int is_console;
	int search;
	int save_msg;
	int use_register;
	int use_input_bar;
	int curr_register;
	int register_saved;
	int number_of_windows;
	int view;
	int show_full;
	int skip_history;
	int vifm_started; /* 0 - no TUI, 1 - TUI, 2 - all */

	int errmsg_shown; /* 0 - none, 1 - error, 2 - query */

	int too_small_term;

	tree_t dirsize_cache;
	
	int last_search_backward;

	int ch_pos; /* for :cd, :pushd and 'letter */

	int confirmed;

	int auto_redraws;

	int cs_base;
	Col_scheme *cs;
	char color_scheme[NAME_MAX];

#ifdef HAVE_LIBGTK
	int gtk_available; /* for mimetype detection */
#endif

	int msg_head, msg_tail;
	char *msgs[51];
	int save_msg_in_list;

#ifdef _WIN32
	int as_admin;
#endif

	int scroll_bind_off;
	enum Split split;
	float splitter_pos;
}Status;

extern Status curr_stats;

void init_status(void);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
