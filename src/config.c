/* vifm
 * Copyright (C) 2001 Ken Steen.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include "../config.h"

#define CP_HELP "cp " PACKAGE_DATA_DIR "/vifm-help.txt ~/.vifm"
#define CP_RC "cp " PACKAGE_DATA_DIR "/vifmrc ~/.vifm"
#define CP_STARTUP "cp " PACKAGE_DATA_DIR "/startup ~/.vifm"

#include <ctype.h> /* isalnum */
#include <stdio.h> /* FILE */
#include <stdlib.h> /* getenv */
#include <string.h>
#include <sys/stat.h> /* mkdir */
#include <unistd.h> /* chdir */

#include "bookmarks.h"
#include "color_scheme.h"
#include "commands.h"
#include "cmds.h"
#include "config.h"
#include "crc32.h"
#include "fileops.h"
#include "filetype.h"
#include "menus.h"
#include "registers.h"
#include "status.h"
#include "utils.h"

#define MAX_LEN 1024

Config cfg;

void
init_config(void)
{
	cfg.num_bookmarks = 0;
	cfg.using_default_config = 0;
	cfg.command_num = 0;
	cfg.filetypes_num = 0;
	cfg.nmapped_num = 0;
	cfg.vim_filter = 0;
	cfg.show_one_window = 0;

	cfg.search_history_len = 15;
	cfg.search_history_num = -1;
	cfg.search_history = (char **)calloc(cfg.search_history_len, sizeof(char*));
	cfg.cmd_history_len = 15;
	cfg.cmd_history_num = -1;
	cfg.cmd_history = (char **)calloc(cfg.cmd_history_len, sizeof(char *));
	cfg.auto_execute = 0;
	cfg.time_format = strdup(" %m/%d %H:%M");
	cfg.wrap_quick_view = 1;
	cfg.color_scheme_num = 0;
	cfg.color_scheme_cur = 0;
	cfg.color_scheme = 1;
	cfg.use_iec_prefixes = 0;
	cfg.undo_levels = 100;
	cfg.sort_numbers = 0;
	cfg.save_location = 0;
	cfg.follow_links = 1;
	cfg.fast_run = 0;
	cfg.confirm = 1;
	cfg.vi_command = strdup("vim");
	cfg.use_trash = 1;
	cfg.fuse_home = strdup("/tmp/vifm_FUSE");
	cfg.use_screen = 0;
	cfg.history_len = 15;
	cfg.use_vim_help = 0;
	cfg.invert_cur_line = 0;
	cfg.wild_menu = 0;
	cfg.ignore_case = 0;
	cfg.smart_case = 0;
	cfg.hl_search = 1;

	/* Maximum argument length to pass to the shell */
	if((cfg.max_args = sysconf(_SC_ARG_MAX)) == 0)
		cfg.max_args = 4096; /* POSIX MINIMUM */
}

static void
create_help_file(void)
{
	char command[PATH_MAX];

	snprintf(command, sizeof(command), CP_HELP);
	fprintf(stderr, "%s", command);
	file_exec(command);
}

static void
create_rc_file(void)
{
	char command[PATH_MAX];

	snprintf(command, sizeof(command), CP_RC);
	file_exec(command);
	add_bookmark('H', cfg.home_dir, "../");
	add_bookmark('z', cfg.config_dir, "../");
}

static void
create_startup_file(void)
{
	char command[PATH_MAX];

	snprintf(command, sizeof(command), CP_STARTUP);
	file_exec(command);
}

/* This is just a safety check so that vifm will still load and run if
 * the configuration file is not present.
 */
static void
load_default_configuration(void)
{
	cfg.using_default_config = 1;

	read_color_scheme_file();
}

void
set_config_dir(void)
{
	char *home_dir;
	FILE *f;
	char help_file[PATH_MAX];
	char rc_file[PATH_MAX];
	char startup_file[PATH_MAX];
	char *escaped;

	home_dir = getenv("HOME");
	if(home_dir == NULL)
		return;

	snprintf(rc_file, sizeof(rc_file), "%s/.vifm/vifmrc", home_dir);
	snprintf(help_file, sizeof(help_file), "%s/.vifm/vifm-help_txt", home_dir);
	snprintf(startup_file, sizeof(startup_file), "%s/.vifm/startup", home_dir);
	snprintf(cfg.home_dir, sizeof(cfg.home_dir), "%s/", home_dir);
	snprintf(cfg.config_dir, sizeof(cfg.config_dir), "%s/.vifm", home_dir);
	snprintf(cfg.trash_dir, sizeof(cfg.trash_dir), "%s/.vifm/Trash", home_dir);
	snprintf(cfg.log_file, sizeof(cfg.log_file), "%s/.vifm/log", home_dir);

	escaped = escape_filename(cfg.trash_dir, 0, 0);
	strcpy(cfg.escaped_trash_dir, escaped);
	free(escaped);

	if(chdir(cfg.config_dir))
	{
		if(mkdir(cfg.config_dir, 0777))
			return;
		if(mkdir(cfg.trash_dir, 0777))
			return;
		if((f = fopen(help_file, "r")) == NULL)
			create_help_file();
		if((f = fopen(rc_file, "r")) == NULL)
			create_rc_file();
		if((f = fopen(startup_file, "r")) == NULL)
			create_startup_file();
	}
}

static void
load_view_defaults(FileView *view)
{
	strncpy(view->regexp, "\\..~$", sizeof(view->regexp) - 1);

	view->filename_filter = strdup("");
	view->prev_filter = strdup("");
	view->invert = TRUE;

	view->sort_type = SORT_BY_NAME;
}

const char *
conv_udf_name(const char *cmd)
{
	static const char *nums[] = {
		"ZERO", "ONE", "TWO", "THREE", "FOUR",
		"FIVE", "SIX", "SEVEN", "EIGHT", "NINE"
	};
	static char buf[256];
	char *p = buf;
	while(*cmd != '\0')
		if(isdigit(*cmd))
		{
			if(p + strlen(nums[*cmd - '0']) + 1 >= buf + sizeof(buf))
				break;
			strcpy(p, nums[*cmd - '0']);
			p += strlen(p);
			cmd++;
		}
		else
		{
			*p++ = *cmd++;
		}
	*p = '\0';
	return buf;
}

/* Returns zero when default configuration is used */
int
read_config_file(void)
{
	FILE *fp;
	char config_file[PATH_MAX];
//	char line[MAX_LEN];
	char *col_scheme = NULL;
//	char *s1 = NULL;
//	char *s2 = NULL;
//	char *s3 = NULL;
//	char *s4 = NULL;
//	char *sx = NULL;
//	int args;

	snprintf(config_file, sizeof(config_file), "%s/vifmrc", cfg.config_dir);

	load_view_defaults(&lwin);
	load_view_defaults(&rwin);

	if((fp = fopen(config_file, "r")) == NULL)
	{
		fprintf(stdout, "Unable to find configuration file.\n Using defaults.");
		load_default_configuration();
		return 0;
	}

//	while(fgets(line, sizeof(line), fp))
//	{
//		args = 0;
//		if(line[0] == '#')
//			continue;
//
//		if((sx = s1 = strchr(line, '=')) != NULL)
//		{
//			s1++;
//			chomp(s1);
//			*sx = '\0';
//			args = 1;
//		}
//		else
//			continue;
//		if((sx = s2 = strchr(s1, '=')) != NULL)
//		{
//			s2++;
//			chomp(s2);
//			*sx = '\0';
//			args = 2;
//		}
//		/* COMMAND is handled here so that the command can have an '=' */
//		if(!strcmp(line, "COMMAND"))
//		{
//			char buf[PATH_MAX];
//			s1 = (char *)conv_udf_name(s1);
//			snprintf(buf, sizeof(buf), "command %s %s", s1, s2);
//			execute_cmd(buf);
//		}
//		else
//		{
//			if(args == 2 && (sx = s3 = strchr(s2, '=')) != NULL)
//			{
//				s3++;
//				chomp(s3);
//				*sx = '\0';
//				args = 3;
//			}
//			if(args == 3 && (sx = s4 = strchr(s3, '=')) != NULL)
//			{
//				s4++;
//				chomp(s4);
//				*sx = '\0';
//				args = 4;
//			}
//		}
//		if(args == 1)
//		{
//			if(!strcmp(line, "VI_COMMAND"))
//			{
//				free(cfg.vi_command);
//				cfg.vi_command = strdup(s1);
//				continue;
//			}
//			if(!strcmp(line, "USE_TRASH"))
//			{
//				cfg.use_trash = atoi(s1);
//				continue;
//			}
//			if(!strcmp(line, "UNDO_LEVELS"))
//			{
//				cfg.undo_levels = atoi(s1);
//				continue;
//			}
//			if(!strcmp(line, "USE_ONE_WINDOW"))
//			{
//				cfg.show_one_window = atoi(s1);
//				continue;
//			}
//			if(!strcmp(line, "USE_SCREEN"))
//			{
//				cfg.use_screen = atoi(s1);
//				continue;
//			}
//			if(!strcmp(line, "HISTORY_LENGTH"))
//			{
//				cfg.history_len = atoi(s1);
//				continue;
//			}
//			if(!strcmp(line, "FAST_RUN"))
//			{
//				cfg.fast_run = atoi(s1);
//				continue;
//			}
//			if(!strcmp(line, "SORT_NUMBERS"))
//			{
//				cfg.sort_numbers = atoi(s1);
//				continue;
//			}
//			if(!strcmp(line, "LEFT_WINDOW_SORT_TYPE"))
//			{
//				lwin.sort_type = atoi(s1);
//				if(lwin.sort_type < 0)
//				{
//					lwin.sort_type = -lwin.sort_type;
//					lwin.sort_descending = 1;
//				}
//				lwin.sort_type--;
//				continue;
//			}
//			if(!strcmp(line, "RIGHT_WINDOW_SORT_TYPE"))
//			{
//				rwin.sort_type = atoi(s1);
//				if(rwin.sort_type < 0)
//				{
//					rwin.sort_type = -rwin.sort_type;
//					rwin.sort_descending = 1;
//				}
//				rwin.sort_type--;
//				continue;
//			}
//			if(!strcmp(line, "LWIN_FILTER"))
//			{
//				lwin.filename_filter = (char *)realloc(lwin.filename_filter,
//						strlen(s1) + 1);
//				strcpy(lwin.filename_filter, s1);
//				lwin.prev_filter = (char *)realloc(lwin.prev_filter, strlen(s1) + 1);
//				strcpy(lwin.prev_filter, s1);
//				continue;
//			}
//			if(!strcmp(line, "LWIN_INVERT"))
//			{
//				lwin.invert = atoi(s1);
//				continue;
//			}
//			if(!strcmp(line, "RWIN_FILTER"))
//			{
//				rwin.filename_filter = (char *)realloc(rwin.filename_filter,
//						strlen(s1) + 1);
//				strcpy(rwin.filename_filter, s1);
//				rwin.prev_filter = (char *)realloc(rwin.prev_filter, strlen(s1) + 1);
//				strcpy(rwin.prev_filter, s1);
//				continue;
//			}
//			if(!strcmp(line, "RWIN_INVERT"))
//			{
//				rwin.invert = atoi(s1);
//				continue;
//			}
//			if(!strcmp(line, "USE_VIM_HELP"))
//			{
//				cfg.use_vim_help = atoi(s1);
//				continue;
//			}
//			if(!strcmp(line, "SAVE_LOCATION"))
//			{
//				cfg.save_location = atoi(s1);
//				continue;
//			}
//			if(!strcmp(line, "FOLLOW_LINKS"))
//			{
//				cfg.follow_links = atoi(s1);
//				continue;
//			}
//			if(!strcmp(line, "RUN_EXECUTABLE"))
//			{
//				cfg.auto_execute = atoi(s1);
//				continue;
//			}
//			if(!strcmp(line, "USE_IEC_PREFIXES"))
//			{
//				cfg.use_iec_prefixes = atoi(s1);
//				continue;
//			}
//			if(!strcmp(line, "COLOR_SCHEME"))
//			{
//				col_scheme = strdup(s1);
//				continue;
//			}
//			if(!strcmp(line, "TIME_STAMP_FORMAT"))
//			{
//				free(cfg.time_format);
//				cfg.time_format = malloc(1 + strlen(s1) + 1);
//				strcpy(cfg.time_format, " ");
//				strcat(cfg.time_format, s1);
//				continue;
//			}
//			if(!strcmp(line, "FUSE_HOME"))
//			{
//				free(cfg.fuse_home);
//				cfg.fuse_home = strdup(s1);
//				continue;
//			}
//			if(!strcmp(line, "LWIN_PATH"))
//			{
//				strcpy(lwin.curr_dir, s1);
//				cfg.save_location = 1;
//				continue;
//			}
//			if(!strcmp(line, "RWIN_PATH"))
//			{
//				strcpy(rwin.curr_dir, s1);
//				cfg.save_location = 1;
//				continue;
//			}
//		}
//		if(args == 3)
//		{
//			if(!strcmp(line, "BOOKMARKS"))
//			{
//				if(isalnum(*s1))
//					add_bookmark(*s1, s2, s3);
//				continue;
//			}
//			if(!strcmp(line, "FILETYPE")) /* backward compatibility */
//			{
//				add_filetype(s1, s2, s3);
//				add_fileviewer(s2, "");
//				continue;
//			}
//		}
//		if(args == 4)
//		{
//			if(!strcmp(line, "FILETYPE"))
//			{
//				add_filetype(s1, s2, s4);
//				add_fileviewer(s2, s3);
//				continue;
//			}
//		}
//	}
//
//	fclose(fp);

	read_color_scheme_file();
	if(col_scheme != NULL)
	{
		cfg.color_scheme_cur = find_color_scheme(col_scheme);
		if(cfg.color_scheme_cur < 0)
			cfg.color_scheme_cur = 0;
		cfg.color_scheme = 1 + MAXNUM_COLOR*cfg.color_scheme_cur;
		free(col_scheme);
	}

	return 1;
}

void
read_info_file(void)
{
	FILE *fp;
	char info_file[PATH_MAX];
	char line[MAX_LEN];
	char *s1 = NULL;
	char *sx = NULL;
	int args;

	snprintf(info_file, sizeof(info_file), "%s/vifminfo", cfg.config_dir);

	if((fp = fopen(info_file, "r")) == NULL)
		return;

	while(fgets(line, sizeof(line), fp))
	{
		args = 0;
		if(line[0] == '#')
			continue;

		if((sx = s1 = strchr(line, '=')) != NULL)
		{
			s1++;
			chomp(s1);
			*sx = '\0';
			args = 1;
		}
		else
			continue;

		if(!strcmp(line, "LWIN_PATH"))
		{
			strcpy(lwin.curr_dir, s1);
			continue;
		}
		if(!strcmp(line, "RWIN_PATH"))
		{
			strcpy(rwin.curr_dir, s1);
			continue;
		}
	}

	fclose(fp);
}

void
write_config_file(void)
{
}

void
write_info_file(void)
{
	FILE *fp;
	char info_file[PATH_MAX];

	if(!cfg.save_location)
		return;

	snprintf(info_file, sizeof(info_file), "%s/vifminfo", cfg.config_dir);

	if((fp = fopen(info_file, "w")) == NULL)
		return;

	fprintf(fp, "# The startup location of panes after :wq.\n");
	fprintf(fp, "# Used only if SAVE_LOCATION in vifmrc is set to 1.\n");
	fprintf(fp, "\nLWIN_PATH=%s\n", lwin.curr_dir);
	fprintf(fp, "\nRWIN_PATH=%s\n", rwin.curr_dir);

	fclose(fp);
}

void
exec_config(void)
{
	FILE *fp;
	char startup_file[PATH_MAX];
	char line[MAX_LEN*2];
	char next_line[MAX_LEN];

	snprintf(startup_file, sizeof(startup_file), "%s/startup", cfg.config_dir);

	if((fp = fopen(startup_file, "r")) == NULL)
		return;

	if(fgets(line, MAX_LEN, fp) != NULL)
	{
		for(;;)
		{
			char *p;

			if((p = fgets(next_line, sizeof(next_line), fp)) != NULL)
			{
				do
				{
					while(isspace(*p))
						p++;
					chomp(p);
					if(*p == '\\')
						strncat(line, p + 1, sizeof(line));
					else
						break;
				}
				while((p = fgets(next_line, sizeof(next_line), fp)) != NULL);
			}
			chomp(line);
			exec_commands(line, curr_view, 0);
			if(p == NULL)
				break;
			strcpy(line, p);
		}
	}

	fclose(fp);
}

int
is_old_config(void)
{
	char config_file[PATH_MAX];
	FILE *fp;
	char line[MAX_LEN];

	snprintf(config_file, sizeof(config_file), "%s/vifmrc", cfg.config_dir);

	if((fp = fopen(config_file, "r")) == NULL)
		return 0;

	while(fgets(line, sizeof(line), fp))
	{
		int i = 0;
		while(isspace(line[i]))
			i++;
		if(line[i] == '#')
			return 1;
	}

	fclose(fp);

	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
