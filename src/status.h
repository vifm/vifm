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

#ifndef VIFM__STATUS_H__
#define VIFM__STATUS_H__

#include <sys/stat.h>

#include <inttypes.h>

#include "utils/tree.h"
#include "utils/fs_limits.h"

#include "color_scheme.h"

typedef enum
{
	HSPLIT,
	VSPLIT,
}SPLIT;

typedef enum
{
	SOURCING_NONE,
	SOURCING_PROCESSING,
	SOURCING_FINISHING,
}SourcingState;

/* Type of execution environment. */
typedef enum
{
	ENVTYPE_LINUX_NATIVE, /* Running in linux native console. */
	ENVTYPE_EMULATOR, /* Running in terminal emulator with no DISPLAY defined. */
	ENVTYPE_EMULATOR_WITH_X, /* Running in emulator within accessible X. */
}EnvType;

/* List of terminal multiplexers. */
typedef enum
{
	TM_NONE,   /* Plain console. */
	TM_SCREEN, /* GNU screen. */
	TM_TMUX,   /* tmux. */
}
TermMultiplexer;

typedef enum
{
	UT_NONE, /* no update needed */
	UT_REDRAW, /* screen redraw requested */
	UT_FULL, /* file lists reload followed by screen redraw requested */
}UpdateType;

typedef struct
{
	UpdateType need_update;
	int last_char;
	int save_msg; /* zero - don't save, 2 - save after resize, other - save */
	int use_register;
	int use_input_bar;
	int curr_register;
	int register_saved;
	int number_of_windows;
	int view; /* Shows whether preview mode is activated. */
	int skip_history;
	int load_stage; /* 0 - no TUI, 1 - part of TUI, 2 - TUI, 3 - all */

	int errmsg_shown; /* 0 - none, 1 - error, 2 - query */

	int too_small_term;

	tree_t dirsize_cache; /* ga command results */
	
	int last_search_backward;

	int ch_pos; /* for :cd, :pushd and 'letter */

	int confirmed;

	int auto_redraws;

	int cs_base;
	col_scheme_t *cs;
	char color_scheme[NAME_MAX];

#ifdef HAVE_LIBGTK
	int gtk_available; /* for mimetype detection */
#endif

	int msg_head, msg_tail;
	char *msgs[51];
	int save_msg_in_list;
	int allow_sb_msg_truncation; /* Whether truncation can be performed. */

#ifdef _WIN32
	int as_admin;
#endif

	int scroll_bind_off;
	SPLIT split;
	int splitter_pos;

	SourcingState sourcing_state;

	/* Set while executing :restart command to prevent excess screen updates. */
	int restart_in_progress;

	EnvType env_type; /* Specifies execution environment type. */

	/* Shows which of supported terminal multiplexers is currently in use, if
	 * any. */
	TermMultiplexer term_multiplexer;

	/* Stores last command-line mode command that was executed or an empty line
	 * (e.g. right after startup or :restart command). */
	char *last_cmdline_command;

	int initial_lines; /* Initial terminal height in lines. */
	int initial_columns; /* Initial terminal width in characters. */
}
status_t;

extern status_t curr_stats;

/* Returns non-zero on error. */
int init_status(void);

/* Resets some part of runtime status information to its initial values.
 * Returns non-zero on error. */
int reset_status(void);

/* Sets internal flag to schedule postponed redraw operation. */
void schedule_redraw(void);

/* Checks for postponed redraw operations. Returns non-zero if redraw operation
 * was scheduled and resets internal flag. */
int is_redraw_scheduled(void);

/* Updates curr_stats to reflect whether terminal multiplexers support is
 * enabled. */
void set_using_term_multiplexer(int use_term_multiplexer);

/* Updates last_cmdline_command field of the status structure. */
void update_last_cmdline_command(const char cmd[]);

#endif /* VIFM__STATUS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
