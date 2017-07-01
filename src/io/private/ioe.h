/* vifm
 * Copyright (C) 2013 xaizek.
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

#ifndef VIFM__IO__PRIVATE__IOE_H__
#define VIFM__IO__PRIVATE__IOE_H__

#include "../ioe.h"

/* ioe - private functions of Input/Output error reporting */

/* Appends new error entry to the elist.  Either error_code shouldn't be equal
 * to IO_ERR_UNKNOWN or msg shouldn't be empty.  Returns non-zero on error,
 * otherwise zero is returned. */
int ioe_errlst_append(ioe_errlst_t *elist, const char path[], int error_code,
		const char msg[]);

/* Appends all errors of *other to *elist.  other still needs to be freed.  On
 * some error, lists are just left where they were. */
void ioe_errlst_splice(ioe_errlst_t *elist, ioe_errlst_t *other);

/* Frees single error.  err can't be NULL. */
void ioe_err_free(ioe_err_t *err);

#endif /* VIFM__IO__PRIVATE__IOETA_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
