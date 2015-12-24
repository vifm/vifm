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
#else
#define SAMPLE_VIFMRC "vifmrc-osx"
#endif

#include <assert.h> /* assert() */
#include <limits.h> /* INT_MIN */
#include <stddef.h> /* size_t */
#include <stdio.h> /* FILE snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* memmove() memset() strdup() */

#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "../compat/reallocarray.h"
#include "../io/iop.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../ui/ui.h"
#include "../utils/env.h"
#include "../utils/file_streams.h"
#include "../utils/fs.h"
#include "../utils/log.h"
#include "../utils/macros.h"
#include "../utils/str.h"
#include "../utils/path.h"
#include "../utils/utils.h"
#include "../cmd_core.h"
#include "../filelist.h"
#include "../marks.h"
#include "../opt_handlers.h"
#include "../status.h"
#include "../types.h"
#include "../vifm.h"
#include "hist.h"

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
static void find_data_dir(void);
static void find_config_file(void);
static int try_myvifmrc_envvar_for_vifmrc(void);
static int try_exe_directory_for_vifmrc(void);
static int try_vifm_vifmrc_for_vifmrc(void);
static void store_config_paths(void);
static void setup_dirs(void);
static void copy_help_file(void);
static void create_scripts_dir(void);
static void copy_rc_file(void);
static void add_default_marks(void);
static int source_file_internal(FILE *fp, const char filename[]);
static void show_sourcing_error(const char filename[], int line_num);
static void disable_history(void);
static void free_view_history(FileView *view);
static void decrease_history(size_t new_len, size_t removed_count);
static void reduce_view_history(FileView *view, int size);
static void reallocate_history(size_t new_len);
static void zero_new_history_items(size_t old_len, size_t delta);
static void save_into_history(const char item[], hist_t *hist, int len);

void
cfg_init(void)
{
	cfg.history_len = 15;

	(void)hist_init(&cfg.cmd_hist, cfg.history_len);
	(void)hist_init(&cfg.search_hist, cfg.history_len);
	(void)hist_init(&cfg.prompt_hist, cfg.history_len);
	(void)hist_init(&cfg.filter_hist, cfg.history_len);

	cfg.auto_execute = 0;
	cfg.time_format = strdup(" %m/%d %H:%M");
	cfg.wrap_quick_view = 1;
	cfg.use_iec_prefixes = 0;
	cfg.undo_levels = 100;
	cfg.sort_numbers = 0;
	cfg.follow_links = 1;
	cfg.fast_run = 0;
	cfg.confirm = 1;
	cfg.vi_command = strdup("vim");
	cfg.vi_cmd_bg = 0;
	cfg.vi_x_command = strdup("");
	cfg.vi_x_cmd_bg = 0;
	cfg.use_trash = 1;
	cfg.use_term_multiplexer = 0;
	cfg.use_vim_help = 0;
	cfg.wild_menu = 0;
	cfg.ignore_case = 0;
	cfg.smart_case = 0;
	cfg.hl_search = 1;
	cfg.vifm_info = VIFMINFO_MARKS | VIFMINFO_BOOKMARKS;
	cfg.auto_ch_pos = 1;
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

	cfg.lines = INT_MIN;
	cfg.columns = INT_MIN;

	cfg.dot_dirs = DD_NONROOT_PARENT;

	cfg.filter_inverted_by_default = 1;

	cfg.apropos_prg = strdup("apropos %a");
	cfg.find_prg = strdup("find %s %a -print , "
			"-type d \\( ! -readable -o ! -executable \\) -prune");
	cfg.grep_prg = strdup("grep -n -H -I -r %i %a %s");
	cfg.locate_prg = strdup("locate %a");
	cfg.delete_prg = strdup("");

	cfg.trunc_normal_sb_msgs = 0;
	cfg.shorten_title_paths = 1;

	cfg.slow_fs_list = strdup("");

	cfg.cd_path = strdup(env_get_def("CDPATH", DEFAULT_CD_PATH));
	replace_char(cfg.cd_path, ':', ',');

	cfg.extra_padding = 1;
	cfg.side_borders_visible = 1;
	cfg.display_statusline = 1;

	cfg.border_filler = strdup(" ");

	cfg.chase_links = 0;

	cfg.timeout_len = 1000;
	cfg.min_timeout_len = 150;

	/* Fill cfg.word_chars as if it was initialized from isspace() fuction. */
	memset(&cfg.word_chars, 1, sizeof(cfg.word_chars));
	cfg.word_chars['\x00'] = 0; cfg.word_chars['\x09'] = 0;
	cfg.word_chars['\x0a'] = 0; cfg.word_chars['\x0b'] = 0;
	cfg.word_chars['\x0c'] = 0; cfg.word_chars['\x0d'] = 0;
	cfg.word_chars['\x20'] = 0;

	cfg.view_dir_size = VDS_SIZE;

	cfg.log_file[0] = '\0';

	cfg_set_shell(env_get_def("SHELL", DEFAULT_SHELL_CMD));

	memset(&cfg.decorations, '\0', sizeof(cfg.decorations));
	cfg.decorations[FT_DIR][DECORATION_SUFFIX] = '/';

	cfg.fast_file_cloning = 0;
}

void
cfg_discover_paths(void)
{
	LOG_FUNC_ENTER;

	find_home_dir();
	find_config_dir();
	find_data_dir();
	find_config_file();

	store_config_paths();

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
	char home[PATH_MAX];
	const char *userprofile = env_get("USERPROFILE");
	if(userprofile == NULL || !is_dir(userprofile))
		return 0;
	snprintf(home, sizeof(home), "%s", userprofile);
	to_forward_slash(home);
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
	char home[PATH_MAX];
	const char *homedrive = env_get("HOMEDRIVE");
	const char *homepath = env_get("HOMEPATH");
	if(homedrive == NULL || !is_dir(homedrive))
		return 0;
	if(homepath == NULL || !is_dir(homepath))
		return 0;

	snprintf(home, sizeof(home), "%s%s", homedrive, homepath);
	to_forward_slash(home);
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

	char exe_dir[PATH_MAX];

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

	char vifm[PATH_MAX];

	const char *home = env_get(HOME_EV);
	if(home == NULL || !is_dir(home))
	{
		return 0;
	}

	snprintf(vifm, sizeof(vifm), "%s/.vifm", home);
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
	char vifm[PATH_MAX];
	const char *appdata = env_get("APPDATA");
	if(appdata == NULL || !is_dir(appdata))
		return 0;
	snprintf(vifm, sizeof(vifm), "%s/Vifm", appdata);
	to_forward_slash(vifm);
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

	return 1;
}

/* Tries to find directory for data files. */
static void
find_data_dir(void)
{
	LOG_FUNC_ENTER;

	const char *const data_home = env_get("XDG_DATA_HOME");
	if(is_null_or_empty(data_home) || !is_path_absolute(data_home))
	{
		snprintf(cfg.data_dir, sizeof(cfg.data_dir) - 4, "%s/.local/share/",
				env_get(HOME_EV));
	}
	else
	{
		snprintf(cfg.data_dir, sizeof(cfg.data_dir) - 4, "%s/", data_home);
	}

	strcat(cfg.data_dir, "vifm");
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

	char exe_dir[PATH_MAX];
	char vifmrc[PATH_MAX];

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

	char vifmrc[PATH_MAX];
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
store_config_paths(void)
{
	LOG_FUNC_ENTER;

	char *fuse_home;
	const char *trash_base = path_exists_at(env_get(VIFM_EV), TRASH, DEREF)
	                       ? cfg.config_dir
	                       : cfg.data_dir;
	const char *base = path_exists(cfg.data_dir, DEREF)
	                 ? cfg.data_dir
	                 : cfg.config_dir;

	snprintf(cfg.home_dir, sizeof(cfg.home_dir), "%s/", env_get(HOME_EV));
	copy_str(cfg.config_dir, sizeof(cfg.config_dir), env_get(VIFM_EV));
	snprintf(cfg.colors_dir, sizeof(cfg.colors_dir), "%s/colors/",
			cfg.config_dir);
	snprintf(cfg.trash_dir, sizeof(cfg.trash_dir), "%%r/.vifm-Trash,%s/" TRASH,
			trash_base);
	snprintf(cfg.log_file, sizeof(cfg.log_file), "%s/" LOG, base);

	fuse_home = format_str("%s/fuse/", base);
	(void)cfg_set_fuse_home(fuse_home);
	free(fuse_home);
}

/* Ensures existence of configuration and data directories.  Performs first run
 * initialization. */
static void
setup_dirs(void)
{
	LOG_FUNC_ENTER;

	char rc_file[PATH_MAX];

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

	char src[PATH_MAX];
	char dst[PATH_MAX];

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
	char scripts[PATH_MAX];
	char readme[PATH_MAX];
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

	char src[PATH_MAX];
	char dst[PATH_MAX];

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

	set_user_mark('H', cfg.home_dir, NO_MARK_FILE);
	set_user_mark('z', cfg.config_dir, NO_MARK_FILE);
}

void
cfg_load(void)
{
	const char *rc;
	const int prev_global_local_settings = curr_stats.global_local_settings;

	/* Make changes of usually local settings during configuration affect all
	 * views. */
	curr_stats.global_local_settings = 1;

	/* Try to load global configuration. */
	(void)cfg_source_file(PACKAGE_SYSCONF_DIR "/" VIFMRC);

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

	FILE *fp;
	int result;
	SourcingState sourcing_state;

	if((fp = os_fopen(filename, "r")) == NULL)
	{
		return 1;
	}

	sourcing_state = curr_stats.sourcing_state;
	curr_stats.sourcing_state = SOURCING_PROCESSING;

	result = source_file_internal(fp, filename);

	curr_stats.sourcing_state = sourcing_state;

	fclose(fp);
	return result;
}

/* Returns non-zero on error. */
static int
source_file_internal(FILE *fp, const char filename[])
{
	char line[MAX_VIFMRC_LINE_LEN + 1];
	char *next_line = NULL;
	int line_num;
	int encoutered_errors = 0;

	if(fgets(line, sizeof(line), fp) == NULL)
	{
		/* File is empty. */
		return 0;
	}
	chomp(line);

	commands_scope_start();

	line_num = 1;
	for(;;)
	{
		char *p;
		int line_num_delta = 0;

		while((p = next_line = read_line(fp, next_line)) != NULL)
		{
			line_num_delta++;
			p = skip_whitespace(p);
			if(*p == '"')
				continue;
			else if(*p == '\\')
				strncat(line, p + 1, sizeof(line) - strlen(line) - 1);
			else
				break;
		}
		if(exec_commands(line, curr_view, CIT_COMMAND) < 0)
		{
			show_sourcing_error(filename, line_num);
			encoutered_errors = 1;
		}
		if(curr_stats.sourcing_state == SOURCING_FINISHING)
			break;

		if(p == NULL)
		{
			/* Artificially increment line number to simulate as if all that happens
			 * after the loop relates to something past end of the file. */
			line_num++;
			break;
		}

		copy_str(line, sizeof(line), p);
		line_num += line_num_delta;
	}

	free(next_line);

	if(commands_scope_finish() != 0)
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
	/* User choice is saved by prompt_error_msgf internally. */
	(void)prompt_error_msgf("File Sourcing Error", "Error in %s at line %d",
			filename, line_num);
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
cfg_resize_histories(int new_len)
{
	const int old_len = MAX(cfg.history_len, 0);
	const int delta = new_len - old_len;

	if(new_len <= 0)
	{
		disable_history();
		return;
	}

	if(delta < 0)
	{
		decrease_history(new_len, -delta);
	}

	reallocate_history(new_len);

	if(delta > 0)
	{
		zero_new_history_items(old_len, delta);
	}

	cfg.history_len = new_len;

	if(old_len == 0)
	{
		save_view_history(&lwin, NULL, NULL, -1);
		save_view_history(&rwin, NULL, NULL, -1);
	}
}

/* Completely disables all histories and clears all of them. */
static void
disable_history(void)
{
	free_view_history(&lwin);
	free_view_history(&rwin);

	(void)hist_reset(&cfg.search_hist, cfg.history_len);
	(void)hist_reset(&cfg.cmd_hist, cfg.history_len);
	(void)hist_reset(&cfg.prompt_hist, cfg.history_len);
	(void)hist_reset(&cfg.filter_hist, cfg.history_len);

	cfg.history_len = 0;
}

/* Clears and frees directory history of the view. */
static void
free_view_history(FileView *view)
{
	cfg_free_history_items(view->history, view->history_num);
	free(view->history);
	view->history = NULL;

	view->history_num = 0;
	view->history_pos = 0;
}

/* Reduces amount of memory taken by the history.  The new_len specifies new
 * size of the history, while removed_count parameter designates number of
 * removed elements. */
static void
decrease_history(size_t new_len, size_t removed_count)
{
	reduce_view_history(&lwin, (int)new_len);
	reduce_view_history(&rwin, (int)new_len);

	hist_trunc(&cfg.search_hist, new_len, removed_count);
	hist_trunc(&cfg.cmd_hist, new_len, removed_count);
	hist_trunc(&cfg.prompt_hist, new_len, removed_count);
	hist_trunc(&cfg.filter_hist, new_len, removed_count);
}

/* Moves items of directory history when size of history becomes smaller. */
static void
reduce_view_history(FileView *view, int size)
{
	const int delta = MIN(view->history_num - size, view->history_pos);
	if(delta <= 0)
	{
		return;
	}

	cfg_free_history_items(view->history, MIN(size, delta));
	memmove(view->history, view->history + delta,
			sizeof(history_t)*(view->history_num - delta));

	if(view->history_num >= size)
	{
		view->history_num = size - 1;
	}
	view->history_pos -= delta;
}

/* Reallocates memory taken by history elements.  The new_len specifies new
 * size of the history. */
static void
reallocate_history(size_t new_len)
{
	lwin.history = reallocarray(lwin.history, new_len, sizeof(history_t));
	rwin.history = reallocarray(rwin.history, new_len, sizeof(history_t));

	cfg.cmd_hist.items = reallocarray(cfg.cmd_hist.items, new_len,
			sizeof(char *));
	cfg.search_hist.items = reallocarray(cfg.search_hist.items, new_len,
			sizeof(char *));
	cfg.prompt_hist.items = reallocarray(cfg.prompt_hist.items, new_len,
			sizeof(char *));
	cfg.filter_hist.items = reallocarray(cfg.filter_hist.items, new_len,
			sizeof(char *));
}

/* Zeroes new elements of the history.  The old_len specifies old history size,
 * while delta parameter designates number of new elements. */
static void
zero_new_history_items(size_t old_len, size_t delta)
{
	const size_t hist_item_len = sizeof(history_t)*delta;
	const size_t str_item_len = sizeof(char *)*delta;

	memset(lwin.history + old_len, 0, hist_item_len);
	memset(rwin.history + old_len, 0, hist_item_len);

	memset(cfg.cmd_hist.items + old_len, 0, str_item_len);
	memset(cfg.search_hist.items + old_len, 0, str_item_len);
	memset(cfg.prompt_hist.items + old_len, 0, str_item_len);
	memset(cfg.filter_hist.items + old_len, 0, str_item_len);
}

int
cfg_set_fuse_home(const char new_value[])
{
	char canonicalized[PATH_MAX];
#ifdef _WIN32
	char with_forward_slashes[strlen(new_value) + 1];
	strcpy(with_forward_slashes, new_value);
	to_forward_slash(with_forward_slashes);
	new_value = with_forward_slashes;
#endif
	canonicalize_path(new_value, canonicalized, sizeof(canonicalized));

	if(!is_path_absolute(new_value))
	{
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
	set_using_term_multiplexer(use_term_multiplexer);
}

void
cfg_free_history_items(const history_t history[], size_t len)
{
	size_t i;
	for(i = 0; i < len; i++)
	{
		free(history[i].dir);
		free(history[i].file);
	}
}

void
cfg_save_command_history(const char command[])
{
	if(is_history_command(command))
	{
		update_last_cmdline_command(command);
		save_into_history(command, &cfg.cmd_hist, cfg.history_len);
	}
}

void
cfg_save_search_history(const char pattern[])
{
	save_into_history(pattern, &cfg.search_hist, cfg.history_len);
}

void
cfg_save_prompt_history(const char input[])
{
	save_into_history(input, &cfg.prompt_hist, cfg.history_len);
}

void
cfg_save_filter_history(const char input[])
{
	save_into_history(input, &cfg.filter_hist, cfg.history_len);
}

/* Adaptor for the hist_add() function, which handles signed history length. */
static void
save_into_history(const char item[], hist_t *hist, int len)
{
	if(len >= 0)
	{
		hist_add(hist, item, len);
	}
}

const char *
cfg_get_last_search_pattern(void)
{
	return hist_is_empty(&cfg.search_hist) ? "" : cfg.search_hist.items[0];
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
