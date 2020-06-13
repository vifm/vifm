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

#ifndef VIFM__MODES__NORMAL_H__
#define VIFM__MODES__NORMAL_H__

#include "../engine/keys.h"

struct view_t;

/* Initializes normal mode. */
void modnorm_init(void);

/* Changes file attributes (permissions or properties). */
void modnorm_cp(struct view_t *view, key_info_t key_info);

/* Redraws with file in bottom of list. */
void modnorm_zb(key_info_t key_info, keys_info_t *keys_info);

/* Redraws with file in top of list. */
void modnorm_zt(key_info_t key_info, keys_info_t *keys_info);

/* Redraws with file in center of list. */
void modnorm_zz(key_info_t key_info, keys_info_t *keys_info);

/* Centers the splitter. */
void modnorm_ctrl_wequal(key_info_t key_info, keys_info_t *keys_info);

void modnorm_ctrl_wless(key_info_t key_info, keys_info_t *keys_info);

void modnorm_ctrl_wgreater(key_info_t key_info, keys_info_t *keys_info);

void modnorm_ctrl_wplus(key_info_t key_info, keys_info_t *keys_info);

void modnorm_ctrl_wminus(key_info_t key_info, keys_info_t *keys_info);

void modnorm_ctrl_wpipe(key_info_t key_info, keys_info_t *keys_info);

/* Kind of callback to allow starting searches from the module and rely on other
 * modules.  Returns new value for status bar message flag, but when
 * print_errors isn't requested can return -1 to indicate issues with the
 * pattern. */
int modnorm_find(struct view_t *view, const char pattern[], int backward,
		int print_errors);

#endif /* VIFM__MODES__NORMAL_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
