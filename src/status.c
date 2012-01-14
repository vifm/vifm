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

#include <limits.h>
#include <string.h>

#include "color_scheme.h"
#include "config.h"

#include "status.h"

status_t curr_stats;

void
init_status(void)
{
	curr_stats.need_redraw = 0;
	curr_stats.last_char = 0;
	curr_stats.is_console = 0;
	curr_stats.search = 0;
	curr_stats.save_msg = 0;
	curr_stats.use_register = 0;
	curr_stats.curr_register = -1;
	curr_stats.register_saved = 0;
	curr_stats.show_full = 0;
	curr_stats.view = 0;
	curr_stats.use_input_bar = 1;
	curr_stats.errmsg_shown = 0;
	curr_stats.load_stage = 0;
	curr_stats.too_small_term = 0;
	curr_stats.dirsize_cache = 0;
	curr_stats.ch_pos = 1;
	curr_stats.confirmed = 0;
	curr_stats.auto_redraws = 0;
	curr_stats.pending_redraw = 0;
	curr_stats.cs_base = DCOLOR_BASE;
	curr_stats.cs = &cfg.cs;
	strcpy(curr_stats.color_scheme, "");

#ifdef HAVE_LIBGTK
	curr_stats.gtk_available = 0;
#endif

	curr_stats.msg_head = 0;
	curr_stats.msg_tail = 0;
	curr_stats.save_msg_in_list = 1;

#ifdef _WIN32
	curr_stats.as_admin = 0;
#endif

	curr_stats.scroll_bind_off = 0;
	curr_stats.split = VSPLIT;
	curr_stats.splitter_pos = -1.0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
