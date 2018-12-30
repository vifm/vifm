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

#include <stdint.h> /* uint64_t */
#include <stdio.h> /* FILE */

#include "compat/fs_limits.h"
#include "ui/color_scheme.h"
#include "utils/hist.h"

/* Special value foe dcache fields meaning that it wasn't set. */
#define DCACHE_UNKNOWN ((uint64_t)-1)

struct config_t;
struct dir_entry_t;

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

/* Types of updates that can be performed by update_screen(). */
typedef enum
{
	UT_NONE,   /* No update needed. */
	UT_REDRAW, /* Screen redraw requested. */
	UT_FULL,   /* File lists reload followed by screen redraw requested. */
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

/* Type of output variables of dcache_get_of(), which represent state of cache
 * entries. */
typedef struct
{
	uint64_t value; /* Cached value or DCACHE_UNKNOWN. */
	int is_valid;   /* Whether value is up-to-date. */
}
dcache_result_t;

/* Current preview (quickview) parameters. */
typedef struct
{
	char *cleanup_cmd;           /* Cleanup command. */
	struct view_info_t *explore; /* State of explored quick view. */
	unsigned int on : 1;         /* Whether preview mode is active.  Use
	                                stats_set_quickview() to change the value. */
	unsigned int graphical : 1;  /* Whether current preview displays graphics. */
	unsigned int clearing : 1;   /* Whether in process of clearing preview. */
}
preview_t;

typedef struct
{
	UpdateType need_update; /* Postponed way of doing an update. */
	int last_char;
	int save_msg; /* zero - don't save, 2 - save after resize, other - save */
	int use_register;
	int use_input_bar;     /* Whether input bar should be updated. */
	int curr_register;
	int register_saved;
	int number_of_windows;
	int drop_new_dir_hist; /* Skip recording of new directory history. */
	int load_stage; /* -1 - test, 0 - no TUI, 1 - part of TUI, 2 - TUI, 3 - all */

	preview_t preview; /* State of preview (quickview). */

	/* Describes terminal state with regard to its dimensions. */
	TermState term_state;

	int last_search_backward;

	/* Flag which controls use of historical cursor positions.  On by default, but
	 * gets temporarily disabled on certain operations possibly depending on value
	 * of options. */
	int ch_pos;

	int confirmed;

	/* Whether to skip complete UI redraw after returning from a shellout. */
	int skip_shellout_redraw;

	/* Color scheme to use for :commands that manipulate colors.  Almost always
	 * refers to cfg.cs, except for when local color scheme is loaded. */
	col_scheme_t *cs;
	/* Name of the color scheme loaded from vifminfo. */
	char color_scheme[NAME_MAX + 1];

	int msg_head, msg_tail;
	char *msgs[51];
	int save_msg_in_list;
	int allow_sb_msg_truncation; /* Whether truncation can be performed. */

	int scroll_bind_off;
	SPLIT split;
	/* Splitter position relative to viewport, negative values mean "centred".
	 * Handling it as a special case prevents drifting from center on resizes due
	 * to rounding. */
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

	int initial_lines;   /* Initial terminal height in lines. */
	int initial_columns; /* Initial terminal width in characters. */

	const char *ellipsis; /* String representation of ellipsis. */

	ShellType shell_type; /* Specifies type of shell. */

	const char *fuse_umount_cmd; /* Command to use for fuse unmounting. */

	FILE *original_stdout; /* Saved original standard output. */

	char *chosen_files_out; /* Destination for writing chosen files. */
	char *chosen_dir_out;   /* Destination for writing chosen directory. */
	char *output_delimiter; /* Delimiter for writing out list of paths. */

	char *on_choose; /* Command to execute on picking files. */

	void *preview_hint; /* Hint on which view is used for preview. */

	int global_local_settings; /* Set local settings globally. */

	int silent_ui; /* Whether UI updates should be "paused". */

	int history_size;   /* Number of elements in histories. */
	hist_t cmd_hist;    /* History of command-line commands. */
	hist_t search_hist; /* History of search patterns. */
	hist_t prompt_hist; /* History of prompt input. */
	hist_t filter_hist; /* History of local filter patterns. */

	struct ipc_t *ipc; /* Handle for IPC mechanism. */

#ifdef HAVE_LIBGTK
	int gtk_available; /* for mimetype detection */
#endif
}
status_t;

extern status_t curr_stats;

/* Initializes curr_stats from the configuration.  Returns non-zero on error,
 * otherwise zero is returned. */
int stats_init(struct config_t *config);

/* Resets some part of runtime status information to its initial values.
 * Returns non-zero on error. */
int stats_reset(const struct config_t *config);

/* Sets internal flag to schedule postponed redraw operation of the UI. */
void stats_redraw_schedule(void);

/* Checks for postponed redraw operations of the UI.  Has side effects.  Returns
 * non-zero if redraw operation was scheduled and resets internal flag. */
int stats_redraw_fetch(void);

/* Updates curr_stats to reflect whether terminal multiplexers support is
 * enabled. */
void stats_set_use_multiplexer(int use_term_multiplexer);

/* Updates curr_stats.shell_type field according to passed shell command. */
void stats_update_shell_type(const char shell_cmd[]);

/* Updates curr_stats.term_state field according to specified terminal
 * dimensions.  Returns new state. */
TermState stats_update_term_state(int screen_x, int screen_y);

/* Sets output location (curr_stats.chosen_files_out) for list of chosen
 * files. */
void stats_set_chosen_files_out(const char output[]);

/* Sets output location (curr_stats.chosen_dir_out) for last visited
 * directory. */
void stats_set_chosen_dir_out(const char output[]);

/* Sets delimiter (curr_stats.output_delimiter) for separating multiple paths in
 * output. */
void stats_set_output_delimiter(const char delimiter[]);

/* Sets command to run on file selection right before exiting
 * exit (curr_stats.on_choose). */
void stats_set_on_choose(const char command[]);

/* Checks whether custom actions on file choosing is set.  Returns non-zero if
 * so, otherwise zero is returned. */
int stats_file_choose_action_set(void);

/* Records status bar message. */
void stats_save_msg(const char msg[]);

/* Updates curr_stats.preview.on field and performs necessary updates in other
 * parts of the application. */
void stats_set_quickview(int on);

/* Non-zero argument makes UI more silent, zero argument makes it less silent.
 * After calls with non-zero and zero arguments balance out, UI gets updated. */
void stats_silence_ui(int more);

/* Managing histories. */

/* Changes size of histories stored in status_t.  Zero or negative value
 * disables them. */
void hists_resize(int new_size);

/* Saves command to command history. */
void hists_commands_save(const char command[]);

/* Saves pattern to search history. */
void hists_search_save(const char pattern[]);

/* Saves input to prompt history. */
void hists_prompt_save(const char input[]);

/* Saves input to local filter history. */
void hists_filter_save(const char pattern[]);

/* Gets the most recently used search pattern.  Returns the pattern or empty
 * string if search history is empty. */
const char * hists_search_last(void);

/* Caching of information about directories. */

/* Retrieves information about the path.  size and/or nitems can be NULL.  On
 * unknown values variables are set to DCACHE_UNKNOWN. */
void dcache_get_at(const char path[], uint64_t *size, uint64_t *nitems);

/* Retrieves information about the entry checking whether it's outdated. */
void dcache_get_of(const struct dir_entry_t *entry, dcache_result_t *size,
		dcache_result_t *nitems);

/* Updates cached sizes of parents by specified amount. */
void dcache_update_parent_sizes(const char path[], uint64_t by);

/* Updates information about the path.  Returns zero on success, otherwise
 * non-zero is returned. */
int dcache_set_at(const char path[], uint64_t size, uint64_t nitems);

#endif /* VIFM__STATUS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
