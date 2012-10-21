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

#ifndef __MACROS_H__
#define __MACROS_H__

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

/* args and flags parameters can equal NULL. The string returned needs to be
 * freed in the calling function. After executing flags is one of MACRO_*
 * values. */
char * expand_macros(FileView *view, const char *command, const char *args,
		MacroFlags *flags);

#ifdef TEST
#include "engine/cmds.h"
#endif

TSTATIC_DEFS(
	char * append_selected_files(FileView *view, char *expanded, int under_cursor,
			int quotes, const char *mod);
)

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
