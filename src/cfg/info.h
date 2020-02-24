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

#ifndef VIFM__CFG__INFO_H__
#define VIFM__CFG__INFO_H__

#include "../utils/test_helpers.h"

/* Reads vifminfo file populating internal structures with information it
 * contains.  Reread should be set to non-zero value when vifminfo is read not
 * during startup process. */
void read_info_file(int reread);

/* Writes vifminfo file updating it with state of the current instance. */
void write_info_file(void);

#ifdef TEST
#include "../utils/parson.h"
#endif
TSTATIC_DEFS(
	JSON_Value * serialize_state(void);
)

#endif /* VIFM__CFG__INFO_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
