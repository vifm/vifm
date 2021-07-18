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

#include "instance.h"

#include "cfg/config.h"
#include "engine/autocmds.h"
#include "engine/cmds.h"
#include "engine/keys.h"
#include "engine/options.h"
#include "engine/variables.h"
#include "int/path_env.h"
#include "lua/vlua.h"
#include "ui/tabs.h"
#include "ui/ui.h"
#include "utils/str.h"
#include "utils/utils.h"
#include "bmarks.h"
#include "dir_stack.h"
#include "filelist.h"
#include "flist_hist.h"
#include "opt_handlers.h"
#include "plugins.h"
#include "registers.h"
#include "status.h"
#include "undo.h"
#include "vifm.h"

void
instance_stop(void)
{
	ui_shutdown();
	stop_process();
}

void
instance_start_restart(void)
{
	view_t *tmp_view;

	curr_stats.restart_in_progress = 1;

	/* All user mappings in all modes. */
	vle_keys_user_clear();

	/* User defined commands. */
	vle_cmds_clear();

	/* Autocommands. */
	vle_aucmd_remove(NULL, NULL);

	/* All kinds of histories. */
	cfg_clear_histories(tabs_count(&lwin) == 0 && tabs_count(&rwin) == 0);

	/* Session status.  Must be reset _before_ options, because options take some
	 * of values from status. */
	(void)stats_reset(&cfg);

	/* Options of current pane. */
	vle_opts_restore_defaults();
	/* Options of other pane. */
	tmp_view = curr_view;
	curr_view = other_view;
	load_view_options(other_view);
	vle_opts_restore_defaults();
	curr_view = tmp_view;

	/* File types and viewers. */
	ft_reset(curr_stats.exec_env_type == EET_EMULATOR_WITH_X);

	/* Undo list. */
	un_reset();

	/* Directory stack. */
	dir_stack_clear();

	/* Registers. */
	regs_reset();

	/* Clear all marks and bookmarks. */
	marks_clear_all();
	bmarks_clear();

	/* Reset variables. */
	clear_envvars();
	init_variables();
	/* This update is needed as clear_variables() will reset $PATH. */
	update_path_env(1);

	reset_views();

	plugs_free(curr_stats.plugs);
	vlua_finish(curr_stats.vlua);

	curr_stats.vlua = vlua_init();
	curr_stats.plugs = plugs_create(curr_stats.vlua);
}

void
instance_finish_restart(void)
{
	flist_hist_save(&lwin);
	flist_hist_save(&rwin);

	/* Color schemes. */
	if(stroscmp(curr_stats.color_scheme, DEF_CS_NAME) != 0 &&
			cs_exists(curr_stats.color_scheme))
	{
		cs_load_primary(curr_stats.color_scheme);
	}
	else
	{
		cs_load_defaults();
	}
	cs_load_pairs();

	cfg_load();
	plugs_load(curr_stats.plugs, cfg.config_dir);

	vifm_reexec_startup_commands();

	curr_stats.restart_in_progress = 0;

	/* Trigger auto-commands for initial directories. */
	vle_aucmd_execute("DirEnter", flist_get_dir(&lwin), &lwin);
	vle_aucmd_execute("DirEnter", flist_get_dir(&rwin), &rwin);

	update_screen(UT_REDRAW);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
