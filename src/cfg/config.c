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

#include "config.h"

#define HOME_EV "HOME"
#define VIFM_EV "VIFM"
#define MYVIFMRC_EV "MYVIFMRC"
#define TRASH "Trash"
#define LOG "log"
#define VIFMRC "vifmrc"

#ifndef __APPLE__
#define SAMPLE_VIFMRC "vifmrc"
#define SAMPLE_MEDIAPRG "vifm-media"
#else
#define SAMPLE_VIFMRC "vifmrc-osx"
#define SAMPLE_MEDIAPRG "vifm-media-osx"
#endif

#include <assert.h> /* assert() */
#include <limits.h> /* INT_MIN */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* FILE snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* memmove() memset() strdup() */

#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "../int/term_title.h"
#include "../io/iop.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../ui/color_scheme.h"
#include "../ui/statusbar.h"
#include "../ui/tabs.h"
#include "../ui/ui.h"
#include "../utils/env.h"
#include "../utils/file_streams.h"
#include "../utils/fs.h"
#include "../utils/log.h"
#include "../utils/macros.h"
#include "../utils/str.h"
#include "../utils/path.h"
#include "../utils/string_array.h"
#include "../utils/utils.h"
#include "../cmd_core.h"
#include "../filelist.h"
#include "../flist_hist.h"
#include "../marks.h"
#include "../opt_handlers.h"
#include "../status.h"
#include "../types.h"
#include "../vifm.h"

/* Maximum supported by the implementation length of line in vifmrc file. */
#define MAX_VIFMRC_LINE_LEN 4*1024

/* Default value of shell command used if $SHELL environment variable isn't
 * set. */
#ifndef _WIN32
#define DEFAULT_SHELL_CMD "/bin/sh"
#else
#define DEFAULT_SHELL_CMD "cmd"
#endif

/* Default value of the cd path list. */
#define DEFAULT_CD_PATH ""

config_t cfg;

static void find_home_dir(void);
static int try_home_envvar_for_home(void);
static int try_userprofile_envvar_for_home(void);
static int try_homepath_envvar_for_home(void);
static void find_config_dir(void);
static int try_vifm_envvar_for_conf(void);
static int try_exe_directory_for_conf(void);
static int try_home_envvar_for_conf(int force);
static int try_appdata_for_conf(void);
static int try_xdg_for_conf(void);
static void find_data_dir(char buf[], size_t buf_size);
static void find_config_file(void);
static int try_myvifmrc_envvar_for_vifmrc(void);
static int try_exe_directory_for_vifmrc(void);
static int try_vifm_vifmrc_for_vifmrc(void);
static void store_config_paths(const char data_dir[]);
static void setup_dirs(void);
static void copy_help_file(void);
static void create_scripts_dir(void);
static void copy_rc_file(void);
static void add_default_marks(void);
static int source_file_internal(strlist_t lines, const char filename[]);
static void show_sourcing_error(const char filename[], int line_num);

void
cfg_init(void)
{
	cfg.session = NULL;

	cfg.history_len = 15;

	cfg.auto_execute = 0;
	cfg.time_format = strdup("%m/%d %H:%M");
	cfg.wrap_quick_view = 1;
	cfg.undo_levels = 100;
	cfg.sort_numbers = 0;
	cfg.follow_links = 1;
	cfg.fast_run = 0;
	cfg.confirm = CONFIRM_DELETE | CONFIRM_PERM_DELETE;
	cfg.vi_command = strdup("vim");
	cfg.vi_cmd_bg = 0;
	cfg.vi_x_command = strdup("");
	cfg.vi_x_cmd_bg = 0;
	cfg.use_trash = 1;
	cfg.use_term_multiplexer = 0;
	cfg.use_vim_help = 0;
	cfg.wild_menu = 0;
	cfg.wild_popup = 0;

	cfg.sug.flags = 0;
	cfg.sug.maxregfiles = 5;
	cfg.sug.delay = 500;

	cfg.ignore_case = 0;
	cfg.smart_case = 0;
	cfg.hl_search = 1;
	cfg.vifm_info = VINFO_MARKS | VINFO_BOOKMARKS;
	cfg.session_options = VINFO_TUI | VINFO_STATE | VINFO_TABS | VINFO_SAVEDIRS
	                    | VINFO_DHISTORY;
	cfg.scroll_off = 0;
	cfg.gdefault = 0;
	cfg.scroll_bind = 0;
	cfg.wrap_scan = 1;
	cfg.inc_search = 0;
	cfg.selection_is_primary = 1;
	cfg.tab_switches_pane = 1;
	cfg.use_system_calls = 0;
	cfg.tab_stop = 8;
	cfg.ruler_format = strdup("%l/%S ");
	cfg.status_line = strdup("");
	cfg.tab_line = strdup("");

	cfg.lines = INT_MIN;
	cfg.columns = INT_MIN;

	cfg.dot_dirs = DD_NONROOT_PARENT | DD_TREE_LEAFS_PARENT;

	cfg.mouse = 0;

	cfg.filter_inverted_by_default = 1;

	cfg.apropos_prg = strdup("apropos %a");
	cfg.find_prg = strdup("find %s %a -print , "
			"-type d \\( ! -readable -o ! -executable \\) -prune");
	cfg.grep_prg = strdup("grep -n -H -I -r %i %a %s");
	cfg.locate_prg = strdup("locate %a");
	cfg.delete_prg = strdup("");
	cfg.media_prg = format_str("%s/" SAMPLE_MEDIAPRG, get_installed_data_dir());

	cfg.nav_open_files = 0;

	cfg.tail_tab_line_paths = 0;
	cfg.trunc_normal_sb_msgs = 0;
	cfg.shorten_title_paths = 1;
	cfg.short_term_mux_titles = 0;

	cfg.slow_fs_list = strdup("");

	cfg.cd_path = strdup(env_get_def("CDPATH", DEFAULT_CD_PATH));
	replace_char(cfg.cd_path, ':', ',');

	cfg.auto_cd = 0;

	cfg.ellipsis_position = 0;
	cfg.extra_padding = 1;
	cfg.side_borders_visible = 1;
	cfg.use_unicode_characters = 0;
	cfg.color_what = CW_ONE_ROW;
	cfg.flexible_splitter = 1;
	cfg.display_statusline = 1;

	cfg.vborder_filler = strdup(" ");
	cfg.hborder_filler = strdup("");

	cfg.set_title = term_title_restorable();

	cfg.chase_links = 0;

	cfg.graphics_delay = 50000;
	cfg.hard_graphics_clear = 0;
	cfg.top_tree_stats = 0;
	cfg.max_tree_depth = 0;

	cfg.timeout_len = 1000;
	cfg.min_timeout_len = 150;

	/* Fill cfg.word_chars as if it was initialized from isspace() function. */
	memset(&cfg.word_chars, 1, sizeof(cfg.word_chars));
	cfg.word_chars['\x00'] = 0; cfg.word_chars['\x09'] = 0;
	cfg.word_chars['\x0a'] = 0; cfg.word_chars['\x0b'] = 0;
	cfg.word_chars['\x0c'] = 0; cfg.word_chars['\x0d'] = 0;
	cfg.word_chars['\x20'] = 0;

	cfg.view_dir_size = VDS_SIZE;

	cfg.log_file[0] = '\0';

	cfg_set_shell(env_get_def("SHELL", DEFAULT_SHELL_CMD));
	cfg.shell_cmd_flag = strdup((curr_stats.shell_type == ST_CMD) ? "/C" : "-c");

	memset(&cfg.type_decs, '\0', sizeof(cfg.type_decs));
	cfg.type_decs[FT_DIR][DECORATION_SUFFIX][0] = '/';

	cfg.name_decs = NULL;
	cfg.name_dec_count = 0;

	cfg.fast_file_cloning = 0;
	cfg.data_sync = 1;

	cfg.cvoptions = 0;

	cfg.case_override = 0;
	cfg.case_ignore = 0;

	cfg.sizefmt.base = 1024;
	cfg.sizefmt.precision = 0;
	cfg.sizefmt.ieci_prefixes = 0;
	cfg.sizefmt.space = 1;

	cfg.pane_tabs = 0;
	cfg.show_tab_line = STL_MULTIPLE;
	cfg.tab_prefix = strdup("[%N:");
	cfg.tab_label = strdup("");
	cfg.tab_suffix = strdup("]");

	cfg.auto_ch_pos = 1;
	cfg.ch_pos_on = CHPOS_STARTUP | CHPOS_DIRMARK | CHPOS_ENTER;
}

void
cfg_discover_paths(void)
{
	LOG_FUNC_ENTER;

	char data_dir[PATH_MAX + 1];

	find_home_dir();
	find_config_dir();
	find_data_dir(data_dir, sizeof(data_dir));
	find_config_file();

	store_config_paths(data_dir);

	setup_dirs();
}

/* Tries to find home directory. */
static void
find_home_dir(void)
{
	LOG_FUNC_ENTER;

	if(try_home_envvar_for_home()) return;
	if(try_userprofile_envvar_for_home()) return;
	if(try_homepath_envvar_for_home()) return;

	vifm_finish("Failed to find user's home directory.");
}

/* Tries to use HOME environment variable to find home directory.  Returns
 * non-zero on success, otherwise zero is returned. */
static int
try_home_envvar_for_home(void)
{
	LOG_FUNC_ENTER;

	const char *home = env_get(HOME_EV);
	return home != NULL && is_dir(home);
}

/* Tries to use USERPROFILE environment variable to find home directory.
 * Returns non-zero on success, otherwise zero is returned. */
static int
try_userprofile_envvar_for_home(void)
{
	LOG_FUNC_ENTER;

#ifndef _WIN32
	return 0;
#else
	char home[PATH_MAX + 1];
	const char *userprofile = env_get("USERPROFILE");
	if(userprofile == NULL || !is_dir(userprofile))
		return 0;
	copy_str(home, sizeof(home), userprofile);
	system_to_internal_slashes(home);
	env_set(HOME_EV, home);
	return 1;
#endif
}

/* Tries to use $HOMEDRIVE/$HOMEPATH as home directory.  Returns non-zero on
 * success, otherwise zero is returned. */
static int
try_homepath_envvar_for_home(void)
{
	LOG_FUNC_ENTER;

#ifndef _WIN32
	return 0;
#else
	char home[PATH_MAX + 1];
	const char *homedrive = env_get("HOMEDRIVE");
	const char *homepath = env_get("HOMEPATH");
	if(homedrive == NULL || !is_dir(homedrive))
		return 0;
	if(homepath == NULL || !is_dir(homepath))
		return 0;

	snprintf(home, sizeof(home), "%s%s", homedrive, homepath);
	system_to_internal_slashes(home);
	env_set(HOME_EV, home);
	return 1;
#endif
}

/* Tries to find configuration directory. */
static void
find_config_dir(void)
{
	LOG_FUNC_ENTER;

	if(try_vifm_envvar_for_conf()) return;
	if(try_exe_directory_for_conf()) return;
	if(try_home_envvar_for_conf(0)) return;
	if(try_appdata_for_conf()) return;
	if(try_xdg_for_conf()) return;

	if(!try_home_envvar_for_conf(1))
	{
		vifm_finish("Failed to determine location of configuration files.");
	}
}

/* Tries to use VIFM environment variable to find configuration directory.
 * Returns non-zero on success, otherwise zero is returned. */
static int
try_vifm_envvar_for_conf(void)
{
	LOG_FUNC_ENTER;

	const char *vifm = env_get(VIFM_EV);
	return vifm != NULL && is_dir(vifm);
}

/* Tries to use directory of executable file as configuration directory.
 * Returns non-zero on success, otherwise zero is returned. */
static int
try_exe_directory_for_conf(void)
{
	LOG_FUNC_ENTER;

	char exe_dir[PATH_MAX + 1];

	if(get_exe_dir(exe_dir, sizeof(exe_dir)) != 0)
	{
		return 0;
	}

	if(!path_exists_at(exe_dir, VIFMRC, DEREF))
	{
		return 0;
	}

	env_set(VIFM_EV, exe_dir);
	return 1;
}

/* Tries to use $HOME/.vifm as configuration directory.  Tries harder on force.
 * Returns non-zero on success, otherwise zero is returned. */
static int
try_home_envvar_for_conf(int force)
{
	LOG_FUNC_ENTER;

	char vifm[PATH_MAX + 1];

	const char *home = env_get(HOME_EV);
	if(home == NULL || !is_dir(home))
	{
		return 0;
	}

	snprintf(vifm, sizeof(vifm), "%s/.vifm", home);
	system_to_internal_slashes(vifm);

	if(!force && !is_dir(vifm))
	{
		return 0;
	}

	env_set(VIFM_EV, vifm);
	return 1;
}

/* Tries to use $APPDATA/Vifm as configuration directory.  Returns non-zero on
 * success, otherwise zero is returned. */
static int
try_appdata_for_conf(void)
{
	LOG_FUNC_ENTER;

#ifndef _WIN32
	return 0;
#else
	char vifm[PATH_MAX + 1];
	const char *appdata = env_get("APPDATA");
	if(appdata == NULL || !is_dir(appdata))
		return 0;
	snprintf(vifm, sizeof(vifm), "%s/Vifm", appdata);
	system_to_internal_slashes(vifm);
	env_set(VIFM_EV, vifm);
	return 1;
#endif
}

/* Tries to use $XDG_CONFIG_HOME/vifm as configuration directory.  Returns
 * non-zero on success, otherwise zero is returned. */
static int
try_xdg_for_conf(void)
{
	LOG_FUNC_ENTER;

	char *config_dir;

	const char *const config_home = env_get("XDG_CONFIG_HOME");
	if(!is_null_or_empty(config_home) && is_path_absolute(config_home))
	{
		config_dir = format_str("%s/vifm", config_home);
	}
	else if(path_exists_at(env_get(HOME_EV), ".config", DEREF))
	{
		config_dir = format_str("%s/.config/vifm", env_get(HOME_EV));
	}
	else
	{
		return 0;
	}

	env_set(VIFM_EV, config_dir);
	free(config_dir);

	/* If XDG is used for configuration directory, create correcponding data
	 * directory, so it's also used at the same time. */
	char data_dir[PATH_MAX + 1];
	find_data_dir(data_dir, sizeof(data_dir));
	(void)create_path(data_dir, S_IRWXU);

	return 1;
}

/* Tries to find XDG directory for storing data files. */
static void
find_data_dir(char buf[], size_t buf_size)
{
	LOG_FUNC_ENTER;

	assert(buf_size >= 4 && "Buffer for find_data_dir() is too small!");

	const char *const data_home = env_get("XDG_DATA_HOME");
	if(is_null_or_empty(data_home) || !is_path_absolute(data_home))
	{
		snprintf(buf, buf_size - 4, "%s/.local/share/", env_get(HOME_EV));
	}
	else
	{
		snprintf(buf, buf_size - 4, "%s/", data_home);
	}

	strcat(buf, "vifm");

	system_to_internal_slashes(buf);
}

/* Tries to find configuration file. */
static void
find_config_file(void)
{
	LOG_FUNC_ENTER;

	if(try_myvifmrc_envvar_for_vifmrc()) return;
	if(try_exe_directory_for_vifmrc()) return;
	if(try_vifm_vifmrc_for_vifmrc()) return;
}

/* Tries to use $MYVIFMRC as configuration file.  Returns non-zero on success,
 * otherwise zero is returned. */
static int
try_myvifmrc_envvar_for_vifmrc(void)
{
	LOG_FUNC_ENTER;

	const char *myvifmrc = env_get(MYVIFMRC_EV);
	return myvifmrc != NULL && path_exists(myvifmrc, DEREF);
}

/* Tries to use vifmrc in directory of executable file as configuration file.
 * Returns non-zero on success, otherwise zero is returned. */
static int
try_exe_directory_for_vifmrc(void)
{
	LOG_FUNC_ENTER;

	char exe_dir[PATH_MAX + 1];
	char vifmrc[PATH_MAX + 8];

	if(get_exe_dir(exe_dir, sizeof(exe_dir)) != 0)
	{
		return 0;
	}

	snprintf(vifmrc, sizeof(vifmrc), "%s/" VIFMRC, exe_dir);
	if(!path_exists(vifmrc, DEREF))
	{
		return 0;
	}

	env_set(MYVIFMRC_EV, vifmrc);
	return 1;
}

/* Tries to use $VIFM/vifmrc as configuration file.  Returns non-zero on
 * success, otherwise zero is returned. */
static int
try_vifm_vifmrc_for_vifmrc(void)
{
	LOG_FUNC_ENTER;

	char vifmrc[PATH_MAX + 1];
	const char *vifm = env_get(VIFM_EV);
	if(vifm == NULL || !is_dir(vifm))
		return 0;
	snprintf(vifmrc, sizeof(vifmrc), "%s/" VIFMRC, vifm);
	if(!path_exists(vifmrc, DEREF))
		return 0;
	env_set(MYVIFMRC_EV, vifmrc);
	return 1;
}

/* Writes path configuration file and directories for further usage. */
static void
store_config_paths(const char data_dir[])
{
	LOG_FUNC_ENTER;

	snprintf(cfg.home_dir, sizeof(cfg.home_dir), "%s/", env_get(HOME_EV));
	copy_str(cfg.config_dir, sizeof(cfg.config_dir), env_get(VIFM_EV));
	snprintf(cfg.colors_dir, sizeof(cfg.colors_dir), "%s/colors/",
			cfg.config_dir);

	const char *const trash_dir_fmt =
#ifndef _WIN32
			"%%r/.vifm-Trash-%%u,%s/" TRASH ",%%r/.vifm-Trash";
#else
			"%%r/.vifm-Trash,%s/" TRASH;
#endif

	/* Determine where to store data giving preference to directories that already
	 * exist. */
	const char *base;
	if(path_exists_at(data_dir, TRASH, DEREF))
	{
		base = data_dir;
	}
	else if(path_exists_at(cfg.config_dir, TRASH, DEREF))
	{
		base = cfg.config_dir;
	}
	else
	{
		base = (is_dir(data_dir) ? data_dir : cfg.config_dir);
	}

	char *trash_base = escape_chars(base, "$");
	snprintf(cfg.trash_dir, sizeof(cfg.trash_dir), trash_dir_fmt, trash_base);
	free(trash_base);

	snprintf(cfg.log_file, sizeof(cfg.log_file), "%s/" LOG, base);

	char *fuse_home = format_str("%s/fuse/", base);
	(void)cfg_set_fuse_home(fuse_home);
	free(fuse_home);
}

/* Ensures existence of configuration and data directories.  Performs first run
 * initialization. */
static void
setup_dirs(void)
{
	LOG_FUNC_ENTER;

	char rc_file[PATH_MAX + 8];

	if(is_dir(cfg.config_dir))
	{
		/* We rely on this file for :help, so make sure it's there on every run. */
		copy_help_file();

		create_scripts_dir();
		return;
	}

	if(make_path(cfg.config_dir, S_IRWXU) != 0)
	{
		return;
	}

	/* This must be first run of Vifm in this environment. */

	copy_help_file();
	copy_rc_file();
	cs_write();

	/* Ensure that just copied sample vifmrc file is used right away. */
	snprintf(rc_file, sizeof(rc_file), "%s/" VIFMRC, cfg.config_dir);
	env_set(MYVIFMRC_EV, rc_file);

	add_default_marks();
}

/* Copies help file from shared files to the ~/.vifm directory if it's not
 * already there. */
static void
copy_help_file(void)
{
	LOG_FUNC_ENTER;

	char src[PATH_MAX + 16];
	char dst[PATH_MAX + 16];

	io_args_t args = {
		.arg1.src = src,
		.arg2.dst = dst,
		.arg3.crs = IO_CRS_FAIL,
	};

	snprintf(src, sizeof(src), "%s/" VIFM_HELP, get_installed_data_dir());
	snprintf(dst, sizeof(dst), "%s/" VIFM_HELP, cfg.config_dir);

	/* Don't care if it fails, also don't overwrite if file exists. */
	(void)iop_cp(&args);
}

/* Responsible for creation of scripts/ directory if it doesn't exist (in which
 * case a README file is created as well). */
static void
create_scripts_dir(void)
{
	char scripts[PATH_MAX + 16];
	char readme[PATH_MAX + 32];
	FILE *fp;

	snprintf(scripts, sizeof(scripts), "%s/" SCRIPTS_DIR, cfg.config_dir);
	if(create_path(scripts, S_IRWXU) != 0)
	{
		return;
	}

	snprintf(readme, sizeof(readme), "%s/README", scripts);
	fp = os_fopen(readme, "w");
	if(fp == NULL)
	{
		return;
	}

	fputs("This directory is dedicated for user-supplied scripts/executables.\n"
				"vifm modifies its PATH environment variable to let user run those\n"
				"scripts without specifying full path.  All subdirectories are added\n"
				"as well.  File in a subdirectory overrules file with the same name\n"
				"in parent directories.  Restart might be needed to recognize files\n"
				"in newly created or renamed subdirectories.", fp);

	fclose(fp);
}

/* Copies example vifmrc file from shared files to the ~/.vifm directory. */
static void
copy_rc_file(void)
{
	LOG_FUNC_ENTER;

	char src[PATH_MAX + 16];
	char dst[PATH_MAX + 16];

	io_args_t args = {
		.arg1.src = src,
		.arg2.dst = dst,
	};

	snprintf(src, sizeof(src), "%s/" SAMPLE_VIFMRC, get_installed_data_dir());
	snprintf(dst, sizeof(dst), "%s/" VIFMRC, cfg.config_dir);

	(void)iop_cp(&args);
}

/* Adds 'H' and 'z' default marks. */
static void
add_default_marks(void)
{
	LOG_FUNC_ENTER;

	marks_set_user(curr_view, 'H', cfg.home_dir, NO_MARK_FILE);
	marks_set_user(curr_view, 'z', cfg.config_dir, NO_MARK_FILE);
}

void
cfg_load(void)
{
	const char *rc;
	const int prev_global_local_settings = curr_stats.global_local_settings;

	/* Make changes of usually local settings during configuration affect all
	 * views. */
	curr_stats.global_local_settings = 1;

	/* Try to load global configuration(s). */
	int i = 0;
	while(!vifm_testing())
	{
		const char *conf_dir = get_sys_conf_dir(i++);
		if(conf_dir == NULL)
		{
			break;
		}

		char rc_path[PATH_MAX + 1];
		build_path(rc_path, sizeof(rc_path), conf_dir, VIFMRC);
		(void)cfg_source_file(rc_path);
	}

	/* Try to load local configuration. */
	rc = env_get(MYVIFMRC_EV);
	if(!is_null_or_empty(rc))
	{
		(void)cfg_source_file(rc);
	}

	curr_stats.global_local_settings = prev_global_local_settings;
}

int
cfg_source_file(const char filename[])
{
	/* TODO: maybe move this to commands.c or separate unit eventually. */

	/* Binary mode is important on Windows. */
	FILE *fp = os_fopen(filename, "rb");
	if(fp == NULL)
	{
		return 1;
	}

	strlist_t lines;
	lines.items = read_file_lines(fp, &lines.nitems);
	fclose(fp);

	SourcingState sourcing_state = curr_stats.sourcing_state;
	curr_stats.sourcing_state = SOURCING_PROCESSING;

	int result = source_file_internal(lines, filename);

	curr_stats.sourcing_state = sourcing_state;

	free_string_array(lines.items, lines.nitems);
	return result;
}

/* Returns non-zero on error. */
static int
source_file_internal(strlist_t lines, const char filename[])
{
	if(lines.nitems == 0)
	{
		return 0;
	}

	cmds_scope_start();

	int encoutered_errors = 0;

	char line[MAX_VIFMRC_LINE_LEN + 1];
	line[0] = '\0';

	int line_num = 0;
	int next_line_num = 0;
	for(;;)
	{
		char *next_line = NULL;
		while(next_line_num < lines.nitems)
		{
			next_line = skip_whitespace(lines.items[next_line_num++]);
			if(*next_line == '"')
			{
				continue;
			}
			else if(*next_line == '\\')
			{
				strncat(line, next_line + 1, sizeof(line) - strlen(line) - 1);
				next_line = NULL;
			}
			else
			{
				break;
			}
		}

		ui_sb_clear();

		if(cmds_dispatch(line, curr_view, CIT_COMMAND) < 0)
		{
			show_sourcing_error(filename, line_num);
			encoutered_errors = 1;
		}
		if(curr_stats.sourcing_state == SOURCING_FINISHING)
			break;

		if(next_line == NULL)
		{
			/* Artificially increment line number to simulate as if all that happens
			 * after the loop relates to something past end of the file. */
			line_num++;
			break;
		}

		copy_str(line, sizeof(line), next_line);
		line_num = next_line_num;
	}

	ui_sb_clear();

	if(cmds_scope_finish() != 0)
	{
		show_sourcing_error(filename, line_num);
		encoutered_errors = 1;
	}

	return encoutered_errors;
}

/* Displays sourcing error message to a user. */
static void
show_sourcing_error(const char filename[], int line_num)
{
	const char *const last_msg = ui_sb_last();

	curr_stats.save_msg = 1;

	/* User choice is saved by prompt_error_msgf internally. */
	if(is_null_or_empty(last_msg))
	{
		(void)prompt_error_msgf("File Sourcing Error", "Error in %s at line %d",
				filename, line_num);
	}
	else
	{
		/* The space is needed because empty lines are automatically removed. */
		(void)prompt_error_msgf("File Sourcing Error", "Error in %s at line %d:\n"
				" \n%s", filename, line_num, last_msg);
	}

	curr_stats.save_msg = 0;
}

const char *
cfg_get_vicmd(int *bg)
{
	if(curr_stats.exec_env_type != EET_EMULATOR_WITH_X)
	{
		*bg = cfg.vi_cmd_bg;
		return cfg.vi_command;
	}
	else if(cfg.vi_x_command[0] != '\0')
	{
		*bg = cfg.vi_x_cmd_bg;
		return cfg.vi_x_command;
	}
	else
	{
		*bg = cfg.vi_cmd_bg;
		return cfg.vi_command;
	}
}

void
cfg_clear_histories(int clear_dhistory)
{
	if(clear_dhistory)
	{
		cfg_resize_histories(0);
	}
	else
	{
		hists_resize(0);
		cfg.history_len = 0;
		/* Directory histories keep their data, which will be truncated or updated
		 * later. */
	}
}

void
cfg_resize_histories(int new_size)
{
	hists_resize(new_size);

	int i;
	tab_info_t tab_info;
	for(i = 0; tabs_enum_all(i, &tab_info); ++i)
	{
		flist_hist_resize(tab_info.view, new_size);
	}

	const int old_size = cfg.history_len;
	cfg.history_len = new_size;

	if(old_size <= 0 && new_size > 0)
	{
		int i;
		tab_info_t tab_info;
		for(i = 0; tabs_enum_all(i, &tab_info); ++i)
		{
			flist_hist_save(tab_info.view);
		}
	}
}

int
cfg_set_fuse_home(const char new_value[])
{
#ifdef _WIN32
	char with_forward_slashes[strlen(new_value) + 1];
	strcpy(with_forward_slashes, new_value);
	system_to_internal_slashes(with_forward_slashes);
	new_value = with_forward_slashes;
#endif

	char canonicalized[PATH_MAX + 1];
	canonicalize_path(new_value, canonicalized, sizeof(canonicalized));

	if(!is_path_absolute(new_value))
	{
		if(cfg.fuse_home == NULL)
		{
			/* Do not leave cfg.fuse_home uninitialized. */
			cfg.fuse_home = strdup("");
		}

		show_error_msgf("Error Setting FUSE Home Directory",
				"The path is not absolute: %s", canonicalized);
		return 1;
	}

	return replace_string(&cfg.fuse_home, canonicalized);
}

void
cfg_set_use_term_multiplexer(int use_term_multiplexer)
{
	cfg.use_term_multiplexer = use_term_multiplexer;
	stats_set_use_multiplexer(use_term_multiplexer);
}

void
cfg_set_shell(const char shell[])
{
	if(replace_string(&cfg.shell, shell) == 0)
	{
		stats_update_shell_type(cfg.shell);
	}
}

int
cfg_is_word_wchar(wchar_t c)
{
	return c >= 256 || cfg.word_chars[c];
}

int
cfg_parent_dir_is_visible(int in_root)
{
	return ((in_root && (cfg.dot_dirs & DD_ROOT_PARENT)) ||
			(!in_root && (cfg.dot_dirs & DD_NONROOT_PARENT)));
}

int
cfg_confirm_delete(int to_trash)
{
	return to_trash
	     ? (cfg.confirm & CONFIRM_DELETE)
	     : (cfg.confirm & CONFIRM_PERM_DELETE);
}

int
cfg_ch_pos_on(ChposWhen when)
{
	return cfg.auto_ch_pos && (cfg.ch_pos_on & when);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
