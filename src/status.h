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

#include <stdio.h> /* FILE */

#include "utils/tree.h"
#include "utils/fs_limits.h"

#include "color_scheme.h"

struct config_t;

typedef enum
{
	HSPLIT,
	VSPLIT,
}
SPLIT;

typedef enum
{
	SOURCING_NONE,
	SOURCING_PROCESSING,
	SOURCING_FINISHING,
}
SourcingState;

/* Type of execution environment. */
typedef enum
{
	EET_LINUX_NATIVE,    /* Linux native console. */
	EET_EMULATOR,        /* Terminal emulator with no DISPLAY defined. */
	EET_EMULATOR_WITH_X, /* Terminal emulator within accessible X. */
}
ExecEnvType;

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
}
UpdateType;

/* Possible states of terminal with regard to its size. */
typedef enum
{
	TS_NORMAL,         /* OK to draw UI. */
	TS_TOO_SMALL,      /* Too small terminal. */
	TS_BACK_TO_NORMAL, /* Was too small some moments ago, need to restore UI. */
}
TermState;

typedef enum
{
	/* Shell that is aware of command escaping and backslashes in paths. */
	ST_NORMAL,
	/* Dumb cmd.exe shell on Windows. */
	ST_CMD,
}
ShellType;

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

	/* Describes terminal state with regard to its dimensions. */
	TermState term_state;

	tree_t dirsize_cache; /* ga command results */

	int last_search_backward;

	int ch_pos; /* for :cd, :pushd and 'letter */

	int confirmed;

	/* Whether to skip complete UI redraw after returning from a shellout. */
	int skip_shellout_redraw;

	col_scheme_t *cs;
	char color_scheme[NAME_MAX];

	int msg_head, msg_tail;
	char *msgs[51];
	int save_msg_in_list;
	int allow_sb_msg_truncation; /* Whether truncation can be performed. */

	int scroll_bind_off;
	SPLIT split;
	int splitter_pos;

	SourcingState sourcing_state;

	/* Set while executing :restart command to prevent excess screen updates. */
	int restart_in_progress;

	ExecEnvType exec_env_type; /* Specifies execution environment type. */

	/* Shows which of supported terminal multiplexers is currently in use, if
	 * any. */
	TermMultiplexer term_multiplexer;

	/* Stores last command-line mode command that was executed or an empty line
	 * (e.g. right after startup or :restart command). */
	char *last_cmdline_command;

	int initial_lines; /* Initial terminal height in lines. */
	int initial_columns; /* Initial terminal width in characters. */

	ShellType shell_type; /* Specifies type of shell. */

	int file_picker_mode; /* Whether vifm was started in file picking mode. */

	const char *fuse_umount_cmd; /* Command to use for fuse unmounting. */

	FILE *original_stdout; /* Saved original standard output. */

	char *output_delimiter; /* Delimiter for writing out list of paths. */

#ifdef HAVE_LIBGTK
	int gtk_available; /* for mimetype detection */
#endif
}
status_t;

extern status_t curr_stats;

/* Returns non-zero on error. */
int init_status(struct config_t *config);

/* Resets some part of runtime status information to its initial values.
 * Returns non-zero on error. */
int reset_status(const struct config_t *config);

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

/* Updates curr_stats.shell_type field according to passed shell command. */
void stats_update_shell_type(const char shell_cmd[]);

/* Updates curr_stats.term_state field according to specified terminal
 * dimensions.  Returns new state. */
TermState stats_update_term_state(int screen_x, int screen_y);

/* Sets delimiter (curr_stats.output_delimiter) for separating multiple paths in
 * output. */
void stats_set_output_delimiter(const char delimiter[]);

/* Checks whether custom actions on file choosing is set.  Returns non-zero if
 * so, otherwise zero is returned. */
int stats_file_choose_action_set(void);

#endif /* VIFM__STATUS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
