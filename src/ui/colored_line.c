/* vifm
 * Copyright (C) 2020 xaizek.
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

#include "colored_line.h"

#include <stddef.h> /* size_t */
#include <stdlib.h> /* free() */
#include <string.h> /* memmove() strdup() */

#include "../cfg/config.h"
#include "../utils/str.h"
#include "../utils/utf8.h"
#include "color_scheme.h"
#include "ui.h"

static size_t effective_chrw(const char line[]);

cline_t
cline_make(void)
{
	cline_t result = { .line = strdup(""), .attrs = strdup("") };
	return result;
}

int
cline_sync(cline_t *cline, int extra_width)
{
	const size_t nchars = utf8_strsw(cline->line) + extra_width;
	if(cline->attrs_len < nchars)
	{
		int width = nchars - cline->attrs_len;
		char *const new_attrs = format_str("%s%*s", cline->attrs, width, "");
		free(cline->attrs);
		cline->attrs = new_attrs;
		cline->attrs_len = nchars;
	}
	return (cline->attrs_len > nchars);
}

void
cline_set_attr(cline_t *cline, char attr)
{
	(void)cline_sync(cline, 1);
	cline->attrs[cline->attrs_len - 1] = attr;
}

void
cline_clear(cline_t *cline)
{
	cline->line[0] = '\0';
	cline->line_len = 0;
	cline->attrs[0] = '\0';
	cline->attrs_len = 0;
}

void
cline_finish(cline_t *cline)
{
	if(cline_sync(cline, 0))
	{
		cline->attrs[--cline->attrs_len] = '\0';
	}
}

void
cline_splice_attrs(cline_t *cline, cline_t *admixture)
{
	char *attrs = admixture->attrs;
	if(cline_sync(cline, 0) && admixture->attrs_len > 0U)
	{
		if(*attrs != ' ')
		{
			cline->attrs[cline->attrs_len - 1U] = *attrs;
		}
		++attrs;
	}
	strappend(&cline->attrs, &cline->attrs_len, attrs);
	free(admixture->attrs);
}

void
cline_print(const cline_t *cline, WINDOW *win, const col_attr_t *def_col)
{
	const cchar_t def_attr = cs_color_to_cchar(def_col, -1);

	const char *line = cline->line;
	const char *attrs = cline->attrs;

	cchar_t attr = def_attr;
	while(*line != '\0')
	{
		if(*attrs == '0')
		{
			attr = def_attr;
		}
		else if(*attrs != ' ')
		{
			const int color = (USER1_COLOR + (*attrs - '1'));
			col_attr_t col = *def_col;
			cs_mix_colors(&col, &cfg.cs.color[color]);
			attr = cs_color_to_cchar(&col, -1);
		}

		const size_t len = effective_chrw(line);
		char char_buf[len + 1];
		copy_str(char_buf, sizeof(char_buf), line);
		wprinta(win, char_buf, &attr, 0);

		line += len;
		attrs += utf8_chrsw(char_buf);
	}
}

/* Computes size of the leading UTF-8 character in bytes including all
 * zero-width characters that follow it (like for umlauts).  Returns byte
 * count. */
static size_t
effective_chrw(const char line[])
{
	size_t effective = utf8_chrw(line);

	while(line[effective] != '\0' && utf8_chrsw(line + effective) == 0)
	{
		effective += utf8_chrw(line + effective);
	}

	return effective;
}

void
cline_left_ellipsis(cline_t *cline, size_t max_width, const char ell[])
{
	if(max_width == 0U)
	{
		/* No room to print anything. */
		cline_clear(cline);
		return;
	}

	size_t width = utf8_strsw(cline->line);
	if(width <= max_width)
	{
		/* No need to change the string. */
		return;
	}

	size_t ell_width = utf8_strsw(ell);
	if(max_width <= ell_width)
	{
		free(cline->line);
		/* Insert as many characters of ellipsis as we can. */
		const int prefix = (int)utf8_nstrsnlen(ell, max_width);
		cline->line = format_str("%.*s", prefix, ell);
		cline->line_len = prefix;
		cline->attrs[0] = '\0';
		cline->attrs_len = 0;
		cline_finish(cline);
		return;
	}

	char *line = cline->line;
	char *attrs = cline->attrs;

	while(width > max_width - ell_width)
	{
		int cw = utf8_chrsw(line);
		width -= cw;
		line += utf8_chrw(line);
		attrs += cw;
	}

	cline->line_len = cline->line_len - (line - cline->line);
	memmove(cline->line, line, cline->line_len + 1);
	cline->attrs_len = cline->attrs_len - (attrs - cline->attrs);
	memmove(cline->attrs, attrs, cline->attrs_len + 1);

	strprepend(&cline->line, &cline->line_len, ell);
	char spaces[ell_width + 1];
	memset(spaces, ' ', sizeof(spaces) - 1);
	spaces[ell_width] = '\0';
	strprepend(&cline->attrs, &cline->attrs_len, spaces);
}

void
cline_dispose(cline_t *cline)
{
	update_string(&cline->line, NULL);
	cline->line_len = 0;
	update_string(&cline->attrs, NULL);
	cline->attrs_len = 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
