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

#ifndef VIFM__IO__PRIVATE__IOETA_H__
#define VIFM__IO__PRIVATE__IOETA_H__

#include <stddef.h> /* size_t */
#include <stdint.h> /* uint64_t */

/* ioeta - private functions of Input/Output estimation */

void ioeta_add_file(eta_estimate_t *estimate, const char path[]);

void ioeta_add_dir(eta_estimate_t *estimate, const char path[]);

/* update_estimate(e, 0, 100); -- One hundred bytes of current item processed. */
/* update_estimate(e, 1, 50); -- Last fifteen bytes of current item processed. */
/* Might calculate speed, time, etc. */
/* Calls progress_changed() handler. */
void ioeta_update_estimate(eta_estimate_t *estimate, int finished, int bytes);

#endif /* VIFM__IO__PRIVATE__IOETA_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
