/* vifm
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

#ifndef VIFM__OPT_HANDLERS_H__
#define VIFM__OPT_HANDLERS_H__

#include "ui.h"

enum
{
	VIFMINFO_OPTIONS   = 1 << 0,
	VIFMINFO_FILETYPES = 1 << 1,
	VIFMINFO_COMMANDS  = 1 << 2,
	VIFMINFO_BOOKMARKS = 1 << 3,
	VIFMINFO_TUI       = 1 << 4,
	VIFMINFO_DHISTORY  = 1 << 5,
	VIFMINFO_STATE     = 1 << 6,
	VIFMINFO_CS        = 1 << 7,
	VIFMINFO_SAVEDIRS  = 1 << 8,
	VIFMINFO_CHISTORY  = 1 << 9,
	VIFMINFO_SHISTORY  = 1 << 10,
	VIFMINFO_DIRSTACK  = 1 << 11,
	VIFMINFO_REGISTERS = 1 << 12,
	VIFMINFO_PHISTORY  = 1 << 13,
	VIFMINFO_FHISTORY  = 1 << 14,
};

const char * cursorline_enum[3];

void init_option_handlers(void);
void load_local_options(FileView *view);
int process_set_args(const char *args);
void load_sort_option(FileView *view);
/* Updates view columns value as if 'viewcolumns' option has been changed.
 * Doesn't change actual value of the option. */
void load_view_columns_option(FileView *view, const char value[]);
/* Updates geometry related options. */
void load_geometry(void);
/* Returns pointer to a statically allocated string containing string
 * representation of the 'classify' option value. */
const char * classify_to_str(void);

#endif /* VIFM__OPT_HANDLERS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
