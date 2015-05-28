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

#ifndef VIFM__IO__IOE_H__
#define VIFM__IO__IOE_H__

/* ioe - I/O error reporting - Input/Output error reporting */

enum
{
	/* Special code, which means that system error code is not available for a
	 * particular error entry. */
	IO_ERR_UNKNOWN
};

/* An error entry containing details about an issue. */
typedef struct
{
	/* File path that caused the error. */
	char *path;
	/* System error code, which may be IO_ERR_UNKNOWN if it is not available. */
	int error_code;
	/* Concise error message about what the problem. */
	char *msg;
}
io_err_t;

/* Possible results of the io_err_cb callback. */
typedef enum
{
	/* Failed operation should be restarted. */
	IO_ECR_RETRY,
	/* Operation should be skipped. */
	IO_ECR_SKIP,
	/* Whole action should be stopped (including all possible next operations). */
	IO_ECR_BREAK
}
IoErrCbResult;

/* Callback used to report information about errors occurred. */
typedef IoErrCbResult (*ioerr_cb)(const io_err_t *err);

/* List of errors. */
typedef struct
{
	/* Errors, can be NULL if error_count is zero. */
	io_err_t *errors;
	/* Number of accumulated error entries. */
	size_t error_count;
}
ioe_errlst_t;

/* Initializes empty error list. */
void ioe_errlst_init(ioe_errlst_t *lst);

/* Appends new error entry to the lst.  Returns zero on error, otherwise
 * non-zero is returned. */
int ioe_errlst_append(ioe_errlst_t *lst, const char path[], int error_code,
		const char msg[]);

/* Frees resources allocated by error list.  lst can be NULL. */
void ioe_errlst_free(ioe_errlst_t *lst);

#endif /* VIFM__IO__IOE_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
