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

#include <assert.h> /* assert() */

#include "cfg/config.h"
#include "engine/abbrevs.h"
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
#include "trash.h"
#include "undo.h"
#include "vifm.h"

void
instance_stop(void)
{
	ui_shutdown();
	stop_process();
}

void
instance_start_restart(RestartType type)
{
	view_t *tmp_view;

	curr_stats.restart_in_progress = type;

	/* All user mappings in all modes. */
	vle_keys_user_clear();

	/* User defined commands. */
	vle_cmds_clear();

	/* Autocommands. */
	vle_aucmd_remove(NULL, NULL);

	/* Abbreviations. */
	vle_abbr_reset();

	/* All kinds of histories. */
	cfg_clear_histories(tabs_count(&lwin) == 0 && tabs_count(&rwin) == 0);
	/* Load new history size into options unit, a possibly more reliable
	 * alternative would be to re-initialize options from current state below
	 * instead of resetting to defaults, but not many options would benefit from
	 * that, history is kinda special. */
	load_history_option();

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

	instance_load_config();
	plugs_load(curr_stats.plugs, curr_stats.plugins_dirs);

	vifm_reexec_startup_commands();

	curr_stats.restart_in_progress = RT_NONE;

	if(cfg.use_trash)
	{
		/* Applying 'trashdir' is postponed during restart to not create directory
		 * at default location. */
		(void)trash_set_specs(cfg.trash_dir);
	}

	/* Trigger auto-commands for initial directories. */
	vle_aucmd_execute("DirEnter", flist_get_dir(&lwin), &lwin);
	vle_aucmd_execute("DirEnter", flist_get_dir(&rwin), &rwin);

	update_screen(UT_REDRAW);
}

void
instance_load_config(void)
{
	cfg_load();

	/* Directory history is filled with data read from the state before its size
	 * limit is known.  For this reason it's allowed to grow unlimited and
	 * diverge from the size limit imposed by the 'history' option.  Setting an
	 * option in the configuration to the default value of 15 or not setting it
	 * there at all leaves the directory history size larger than it should be.
	 * Enforce the expected size here to avoid the history growing unbounded. */
	opt_t *history_opt = vle_opts_find("history", OPT_GLOBAL);
	assert(history_opt != NULL && "'history' option must be there.");
	cfg_resize_histories(history_opt->val.int_val);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
