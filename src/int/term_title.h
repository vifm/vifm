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

#ifndef VIFM__INT__TERM_TITLE_H__
#define VIFM__INT__TERM_TITLE_H__

#include "../utils/test_helpers.h"

/* Determines whether terminal title can be restored.  Returns non-zero if so,
 * otherwise zero is returned. */
int term_title_restorable(void);

/* Updates terminal title.  If title_part is NULL, resets terminal title. */
void term_title_update(const char title_part[]);

#ifdef TEST
/* Kind of title we're working with. */
typedef enum
{
	TK_ABSENT,  /* No title support. */
	TK_REGULAR, /* Normal for the platform (xterm for *nix). */
	TK_SCREEN,  /* GNU screen compatible. */
}
TitleKind;
#endif
TSTATIC_DEFS(
	TitleKind title_kind_for_termenv(const char term[]);
)

#endif /* VIFM__INT__TERM_TITLE_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
