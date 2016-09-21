/* vifm
 * Copyright (C) 2016 xaizek.
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

#ifndef VIFM__IO__PRIVATE__IOC_H__
#define VIFM__IO__PRIVATE__IOC_H__

#include "../ioc.h"

/* Convenience function that checks whether given I/O operation was cancelled
 * (uses hook provided in operation arguments).  Returns non-zero if so,
 * otherwise zero is returned. */
int io_cancelled(const io_args_t *args);

/* Checks whether operation was cancelled via cancellation hook.  Returns
 * non-zero if so, otherwise zero is returned.  */
int cancelled(const io_cancellation_t *info);

#endif /* VIFM__IO__PRIVATE__IOC_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
