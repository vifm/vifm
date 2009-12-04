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


#include<ctype.h> /* isspace() */
#include<ncurses.h>
#include<signal.h>
#include<stdio.h>
#include<stdlib.h> /*  system() */
#include<string.h> /* strncmp() */
#include<time.h>
#include<unistd.h> /* chdir() */

#include"bookmarks.h"
#include"background.h"
#include"color_scheme.h"
#include"commands.h"
#include"config.h"
#include"filelist.h"
#include"fileops.h"
#include"keys.h"
#include"menus.h"
#include"search.h"
#include"signals.h"
#include"sort.h"
#include"status.h"
#include"ui.h"
#include"utils.h"
#include "rline.h"

enum
{
	COM_EXECUTE,
	COM_APROPOS,
	COM_CHANGE,
	COM_CD, 		
	COM_CMAP,			
	COM_COLORSCHEME,
	COM_COMMAND,		
	COM_DELETE, 		
	COM_DELCOMMAND,	
	COM_DISPLAY,
	COM_EDIT,				
	COM_EMPTY,
	COM_FILTER,
	COM_FILE,
	COM_HELP,
	COM_HISTORY,
	COM_INVERT,
	COM_JOBS,	
	COM_LOCATE,
	COM_LS,
	COM_MAP,
	COM_MARKS,
	COM_NMAP,
	COM_NOH,
	COM_ONLY,
	COM_PWD,
	COM_QUIT,
	COM_REGISTER,
	COM_SORT,
	COM_SCREEN,
	COM_SHELL,
	COM_SPLIT,
	COM_SYNC,
	COM_UNMAP,
	COM_VIEW,
	COM_VIFM,
	COM_VMAP,
	COM_YANK,	
	COM_X
};

	/* The order of the commands is important as :e will match the first 
	 * command starting with e.
	 */
char *reserved_commands[] = {
	"!",
	"apropos",
	"change",
	"cd",
	"cmap",
	"colorscheme",
	"command",
	"delete",
	"delcommand",
	"display",
	"edit",
	"empty",
	"filter",
	"file",
	"help",
	"history",
	"invert",
	"jobs",
	"locate",
	"ls",
	"map",
	"marks",
	"nmap",
	"nohlsearch",
	"only",
	"pwd",
	"quit",
	"register",
	"sort",
	"screen",
	"shell",
	"split",
	"sync",
	"unmap",
	"view",
	"vifm",
	"vmap",
	"yank",
	"x"
	};

#define RESERVED 39

typedef struct current_command
{
	int start_range;
	int end_range;
	int count;
	char *cmd_name;
	char *args;
	char *curr_files; /* holds %f macro files */
	char *other_files; /* holds %F macro files */
	char *user_args; /* holds %a macro string */
	char *order; /* holds the order of macros command %a %f or command %f %a */
	int background;
	int split;
	int builtin;
	int is_user;
	int pos;
	int pause;
}cmd_t;

int
sort_this(const void *one, const void *two)
{
	const command_t *first = (const command_t *)one;
	const command_t *second = (const command_t *)two;

	return strcmp(first->name, second->name);
}

int
command_is_reserved(char *name)
{
	int x;
    int len = strlen(name);

	for(x = 0; x < RESERVED; x++)
	{
		if(!strncmp(reserved_commands[x], name, len))
				return x;
	}
	return -1;
}

int
command_is_being_used(char *command)
{
	int x;
	for(x = 0; x < cfg.command_num; x++)
	{
		if(!strcmp(command_list[x].name, command))
				return 1;
	}
	return 0;
		
}

/* On the first call to this function,
 * the string to be parsed should be specified in str.
 * In each subsequent call that should parse the same string, str should be NULL
 */
char *
command_completion(char *str)
{
    static char *string;
    static int offset;
    int pos = -1;
    int found = 0;
    int i = 0;
    int len;

    if (str != NULL)
    {
        string = str;
        offset = 0;
    }
    else
        offset++;

    len = strlen(string);

    if ((pos = command_is_reserved(string)) > -1)
    {
        found = 1;

        while (i < offset)
        {
            if (pos >= RESERVED - 1)
                break;

            if (!strncmp(string, reserved_commands[++pos], len))
                i++;
        }

        if (i == offset)
            return strdup(reserved_commands[pos]);
    }

    if ((pos = is_user_command(string)) > -1)
    {
        found = 1;

        while (i < offset)
        {
            if (pos >= cfg.command_num - 1)
                break;

            if (!strncmp(string, command_list[++pos].name, len))
                i++;
        }

        if (i == offset)
            return strdup(command_list[pos].name);
    }

    if (!found)
        return NULL;
    else if (i != offset)
    {
        offset = -1;
        return strdup(string);
    }

    show_error_msg(" Debug error ", "command_completion():"
            " Harmless error in commands.c: line 254");
    return NULL;
}

void
save_search_history(char *pattern)
{
	int x = 0;

	if ((cfg.search_history_num + 1) >= cfg.search_history_len)
		cfg.search_history_num = x  = cfg.search_history_len - 1;
	else
		x = cfg.search_history_num + 1;

	for (; x > 0; x--)
	{
		cfg.search_history[x] = (char *)realloc(cfg.search_history[x], 
				strlen(cfg.search_history[x - 1]) + 1);
		strcpy(cfg.search_history[x], cfg.search_history[x - 1]);
	}

	cfg.search_history[0] = (char *)realloc(cfg.search_history[0], 
			strlen(pattern) + 1);
	strcpy(cfg.search_history[0], pattern);
	cfg.search_history_num++;
	if (cfg.search_history_num >= cfg.search_history_len)
		cfg.search_history_num = cfg.search_history_len - 1;
}

void
save_command_history(char *command)
{
	int x = 0;

	/* Don't add :!! or :! to history list */
	if (!strcmp(command, "!!") || !strcmp(command, "!"))
		return;

	if ((cfg.cmd_history_num + 1) >= cfg.cmd_history_len)
		cfg.cmd_history_num = x = cfg.cmd_history_len - 1;
	else
		x = cfg.cmd_history_num + 1;

	for (; x > 0; x--)
	{
		cfg.cmd_history[x] = (char *)realloc(cfg.cmd_history[x], 
				strlen(cfg.cmd_history[x - 1]) + 1);
		strcpy(cfg.cmd_history[x], cfg.cmd_history[x - 1]);
	}

	cfg.cmd_history[0] = (char *)realloc(cfg.cmd_history[0], 
			strlen(command) + 1);
	strcpy(cfg.cmd_history[0], command);
	cfg.cmd_history_num++;
	if (cfg.cmd_history_num >= cfg.cmd_history_len)
		cfg.cmd_history_num = cfg.cmd_history_len -1;

}

/* The string returned needs to be freed in the calling function */
char *
expand_macros(FileView *view, char *command, char *args,
		int *use_menu, int *split)
{
	char * expanded = NULL;
	int x;
	int y = 0;
	int len = 0;

	curr_stats.getting_input = 1;

	expanded = (char *)calloc(strlen(command) +1, sizeof(char *));

	for(x = 0; x < strlen(command); x++)
			if(command[x] == '%')
				break;

	strncat(expanded, command, x);
	x++;
	len = strlen(expanded);

	do
	{
		switch(command[x])
		{
			case 'a': /* user arguments */
				{
					if(!args)
						break;
					else
					{
						char arg_buf[strlen(args) +2];

						expanded = (char *)realloc(expanded, 
								strlen(expanded) + strlen(args) +3);
						snprintf(arg_buf, sizeof(arg_buf), "%s ", args);
						strcat(expanded, arg_buf);
						len = strlen(expanded);
					}
				}
				break;
			case 'f': /* current dir selected files */
				{
					if(view->selected_files)
					{
						int y = 0;
						for(y = 0; y < view->list_rows; y++)
						{
							if(view->dir_entry[y].selected)
							{
								int dir = 0;
								char *temp = NULL;
								expanded = (char *)realloc(expanded, 
									len + strlen(view->dir_entry[y].name) +5);

					/* Directory has / appended to the name this removes it. */
								if (view->dir_entry[y].type == DIRECTORY)
									dir = 1;

								temp = escape_filename(view->dir_entry[y].name,
										strlen(view->dir_entry[y].name)-dir, 1);
								expanded = (char *)realloc(expanded, strlen(expanded) + 
									strlen(temp) +3);
								strcat(expanded, temp);

								my_free(temp);

								strcat(expanded, " ");
								len = strlen(expanded);
							}
						}
					}
					else
					{
						int dir = 0;
						char *temp = 
						  escape_filename(view->dir_entry[view->list_pos].name,
					      strlen(view->dir_entry[view->list_pos].name)-dir, 1);


						expanded = (char *)realloc(expanded, strlen(expanded) + 
								strlen(view->dir_entry[view->list_pos].name) +3);

					/* Directory has / appended to the name this removes it. */
						if (view->dir_entry[view->list_pos].type == DIRECTORY)
							dir = 1;

						expanded = (char *)realloc(expanded, strlen(expanded) + 
							strlen(temp) +3);
						strcat(expanded, temp);
						my_free(temp);

						len = strlen(expanded);
					}
				}
				break;
			case 'F': /* other dir selected files */
				{
					if(other_view->selected_files)
					{
						int y = 0;

						for(y = 0; y < other_view->list_rows; y++)
						{
							if(other_view->dir_entry[y].selected)
							{
								expanded = (char *)realloc(expanded, len +
										strlen(other_view->dir_entry[y].name) +
										strlen(other_view->curr_dir) + 3);

								if(expanded == NULL)
								{
									show_error_msg("Memory Error", "Unable to allocate memory");
									return NULL;
								}

								strcat(expanded, other_view->curr_dir);
								strcat(expanded, "/");

								int dir = 0;
								/* Directory has / appended to the name this removes it. */
								if (other_view->dir_entry[y].type == DIRECTORY)
									dir = 1;

								char *temp = NULL;
								temp = escape_filename(other_view->dir_entry[y].name,
										strlen(other_view->dir_entry[y].name)-dir, 1);

								expanded = (char *)realloc(expanded, strlen(expanded) +
										strlen(temp +3));
								strcat(expanded, temp);

								my_free(temp);

								strcat(expanded, " ");
								len = strlen(expanded);
							}
						}
					}
					else
					{
						expanded = (char *)realloc(expanded, len + 
								strlen(other_view->dir_entry[other_view->list_pos].name) +
								strlen(other_view->curr_dir) +4);
						if(expanded == NULL)
						{
							show_error_msg("Memory Error", "Unable to allocate memory");
							return NULL;
						}

						strcat(expanded, other_view->curr_dir);
						strcat(expanded, "/");

						int dir = 0;
						/* Directory has / appended to the name this removes it. */
						if (other_view->dir_entry[other_view->list_pos].type == DIRECTORY)
							dir = 1;

						char *temp =
							escape_filename(other_view->dir_entry[other_view->list_pos].name,
									strlen(other_view->dir_entry[other_view->list_pos].name)-dir, 1);
						expanded = (char *)realloc(expanded, strlen(expanded) +
								strlen(temp) + 3);
						strcat(expanded, temp);
						my_free(temp);

						len = strlen(expanded);
					}
				}
				break;
			case 'd': /* current directory */
				{
					expanded = (char *)realloc(expanded, 
							len + strlen(view->curr_dir) +3);
					strcat(expanded, "\"");
					strcat(expanded, view->curr_dir);
					strcat(expanded, "\"");
					len = strlen(expanded);
				}
				break;
			case 'D': /* other directory */
				{
					expanded = (char *)realloc(expanded, 
							len + strlen(other_view->curr_dir) +3);
					if(!expanded)
					{
						show_error_msg("Memory Error", "Unable to allocate memory");
						return NULL;
					}
					strcat(expanded, "\'");
					strcat(expanded, other_view->curr_dir);
					strcat(expanded, "\'");
					len = strlen(expanded);
				}
				break;
			case 'm': /* use menu */
				*use_menu = 1;
				break;
			case 's': /* split in new screen region */
				*split = 1;
				
				break;
			default:
				break;
		}
		x++;
		y = x;

		for(; x < strlen(command); x++)
		{
				if(command[x] == '%')
					break;
		}
		expanded = (char *)realloc(expanded, len + strlen(command) +1); 
		strncat(expanded, command + y, x - y);
		len = strlen(expanded);
		x++;
		}while(x < strlen(command));

	len++;
	expanded[len] = '\0';
	if (len > cfg.max_args/2)
		show_error_msg("Argument is too long", " FIXME ");

	curr_stats.getting_input = 0;
	
	return expanded;
}

int
is_user_command(char *command)
{
	char buf[strlen(command) +1]; 
	char *com;
	char *ptr;
	int x;

	com = strcpy(buf, command);

	if((ptr = strchr(com, ' ')) != NULL)
		*ptr = '\0';

	for(x = 0; x < cfg.command_num; x++)
	{
		if(!strncmp(com, command_list[x].name, strlen(com)))
		{
			return x;
		}
	}

	return -1;
}

void
remove_command(char *name)
{
	char *ptr = NULL;
	char *s = name;
	int x;
	int found = 0;

	if((ptr = strchr(s, ' ')) != NULL)
		*ptr = '\0';

	if(command_is_reserved(s) > -1)
	{
		show_error_msg(" Trying to delete a reserved Command ", s);
		return;
	}

	for(x = 0; x < cfg.command_num; x++)
	{
		if(!strcmp(s, command_list[x].name))
		{
			found = 1;
			break;
		}
	}
	if(found)
	{
		cfg.command_num--;
		while(x < cfg.command_num)
		{
			command_list[x].name = (char *)realloc(command_list[x].name, 
					strlen(command_list[x +1].name +1));
			strcpy(command_list[x].name, command_list[x +1].name);
			command_list[x].action = (char *)realloc(command_list[x].action, 
					strlen(command_list[x +1].action +1));
			strcpy(command_list[x].action, command_list[x +1].action);
			x++;
		}

		if(strlen(command_list[x].name))
			my_free(command_list[x].name);

		if(strlen(command_list[x].action))
			my_free(command_list[x].action);
		curr_stats.setting_change = 1;

	}
	else
		show_error_msg(" Command Not Found ", s);
}

void
add_command(char *name, char *action)
{
	if(command_is_reserved(name) > -1)
			return;
	if(isdigit(*name))
	{
		show_error_msg(" Invalid Command Name ", 
				"Commands cannot start with a number.");
		return;
	}

	if(command_is_being_used(name))
	{
		return;
	}

	command_list = (command_t *)realloc(command_list, 
			(cfg.command_num +1) * sizeof(command_t));

	command_list[cfg.command_num].name = (char *)malloc(strlen(name) +1);
	strcpy(command_list[cfg.command_num].name, name);
	command_list[cfg.command_num].action = (char *)malloc(strlen(action) +1);
	strcpy(command_list[cfg.command_num].action, action);
	cfg.command_num++;
	curr_stats.setting_change = 1;

	qsort(command_list, cfg.command_num, sizeof(command_t), sort_this);
}

static void
set_user_command(char * command, int overwrite, int background)
{
	char buf[80];
	char *ptr = NULL;
	char *com_name = NULL;
	char *com_action = NULL;

	while(isspace(*command))
		command++;

	com_name = command;

	if((ptr = strchr(command, ' ')) == NULL)
			return;

	*ptr = '\0';
	ptr++;

	while(isspace(*ptr) && *ptr != '\0')
		ptr++;

	if((strlen(ptr) < 1))
	{
		show_error_msg(" To set a Command Use: ", 
				":com command_name command_action");
		return;
	}

	com_action = strdup(ptr);

	if(background)
	{
		com_action = (char *)realloc(com_action, 
				(strlen(com_action) + 4) * sizeof(char));
		snprintf(com_action, (strlen(com_action) + 3) * sizeof(char), "%s &", ptr);
	}

	if(command_is_reserved(com_name) > -1)
	{
		snprintf(buf, sizeof(buf), "%s is a reserved command name", com_name);
		show_error_msg("", buf);
		my_free(com_action);
		return;
	}
	if(command_is_being_used(com_name))
	{
		if(overwrite)
		{
			remove_command(com_name);
			add_command(com_name, com_action);
		}
		else
		{
			snprintf(buf, sizeof(buf), "%s is already set. Use :com! to overwrite.",
					com_name);
			show_error_msg("", buf);
		}
	}
	else
	{
		add_command(com_name, com_action);
	}
}

static void
split_screen(FileView *view, char *command)
{
	char buf[1024];

	if (command)
	{
		snprintf(buf, sizeof(buf), "screen -X eval \'chdir %s\'", view->curr_dir);
		my_system(buf);

		snprintf(buf,sizeof(buf), "screen-open-region-with-program \"%s\' ",
				command);

		my_system(buf);

	}
	else
	{
		char *sh = getenv("SHELL");
		snprintf(buf, sizeof(buf), "screen -X eval \'chdir %s\'", view->curr_dir);
		my_system(buf);

		snprintf(buf, sizeof(buf), "screen-open-region-with-program %s", sh);
		my_system(buf);
	}
}

void
shellout(char *command, int pause)
{
	char buf[1024];

	if(command != NULL)
	{
		if(cfg.use_screen)
		{
			char *ptr = (char *)NULL;
			char *title = strstr(command, cfg.vi_command);

			/* Needed for symlink directories and sshfs mounts */
			snprintf(buf, sizeof(buf), "screen -X setenv PWD \'%s\'",
				   	curr_view->curr_dir);

			my_system(buf);

			if(title != NULL)
			{
				if(pause)
					snprintf(buf, sizeof(buf), "screen -t \"%s\" sh -c \"pauseme %s\"", 
							title + strlen(cfg.vi_command) +1, command);
				else
					snprintf(buf, sizeof(buf), "screen -t \"%s\" sh -c \"%s\"", 
							title + strlen(cfg.vi_command) +1, command);
			}
			else
			{
				ptr = strchr(command, ' ');
				if (ptr != NULL)
				{
					*ptr = '\0';
					title = strdup(command);
					*ptr = ' ';
				}
				else
					title = strdup("Shell");

				if(pause)
					snprintf(buf, sizeof(buf), "screen -t \"%.10s\" sh -c \"pauseme %s\"", 
							title, command);
				else
					snprintf(buf, sizeof(buf), "screen -t \"%.10s\" sh -c \"%s\"", title, command);
				my_free(title);
			}
		}
		else
		{
			if(pause)
				snprintf(buf, sizeof(buf), "sh -c \"pauseme %s\"", command);
			else
				snprintf(buf, sizeof(buf), "sh -c \"%s\"", command);
		}
	}
	else
	{
		if(cfg.use_screen)
		{
			snprintf(buf, sizeof(buf), "screen -X setenv PWD \'%s\'",
				   	curr_view->curr_dir);

			my_system(buf);

			snprintf(buf, sizeof(buf), "screen");
		}
		else
		{
			char *sh = getenv("SHELL");
			snprintf(buf, sizeof(buf), "%s", sh);
		}
	}

	def_prog_mode();
	endwin();

	my_system("clear");
	my_system(buf);


	/* There is a problem with using the screen program and 
	 * catching all the SIGWICH signals.  So just redraw the window.
	 */
	if (!isendwin())
		redraw_window();

	curs_set(0);
}

static void
initialize_command_struct(cmd_t *cmd)
{
	cmd->start_range = 0;
	cmd->end_range = 0;
	cmd->count = 0;
	cmd->cmd_name = NULL;
	cmd->args = NULL;
	cmd->background = 0;
	cmd->split = 0;
	cmd->builtin = -1;
	cmd->is_user = -1;
	cmd->pos = 0;
	cmd->pause = 0;
}

static int
select_files_in_range(FileView *view, cmd_t * cmd)
{

				int x;
				int y = 0;

				/* Both a starting range and an ending range are given. */
				if(cmd->start_range > -1)
				{
					if(cmd->end_range < cmd->start_range)
					{
						show_error_msg(" Command Error ", "Backward range given.");
						//save_msg = 1;
						//break;
					}

					for(x = 0; x < view->list_rows; x++)
						view->dir_entry[x].selected = 0;

					for(x = cmd->start_range; x <= cmd->end_range; x++)
					{
						view->dir_entry[x].selected = 1;
						y++;
					}
					view->selected_files = y;
				}
				/* A count is given */
				else if(cmd->count)
				{
					if(!cmd->count)
						cmd->count = 1;

					/* A one digit range with a count. :4y5 */
					if(cmd->end_range)
					{
						y = 0;
						for(x = 0; x < view->list_rows; x++)
							view->dir_entry[x].selected = 0;

						for(x = cmd->end_range; x < view->list_rows; x++)
						{
							if(cmd->count == y)
								break;
							view->dir_entry[x].selected = 1;
							y++;

						}
						view->selected_files = y;
					}
					/* Just a count is given. */ 
					else
					{
						y = 0;

						for(x = 0; x < view->list_rows; x++)
							view->dir_entry[x].selected = 0;

						for(x = view->list_pos; x < view->list_rows; x++)
						{
							if(cmd->count == y )
								break;

							view->dir_entry[x].selected = 1;
							y++;
						}
						view->selected_files = y;

					}
				}

	return 0;
}

static int
check_for_range(FileView *view, char *command, cmd_t *cmd)
{

	while(isspace(command[cmd->pos]) && cmd->pos < strlen(command))
			cmd->pos++;

	/*
	 * First check for a count or a range
	 * This should be changed to include the rest of the range 
	 * characters [/?\/\?\&]
	 */
	if(command[cmd->pos] == '\'')
	{
		char mark;
		cmd->pos++;
		mark = command[cmd->pos];
		cmd->start_range = check_mark_directory(view, mark);
		if(cmd->start_range < 0)
		{
			show_error_msg("Invalid mark in range", "Trying to use an invalid mark.");
			return -1;
		}
		cmd->pos++;
	}
	else if(command[cmd->pos] == '$')
	{
		cmd->count = view->list_rows;
		if(strlen(command) == strlen("$"))
		{
			moveto_list_pos(view, cmd->count -1);
			return -10;
		}
		cmd->pos++;
	}
	else if(command[cmd->pos] == '.')
	{
		cmd->start_range = view->list_pos;
		cmd->pos++;
	}
	else if(command[cmd->pos] == '%')
	{
		cmd->start_range = 1;
		cmd->end_range = view->list_rows;
		cmd->pos++;
	}
	else if(isdigit(command[cmd->pos]))
	{
		char num_buf[strlen(command)];
		int z = 0;
		while(isdigit(command[cmd->pos]))
		{
			num_buf[z] = command[cmd->pos];
				cmd->pos++;
				z++;
		}
		num_buf[z] = '\0';
		cmd->start_range = atoi(num_buf);

		/* The command is just a number */
		if(strlen(num_buf) == strlen(command))
		{
			moveto_list_pos(view, cmd->start_range - 1);
			return -10;
		}
	}
	while(isspace(command[cmd->pos]) && cmd->pos < strlen(command))
			cmd->pos++;

	/* Check for second number of range. */
	if(command[cmd->pos] == ',')
	{
		cmd->pos++;

		if(command[cmd->pos] == '\'')
		{
			char mark;
			cmd->pos++;
			mark = command[cmd->pos];
			cmd->end_range = check_mark_directory(view, mark);
			if(cmd->end_range < 0)
			{
				show_error_msg("Invalid mark in range", 
						"Trying to use an invalid mark.");
				return -1;
			}
			cmd->pos++;
		}
		else if(command[cmd->pos] == '$')
		{
			cmd->end_range = view->list_rows - 1;
			cmd->pos++;
		}
		else if(command[cmd->pos] == '.')
		{
			cmd->end_range = view->list_pos;
			cmd->pos++;
		}
		else if(isdigit(command[cmd->pos]))
		{
			char num_buf[strlen(command)];
			int z = 0;
			while(isdigit(command[cmd->pos]))
			{
				num_buf[z] = command[cmd->pos];
					cmd->pos++;
					z++;
			}
			num_buf[z] = '\0';
			cmd->end_range = atoi(num_buf);
		}
		else
			cmd->pos--;
	}
	else if(!cmd->end_range)/* Only one number is given for the range */
	{
		cmd->end_range = cmd->start_range;
		cmd->start_range = -1;
	}
	return 1;
}


static int
parse_command(FileView *view, char *command, cmd_t *cmd)
{
	int result;

	initialize_command_struct(cmd);

	while(strlen(command) > 0 && isspace(command[strlen(command) -1]))
			command[strlen(command) -1] = '\0';

	result = check_for_range(view, command, cmd);


	/* Just a :number - :12 - or :$ handled in check_for_range() */
	if(result == -10)
		return 0;

	/* If the range is invalid give up. */
	if(result < 0)
		return -1;

	/* If it is :!! handle and skip everything else. */
	if(!strcmp(command + cmd->pos, "!!"))
	{
		if(cfg.cmd_history[0] != NULL)
		{
			/* Just a safety check this should never happen. */
			if (!strcmp(cfg.cmd_history[0], "!!"))
				show_error_msg("Command Error", "Error in parsing command.");
			else
				execute_command(view, cfg.cmd_history[0]);
		}
		else
			show_error_msg(" Command Error", "No Previous Commands in History List");

		return 0;
	}

	cmd->cmd_name = strdup(command + cmd->pos);

	/* The builtin commands do not contain numbers and ! is only used at the
	 * end of the command name.
	 */
	while(cmd->cmd_name[cmd->pos] != ' ' && cmd->pos < strlen(cmd->cmd_name) 
			&& cmd->cmd_name[cmd->pos] != '!')
		cmd->pos++;

	if (cmd->cmd_name[cmd->pos] != '!' || cmd->pos == 0)
	{
		cmd->cmd_name[cmd->pos] = '\0';
		cmd->pos++;
	}
	else /* Prevent eating '!' after command name. by not doing cmd->pos++ */
		cmd->cmd_name[cmd->pos] = '\0';



	if(strlen(command) > cmd->pos)
	{

		/* Check whether to run command in background */
		if(command[strlen(command) -1] == '&' && command[strlen(command) -2] == ' ')
		{
			int x = 2;

			command[strlen(command) - 1] = '\0';

			while(isspace(command[strlen(command) - x]))
			{
				command[strlen(command) - x] = '\0';
				x++;
			}
			cmd->background = 1;

		}
		cmd->args = strdup(command + cmd->pos);
	}

	/* Get the actual command name. */
	if((cmd->builtin = command_is_reserved(cmd->cmd_name)) > -1)
	{
		cmd->cmd_name = (char *)realloc(cmd->cmd_name, 
				strlen(reserved_commands[cmd->builtin]) +1);
		snprintf(cmd->cmd_name, sizeof(reserved_commands[cmd->builtin]),
				"%s", reserved_commands[cmd->builtin]);
	}
	else if((cmd->is_user = is_user_command(cmd->cmd_name)) > -1)
	{
		cmd->cmd_name =(char *)realloc(cmd->cmd_name,
				strlen(command_list[cmd->is_user].name) + 1);
		snprintf(cmd->cmd_name, sizeof(command_list[cmd->is_user].name),
				"%s", command_list[cmd->is_user].name);
	}
	else
	{
		my_free(cmd->cmd_name);
		my_free(cmd->args);
		status_bar_message("Unknown Command");
		return -1;
	}

	return 1;
}

int
execute_builtin_command(FileView *view, cmd_t *cmd)
{
	int save_msg = 0;

	switch(cmd->builtin)
	{
		case COM_EXECUTE:
			{
				int i = 0;
				int pause = 0;
				char *com = NULL;

				if (cmd->args)
				{
					int m = 0;
					int s = 0;
					if(strchr(cmd->args, '%') != NULL)
						com = expand_macros(view, cmd->args, NULL, &m, &s);
					else
						com = strdup(cmd->args);

					if(com[i] == '!')
					{
						pause = 1;
						i++;
					}
					while(isspace(com[i]) && i < strlen(com))
							i++;
					if((strlen(com + i) > 0) && cmd->background)
						start_background_job(com + i);
					else if(strlen(com + i) > 0)
					{
						shellout(com +i, pause);
					}
					if(!cmd->background)
						my_free(com);
						
				}
				else
				{
					show_error_msg(" Command Error ",
						"The :! command requires an argument - :!command");
			 		save_msg = 1;
				}
			}
			break;
		case COM_APROPOS:
			{
				if(cmd->args)
				{
					show_apropos_menu(view, cmd->args);
				}
			}
			break;
		case COM_CHANGE:
			show_change_window(view, FILE_CHANGE);
			break;
		case COM_CD:
			{
				char dir[PATH_MAX];

				if(cmd->args)
				{
					if(cmd->args[0] == '/')
					{
						change_directory(view, cmd->args);
						load_dir_list(view, 0);
						moveto_list_pos(view, view->list_pos);
					}
					else if(cmd->args[0] == '~')
					{
						snprintf(dir, sizeof(dir), "%s%s", getenv("HOME"), cmd->args +1);
						change_directory(view, dir);
						load_dir_list(view, 0);
						moveto_list_pos(view, view->list_pos);
					}
					else
					{
						snprintf(dir, sizeof(dir), "%s/%s", view->curr_dir, cmd->args);
						change_directory(view, dir);
						load_dir_list(view, 0);
						moveto_list_pos(view, view->list_pos);
					}
				}
				else
				{
					change_directory(view, getenv("HOME"));
					load_dir_list(view, 0);
					moveto_list_pos(view, view->list_pos);
				}
			}
			break;
		case COM_CMAP:
			break;
		case COM_COLORSCHEME:
		{
			if(cmd->args)
			{
				//snprintf(buf, sizeof(buf), "args are %s", cmd->args);
				show_error_msg("Color Scheme",
						"The :colorscheme command is reserved ");

			}
			else /* Should show error message with colorschemes listed */
			{
				show_error_msg("Color Scheme",
						"The :colorscheme command is reserved ");
			}

			break;
		}
		case COM_COMMAND:
			{
				if(cmd->args)
				{
					if(cmd->args[0] == '!')
					{
						int x = 1;
						while(isspace(cmd->args[x]) && x < strlen(cmd->args))
							x++;
						set_user_command(cmd->args + x, 1, cmd->background);
					}
					else
						set_user_command(cmd->args, 0, cmd->background);
				}
				else
					show_commands_menu(view);
			}
			break;
		case COM_DELETE:
			{
				/*
				int selection_worked;

				selection_worked = select_files_in_range(view, cmd);

				if (selection_worked)
				*/
				select_files_in_range(view, cmd);
				delete_file(view);
			}
			break;
		case COM_DELCOMMAND:
			{
				if(cmd->args)
				{
					remove_command(cmd->args);
					//write_config_file();
				}
			}
			break;
		case COM_DISPLAY:
		case COM_REGISTER:
			show_register_menu(view);
			break;
		case COM_EDIT:
			{
				if((!view->selected_files) || 
						(!view->dir_entry[view->list_pos].selected))
				{
					char buf[PATH_MAX];
					if(view->dir_entry[view->list_pos].name != NULL)
					{
						char *temp = 
							escape_filename(view->dir_entry[view->list_pos].name,
									strlen(view->dir_entry[view->list_pos].name), 0);
						snprintf(buf, sizeof(buf), "%s %s/%s", cfg.vi_command, 
								view->curr_dir, temp); 
						shellout(buf, 0);
						my_free(temp);
					}
				}
				else
				{
					int m = 0;
					int s = 0;
					char *buf = NULL;
					char *files = expand_macros(view, "%f", NULL, &m, &s);

					if((buf = (char *)malloc(strlen(cfg.vi_command) + strlen(files) + 2))
							== NULL)
					{
						show_error_msg("Unable to allocate enough memory", 
								"Cannot load file");
						break;
					}
					snprintf(buf, strlen(cfg.vi_command) + strlen(files) +1, 
							"%s %s", cfg.vi_command, files);
					
					shellout(buf, 0);
					my_free(files);
					my_free(buf);
				}
			}
			break;
		case COM_EMPTY:
			{
				char buf[256];
				snprintf(buf, sizeof(buf), "%s/Trash", cfg.config_dir);
				if(chdir(buf))
					break;

				start_background_job("rm -fr * .[!.]*"); 
				chdir(view->curr_dir);
			}
			break;
		case COM_FILTER:
			{
				if(cmd->args)
				{
					view->invert = 1;
					view->filename_filter = (char *)realloc(view->filename_filter,
						strlen(cmd->args) +2);
					snprintf(view->filename_filter, strlen(cmd->args) +1, 
							"%s", cmd->args); 
					load_dir_list(view, 1);
					moveto_list_pos(view, 0);
					curr_stats.setting_change = 1;
				}
				else
				{
					show_error_msg(" Command Error ", 
							"The :filter command requires an argument - :filter pattern");
					save_msg = 1;
				}
			}
			break;
		case COM_FILE:
			show_filetypes_menu(view);
			break;
		case COM_HELP:
			{
				char help_file[PATH_MAX];

				if(cfg.use_vim_help)
				{
					if(cmd->args)
					{
						snprintf(help_file, sizeof(help_file), 
								"%s -c \'help %s\' -c only", cfg.vi_command, cmd->args);
						shellout(help_file, 0);
					}
					else 
					{
						snprintf(help_file, sizeof(help_file), 
								"%s -c \'help vifm\' -c only", cfg.vi_command);
						shellout(help_file, 0);
					}
				}
				else
				{
					snprintf(help_file, sizeof(help_file),
						   	"%s %s/vifm-help.txt",
							cfg.vi_command, cfg.config_dir);

					shellout(help_file, 0);
				}
			}
			break;
		case COM_HISTORY:
			show_history_menu(view);
			break;
		case COM_INVERT:
			{
				if(view->invert)
					view->invert = 0;
				else
					view->invert = 1;
				load_dir_list(view, 1);
				moveto_list_pos(view, 0);
				curr_stats.setting_change = 1;
			}
			break;
		case COM_JOBS:	
			show_jobs_menu(view);
			break;
		case COM_LOCATE:
			{
				if(cmd->args)
				{
					show_locate_menu(view, cmd->args);
				}
			}
			break;
		case COM_LS:
			if (!cfg.use_screen)
				break;
			my_system("screen -X eval 'windowlist'");
			break;
		case COM_MAP:
			break;
		case COM_MARKS:
			show_bookmarks_menu(view);
			break;
		case COM_NMAP:
			break;
		case COM_NOH:
			{
				if(view->selected_files)
				{
					int y = 0;
					for(y = 0; y < view->list_rows; y++)
					{
						if(view->dir_entry[y].selected)
							view->dir_entry[y].selected = 0;
					}
					draw_dir_list(view, view->top_line, view->list_pos);
					moveto_list_pos(view, view->list_pos);
				}
			}
			break;
		case COM_ONLY:
			{
				curr_stats.number_of_windows = 1;
				redraw_window();
			//my_system("screen -X eval \"only\"");
			}
			break;
		case COM_PWD:
			status_bar_message(view->curr_dir);
			save_msg = 1;
			break;
		case COM_X:
		case COM_QUIT:
			{
				if(cfg.vim_filter)
				{
					char buf[256];
					FILE *fp;


					snprintf(buf, sizeof(buf), "%s/vimfiles", cfg.config_dir);
					fp = fopen(buf, "w");
					endwin();
					fprintf(fp, "NULL");
					fclose(fp);
					exit(0);
				}

				write_config_file();

				endwin();
				system("clear");
				exit(0);
			}
			break;
		case COM_SORT:
			show_sort_menu(view);
			break;
		case COM_SCREEN:
			{
				if(cfg.use_screen)
					cfg.use_screen = 0;
				else
					cfg.use_screen = 1;
				curr_stats.setting_change = 1;
			}
			break;
		case COM_SHELL:
			shellout(NULL, 0);
			break;
		case COM_SPLIT:
			{
				curr_stats.number_of_windows = 2;
				redraw_window();
				/*
				char *tmp = NULL;

				if (!cfg.use_screen)
					break;

				if (cmd->args) 
				{
					if (strchr(cmd->args, '%'))
					{
						tmp = expand_macros(view, cmd->args, NULL, 0, 0);
					}
					else
						tmp = strdup(cmd->args);
				}
				split_screen(view, tmp);
				*/
			}
			break;
		case COM_SYNC:
			change_directory(other_view, view->curr_dir);
			load_dir_list(other_view, 0);
			break;
		case COM_UNMAP:
			break;
		case COM_VIEW:
			{
				if(curr_stats.number_of_windows == 1)
				{
					show_error_msg("Cannot view files",
							"Cannot view files in one window mode ");
					break;
				}
				if(curr_stats.view)
				{
					curr_stats.view = 0;

					wbkgdset(other_view->title,
						COLOR_PAIR(BORDER_COLOR + other_view->color_scheme));
					wbkgdset(other_view->win,
						COLOR_PAIR(WIN_COLOR + other_view->color_scheme));
					change_directory(other_view, other_view->curr_dir);
					load_dir_list(other_view, 0);
					change_directory(curr_view, curr_view->curr_dir);
					load_dir_list(curr_view, 0);
					moveto_list_pos(curr_view, curr_view->list_pos);
				}
				else
				{
					curr_stats.view = 1;
					quick_view_file(view);
				}
			}
			break;
		case COM_VIFM:
			show_error_msg(" I haven't gotten here yet ",
						"Sorry this is not implemented ");
			break;
		case COM_YANK:
			{
				/*
				int selection_worked = 0;

				selection_worked  = select_files_in_range(view);

				if (selection_worked)
					yank_selected_files(view);
					*/
				show_error_msg(":yank is not implemented yet",
					   	":yank is not implemented yet ");
			}
			break;
		case COM_VMAP:
			break;
		default:
			{
				char buf[48];
				snprintf(buf, sizeof(buf), "Builtin is %d", cmd->builtin);
				show_error_msg("Internal Error in Command.c", buf);
			}
			break;
	}

	if (view->selected_files)
	{
		int x;
		for (x = 0; x < view->list_rows; x++)
			view->dir_entry[x].selected = 0;

	}

	return save_msg;
}

int 
execute_user_command(FileView *view, cmd_t *cmd)
{
	char *expanded_com = NULL;
	int use_menu = 0;
	int split = 0;

	if(strchr(command_list[cmd->is_user].action, '%') != NULL)
		expanded_com = expand_macros(view, command_list[cmd->is_user].action, 
		cmd->args, &use_menu, &split);
	else
		expanded_com = strdup(command_list[cmd->is_user].action);

	while(isspace(expanded_com[strlen(expanded_com) -1]))
		expanded_com[strlen(expanded_com) -1] = '\0';

	if(expanded_com[strlen(expanded_com)-1] == '&' 
			&& expanded_com[strlen(expanded_com) -2] == ' ')
	{
		expanded_com[strlen(expanded_com)-1] = '\0';
		cmd->background = 1;
	}

	if (use_menu)
	{

		show_user_menu(view, expanded_com);

		if(!cmd->background)
			my_free(expanded_com);

		return 0;
	}

	if (split)
	{
		if (!cfg.use_screen)
		{
			my_free(expanded_com);
			return 0;
		}
			
		split_screen(view, expanded_com);
		my_free(expanded_com);
		return 0;

	}

	if(!strncmp(expanded_com, "filter ", 7)) 
	{
		view->invert = 1;
		view->filename_filter = (char *)realloc(view->filename_filter,
				strlen(strchr(expanded_com, ' ')) +1);
		snprintf(view->filename_filter, 
				strlen(strchr(expanded_com, ' ')) +1, "%s", 
				strchr(expanded_com, ' ') +1); 

		load_dir_list(view, 1);
		moveto_list_pos(view, 0);
	}
	else if(!strncmp(expanded_com, "!", 1))
	{
		char buf[strlen(expanded_com) + 1];
		char *tmp = strcpy(buf, expanded_com);
		int pause = 0;
		tmp++;
		if(*tmp == '!')
		{
			pause = 1;
			tmp++;
		}
		while(isspace(*tmp))
				tmp++;

		if((strlen(tmp) > 0) && cmd->background)
			start_background_job(tmp);
		else if(strlen(tmp) > 0)
			shellout(tmp, pause);
	}
	else if(!strncmp(expanded_com, "/", 1))
	{
		strncpy(view->regexp, expanded_com +1, sizeof(view->regexp));
		return find_pattern(view, view->regexp);
	}
	else if(cmd->background)
	{
		char buf[strlen(expanded_com) + 1];
		char *tmp = strcpy(buf, expanded_com);
		start_background_job(tmp);
	}
	else
		shellout(expanded_com, 0);

	if(!cmd->background)
		my_free(expanded_com);


	if(view->selected_files)
	{
		free_selected_file_array(view);
		view->selected_files = 0;
		load_dir_list(view, 1);
	}
	return 0;
}

int
execute_command(FileView *view, char *command)
{
	cmd_t cmd;
	int result;

	cmd.cmd_name = NULL;
	cmd.args = NULL;

	result = parse_command(view, command, &cmd);

	/* !! command  is already handled in parse_command() */
	if(result == 0)
		return 0;

	/* Invalid command or range. */
	if(result < 0)
		return 1;

	if(cmd.builtin > -1)
		execute_builtin_command(view, &cmd);
	else
		execute_user_command(view, &cmd);

	my_free(cmd.cmd_name);
	my_free(cmd.args);

	return 0;
}

int
get_command(FileView *view, int type, void * ptr)
{
	char * command = my_rl_gets(type);

	if(command == NULL)
	{
		if (type == GET_SEARCH_PATTERN || type == MAPPED_SEARCH)
			return find_pattern(view, view->regexp);
		else
			return 1;
	}

	if(type == GET_COMMAND || type == MAPPED_COMMAND 
			|| type == GET_VISUAL_COMMAND)
	{
		save_command_history(command);
		return execute_command(view, command);
	}
	else if(type == GET_SEARCH_PATTERN || type == MAPPED_SEARCH)
	{
		strncpy(view->regexp, command, sizeof(view->regexp));
		save_search_history(command);
		return find_pattern(view, command);
	}
	else if (type == MENU_SEARCH)
		return search_menu_list(view, command, ptr);
	else if (type == MENU_COMMAND)
		return execute_menu_command(view, command, ptr);

	return 0;
}

