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

#ifdef HAVE_LIBGTK
#include <gio/gio.h>
#include <gtk/gtk.h>
#undef MAX
#undef MIN
#endif

#include <limits.h>
#include <string.h>

#include "cfg/config.h"
#include "utils/env.h"
#include "utils/macros.h"
#include "utils/str.h"
#include "utils/tree.h"
#include "color_scheme.h"

#include "status.h"

static void load_def_values(status_t *stats);
static void set_gtk_available(status_t *stats);
static void set_number_of_windows(status_t *stats);
static void set_env_type(status_t *stats);
static int reset_dircache(status_t *stats);

status_t curr_stats;

static int pending_redraw;

int
init_status(void)
{
	load_def_values(&curr_stats);
	set_gtk_available(&curr_stats);
	set_number_of_windows(&curr_stats);
	set_env_type(&curr_stats);

	return reset_status();
}

static void
load_def_values(status_t *stats)
{
	pending_redraw = 0;

	stats->need_update = UT_NONE;
	stats->last_char = 0;
	stats->search = 0;
	stats->save_msg = 0;
	stats->use_register = 0;
	stats->curr_register = -1;
	stats->register_saved = 0;
	stats->view = 0;
	stats->use_input_bar = 1;
	stats->errmsg_shown = 0;
	stats->load_stage = 0;
	stats->too_small_term = 0;
	stats->dirsize_cache = NULL_TREE;
	stats->ch_pos = 1;
	stats->confirmed = 0;
	stats->auto_redraws = 0;
	stats->cs_base = DCOLOR_BASE;
	stats->cs = &cfg.cs;
	strcpy(stats->color_scheme, "");

#ifdef HAVE_LIBGTK
	stats->gtk_available = 0;
#endif

	stats->msg_head = 0;
	stats->msg_tail = 0;
	stats->save_msg_in_list = 1;

#ifdef _WIN32
	stats->as_admin = 0;
#endif

	stats->scroll_bind_off = 0;
	stats->split = VSPLIT;
	stats->splitter_pos = -1.0;

	stats->sourcing_state = SOURCING_NONE;

	stats->restart_in_progress = 0;

	stats->env_type = ENVTYPE_EMULATOR;
}

static void
set_gtk_available(status_t *stats)
{
#ifdef HAVE_LIBGTK
	char *argv[] = { "vifm", NULL };
	int argc = ARRAY_LEN(argv) - 1;
	char **ptr = argv;
	curr_stats.gtk_available = gtk_init_check(&argc, &ptr);
#endif
}

static void
set_number_of_windows(status_t *stats)
{
	if(cfg.show_one_window)
		curr_stats.number_of_windows = 1;
	else
		curr_stats.number_of_windows = 2;
}

/* Checks if running in X, terminal emulator or linux native console. */
static void
set_env_type(status_t *stats)
{
#ifndef _WIN32
	const char *term = env_get("TERM");
	if(term != NULL && ends_with(term, "linux"))
	{
		curr_stats.env_type = ENVTYPE_LINUX_NATIVE;
	}
	else
	{
		const char *display = env_get("DISPLAY");
		curr_stats.env_type = is_null_or_empty(display) ?
			ENVTYPE_EMULATOR : ENVTYPE_EMULATOR_WITH_X;
	}
#else
	curr_stats.env_type = ENVTYPE_EMULATOR_WITH_X;
#endif
}

int
reset_status(void)
{
	return reset_dircache(&curr_stats);
}

/* Returns non-zero on error. */
static int
reset_dircache(status_t *stats)
{
	tree_free(stats->dirsize_cache);
	stats->dirsize_cache = tree_create(0, 0);
	return stats->dirsize_cache == NULL_TREE;
}

void
schedule_redraw(void)
{
	pending_redraw = 1;
}

int
is_redraw_scheduled(void)
{
	if(pending_redraw)
	{
		pending_redraw = 0;
		return 1;
	}
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
