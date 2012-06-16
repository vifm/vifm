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

#include "../../config.h"

#define HOME_EV "HOME"
#define VIFM_EV "VIFM"
#define MYVIFMRC_EV "MYVIFMRC"
#define TRASH "Trash"
#define LOG "log"
#define VIFMRC "vifmrc"

#ifndef _WIN32
#define CP_HELP "cp " PACKAGE_DATA_DIR "/" VIFM_HELP " ~/.vifm"
#define CP_RC "cp " PACKAGE_DATA_DIR "/" VIFMRC " ~/.vifm"
#endif

#include <ctype.h> /* isalnum */
#include <errno.h>
#include <stdio.h> /* FILE */
#include <stdlib.h>
#include <string.h>

#include "../menus/menus.h"
#include "../utils/env.h"
#include "../utils/fs.h"
#include "../utils/log.h"
#include "../utils/str.h"
#ifdef _WIN32
#include "../utils/path.h"
#endif
#include "../utils/string_array.h"
#include "../bookmarks.h"
#include "../color_scheme.h"
#include "../commands.h"
#include "../fileops.h"
#include "../filelist.h"
#include "../opt_handlers.h"
#include "../status.h"

#include "config.h"

#define MAX_LEN 1024

config_t cfg;

static void find_home_dir(void);
static int try_home_envvar_for_home(void);
static int try_userprofile_envvar_for_home(void);
static int try_homepath_envvar_for_home(void);
static void find_config_dir(void);
static int try_vifm_envvar_for_conf(void);
static int try_exe_directory_for_conf(void);
static int try_home_envvar_for_conf(void);
static int try_appdata_for_conf(void);
static void find_config_file(void);
static int try_myvifmrc_envvar_for_vifmrc(void);
static int try_exe_directory_for_vifmrc(void);
static int try_vifm_vifmrc_for_vifmrc(void);
static void store_config_paths(void);
static void create_config_dir(void);
#ifndef _WIN32
static void create_help_file(void);
static void create_rc_file(void);
#endif
static int source_file_internal(FILE *fp, const char *filename);
static void free_view_history(FileView *view);
static void reduce_view_history(FileView *view, size_t size);

void
init_config(void)
{
	const char *p;

	cfg.num_bookmarks = 0;
	cfg.command_num = 0;
	cfg.vim_filter = 0;
	cfg.show_one_window = 0;

	cfg.search_history_len = 15;
	cfg.search_history_num = -1;
	cfg.search_history = calloc(cfg.search_history_len, sizeof(char *));

	cfg.cmd_history_len = 15;
	cfg.cmd_history_num = -1;
	cfg.cmd_history = calloc(cfg.cmd_history_len, sizeof(char *));

	cfg.prompt_history_len = 15;
	cfg.prompt_history_num = -1;
	cfg.prompt_history = calloc(cfg.prompt_history_len, sizeof(char *));

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
	cfg.fuse_home = strdup("/tmp/vifm_FUSE");
	cfg.use_screen = 0;
	cfg.history_len = 15;
	cfg.use_vim_help = 0;
	cfg.wild_menu = 0;
	cfg.ignore_case = 0;
	cfg.smart_case = 0;
	cfg.hl_search = 1;
	cfg.vifm_info = VIFMINFO_BOOKMARKS;
	cfg.auto_ch_pos = 1;
	cfg.timeout_len = 1000;
	cfg.scroll_off = 0;
	cfg.gdefault = 0;
#ifndef _WIN32
	cfg.slow_fs_list = strdup("");
#endif
	cfg.scroll_bind = 0;
	cfg.wrap_scan = 1;
	cfg.inc_search = 0;
	cfg.selection_cp = 1;
	cfg.last_status = 1;
	cfg.tab_stop = 8;
	cfg.ruler_format = strdup("%=%l/%S ");
	cfg.status_line = strdup("");

	cfg.lines = -1;
	cfg.columns = -1;

#ifndef _WIN32
	snprintf(cfg.log_file, sizeof(cfg.log_file), "/var/log/vifm-startup-log");
#else
	GetModuleFileNameA(NULL, cfg.log_file, sizeof(cfg.log_file));
	to_forward_slash(cfg.log_file);
	*strrchr(cfg.log_file, '/') = '\0';
	strcat(cfg.log_file, "/startup-log");
#endif

	p = env_get("SHELL");
	if(p == NULL || *p == '\0')
	{
#ifndef _WIN32
		cfg.shell = strdup("sh");
#else
		cfg.shell = strdup("cmd");
#endif
	}
	else
	{
		cfg.shell = strdup(p);
	}

#ifndef _WIN32
	/* Maximum argument length to pass to the shell */
	if((cfg.max_args = sysconf(_SC_ARG_MAX)) == 0)
#endif
		cfg.max_args = 4096; /* POSIX MINIMUM */
}

/* searches for configuration file and directories, stores them and ensures
 * existence of some of them */
void
set_config_paths(void)
{
	LOG_FUNC_ENTER;

	find_home_dir();
	find_config_dir();
	find_config_file();

	store_config_paths();

	create_config_dir();
}

/* tries to find home directory */
static void
find_home_dir(void)
{
	LOG_FUNC_ENTER;

	if(try_home_envvar_for_home())
		return;
	if(try_userprofile_envvar_for_home())
		return;
	if(try_homepath_envvar_for_home())
		return;
}

/* tries to use HOME environment variable to find home directory */
static int
try_home_envvar_for_home(void)
{
	LOG_FUNC_ENTER;

	const char *home = env_get(HOME_EV);
	return home != NULL && is_dir(home);
}

/* tries to use USERPROFILE environment variable to find home directory */
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

/* tries to find configuration directory */
static void
find_config_dir(void)
{
	LOG_FUNC_ENTER;

	if(try_vifm_envvar_for_conf())
		return;
	if(try_exe_directory_for_conf())
		return;
	if(try_home_envvar_for_conf())
		return;
	if(try_appdata_for_conf())
		return;
}

/* tries to use VIFM environment variable to find configuration directory */
static int
try_vifm_envvar_for_conf(void)
{
	LOG_FUNC_ENTER;

	const char *vifm = env_get(VIFM_EV);
	return vifm != NULL && is_dir(vifm);
}

/* tries to use directory of executable file as configuration directory */
static int
try_exe_directory_for_conf(void)
{
	LOG_FUNC_ENTER;

#ifndef _WIN32
	return 0;
#else
	char exe_dir[PATH_MAX];
	GetModuleFileNameA(NULL, exe_dir, sizeof(exe_dir));
	to_forward_slash(exe_dir);
	*strrchr(exe_dir, '/') = '\0';
	if(!path_exists_at(exe_dir, VIFMRC))
		return 0;
	env_set(VIFM_EV, exe_dir);
	return 1;
#endif
}

/* tries to use $HOME/.vifm as configuration directory */
static int
try_home_envvar_for_conf(void)
{
	LOG_FUNC_ENTER;

	char vifm[PATH_MAX];
	const char *home = env_get(HOME_EV);
	if(home == NULL || !is_dir(home))
		return 0;
	snprintf(vifm, sizeof(vifm), "%s/.vifm", home);
#ifdef _WIN32
	if(!is_dir(vifm))
		return 0;
#endif
	env_set(VIFM_EV, vifm);
	return 1;
}

/* tries to use $APPDATA/Vifm as configuration directory */
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

/* tries to find configuration file */
static void
find_config_file(void)
{
	LOG_FUNC_ENTER;

	if(try_myvifmrc_envvar_for_vifmrc())
		return;
	if(try_exe_directory_for_vifmrc())
		return;
	if(try_vifm_vifmrc_for_vifmrc())
		return;
}

/* tries to use $MYVIFMRC as configuration file */
static int
try_myvifmrc_envvar_for_vifmrc(void)
{
	LOG_FUNC_ENTER;

	const char *myvifmrc = env_get(MYVIFMRC_EV);
	return myvifmrc != NULL && path_exists(myvifmrc);
}

/* tries to use vifmrc in directory of executable file as configuration file */
static int
try_exe_directory_for_vifmrc(void)
{
	LOG_FUNC_ENTER;

#ifndef _WIN32
	return 0;
#else
	char vifmrc[PATH_MAX];
	GetModuleFileNameA(NULL, vifmrc, sizeof(vifmrc));
	to_forward_slash(vifmrc);
	*strrchr(vifmrc, '/') = '\0';
	strcat(vifmrc, "/" VIFMRC);
	if(!path_exists(vifmrc))
		return 0;
	env_set(MYVIFMRC_EV, vifmrc);
	return 1;
#endif
}

/* tries to use $VIFM/vifmrc as configuration file */
static int
try_vifm_vifmrc_for_vifmrc(void)
{
	LOG_FUNC_ENTER;

	char vifmrc[PATH_MAX];
	const char *vifm = env_get(VIFM_EV);
	if(vifm == NULL || !is_dir(vifm))
		return 0;
	snprintf(vifmrc, sizeof(vifmrc), "%s/" VIFMRC, vifm);
	if(!path_exists(vifmrc))
		return 0;
	env_set(MYVIFMRC_EV, vifmrc);
	return 1;
}

/* writes path configuration file and directories for further usage */
static void
store_config_paths(void)
{
	LOG_FUNC_ENTER;

	snprintf(cfg.home_dir, sizeof(cfg.home_dir), "%s/", env_get(HOME_EV));
	snprintf(cfg.config_dir, sizeof(cfg.config_dir), "%s", env_get(VIFM_EV));
	snprintf(cfg.trash_dir, sizeof(cfg.trash_dir), "%s/" TRASH, cfg.config_dir);
	snprintf(cfg.log_file, sizeof(cfg.log_file), "%s/" LOG, cfg.config_dir);
}

/* ensures existence of configuration directory */
static void
create_config_dir(void)
{
	LOG_FUNC_ENTER;

	/* ensure existence of configuration directory */
	if(!is_dir(cfg.config_dir))
	{
#ifndef _WIN32
		FILE *f;
		char help_file[PATH_MAX];
		char rc_file[PATH_MAX];

		if(make_dir(cfg.config_dir, 0777) != 0)
			return;

		snprintf(help_file, sizeof(help_file), "%s/" VIFM_HELP, cfg.config_dir);
		if((f = fopen(help_file, "r")) == NULL)
			create_help_file();
		else
			fclose(f);

		snprintf(rc_file, sizeof(rc_file), "%s/" VIFMRC, cfg.config_dir);
		if((f = fopen(rc_file, "r")) == NULL)
			create_rc_file();
		else
			fclose(f);
#else
		if(make_dir(cfg.config_dir, 0777) != 0)
			return;
#endif
	}
}

#ifndef _WIN32
static void
create_help_file(void)
{
	LOG_FUNC_ENTER;

	char command[PATH_MAX];

	snprintf(command, sizeof(command), CP_HELP);
	fprintf(stderr, "%s", command);
	file_exec(command);
}

static void
create_rc_file(void)
{
	LOG_FUNC_ENTER;

	char command[PATH_MAX];

	snprintf(command, sizeof(command), CP_RC);
	file_exec(command);
	add_bookmark('H', cfg.home_dir, "../");
	add_bookmark('z', cfg.config_dir, "../");
}
#endif

/* ensures existence of trash directory */
void
create_trash_dir(void)
{
	LOG_FUNC_ENTER;

	if(is_dir_writable(cfg.trash_dir))
		return;

	if(make_dir(cfg.trash_dir, 0777) != 0)
		show_error_msgf("Error Setting Trash Directory",
				"Could not set trash directory to %s: %s",
				cfg.trash_dir, strerror(errno));
}

static void
load_view_defaults(FileView *view)
{
	int i;

	strncpy(view->regexp, "\\..~$", sizeof(view->regexp) - 1);
	replace_string(&view->prev_filter, "");
	set_filename_filter(view, "");
	view->invert = 1;

#ifndef _WIN32
	view->sort[0] = SORT_BY_NAME;
#else
	view->sort[0] = SORT_BY_INAME;
#endif
	for(i = 1; i < NUM_SORT_OPTIONS; i++)
		view->sort[i] = NUM_SORT_OPTIONS + 1;
}

void
load_default_configuration(void)
{
	load_view_defaults(&lwin);
	load_view_defaults(&rwin);
}

void
exec_config(void)
{
	(void)source_file(env_get(MYVIFMRC_EV));
}

int
source_file(const char *file)
{
	FILE *fp;
	int result;
	SourcingState sourcing_state;

	if((fp = fopen(file, "r")) == NULL)
		return 1;

	sourcing_state = curr_stats.sourcing_state;
	curr_stats.sourcing_state = SOURCING_PROCESSING;

	result = source_file_internal(fp, file);

	curr_stats.sourcing_state = sourcing_state;

	fclose(fp);
	return result;
}

static int
source_file_internal(FILE *fp, const char *filename)
{
	char line[MAX_LEN*2];
	int line_num;

	if(fgets(line, MAX_LEN, fp) == NULL)
	{
		return 1;
	}

	line_num = 1;
	for(;;)
	{
		char next_line[MAX_LEN];
		char *p;
		int line_num_delta = 0;

		if((p = fgets(next_line, sizeof(next_line), fp)) != NULL)
		{
			do
			{
				line_num_delta++;
				p = skip_whitespace(p);
				chomp(p);
				if(*p == '"')
					continue;
				else if(*p == '\\')
					strncat(line, p + 1, sizeof(line) - strlen(line) - 1);
				else
					break;
			}
			while((p = fgets(next_line, sizeof(next_line), fp)) != NULL);
		}
		chomp(line);
		if(exec_commands(line, curr_view, GET_COMMAND) < 0)
		{
			show_error_msgf("File Sourcing Error", "Error in %s at %d line", filename,
					line_num);
		}
		if(curr_stats.sourcing_state == SOURCING_FINISHING)
			break;
		if(p == NULL)
			break;
		strcpy(line, p);
		line_num += line_num_delta;
	}

	return 0;
}

static int
is_conf_file(const char *file)
{
	FILE *fp;
	char line[MAX_LEN];

	if((fp = fopen(file, "r")) == NULL)
		return 0;

	while(fgets(line, sizeof(line), fp))
	{
		if(skip_whitespace(line)[0] == '#')
		{
			fclose(fp);
			return 1;
		}
	}

	fclose(fp);

	return 0;
}

int
is_old_config(void)
{
	return is_conf_file(env_get(MYVIFMRC_EV));
}

int
are_old_color_schemes(void)
{
	char colors_dir[PATH_MAX];
	snprintf(colors_dir, sizeof(colors_dir), "%s/colors", cfg.config_dir);
	return !is_dir(colors_dir) && path_exists_at(cfg.config_dir, "colorschemes");
}

const char *
get_vicmd(int *bg)
{
	if(curr_stats.is_console)
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
resize_history(size_t new_len)
{
	int delta;

	if(new_len == 0)
	{
		free_view_history(&lwin);
		free_view_history(&rwin);

		cfg.history_len = new_len;
		cfg.cmd_history_len = new_len;
		cfg.prompt_history_len = new_len;
		cfg.search_history_len = new_len;

		cfg.cmd_history_num = -1;
		cfg.prompt_history_num = -1;
		cfg.search_history_num = -1;
		return;
	}

	if(cfg.history_len > new_len)
	{
		reduce_view_history(&lwin, new_len);
		reduce_view_history(&rwin, new_len);
	}

	lwin.history = realloc(lwin.history, sizeof(history_t)*new_len);
	rwin.history = realloc(rwin.history, sizeof(history_t)*new_len);

	if(cfg.history_len <= 0)
	{
		cfg.history_len = new_len;

		save_view_history(&lwin, NULL, NULL, -1);
		save_view_history(&rwin, NULL, NULL, -1);
	}
	else
	{
		cfg.history_len = new_len;
	}

	delta = (int)new_len - cfg.cmd_history_len;
	if(delta < 0 && cfg.cmd_history_len > 0)
	{
		size_t i;
		for(i = new_len; i < cfg.cmd_history_len; i++)
			free(cfg.cmd_history[i]);
		for(i = new_len; i < cfg.cmd_history_len; i++)
			free(cfg.prompt_history[i]);
		for(i = new_len; i < cfg.cmd_history_len; i++)
			free(cfg.search_history[i]);
	}
	cfg.cmd_history = realloc(cfg.cmd_history, new_len*sizeof(char *));
	cfg.prompt_history = realloc(cfg.prompt_history, new_len*sizeof(char *));
	cfg.search_history = realloc(cfg.search_history, new_len*sizeof(char *));
	if(delta > 0)
	{
		size_t i;
		for(i = cfg.cmd_history_len; i < new_len; i++)
			cfg.cmd_history[i] = NULL;
		for(i = cfg.cmd_history_len; i < new_len; i++)
			cfg.prompt_history[i] = NULL;
		for(i = cfg.cmd_history_len; i < new_len; i++)
			cfg.search_history[i] = NULL;
	}

	cfg.cmd_history_len = new_len;
	cfg.prompt_history_len = new_len;
	cfg.search_history_len = new_len;
}

/* Clears and frees directory history of the view. */
static void
free_view_history(FileView *view)
{
	free(view->history);
	view->history = NULL;
	view->history_num = 0;
	view->history_pos = 0;
}

/* Moves items of directory history when size of history becomes smaller. */
static void
reduce_view_history(FileView *view, size_t size)
{
	int i;
	int delta;

	delta = MIN(view->history_num - (int)size, view->history_pos);
	if(delta <= 0)
		return;

	for(i = 0; i < size && i + delta <= view->history_num; i++)
	{
		strncpy(view->history[i].file, view->history[i + delta].file,
				sizeof(view->history[i].file));
		strncpy(view->history[i].dir, view->history[i + delta].dir,
				sizeof(view->history[i].dir));
	}

	if(view->history_num >= size)
		view->history_num = size - 1;
	view->history_pos -= delta;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
