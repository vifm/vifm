/* vifm
 * Copyright (C) 2014 xaizek.
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

#ifndef VIFM__IO__PRIVATE__IONOTIF_H__
#define VIFM__IO__PRIVATE__IONOTIF_H__

#include "../ioeta.h"
#include "../ionotif.h"

/* ionotif - private functions of client code callbacks management */

/* Invokes progress changed callback previously registered by
 * ionotif_register(). */
void ionotif_notify(IoPs stage, ioeta_estim_t *estim);

#endif /* VIFM__IO__PRIVATE__IONOTIF_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
