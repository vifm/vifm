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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifndef __NORMAL_H__
#define __NORMAL_H__

#include "keys.h"

void init_normal_mode(int *key_mode);
void normal_cmd_zb(struct key_info, struct keys_info *);
void normal_cmd_zt(struct key_info, struct keys_info *);
void normal_cmd_zz(struct key_info, struct keys_info *);
void normal_cmd_ctrl_wequal(struct key_info, struct keys_info *);
void normal_cmd_ctrl_wless(struct key_info, struct keys_info *);
void normal_cmd_ctrl_wgreater(struct key_info, struct keys_info *);
void normal_cmd_ctrl_wplus(struct key_info, struct keys_info *);
void normal_cmd_ctrl_wminus(struct key_info, struct keys_info *);
void normal_cmd_ctrl_wpipe(struct key_info, struct keys_info *);
int ffind(int ch, int backward, int wrap);
int cmd_paren(int lb, int ub, int inc);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
