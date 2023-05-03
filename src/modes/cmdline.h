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

#include "../utils/test_helpers.h"

/* Submodes of command-line mode. */
typedef enum
{
	CLS_COMMAND,      /* Regular command-line command. */
	CLS_MENU_COMMAND, /* Menu command-line command. */
	CLS_MENU_FSEARCH, /* Forward search in menu mode. */
	CLS_MENU_BSEARCH, /* Backward search in menu mode. */
	CLS_FSEARCH,      /* Forward search in normal mode. */
	CLS_BSEARCH,      /* Backward search in normal mode. */
	CLS_VFSEARCH,     /* Forward search in visual mode. */
	CLS_VBSEARCH,     /* Backward search in visual mode. */
	CLS_VWFSEARCH,    /* Forward search in view mode. */
	CLS_VWBSEARCH,    /* Backward search in view mode. */
	CLS_FILTER,       /* Filter value. */
	CLS_PROMPT,       /* Input request. */
}
CmdLineSubmode;

struct menu_data_t;

/* Callback for prompt input.  Invoked with NULL on cancellation.  arg is user
 * supplied value, which is passed through. */
typedef void (*prompt_cb)(const char response[], void *arg);

/* Custom prompt line completion function.  arg is user supplied value, which is
 * passed through.  Should return completion offset. */
typedef int (*complete_cmd_func)(const char cmd[], void *arg);

/* Initializes command-line mode. */
void modcline_init(void);

/* Initializes navigation mode, which is nested into the command-line mode. */
void modnav_init(void);

/* Enters command-line editing mode with specified submode.  initial is the
 * start value. */
void modcline_enter(CmdLineSubmode sub_mode, const char initial[]);

/* Version of modcline_enter() specific to CLS_MENU_* submodes. */
void modcline_in_menu(CmdLineSubmode sub_mode, const char initial[],
		struct menu_data_t *m);

/* Enters command-line editing mode with prompt submode activated.  initial is
 * the start value, cb is callback called with the result on success and
 * with NULL on cancellation, complete is completion function (can be NULL),
 * allow_ee specifies whether issuing external editor is allowed. */
void modcline_prompt(const char prompt[], const char initial[], prompt_cb cb,
		void *cb_arg, complete_cmd_func complete, int allow_ee);

/* Redraws UI elements of the command-line mode. */
void modcline_redraw(void);

/* Completes paths ignoring files.  Returns completion offset. */
int modcline_complete_dirs(const char str[], void *arg);

/* Completes paths to both files and directories.  Returns completion offset. */
int modcline_complete_files(const char str[], void *arg);

#if defined(TEST) || defined(CMDLINE_IMPL)

#include <stddef.h> /* size_t wchar_t */

#include "../compat/fs_limits.h"
#include "../utils/hist.h"

/* History search mode. */
typedef enum
{
	HIST_NONE,   /* No search in history is active. */
	HIST_GO,     /* Retrieving items from history one by one. */
	HIST_SEARCH, /* Retrieving items that match entered prefix skipping others. */
}
HIST;

/* Describes possible states of a prompt used for interactive search. */
typedef enum
{
	PS_NORMAL,        /* Normal state (empty input or input is OK). */
	PS_WRONG_PATTERN, /* Pattern contains a mistake. */
	PS_NO_MATCH,      /* Pattern is OK, but no matches found. */
}
PromptState;

/* Holds state of the command-line editing mode. */
typedef struct
{
	/* Mode management. */

	/* Mode that entered command-line mode. */
	int prev_mode;
	/* Kind of command-line mode. */
	CmdLineSubmode sub_mode;
	/* Whether performing quick navigation. */
	int navigating;
	/* Whether current submode allows external editing. */
	int sub_mode_allows_ee;
	/* CLS_MENU_*-specific data. */
	struct menu_data_t *menu;
	/* CLS_PROMPT-specific data. */
	prompt_cb prompt_callback;
	void *prompt_callback_arg;

	/* Line editing state. */
	wchar_t *line;                /* The line reading. */
	wchar_t *last_line;           /* Previous contents of the line. */
	wchar_t *initial_line;        /* Initial state of the line. */
	int index;                    /* Index of the current character in cmdline. */
	int curs_pos;                 /* Position of the cursor in status bar. */
	int len;                      /* Length of the string. */
	int cmd_pos;                  /* Position in the history. */
	wchar_t prompt[NAME_MAX + 1]; /* Prompt message. */
	int prompt_wid;               /* Width of the prompt. */

	/* Dot completion. */
	int dot_pos;      /* History position or < 0 if it's not active. */
	size_t dot_index; /* Line index. */
	size_t dot_len;   /* Previous completion length. */

	/* Command completion. */
	size_t prefix_len;          /* Prefix length for the active completion. */
	int complete_continue;      /* If non-zero, continue previous completion. */
	int reverse_completion;     /* Completion in the opposite direction. */
	complete_cmd_func complete; /* Completion function. */

	/* History completion. */
	HIST history_search; /* One of the HIST_* constants. */
	int hist_search_len; /* Length of history search pattern. */
	wchar_t *line_buf;   /* Content of line before using history. */

	/* For search prompt. */
	int search_mode;        /* If it's a search prompt. */
	int search_match_found; /* Reflects interactive search success/failure. */
	int old_top;            /* Saved top for interactive searching. */
	int old_pos;            /* Saved position for interactive searching. */

	/* Other state. */
	int line_edited;         /* Cache for whether input line changed flag. */
	int enter_mapping_state; /* The mapping state at entering the mode. */
	int expanding_abbrev;    /* Abbreviation expansion is in progress. */
	PromptState state;       /* Prompt state with regard to current input. */
}
line_stats_t;

#endif

TSTATIC_DEFS(
	int line_completion(line_stats_t *stat);
	const wchar_t * extract_abbrev(line_stats_t *stat, int *pos, int *no_remap);
	void hist_prev(line_stats_t *stat, const hist_t *hist, size_t len);
	line_stats_t * get_line_stats(void);
)

#endif /* VIFM__MODES__CMDLINE_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
