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

#include "status.h"

#ifdef HAVE_LIBGTK
#include <gio/gio.h>
#include <gtk/gtk.h>
#undef MAX
#undef MIN
#endif

#include <assert.h> /* assert() */
#include <limits.h> /* INT_MIN */
#include <string.h>

#include "cfg/config.h"
#include "utils/env.h"
#include "utils/fs_limits.h"
#include "utils/log.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/tree.h"
#include "utils/utils.h"
#include "colors.h"
#include "commands_completion.h"

/* Environment variables by which application hosted by terminal multiplexer can
 * identify the host. */
#define SCREEN_ENVVAR "STY"
#define TMUX_ENVVAR "TMUX"

static void load_def_values(status_t *stats, config_t *config);
static void determine_fuse_umount_cmd(status_t *stats);
static void set_gtk_available(status_t *stats);
static int reset_dircache(status_t *stats);
static void set_last_cmdline_command(const char cmd[]);

status_t curr_stats;

static int pending_redraw;
static int inside_screen;
static int inside_tmux;

int
init_status(config_t *config)
{
	inside_screen = !is_null_or_empty(env_get(SCREEN_ENVVAR));
	inside_tmux = !is_null_or_empty(env_get(TMUX_ENVVAR));

	load_def_values(&curr_stats, config);
	determine_fuse_umount_cmd(&curr_stats);
	set_gtk_available(&curr_stats);
	curr_stats.exec_env_type = get_exec_env_type();
	stats_update_shell_type(config->shell);

	return reset_status(config);
}

/* Initializes most fields of the status structure, some are left to be
 * initialized by the reset_status() function. */
static void
load_def_values(status_t *stats, config_t *config)
{
	pending_redraw = 0;

	stats->need_update = UT_NONE;
	stats->last_char = 0;
	stats->save_msg = 0;
	stats->use_register = 0;
	stats->curr_register = -1;
	stats->register_saved = 0;
	stats->number_of_windows = 2;
	stats->view = 0;
	stats->use_input_bar = 1;
	stats->load_stage = 0;
	stats->too_small_term = 0;
	stats->dirsize_cache = NULL_TREE;
	stats->ch_pos = 1;
	stats->confirmed = 0;
	stats->skip_shellout_redraw = 0;
	stats->cs = &config->cs;
	strcpy(stats->color_scheme, "");

	stats->msg_head = 0;
	stats->msg_tail = 0;
	stats->save_msg_in_list = 1;
	stats->allow_sb_msg_truncation = 1;

	stats->scroll_bind_off = 0;
	stats->split = VSPLIT;
	stats->splitter_pos = -1.0;

	stats->sourcing_state = SOURCING_NONE;

	stats->restart_in_progress = 0;

	stats->exec_env_type = EET_EMULATOR;

	stats->term_multiplexer = TM_NONE;

	stats->initial_lines = INT_MIN;
	stats->initial_columns = INT_MIN;

	stats->shell_type = ST_NORMAL;

	stats->file_picker_mode = 0;

	stats->fuse_umount_cmd = "";

#ifdef HAVE_LIBGTK
	stats->gtk_available = 0;
#endif
}

/* Initializes stats->fuse_umount_cmd field of the stats. */
static void
determine_fuse_umount_cmd(status_t *stats)
{
	if(external_command_exists("fusermount"))
	{
		stats->fuse_umount_cmd = "fusermount -u";
	}
	else if(external_command_exists("umount"))
	{
		/* Some systems use regular umount command for FUSE. */
		stats->fuse_umount_cmd = "umount";
	}
	else
	{
		/* Leave default value. */
	}
}

static void
set_gtk_available(status_t *stats)
{
#ifdef HAVE_LIBGTK
	char *argv[] = { "vifm", NULL };
	int argc = ARRAY_LEN(argv) - 1;
	char **ptr = argv;
	stats->gtk_available = gtk_init_check(&argc, &ptr);
#endif
}

int
reset_status(const config_t *config)
{
	set_last_cmdline_command("");

	curr_stats.initial_lines = config->lines;
	curr_stats.initial_columns = config->columns;

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

void
set_using_term_multiplexer(int use_term_multiplexer)
{
	if(!use_term_multiplexer)
	{
		curr_stats.term_multiplexer = TM_NONE;
	}
	else if(inside_screen)
	{
		curr_stats.term_multiplexer = TM_SCREEN;
	}
	else if(inside_tmux)
	{
		curr_stats.term_multiplexer = TM_TMUX;
	}
	else
	{
		curr_stats.term_multiplexer = TM_NONE;
	}
}

void
update_last_cmdline_command(const char cmd[])
{
	if(!curr_stats.restart_in_progress && curr_stats.load_stage == 3)
	{
		set_last_cmdline_command(cmd);
	}
}

/* Sets last_cmdline_command field of the status structure. */
static void
set_last_cmdline_command(const char cmd[])
{
	const int err = replace_string(&curr_stats.last_cmdline_command, cmd);
	if(err != 0)
	{
		LOG_ERROR_MSG("replace_string() failed on duplicating: %s", cmd);
	}
	assert(curr_stats.last_cmdline_command != NULL &&
			"The field was not initialized properly");
}

void
stats_update_shell_type(const char shell_cmd[])
{
	curr_stats.shell_type = get_shell_type(shell_cmd);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
