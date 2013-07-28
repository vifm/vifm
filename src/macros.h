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

#include "utils/test_helpers.h"
#include "ui.h"

/* Macros that affect running of commands and processing their output. */
typedef enum
{
	MACRO_NONE, /* no special macro specified */
	MACRO_MENU_OUTPUT, /* redirect output to the menu */
	MACRO_MENU_NAV_OUTPUT, /* redirect output to the navigation menu */
	MACRO_STATUSBAR_OUTPUT, /* redirect output to the status bar */
	MACRO_SPLIT, /* run command in a new screen region */
	MACRO_IGNORE, /* completely ignore command output */
}
MacroFlags;

/* Description of a macro for the expand_custom_macros() function. */
typedef struct
{
	char letter;       /* Macro identifier in the pattern. */
	const char *value; /* A value to replace macro with. */
	int uses_left;     /* Number of mandatory uses of the macro. */
}
custom_macro_t;

/* args and flags parameters can equal NULL. The string returned needs to be
 * freed in the calling function. After executing flags is one of MACRO_*
 * values. */
char * expand_macros(const char *command, const char *args, MacroFlags *flags,
		int for_shell);

/* Expands macros of form %x in the pattern (%% is expanded to %) according to
 * macros specification. */
char * expand_custom_macros(const char pattern[], size_t nmacros,
		custom_macro_t macros[]);

#ifdef TEST
#include "engine/cmds.h"
#endif

TSTATIC_DEFS(
	char * append_selected_files(FileView *view, char *expanded, int under_cursor,
			int quotes, const char *mod);
)

#endif /* VIFM__MACROS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
