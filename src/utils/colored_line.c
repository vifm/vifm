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

#include "../cfg/config.h"
#include "../ui/color_manager.h"
#include "../ui/color_scheme.h"
#include "../ui/ui.h"
#include "str.h"
#include "utf8.h"

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
	cchar_t def_attr;
	setcchar(&def_attr, L" ", def_col->attr,
			colmgr_get_pair(def_col->fg, def_col->bg), NULL);

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
			setcchar(&attr, L" ", col.attr, colmgr_get_pair(col.fg, col.bg), NULL);
		}

		const size_t len = utf8_chrw(line);
		char char_buf[len + 1];
		copy_str(char_buf, sizeof(char_buf), line);
		wprinta(win, char_buf, &attr, 0);

		line += len;
		attrs += utf8_chrsw(char_buf);
	}
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
