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

#define CP_HELP "cp /usr/local/share/vifm/vifm-help.txt ~/.vifm"
#define CP_RC "cp /usr/local/share/vifm/vifmrc ~/.vifm"

#include<stdio.h> /* FILE */
#include<stdlib.h> /* getenv */
#include<unistd.h> /* chdir */
#include<sys/stat.h> /* mkdir */
#include<string.h>
#include<ctype.h> /* isalnum */

#include"utils.h"
#include"bookmarks.h"
#include"color_scheme.h"
#include"fileops.h"
#include"filetype.h"
#include"registers.h"
#include"status.h"
#include"commands.h"
#include"config.h"
#include"menus.h"

#define MAX_LEN 1024
#define DEFAULT_FILENAME_FILTER "\\.o$"

void
init_config(void)
{
	int i;

	/* init bookmarks */
	for (i = 0; i < NUM_BOOKMARKS; ++i)
	{
		bookmarks[i].directory = NULL;
		bookmarks[i].file = NULL;
	}
	cfg.num_bookmarks = 0;
	for ( i = 0; i < NUM_REGISTERS; ++i)
	{
		reg[i].name = valid_registers[i];
		reg[i].num_files = 0;
		reg[i].files = NULL;
		reg[i].deleted = 0;
	}
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
	add_bookmark('H', getenv("HOME"), "../");
	add_bookmark('z', cfg.config_dir, "../");
}

/* This is just a safety check so that vifm will still load and run if
 * the configuration file is not present.
 */
static void
load_default_configuration(void)
{
	cfg.using_default_config = 1;
	cfg.use_trash = 1;
	cfg.vi_command = strdup("vim");
/*_SZ_BEGIN_*/
	cfg.fuse_home = strdup("/tmp/vifm_FUSE");
/*_SZ_END_*/
	cfg.use_screen = 0;
	cfg.history_len = 15;
	cfg.use_vim_help = 0;
	strncpy(lwin.regexp, "\\..~$", sizeof(lwin.regexp) -1);

 	lwin.filename_filter = (char *)realloc(lwin.filename_filter, 
 			strlen(DEFAULT_FILENAME_FILTER) +1);
 	strcpy(lwin.filename_filter, DEFAULT_FILENAME_FILTER);
  	lwin.prev_filter = (char *)realloc(lwin.prev_filter, 
 			strlen(DEFAULT_FILENAME_FILTER) +1);
 	strcpy(lwin.prev_filter, DEFAULT_FILENAME_FILTER);

	lwin.invert = TRUE;
	strncpy(rwin.regexp, "\\..~$", sizeof(rwin.regexp) -1);

    rwin.filename_filter = (char *)realloc(rwin.filename_filter, 
 			strlen(DEFAULT_FILENAME_FILTER) +1);
 	strcpy(rwin.filename_filter, DEFAULT_FILENAME_FILTER);
 	rwin.prev_filter = (char *)realloc(rwin.prev_filter, 
 			strlen(DEFAULT_FILENAME_FILTER) +1);
 	strcpy(rwin.prev_filter, DEFAULT_FILENAME_FILTER);

	rwin.invert = TRUE;
	lwin.sort_type = SORT_BY_NAME;
	rwin.sort_type = SORT_BY_NAME;

	read_color_scheme_file();
}

void
set_config_dir(void)
{
	char *home_dir = getenv("HOME");

	if(home_dir)
	{
		FILE *f;
		char help_file[PATH_MAX];
		char rc_file[PATH_MAX];

		snprintf(rc_file, sizeof(rc_file), "%s/.vifm/vifmrc", home_dir);
		snprintf(help_file, sizeof(help_file), "%s/.vifm/vifm-help_txt", 
				home_dir);
		snprintf(cfg.config_dir, sizeof(cfg.config_dir), "%s/.vifm", home_dir);
		snprintf(cfg.trash_dir, sizeof(cfg.trash_dir), "%s/.vifm/Trash", home_dir);

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
		}

	}
}


int
read_config_file(void)
{
	FILE *fp;
	char config_file[PATH_MAX];
	char line[MAX_LEN];
	char *s1 = NULL;
	char *s2 = NULL;
	char *s3 = NULL;
	char *sx = NULL;
	int args;

	snprintf(config_file, sizeof(config_file), "%s/vifmrc", cfg.config_dir);


	if((fp = fopen(config_file, "r")) == NULL)
	{
		fprintf(stdout, "Unable to find configuration file.\n Using defaults.");
		load_default_configuration();
		return 0;
	}

	while(fgets(line, MAX_LEN, fp))
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
		if((sx = s2 = strchr(s1, '=')) != NULL)
		{
			s2++;
			chomp(s2);
			*sx = '\0';
			args = 2;
		}
		/* COMMAND is handled here so that the command can have an '=' */
		if(!strcmp(line, "COMMAND"))
			add_command(s1, s2);
		else if((args == 2) && ((sx = s3 = strchr(s2, '=')) != NULL))
		{
			s3++;
			chomp(s3);
			*sx = '\0';
			args = 3;
		}
		if(args == 1)
		{
			if(!strcmp(line, "VI_COMMAND"))
			{
				if(cfg.vi_command != NULL)
					free(cfg.vi_command);

				cfg.vi_command = strdup(s1);
				continue;
			}

			if(!strcmp(line, "USE_TRASH"))
			{
				cfg.use_trash = atoi(s1);
				continue;
			}
			if(!strcmp(line, "USE_ONE_WINDOW"))
			{
				cfg.show_one_window = atoi(s1);
				continue;
			}
			if(!strcmp(line, "USE_SCREEN"))
			{
				cfg.use_screen = atoi(s1);
				continue;
			}
			if(!strcmp(line, "HISTORY_LENGTH"))
			{
				cfg.history_len = atoi(s1);
				continue;
			}
			if(!strcmp(line, "LEFT_WINDOW_SORT_TYPE"))
			{
				lwin.sort_type = atoi(s1);
				continue;
			}
			if(!strcmp(line, "RIGHT_WINDOW_SORT_TYPE"))
			{
				rwin.sort_type = atoi(s1);
				continue;
			}
			if(!strcmp(line, "LWIN_FILTER"))
			{
				lwin.filename_filter = (char *)realloc(lwin.filename_filter, 
				strlen(s1) +1);
				strcpy(lwin.filename_filter, s1);
				lwin.prev_filter = (char *)realloc(lwin.prev_filter, 
					strlen(s1) +1);
				strcpy(lwin.prev_filter, s1);
				continue;
			}
			if(!strcmp(line, "LWIN_INVERT"))
			{
				lwin.invert = atoi(s1);
				continue;
			}
			if(!strcmp(line, "RWIN_FILTER"))
			{
				rwin.filename_filter = (char *)realloc(rwin.filename_filter, 
				strlen(s1) +1);
				strcpy(rwin.filename_filter, s1);
				rwin.prev_filter = (char *)realloc(rwin.prev_filter, 
					strlen(s1) +1);
				strcpy(rwin.prev_filter, s1);
				continue;
			}
			if(!strcmp(line, "RWIN_INVERT"))
			{
				rwin.invert = atoi(s1);
				continue;
			}
			if(!strcmp(line, "USE_VIM_HELP"))
			{
				cfg.use_vim_help = atoi(s1);
				continue;
			}
			if(!strcmp(line, "RUN_EXECUTABLE"))
			{
				cfg.auto_execute = atoi(s1);
				continue;
			}
/*_SZ_BEGIN_*/
			if(!strcmp(line, "FUSE_HOME"))
			{
				if(cfg.fuse_home != NULL)
					free(cfg.fuse_home);

				cfg.fuse_home = strdup(s1);
				continue;
			}
/*_SZ_END_*/
		}
		if(args == 3)
		{
			if(!strcmp(line, "FILETYPE"))
			{
				add_filetype(s1, s2, s3);
				continue;
			}
			if(!strcmp(line, "BOOKMARKS"))
			{
				if(isalnum(*s1))
					add_bookmark(*s1, s2, s3);
				continue;
			}
		}
	}

	fclose(fp);

	read_color_scheme_file();

	return 1;
}

void
write_config_file(void)
{
	FILE *fp;
	int x = 0;
	char config_file[PATH_MAX];
	struct stat stat_buf;


	/* None of the user settings have changed. */
	if ((!curr_stats.setting_change) && (!cfg.using_default_config))
		return;

	curr_stats.getting_input = 1;

	snprintf(config_file, sizeof(config_file), "%s/vifmrc", cfg.config_dir);

	if(stat(config_file, &stat_buf) == 0) 
	{
		if ((stat_buf.st_mtime > curr_stats.config_file_mtime) &&
				(!cfg.using_default_config))
		{
			if (! query_user_menu(" Vifmrc file has been modified ",
				 "File has been modified would you still like to write to file? "))
				return;
		}
	}

	if((fp = fopen(config_file, "w")) == NULL)
		return;

	fprintf(fp, "# You can edit this file by hand.\n");
	fprintf(fp, "# The # character at the beginning of a line comments out the line.\n");
	fprintf(fp, "# Blank lines are ignored.\n");
	fprintf(fp, "# The basic format for each item is shown with an example.\n");
	fprintf(fp,
		"# The '=' character is used to separate fields within a single line.\n");
	fprintf(fp, "# Most settings are true = 1 or false = 0.\n");
	
	fprintf(fp, "\n# This is the actual command used to start vi.  The default is vi.\n");
	fprintf(fp,
			"# If you would like to use another vi clone such as Vim, Elvis, or Vile\n");
	fprintf(fp, "# you will need to change this setting.\n");
	fprintf(fp, "\nVI_COMMAND=%s", cfg.vi_command);
	fprintf(fp, "\n# VI_COMMAND=vim");
	fprintf(fp, "\n# VI_COMMAND=elvis -G termcap");
	fprintf(fp, "\n# VI_COMMAND=vile");
	fprintf(fp, "\n");
	fprintf(fp, "\n# Trash Directory\n");
	fprintf(fp, "# The default is to move files that are deleted with dd or :d to\n");
	fprintf(fp, "# the trash directory.  1 means use the trash directory 0 means\n");
	fprintf(fp, "# just use rm.  If you change this you will not be able to move\n");
	fprintf(fp, "# files by deleting them and then using p to put the file in the new location.\n");
	fprintf(fp, "# I recommend not changing this until you are familiar with vifm.\n");
	fprintf(fp, "# This probably shouldn't be an option.\n");
	fprintf(fp, "\nUSE_TRASH=%d\n", cfg.use_trash);

	fprintf(fp, "\n# Show only one Window\n");
	fprintf(fp, "# If you would like to start vifm with only one window set this to 1\n");
	fprintf(fp, "\nUSE_ONE_WINDOW=%d\n", curr_stats.number_of_windows == 1 ? 1 : 0);

	fprintf(fp, "\n# Screen configuration.  If you would like to use vifm with\n"); 
	fprintf(fp, "# the screen program set this to 1.\n");
	fprintf(fp, "\nUSE_SCREEN=%d\n", cfg.use_screen);

	fprintf(fp, "\n# 1 means use color if the terminal supports it.\n");
	fprintf(fp, "# 0 means don't use color even if supported.\n");

	fprintf(fp, "\n# This is how many files to show in the directory history menu.\n");
	fprintf(fp, "\nHISTORY_LENGTH=%d\n", cfg.history_len);

	fprintf(fp, "\n# The sort type is how the files will be sorted in the file listing.\n");
	fprintf(fp, "# Sort by File Extension = 0\n");
	fprintf(fp, "# Sort by File Name = 1\n");
	fprintf(fp, "# Sort by Group ID = 2\n");
	fprintf(fp, "# Sort by Group Name = 3\n");
	fprintf(fp, "# Sort by Mode = 4\n");
	fprintf(fp, "# Sort by Owner ID = 5\n");
	fprintf(fp, "# Sort by Owner Name = 6\n");
	fprintf(fp, "# Sort by Size (Ascending) = 7\n");
	fprintf(fp, "# Sort by Size (Descending) = 8\n");

	fprintf(fp, "# Sort by Time Accessed =9\n");
	fprintf(fp, "# Sort by Time Changed =10\n");
	fprintf(fp, "# Sort by Time Modified =11\n");
	fprintf(fp, "# This can be set with the :sort command in vifm.\n");
	fprintf(fp, "\nLEFT_WINDOW_SORT_TYPE=%d\n", lwin.sort_type);
	fprintf(fp, "\nRIGHT_WINDOW_SORT_TYPE=%d\n", rwin.sort_type);

	fprintf(fp, "\n# The regular expression used to filter files out of\n");
	fprintf(fp, "# the directory listings.\n");
	fprintf(fp, "# LWIN_FILTER=\\.o$ and LWIN_INVERT=1 would filter out all\n");
	fprintf(fp, "# of the .o files from the directory listing. LWIN_INVERT=0\n");
	fprintf(fp, "# would show only the .o files\n");
	fprintf(fp, "\nLWIN_FILTER=%s\n", lwin.filename_filter);
	fprintf(fp, "LWIN_INVERT=%d\n", lwin.invert);
	fprintf(fp, "RWIN_FILTER=%s\n", rwin.filename_filter);
	fprintf(fp, "RWIN_INVERT=%d\n", rwin.invert);

	fprintf(fp, "\n# If you installed the vim.txt help file change this to 1.\n");
	fprintf(fp, "# If would rather use a plain text help file set this to 0.\n");
	fprintf(fp, "\nUSE_VIM_HELP=%d\n", cfg.use_vim_help);

	fprintf(fp, "\n# If you would like to run an executable file when you \n");
	fprintf(fp, "# press return on the file name set this to 1.\n");
	fprintf(fp, "\nRUN_EXECUTABLE=%d\n", cfg.auto_execute);

	fprintf(fp, "\n# BOOKMARKS=mark=/full/directory/path=filename\n\n");
	for(x = 0; x < NUM_BOOKMARKS; x++)
	{
		if (is_bookmark(x))
		{
			fprintf(fp, "BOOKMARKS=%c=%s=%s\n",
					index2mark(x),
					bookmarks[x].directory, bookmarks[x].file);
		}
	}

	fprintf(fp, "\n# COMMAND=command_name=action\n");
	fprintf(fp, "# The following macros can be used in a command\n");
	fprintf(fp, "# %%a is replaced with the user arguments.\n");
	fprintf(fp, "# %%f the current selected file, or files.\n");
	fprintf(fp, "# %%F the current selected file, or files in the other directoy.\n");
	fprintf(fp, "# %%d the current directory name.\n");
	fprintf(fp, "# %%D the other window directory name.\n");
	fprintf(fp, "# %%m run the command in a menu window\n\n");
	for(x = 0; x < cfg.command_num; x++)
	{
		fprintf(fp, "COMMAND=%s=%s\n", command_list[x].name, 
				command_list[x].action);
	}

	fprintf(fp, "\n# The file type is for the default programs to be used with\n");
	fprintf(fp, "# a file extension. \n");
	fprintf(fp, "# FILETYPE=description=extension1,extension2=defaultprogram, program2\n");
	fprintf(fp, "# FILETYPE=Web=html,htm,shtml=links,mozilla,elvis\n");
	fprintf(fp, "# would set links as the default program for .html .htm .shtml files\n");
	fprintf(fp, "# The other programs for the file type can be accessed with the :file command\n");
	fprintf(fp, "# The command macros %%f, %%F, %%d, %%F may be used in the commands.\n");
	fprintf(fp, "# The %%a macro is ignored.  To use a %% you must put %%%%.\n\n");
	for(x = 0; x < cfg.filetypes_num; x++)
	{
		fprintf(fp, "FILETYPE=%s=%s=%s\n", filetypes[x].type, filetypes[x].ext, filetypes[x].programs);
	}

/*_SZ_BEGIN_*/
	fprintf(fp, "\n# For automated FUSE mounts, you must register an extension with FILETYPE=..\n"); 
	fprintf(fp, "# in the following format:\n");
	fprintf(fp, "# FILETYPE=description=extensions=FUSE_MOUNT|some_mount_command using %%SOURCE_FILE and %%DESTINATION_DIR variables\n");
	fprintf(fp, "# %%SOURCE_FILE and %%DESTINATION_DIR are filled in by vifm at runtime.\n");
	fprintf(fp, "# A sample line might look like this:\n");
	fprintf(fp, "# FILETYPE=FuseZipMount=zip,jar,war,ear=FUSE_MOUNT|fuse-zip %%SOURCE_FILE %%DESTINATION_DIR\n\n");
	fprintf(fp, "# The FUSE_HOME directory will be used as a root dir for all FUSE mounts.\n");
	fprintf(fp, "# Unless it exists with write/exec permissions set, vifm will attempt to create it.\n");
	fprintf(fp, "\nFUSE_HOME=%s\n", cfg.fuse_home);
/*_SZ_END_*/

	fclose(fp);

	curr_stats.getting_input = 0;
}
