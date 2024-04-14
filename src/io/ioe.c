/* vifm
 * Copyright (C) 2015 xaizek.
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

#include "ioe.h"

#include <stddef.h> /* NULL size_t */
#include <stdlib.h> /* free() */
#include <string.h> /* strerror() */

#include "../engine/text_buffer.h"
#include "../utils/path.h"
#include "private/ioe.h"

static void print_err_to_buf(const ioe_err_t *err, vle_textbuf *str);

void
ioe_errlst_init(ioe_errlst_t *elist)
{
	static const ioe_errlst_t initializer = IOE_ERRLST_INIT;
	*elist = initializer;
}

void
ioe_errlst_free(ioe_errlst_t *elist)
{
	size_t i;

	if(elist == NULL)
	{
		return;
	}

	for(i = 0U; i < elist->error_count; ++i)
	{
		ioe_err_free(&elist->errors[i]);
	}
	free(elist->errors);
}

char *
ioe_errlst_to_str(const ioe_errlst_t *elist)
{
	size_t i;

	vle_textbuf *str = vle_tb_create();
	if(str == NULL)
	{
		return NULL;
	}

	for(i = 0U; i < elist->error_count; ++i)
	{
		if(i > 0U)
		{
			vle_tb_append(str, "\n");
		}

		print_err_to_buf(&elist->errors[i], str);
	}

	return vle_tb_release(str);
}

char *
ioe_err_to_str(const ioe_err_t *err)
{
	vle_textbuf *str = vle_tb_create();
	if(str == NULL)
	{
		return NULL;
	}

	print_err_to_buf(err, str);
	return vle_tb_release(str);
}

/* Formats error into a buffer without adding newline. */
static void
print_err_to_buf(const ioe_err_t *err, vle_textbuf *str)
{
	const char *path = err->path;

	char *tilde_path = make_tilde_path(path);
	if(tilde_path != NULL)
	{
		path = tilde_path;
	}

	if(err->error_code == IO_ERR_UNKNOWN)
	{
		vle_tb_appendf(str, "%s: %s", path, err->msg);
	}
	else if(err->msg[0] == '\0')
	{
		vle_tb_appendf(str, "%s: %s", path, strerror(err->error_code));
	}
	else
	{
		vle_tb_appendf(str, "%s: %s (%s)", path, err->msg,
				strerror(err->error_code));
	}

	free(tilde_path);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
