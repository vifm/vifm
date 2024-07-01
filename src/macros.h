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

#ifndef VIFM__MACROS_H__
#define VIFM__MACROS_H__

#include <stddef.h> /* size_t */

#include "utils/test_helpers.h"

/* Kind of a macro.  Each kind corresponds to some %-sequence disregarding
 * quoting (e.g., both %c and %"c map to MK_c). */
typedef enum
{
	MK_NONE,    /* Signifies absence of a macro. */
	MK_INVALID, /* Signifies an unrecognized macro that needs to be skipped. */
	MK_PERCENT, /* A literal percent sign. */

	/*
	 * Argument-less macros.
	 */

	MK_a, /* User arguments. */
	MK_b, /* Selected files of both views. */

	/* Pairs of macros for active/inactive views. */
	MK_c, /* Current view's file under the cursor. */
	MK_C, /* Other view's file under the cursor. */
	MK_f, /* Current view's selected files. */
	MK_F, /* Other view's selected files. */
	MK_l, /* Current view's selected files or nothing if no selection. */
	MK_L, /* Other view's selected files or nothing if no selection. */
	MK_d, /* Current view's path. */
	MK_D, /* Other view's path. */

	/* Flags (don't expand to anything). */
	MK_n, /* Forbid using of terminal multiplexer, even if active. */
	MK_N, /* Do not run the command in a separate terminal session or process
	         group. */
	MK_m, /* Build a menu out of command's output. */
	MK_M, /* Build a menu with navigation out of command's output. */
	MK_S, /* Show command' output in the status bar. */
	MK_q, /* Show command' output in the preview. */
	MK_s, /* Execute command in a new horizontal split. */
	MK_v, /* Execute command in a new vertical split. */
	MK_u, /* Parse output as list of files and compose custom view. */
	MK_U, /* Parse output as list of files and compose unsorted view. */
	MK_i, /* Ignore output. */

	/*
	 * Single-argument macros.
	 */
	MK_r, /* Registers' content. */

	/*
	 * Grouped macros, all argument-less.
	 */

	/* Interactive custom view flags. */
	MK_Iu, /* Interactive sorted custom view. */
	MK_IU, /* Interactive unsorted custom view. */

	/* File list piping flags. */
	MK_Pl, /* Pipe in newline-separated file list. */
	MK_Pz, /* Pipe in NUL-separated file list. */

	/* Preview-related. */
	MK_pc, /* End of the preview command, start of clear command. */
	MK_pd, /* Pass-through (direct) preview. */
	MK_pu, /* Do not cache preview result. */
	MK_ph, /* Height of preview area. */
	MK_pw, /* Width of preview area. */
	MK_px, /* X coordinate of the top-left corner of preview area. */
	MK_py, /* Y coordinate of the top-left corner of preview area. */
}
MacroKind;

/* Macros that affect running of commands and processing their output. */
typedef enum
{
	MF_NONE, /* No special macro specified. */

	/* First set of mutually exclusive flags. */
	MF_FIRST_SET_ = 0x01,

	MF_MENU_OUTPUT,      /* Redirect output to the menu. */
	MF_MENU_NAV_OUTPUT,  /* Redirect output to the navigation menu. */
	MF_STATUSBAR_OUTPUT, /* Redirect output to the status bar. */
	MF_PREVIEW_OUTPUT,   /* Redirect output to the preview. */

	/* Redirect output into custom view. */
	MF_CUSTOMVIEW_OUTPUT,     /* Applying current sorting. */
	MF_VERYCUSTOMVIEW_OUTPUT, /* Not changing ordering. */

	/* Redirect output from interactive application into custom view. */
	MF_CUSTOMVIEW_IOUTPUT,     /* Applying current sorting. */
	MF_VERYCUSTOMVIEW_IOUTPUT, /* Not changing ordering. */

	MF_SPLIT,       /* Run command in a new horizontally split screen region. */
	MF_SPLIT_VERT,  /* Run command in a new vertically split screen region. */
	MF_IGNORE,      /* Completely ignore command output. */
	MF_NO_TERM_MUX, /* Forbid using terminal multiplexer, even if active. */

	/* Second set of mutually exclusive flags. */
	MF_SECOND_SET_ = 0x10,

	MF_PIPE_FILE_LIST   = 0x20, /* Provide \n-separated file list on stdin. */
	MF_PIPE_FILE_LIST_Z = 0x30, /* Provide \0-separated file list on stdin. */

	/* Third set of mutually exclusive flags. */
	MF_THIRD_SET_ = 0x100,

	MF_NO_CACHE = 0x200, /* Inhibit caching of preview results. */

	/* Fourth set of mutually exclusive flags. */
	MF_FOURTH_SET_ = 0x1000,

	/* Don't detach command from terminal session or process group.  In separate
	 * group so it can safely appear among other flags without disabling them. */
	MF_KEEP_IN_FG = 0x2000,
}
MacroFlags;

/* Greater context in which macro expansion takes place.  External command
 * context imply escaping specific to configured shell and current OS.
 * Operation is anything that uses marking possibly derived from selection,
 * non-operations process selection or current file. */
typedef enum
{
	MER_OP,       /* Paths are collected for an internal operation. */
	MER_SHELL_OP, /* Paths are collected for an operation via external command. */
	MER_SHELL,    /* Paths are collected for an external command which isn't part
	                 of a file-processing operation. */
	MER_DISPLAY,  /* Paths are collected for other non-shell purposes. */
}
MacroExpandReason;

/* Named boolean values of "with_opt" parameter for better readability. */
enum
{
	MA_NOOPT, /* Do not parse %[ and %] macros. */
	MA_OPT    /* Parse %[ and %] keeping their insides only with non-empty
	             expansions. */
};

/* Description of a macro for the ma_expand_custom() function. */
typedef struct
{
	char letter;        /* Macro identifier in the pattern. */
	const char *value;  /* A value to replace macro with. */
	int uses_left;      /* Number of mandatory uses of the macro for group
	                       head. */
	int group;          /* Index of macro group head or -1. */
	int explicit_use;   /* Set to non-zero on explicit expansion. */
	int expand_mods;    /* Process file modifiers for this macro. */
	const char *parent; /* Parent directory for expanding modifiers (must be an
	                       absolute paths when expand_mods is non-zero). */
	int flag;           /* Value is ignored, but whether it's empty is noted. */
}
custom_macro_t;

/* Performs substitution of macros with their values.  args and flags
 * parameters can be NULL.  The string returned needs to be freed by the
 * caller.  After executing flags is one of MF_* values.  On error NULL is
 * returned. */
char * ma_expand(const char command[], const char args[], MacroFlags *flags,
		MacroExpandReason reason);

/* Like ma_expand(), but expands only single element macros and aims for
 * single string, so escaping is disabled. */
char * ma_expand_single(const char command[]);

/* Gets clear part of the viewer.  Returns NULL if there is none, otherwise
 * pointer inside the cmd string is returned. */
const char * ma_get_clear_cmd(const char cmd[]);

/* Checks that the command contains at least one macro out of up to 2 specified
 * kinds (MK_NONE can be passed in to much anything).  Returns non-zero if
 * so. */
int ma_contains2(const char cmd[], MacroKind kind1, MacroKind kind2);

/* Expands macros of form %x in the pattern (%% is expanded to %) according to
 * macros specification.  Optionally handles %[opt%] nested expansions.  Returns
 * expanded string. */
char * ma_expand_custom(const char pattern[], size_t nmacros,
		custom_macro_t macros[], int with_opt);

/* Same as ma_expand_custom() but supports %x* macros for specifying color
 * groups.  Returns colored line. */
struct cline_t ma_expand_colored_custom(const char pattern[], size_t nmacros,
		custom_macro_t macros[], int with_opt);

/* Sets a flag in *flags, if it isn't NULL. */
void ma_flags_set(MacroFlags *flags, MacroFlags flag);

/* Checks whether flag is set.  Returns non-zero if so, otherwise zero is
 * returned. */
int ma_flags_present(MacroFlags flags, MacroFlags flag);

/* Checks whether flag is not set.  Returns non-zero if so, otherwise zero is
 * returned. */
int ma_flags_missing(MacroFlags flags, MacroFlags flag);

/* Maps flag to corresponding string representation of the macro using %-syntax.
 * Returns the string representation. */
const char * ma_flags_to_str(MacroFlags flags);

TSTATIC_DEFS(
	struct dir_entry_t;
	struct view_t;
	typedef int (*iter_func)(struct view_t *view, struct dir_entry_t **entry);
	char * append_selected_files(struct view_t *view, char expanded[],
		int under_cursor, int quotes, const char mod[], iter_func iter,
		int for_shell);
)

#endif /* VIFM__MACROS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
