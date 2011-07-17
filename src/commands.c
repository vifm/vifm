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

#include <curses.h>

#include <sys/wait.h>
#include <unistd.h> /* chdir() */

#include <assert.h>
#include <ctype.h> /* isspace() */
#include <errno.h> /* errno */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h> /*  system() */
#include <string.h> /* strncmp() */
#include <time.h>

#include "background.h"
#include "bookmarks.h"
#include "cmdline.h"
#include "color_scheme.h"
#include "commands.h"
#include "config.h"
#include "dir_stack.h"
#include "filelist.h"
#include "fileops.h"
#include "keys.h"
#include "log.h"
#include "macros.h"
#include "menu.h"
#include "menus.h"
#include "modes.h"
#include "opt_handlers.h"
#include "permissions_dialog.h"
#include "registers.h"
#include "search.h"
#include "signals.h"
#include "sort.h"
#include "sort_dialog.h"
#include "status.h"
#include "ui.h"
#include "undo.h"
#include "utils.h"

/* The order of the commands is important as :e will match the first
 * command starting with e.
 */
#ifndef TEST
static const struct {
	const char *name;
	int alias;
	int id;
}reserved_cmds[] = {
#else
const struct rescmd_info reserved_cmds[] = {
#endif
	{ .name = "!",           .alias = 0, .id = COM_EXECUTE     },
	{ .name = "apropos",     .alias = 0, .id = COM_APROPOS     },
	{ .name = "cd",          .alias = 0, .id = COM_CD          },
	{ .name = "change",      .alias = 0, .id = COM_CHANGE      },
	{ .name = "cm",          .alias = 1, .id = COM_CMAP        },
	{ .name = "cmap",        .alias = 0, .id = COM_CMAP        },
	{ .name = "cmdhistory",  .alias = 0, .id = COM_CMDHISTORY  },
	{ .name = "colo",        .alias = 1, .id = COM_COLORSCHEME },
	{ .name = "colorscheme", .alias = 0, .id = COM_COLORSCHEME },
	{ .name = "com",         .alias = 1, .id = COM_COMMAND     },
	{ .name = "command",     .alias = 0, .id = COM_COMMAND     },
	{ .name = "d",           .alias = 1, .id = COM_DELETE      },
	{ .name = "delc",        .alias = 1, .id = COM_DELCOMMAND  },
	{ .name = "delcommand",  .alias = 0, .id = COM_DELCOMMAND  },
	{ .name = "delete",      .alias = 0, .id = COM_DELETE      },
	{ .name = "di",          .alias = 1, .id = COM_DISPLAY     },
	{ .name = "dirs",        .alias = 0, .id = COM_DIRS        },
	{ .name = "display",     .alias = 0, .id = COM_DISPLAY     },
	{ .name = "e",           .alias = 1, .id = COM_EDIT        },
	{ .name = "edit",        .alias = 0, .id = COM_EDIT        },
	{ .name = "empty",       .alias = 0, .id = COM_EMPTY       },
	{ .name = "file",        .alias = 0, .id = COM_FILE        },
	{ .name = "filter",      .alias = 0, .id = COM_FILTER      },
	{ .name = "h",           .alias = 1, .id = COM_HELP        },
	{ .name = "help",        .alias = 0, .id = COM_HELP        },
	{ .name = "his",         .alias = 1, .id = COM_HISTORY     },
	{ .name = "history",     .alias = 0, .id = COM_HISTORY     },
	{ .name = "invert",      .alias = 0, .id = COM_INVERT      },
	{ .name = "jobs",        .alias = 0, .id = COM_JOBS        },
	{ .name = "locate",      .alias = 0, .id = COM_LOCATE      },
	{ .name = "ls",          .alias = 0, .id = COM_LS          },
	{ .name = "map",         .alias = 0, .id = COM_MAP         },
	{ .name = "marks",       .alias = 0, .id = COM_MARKS       },
	{ .name = "nm",          .alias = 1, .id = COM_NMAP        },
	{ .name = "nmap",        .alias = 0, .id = COM_NMAP        },
	{ .name = "noh",         .alias = 1, .id = COM_NOHLSEARCH  },
	{ .name = "nohlsearch",  .alias = 0, .id = COM_NOHLSEARCH  },
	{ .name = "on",          .alias = 1, .id = COM_ONLY        },
	{ .name = "only",        .alias = 0, .id = COM_ONLY        },
	{ .name = "popd",        .alias = 0, .id = COM_POPD        },
	{ .name = "pushd",       .alias = 0, .id = COM_PUSHD       },
	{ .name = "pwd",         .alias = 0, .id = COM_PWD         },
	{ .name = "q",           .alias = 1, .id = COM_QUIT        },
	{ .name = "quit",        .alias = 0, .id = COM_QUIT        },
	{ .name = "reg",         .alias = 1, .id = COM_REGISTERS   },
	{ .name = "registers",   .alias = 0, .id = COM_REGISTERS   },
	{ .name = "rename",      .alias = 0, .id = COM_RENAME      },
	{ .name = "screen",      .alias = 0, .id = COM_SCREEN      },
	{ .name = "se",          .alias = 1, .id = COM_SET         },
	{ .name = "set",         .alias = 0, .id = COM_SET         },
	{ .name = "sh",          .alias = 1, .id = COM_SHELL       },
	{ .name = "shell",       .alias = 0, .id = COM_SHELL       },
	{ .name = "sor",         .alias = 1, .id = COM_SORT        },
	{ .name = "sort",        .alias = 0, .id = COM_SORT        },
	{ .name = "sp",          .alias = 1, .id = COM_SPLIT       },
	{ .name = "split",       .alias = 0, .id = COM_SPLIT       },
	{ .name = "sync",        .alias = 0, .id = COM_SYNC        },
	{ .name = "undol",       .alias = 1, .id = COM_UNDOLIST    },
	{ .name = "undolist",    .alias = 0, .id = COM_UNDOLIST    },
	{ .name = "unmap",       .alias = 0, .id = COM_UNMAP       },
	{ .name = "view",        .alias = 0, .id = COM_VIEW        },
	{ .name = "vifm",        .alias = 0, .id = COM_VIFM        },
	{ .name = "vm",          .alias = 1, .id = COM_VMAP        },
	{ .name = "vmap",        .alias = 0, .id = COM_VMAP        },
	{ .name = "w",           .alias = 1, .id = COM_WRITE       },
	{ .name = "wq",          .alias = 0, .id = COM_WQ          },
	{ .name = "write",       .alias = 0, .id = COM_WRITE       },
	{ .name = "x",           .alias = 0, .id = COM_X           },
	{ .name = "y",           .alias = 1, .id = COM_YANK        },
	{ .name = "yank",        .alias = 0, .id = COM_YANK        },
};

static int _gnuc_unused reserved_commands_size_guard[
	(ARRAY_LEN(reserved_cmds) == RESERVED) ? 1 : -1
];

#ifndef TEST
typedef struct current_command
{
	int start_range;
	int end_range;
	int count;
	char *cmd_name;
	char *args;
	char **argv;
	int argc;
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
}cmd_params;
#endif

static wchar_t * substitute_specs(const char *cmd);
static const char *skip_spaces(const char *cmd);
static const char *skip_word(const char *cmd);
static int exec_command(char *cmd, FileView *view, int type);
#ifndef TEST
static
#endif
char ** dispatch_line(const char *args, int *count);

/* TODO generalize command handling */

int
sort_this(const void *one, const void *two)
{
	const command_t *first = (const command_t *)one;
	const command_t *second = (const command_t *)two;

	return strcmp(first->name, second->name);
}

/* Returns command id or -1 */
#ifndef TEST
static
#endif
int
command_is_reserved(const char *name)
{
	int x;
	int len = strlen(name);

	if(len > 0 && name[len - 1] == '!')
		len--;

	for(x = 0; x < RESERVED; x++)
	{
		if(strncmp(reserved_cmds[x].name, name, len) == 0)
			return reserved_cmds[x].id;
	}
	return -1;
}

/* len could be NULL */
static const char *
get_cmd_name_info(const char *cmd_line, size_t *len)
{
	const char *p, *q;

	while(cmd_line[0] != '\0' && isspace(cmd_line[0]))
	{
		cmd_line++;
	}

	if(cmd_line[0] == '!') {
		if(len != NULL)
			*len = 1;
		return cmd_line;
	}

	if((p = strchr(cmd_line, ' ')) == NULL)
		p = cmd_line + strlen(cmd_line);

	q = p - 1;
	if(q >= cmd_line && *q == '!')
	{
		q--;
		p--;
	}
	while(q >= cmd_line && isalpha(*q))
		q--;
	q++;

	if(q[0] == '\'' && q[1] != '\0')
		q += 2;
	if(q[0] == '\'' && q[1] != '\0')
		q += 2;

	if(len != NULL)
		*len = p - q;
	return q;
}

int
get_buildin_id(const char *cmd_line)
{
	char buf[128];
	size_t len;
	const char *p;

	while(*cmd_line == ' ' || *cmd_line == ':')
		cmd_line++;

	p = get_cmd_name_info(cmd_line, &len);

	snprintf(buf, len + 1, "%s", p);

	if(buf[0] == '\0')
		return -1;

	return command_is_reserved(buf);
}

static int
command_is_being_used(char *command)
{
	int x;
	for(x = 0; x < cfg.command_num; x++)
	{
		if(strcmp(command_list[x].name, command) == 0)
			return 1;
	}
	return 0;
}

static char *
add_prefixes(const char *str, const char *completed)
{
	char *result;
	const char *p;
	p = strrchr(str, ' ');
	if(p == NULL)
		p = str;
	else
		p++;

	result = malloc(p - str + strlen(completed) + 1);
	if(result == NULL)
		return NULL;

	snprintf(result, p - str + 1, "%s", str);
	strcat(result, completed);
	return result;
}

static int
is_user_command(char *command)
{
	char buf[strlen(command) + 1];
	char *com;
	char *ptr;
	int x;

	com = strcpy(buf, command);

	if((ptr = strchr(com, ' ')) != NULL)
		*ptr = '\0';

	for(x = 0; x < cfg.command_num; x++)
	{
		if(strncmp(com, command_list[x].name, strlen(com)) == 0)
		{
			return x;
		}
	}

	return -1;
}

static int
skip_aliases(int pos_b)
{
	while(pos_b < RESERVED && reserved_cmds[pos_b].alias)
		pos_b++;
	return pos_b;
}

/* On the first call to this function,
 * the string to be parsed should be specified in str.
 * In each subsequent call that should parse the same string, str should be NULL
 * Returned string should be freed in caller.
 */
char *
command_completion(char *str, int users_only)
{
	static char *cmdstring;
	static char *string;
	static size_t len;
	static int offset;

	int pos_b_saved = 0, pos_u_saved = 0;
	int pos_b, pos_u;
	int i;

	if(str != NULL)
	{
		free(cmdstring);
		cmdstring = strdup(str);
		string = strrchr(cmdstring, ' ');
		if(string == NULL)
			string = cmdstring;
		else
			string++;

		len = strlen(string);
		offset = 0;
	}
	else
		offset++;

	if(users_only)
	{
		pos_b = -1;
	}
	else
	{
		pos_b = (string[0] == '\0') ? 0 : get_buildin_id(string);
		if(pos_b != -1)
			pos_b = skip_aliases(pos_b);
	}
	pos_u = is_user_command(string);

	i = 0;
	while(i < offset && (pos_b != -1 || pos_u != -1))
	{
		pos_b_saved = pos_b;
		pos_u_saved = pos_u;

		i++;
		if(pos_b != -1 && pos_u != -1)
		{
			if(strcmp(reserved_cmds[pos_b].name, command_list[pos_u].name) < 0)
				pos_b = skip_aliases(pos_b + 1);
			else
				pos_u++;
		}
		else
		{
			if(pos_b != -1)
				pos_b = skip_aliases(pos_b + 1);
			else
				pos_u++;
		}

		if(pos_b == RESERVED)
			pos_b = -1;
		else if(pos_b >= 0 && strncmp(reserved_cmds[pos_b].name, string, len) != 0)
			pos_b = -1;

		if(pos_u == cfg.command_num)
			pos_u = -1;
		else if(pos_u >= 0 && strncmp(command_list[pos_u].name, string, len) != 0)
			pos_u = -1;
	}

	if(pos_b == -1 && pos_u == -1)
	{
		if(offset == 1)
		{
			offset = -1;
			if(pos_b_saved != -1)
				return add_prefixes(cmdstring, reserved_cmds[pos_b_saved].name);
			else
				return add_prefixes(cmdstring, command_list[pos_u_saved].name);
		}
		offset = -1;
		return strdup(cmdstring);
	}
	else if(pos_b != -1 && pos_u != -1)
	{
		if(strcmp(reserved_cmds[pos_b].name, command_list[pos_u].name) < 0)
			return add_prefixes(cmdstring, reserved_cmds[pos_b].name);
		else
			return add_prefixes(cmdstring, command_list[pos_u].name);
	}
	else
	{
		if(pos_b != -1)
			return add_prefixes(cmdstring, reserved_cmds[pos_b].name);
		else
			return add_prefixes(cmdstring, command_list[pos_u].name);
	}
}

static void
save_search_history(char *pattern)
{
	int x = 0;

	if((cfg.search_history_num + 1) >= cfg.search_history_len)
		cfg.search_history_num = x = cfg.search_history_len - 1;
	else
		x = cfg.search_history_num + 1;

	while(x > 0)
	{
		cfg.search_history[x] = (char *)realloc(cfg.search_history[x],
				strlen(cfg.search_history[x - 1]) + 1);
		strcpy(cfg.search_history[x], cfg.search_history[x - 1]);
		x--;
	}

	cfg.search_history[0] = (char *)realloc(cfg.search_history[0],
			strlen(pattern) + 1);
	strcpy(cfg.search_history[0], pattern);
	cfg.search_history_num++;
	if(cfg.search_history_num >= cfg.search_history_len)
		cfg.search_history_num = cfg.search_history_len - 1;
}

static void
save_command_history(const char *command)
{
	int x = 0;

	/* Don't add :!! or :! to history list */
	if(!strcmp(command, "!!") || !strcmp(command, "!"))
		return;

	/* Don't add duplicates */
	if(cfg.cmd_history_num >= 0 && !strcmp(command, cfg.cmd_history[0]))
		return;

	if(cfg.cmd_history_num + 1 >= cfg.cmd_history_len)
		cfg.cmd_history_num = x = cfg.cmd_history_len - 1;
	else
		x = cfg.cmd_history_num + 1;

	while(x > 0)
	{
		cfg.cmd_history[x] = (char *)realloc(cfg.cmd_history[x],
				strlen(cfg.cmd_history[x - 1]) + 1);
		strcpy(cfg.cmd_history[x], cfg.cmd_history[x - 1]);
		x--;
	}

	cfg.cmd_history[0] = (char *)realloc(cfg.cmd_history[0], strlen(command) + 1);
	strcpy(cfg.cmd_history[0], command);
	cfg.cmd_history_num++;
	if (cfg.cmd_history_num >= cfg.cmd_history_len)
		cfg.cmd_history_num = cfg.cmd_history_len -1;
}

static int
is_entry_dir(FileView *view, int pos)
{
	char full[PATH_MAX];

	snprintf(full, sizeof(full), "%s/%s", view->curr_dir,
			view->dir_entry[pos].name);
	if(view->dir_entry[pos].type == UNKNOWN)
	{
		struct stat st;
		if(stat(full, &st) != 0)
			return 0;
		return S_ISDIR(st.st_mode);
	}

	if(view->dir_entry[pos].type == DIRECTORY)
		return 1;
	if(view->dir_entry[pos].type == LINK && check_link_is_dir(full))
		return 1;
	return 0;
}

#ifndef TEST
static
#endif
char *
append_selected_files(FileView *view, char *expanded, int under_cursor)
{
	int dir_name_len = 0;

	if(view == other_view)
		dir_name_len = strlen(other_view->curr_dir) + 1;

	if(view->selected_files && !under_cursor)
	{
		int y, x = 0;
		size_t len = strlen(expanded);
		for(y = 0; y < view->list_rows; y++)
		{
			int dir;
			char *temp;

			if(!view->dir_entry[y].selected)
				continue;

			/* Directory has / appended to the name this removes it. */
			dir = is_entry_dir(view, y);

			temp = escape_filename(view->dir_entry[y].name,
					strlen(view->dir_entry[y].name) - dir, 0);
			expanded = (char *)realloc(expanded,
					len + dir_name_len + 1 + strlen(temp) + 1 + 1);
			if(dir_name_len != 0)
				strcat(strcat(expanded, view->curr_dir), "/");
			strcat(expanded, temp);
			if(++x != view->selected_files)
				strcat(expanded, " ");

			free(temp);

			len = strlen(expanded);
		}
	}
	else
	{
		int dir;
		char *temp;

		/* Directory has / appended to the name this removes it. */
		dir = is_entry_dir(view, view->list_pos);

		temp = escape_filename(view->dir_entry[view->list_pos].name,
				strlen(view->dir_entry[view->list_pos].name) - dir, 0);

		expanded = (char *)realloc(expanded,
				strlen(expanded) + dir_name_len + 1 + strlen(temp) + 1 + 1);
		if(dir_name_len != 0)
			strcat(strcat(expanded, view->curr_dir), "/");
		strcat(expanded, temp);

		free(temp);
	}

	return expanded;
}

static char *
expand_directory_path(FileView *view, char *expanded)
{
	char *t;
	char *escaped;

	if((escaped = escape_filename(view->curr_dir, 0, 0)) == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate memory");
		free(expanded);
		return NULL;
	}

	t = realloc(expanded, strlen(expanded) + strlen(escaped) + 1);
	if(t == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate memory");
		free(expanded);
		return NULL;
	}
	strcat(t, escaped);
	return t;
}

/* args could be equal NULL
 * The string returned needs to be freed in the calling function */
char *
expand_macros(FileView *view, const char *command, const char *args,
		int *use_menu, int *split)
{
	size_t cmd_len;
	char *expanded;
	size_t x;
	int y = 0;
	int len = 0;

	cmd_len = strlen(command);

	expanded = (char *)calloc(cmd_len + 1, sizeof(char *));

	for(x = 0; x < cmd_len; x++)
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
					if(args == NULL)
						break;
					else
					{
						char arg_buf[strlen(args) + 2];

						expanded = (char *)realloc(expanded,
								strlen(expanded) + strlen(args) + 3);
						snprintf(arg_buf, sizeof(arg_buf), "%s", args);
						strcat(expanded, arg_buf);
						len = strlen(expanded);
					}
				}
				break;
			case 'b': /* selected files of both dirs */
				expanded = append_selected_files(curr_view, expanded, 0);
				len = strlen(expanded);
				expanded = realloc(expanded, len + 1 + 1);
				strcat(expanded, " ");
				expanded = append_selected_files(other_view, expanded, 0);
				len = strlen(expanded);
				break;
			case 'c': /* current dir file under the cursor */
				expanded = append_selected_files(curr_view, expanded, 1);
				len = strlen(expanded);
				break;
			case 'C': /* other dir file under the cursor */
				expanded = append_selected_files(other_view, expanded, 1);
				len = strlen(expanded);
				break;
			case 'f': /* current dir selected files */
				expanded = append_selected_files(curr_view, expanded, 0);
				len = strlen(expanded);
				break;
			case 'F': /* other dir selected files */
				expanded = append_selected_files(other_view, expanded, 0);
				len = strlen(expanded);
				break;
			case 'd': /* current directory */
				expanded = expand_directory_path(curr_view, expanded);
				len = strlen(expanded);
				break;
			case 'D': /* other directory */
				expanded = expand_directory_path(other_view, expanded);
				len = strlen(expanded);
				break;
			case 'm': /* use menu */
				*use_menu = 1;
				break;
			case 's': /* split in new screen region */
				*split = 1;
				break;
			case '%':
				expanded = (char *)realloc(expanded, len + 2);
				strcat(expanded, "%");
				len++;
				break;
			default:
				break;
		}
		x++;
		y = x;

		while(x < cmd_len)
		{
			if(command[x] == '%')
				break;
			x++;
		}
		expanded = (char *)realloc(expanded, len + cmd_len + 1);
		strncat(expanded, command + y, x - y);
		len = strlen(expanded);
		x++;
	}while(x < cmd_len);

	len++;
	expanded[len] = '\0';
	if(len > cfg.max_args/2)
		show_error_msg("Argument is too long", " FIXME ");

	return expanded;
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
		show_error_msg("Trying to delete a reserved Command", s);
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
		/* TODO rewrite this strange piece of code */
		while(x < cfg.command_num)
		{
			command_list[x].name = (char *)realloc(command_list[x].name,
					strlen(command_list[x + 1].name) + 1);
			strcpy(command_list[x].name, command_list[x +1].name);
			command_list[x].action = (char *)realloc(command_list[x].action,
					strlen(command_list[x + 1].action) + 1);
			strcpy(command_list[x].action, command_list[x +1].action);
			x++;
		}

		free(command_list[x].name);
		free(command_list[x].action);
		curr_stats.setting_change = 1;
	}
	else
		show_error_msg("Command Not Found", s);
}

void
add_command(char *name, char *action)
{
	int i, len;

	if(command_is_reserved(name) > -1)
		return;
	if(isdigit(*name))
	{
		show_error_msg("Invalid Command Name",
				"Commands cannot start with a number.");
		return;
	}

	if(command_is_being_used(name))
		return;

	len = strlen(name);
	for(i = 0; i < len; i++)
		if(name[i] == '!' && i != len - 1)
		{
			show_error_msg("Invalid Command Name",
					"Commands can contain exclamation mark only as the last character.");
			return;
		}

	command_list = (command_t *)realloc(command_list,
			(cfg.command_num + 1) * sizeof(command_t));

	command_list[cfg.command_num].name = (char *)malloc(strlen(name) + 1);
	strcpy(command_list[cfg.command_num].name, name);
	command_list[cfg.command_num].action = strdup(action);
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
	{
		show_error_msg("Not enough arguments",
				"Syntax is :com command_name command_action");
		return;
	}

	*ptr = '\0';
	ptr++;

	while(isspace(*ptr) && *ptr != '\0')
		ptr++;

	if((strlen(ptr) < 1))
	{
		show_error_msg("To set a Command Use:", ":com command_name command_action");
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
		snprintf(buf, sizeof(buf), "\"%s\" is a reserved command name", com_name);
		show_error_msg("", buf);
		free(com_action);
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

	free(com_action);
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

/*
 * pause:
 *  > 0 - pause always
 *  = 0 - not pause
 *  < 0 - pause on error
 */
int
shellout(char *command, int pause)
{
	char buf[cfg.max_args];
	int result;

	if(command != NULL)
	{
		if(cfg.use_screen)
		{
			char *escaped;
			char *ptr = (char *)NULL;
			char *title = strstr(command, cfg.vi_command);

			/* Needed for symlink directories and sshfs mounts */
			escaped = escape_filename(curr_view->curr_dir, 0, 0);
			snprintf(buf, sizeof(buf), "screen -X setenv PWD %s", escaped);
			free(escaped);

			my_system(buf);

			if(title != NULL)
			{
				if(pause > 0)
					snprintf(buf, sizeof(buf), "screen -t \"%s\" sh -c '%s; vifm-pause'",
							title + strlen(cfg.vi_command) +1, command);
				else
				{
					escaped = escape_filename(command, 0, 0);
					snprintf(buf, sizeof(buf), "screen -t \"%s\" sh -c %s",
							title + strlen(cfg.vi_command) + 1, escaped);
					free(escaped);
				}
			}
			else
			{
				ptr = strchr(command, ' ');
				if(ptr != NULL)
				{
					*ptr = '\0';
					title = strdup(command);
					*ptr = ' ';
				}
				else
					title = strdup("Shell");

				if(pause > 0)
					snprintf(buf, sizeof(buf),
							"screen -t \"%.10s\" sh -c '%s; vifm-pause'", title, command);
				else
				{
					escaped = escape_filename(command, 0, 0);
					snprintf(buf, sizeof(buf), "screen -t \"%.10s\" sh -c %s", title,
							escaped);
					free(escaped);
				}
				free(title);
			}
		}
		else
		{
			if(pause > 0)
				snprintf(buf, sizeof(buf), "%s; vifm-pause", command);
			else
				snprintf(buf, sizeof(buf), "%s", command);
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

	/* force views update */
	lwin.dir_mtime = 0;
	rwin.dir_mtime = 0;

	result = WEXITSTATUS(my_system(buf));

	if(result != 0 && pause < 0)
		my_system("vifm-pause");

	/* There is a problem with using the screen program and
	 * catching all the SIGWICH signals.  So just redraw the window.
	 */
	if(!isendwin() && cfg.use_screen)
		redraw_window();
	else
		reset_prog_mode();

	curs_set(0);

	return result;
}

#ifndef TEST
static
#endif
void
initialize_command_struct(cmd_params *cmd)
{
	cmd->start_range = 0;
	cmd->end_range = 0;
	cmd->count = 0;
	cmd->cmd_name = NULL;
	cmd->args = NULL;
	cmd->argv = NULL;
	cmd->argc = 0;
	cmd->background = 0;
	cmd->split = 0;
	cmd->builtin = -1;
	cmd->is_user = -1;
	cmd->pos = 0;
	cmd->pause = 0;
}

/* Returns zero on success */
#ifndef TEST
static
#endif
int
select_files_in_range(FileView *view, cmd_params *cmd)
{
	int x;
	int y = 0;

	/* Both a starting range and an ending range are given. */
	if(cmd->start_range > -1)
	{
		if(cmd->end_range < cmd->start_range)
		{
			int t;
			if(!query_user_menu("Command Error",
						"Backwards range given, OK to swap?"))
				return 1;
			t = cmd->end_range;
			cmd->end_range = cmd->start_range;
			cmd->start_range = t;
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
	else if(view->selected_files == 0)
	{
		if(!cmd->count)
			cmd->count = 1;

		/* A one digit range with a count. :4y5 */
		if(cmd->end_range)
		{
			for(x = 0; x < view->list_rows; x++)
				view->dir_entry[x].selected = 0;

			y = 0;
			for(x = cmd->end_range; x < view->list_rows; x++)
			{
				if(y == cmd->count)
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
				if(cmd->count == y)
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
check_for_range(FileView *view, char *command, cmd_params *cmd)
{
	while(isspace(command[cmd->pos]) && (size_t)cmd->pos < strlen(command))
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
		cmd->end_range = view->list_rows - 1;
		cmd->pos++;
	}
	else if(command[cmd->pos] == ',')
	{
		cmd->start_range = view->list_pos;
	}
	else if(command[cmd->pos] == '.')
	{
		cmd->start_range = view->list_pos;
		cmd->pos++;
	}
	else if(command[cmd->pos] == '%')
	{
		cmd->start_range = 1;
		cmd->end_range = view->list_rows - 1;
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
		cmd->start_range = atoi(num_buf) - 1;
	}
	while(isspace(command[cmd->pos]) && (size_t)cmd->pos < strlen(command))
		cmd->pos++;

	/* Check for second number of range. */
	if(command[cmd->pos] == ',')
	{
		cmd->pos++;
		while(isspace(command[cmd->pos]) && (size_t)cmd->pos < strlen(command))
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
			cmd->end_range = atoi(num_buf) - 1;
		}
		else
			cmd->end_range = view->list_pos;
	}
	else if(!cmd->end_range)/* Only one number is given for the range */
	{
		cmd->end_range = cmd->start_range;
		cmd->start_range = -1;
	}

	while(isspace(command[cmd->pos]) && (size_t)cmd->pos < strlen(command))
		cmd->pos++;

	if(command[cmd->pos] == '\0')
	{
		moveto_list_pos(view, cmd->end_range);
		return -10;
	}

	return 1;
}

#ifndef TEST
static
#endif
int
parse_command(FileView *view, char *command, cmd_params *cmd)
{
	size_t len;
	size_t pre_cmdname_len;
	int result;

	initialize_command_struct(cmd);

	len = strlen(command);
	while(len > 0 && isspace(command[0]))
	{
		command++;
		len--;
	}
	while(len > 0 && isspace(command[len - 1]))
		command[--len] = '\0';

	if(len == 0)
		return 0;

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
			show_error_msg("Command Error", "No Previous Commands in History List");

		return 0;
	}

	pre_cmdname_len = cmd->pos;
	cmd->cmd_name = strdup(command + cmd->pos);

	/* The builtin commands do not contain numbers and ! is only used at the
	 * end of the command name.
	 */
	while(cmd->cmd_name[cmd->pos - pre_cmdname_len] != ' '
			&& (size_t)cmd->pos < strlen(cmd->cmd_name) + pre_cmdname_len
			&& cmd->cmd_name[cmd->pos - pre_cmdname_len] != '!')
		cmd->pos++;

	if(cmd->cmd_name[cmd->pos - pre_cmdname_len] != '!'
			|| cmd->pos == pre_cmdname_len)
	{
		cmd->cmd_name[cmd->pos - pre_cmdname_len] = '\0';
		cmd->pos++;
	}
	else /* Prevent eating '!' after command name. by not doing cmd->pos++ */
	{
		int was_em = cmd->cmd_name[cmd->pos - pre_cmdname_len] == '!';

		cmd->cmd_name[cmd->pos - pre_cmdname_len] = '\0';
		if(was_em && command_is_reserved(cmd->cmd_name) < 0 &&
				is_user_command(cmd->cmd_name) > - 1)
		{
			cmd->cmd_name[cmd->pos - pre_cmdname_len] = '!';
			cmd->cmd_name[cmd->pos - pre_cmdname_len + 1] = '\0';
			cmd->pos++;
		}
	}

	if(strlen(command) > (size_t)cmd->pos)
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
		cmd->argv = dispatch_line(cmd->args, &cmd->argc);
	}

	/* Get the actual command name. */
	if((cmd->builtin = command_is_reserved(cmd->cmd_name)) > - 1)
	{
		cmd->cmd_name = (char *)realloc(cmd->cmd_name,
				strlen(reserved_cmds[cmd->builtin].name) + 1);
		snprintf(cmd->cmd_name, sizeof(reserved_cmds[cmd->builtin].name),
				"%s", reserved_cmds[cmd->builtin].name);
		return 1;
	}
	else if((cmd->is_user = is_user_command(cmd->cmd_name)) > - 1)
	{
		cmd->cmd_name = realloc(cmd->cmd_name,
				strlen(command_list[cmd->is_user].name) + 1);
		snprintf(cmd->cmd_name, sizeof(command_list[cmd->is_user].name),
				"%s", command_list[cmd->is_user].name);
		return 1;
	}
	else
	{
		free(cmd->cmd_name);
		free(cmd->args);
		free_string_array(cmd->argv, cmd->argc);
		status_bar_message("Unknown Command");
		return -1;
	}
}

static int
do_map(cmd_params *cmd, const char *map_type, const char *map_cmd, int mode)
{
	char err_msg[128];
	wchar_t *keys, *mapping;
	char *raw_rhs, *rhs;
	char t;
	int result;

	sprintf(err_msg, "The :%s command requires two arguments - :%s lhs rhs",
			map_cmd, map_cmd);

	if(cmd->args == NULL || *cmd->args == '\0')
	{
		if(map_type[0] == '\0')
		{
			show_error_msg("Command Error", err_msg);
			return -1;
		}
		else
		{
			show_map_menu(curr_view, map_type, list_cmds(mode));
			return 0;
		}
	}

	raw_rhs = (char*)skip_word(cmd->args);
	if(*raw_rhs == '\0')
	{
		show_error_msg("Command Error", err_msg);
		return 0;
	}
	t = *raw_rhs;
	*raw_rhs = '\0';

	rhs = (char*)skip_spaces(raw_rhs + 1);
	keys = substitute_specs(cmd->args);
	mapping = substitute_specs(rhs);
	result = add_user_keys(keys, mapping, mode);
	free(mapping);
	free(keys);

	*raw_rhs = t;

	if(result == -1)
		show_error_msg("Mapping Error", "Not enough memory");

	return 0;
}

static void
comm_cd(FileView *view, cmd_params *cmd)
{
	char dir[PATH_MAX];

	if(cmd->args)
	{
		if(cmd->args[0] == '/')
			snprintf(dir, sizeof(dir), "%s", cmd->args);
		else if(cmd->args[0] == '~')
			snprintf(dir, sizeof(dir), "%s%s", cfg.home_dir, cmd->args + 1);
		else if(strcmp(cmd->args, "%D") == 0)
			snprintf(dir, sizeof(dir), "%s", other_view->curr_dir);
		else if(strcmp(cmd->args, "-") == 0)
			snprintf(dir, sizeof(dir), "%s", view->last_dir);
		else
			snprintf(dir, sizeof(dir), "%s/%s", view->curr_dir, cmd->args);
	}
	else
	{
		snprintf(dir, sizeof(dir), "%s", cfg.home_dir);
	}

	if(access(dir, F_OK) != 0)
	{
		char buf[1 + PATH_MAX + 1 + 1];

		LOG_SERROR_MSG(errno, "Can't access(,F_OK) \"%s\"", dir);

		snprintf(buf, sizeof(buf), "\"%s\"", dir);
		show_error_msg("Can't access destination", buf);
		return;
	}

	if(change_directory(view, dir) < 0)
		return;

	load_dir_list(view, 0);
	moveto_list_pos(view, view->list_pos);
}

char *
fast_run_complete(char *cmd)
{
	char *buf = NULL;
	char *completed1, *completed2;
	char *p;

	p = strchr(cmd, ' ');
	if(p == NULL)
		p = cmd + strlen(cmd);
	else
		*p = '\0';

	completed1 = exec_completion(cmd);
	completed2 = exec_completion(NULL);

	if(strcmp(cmd, completed2) != 0)
	{
		status_bar_message("Command beginning is ambiguous");
	}
	else
	{
		buf = malloc(strlen(completed1) + 1 + strlen(p) + 1);
		sprintf(buf, "%s %s", completed1, p);
	}
	free(completed2);
	free(completed1);

	return buf;
}

#ifndef TEST
static
#endif
char *
edit_cmd_selection(FileView *view)
{
	int use_menu = 0;
	int split = 0;
	char *buf;
	char *files = expand_macros(view, "%f", NULL, &use_menu, &split);

	if((buf = (char *)malloc(strlen(cfg.vi_command) + strlen(files) + 2)) != NULL)
		snprintf(buf, strlen(cfg.vi_command) + 1 + strlen(files) + 1, "%s %s",
				cfg.vi_command, files);

	free(files);
	return buf;
}

static void
set_view_filter(FileView *view, const char *filter, int invert)
{
	view->invert = invert;
	view->filename_filter = realloc(view->filename_filter, strlen(filter) + 1);
	strcpy(view->filename_filter, filter);
	load_saving_pos(view, 1);
	curr_stats.setting_change = 1;
}

static int
execute_builtin_command(FileView *view, cmd_params *cmd)
{
	int save_msg = 0;

	switch(cmd->builtin)
	{
		case COM_EXECUTE:
			{
				int i = 0;
				int pause = 0;
				char *com = NULL;

				if(cmd->args)
				{
					int use_menu = 0;
					int split = 0;
					if(strchr(cmd->args, '%') != NULL)
						com = expand_macros(view, cmd->args, NULL, &use_menu, &split);
					else
						com = strdup(cmd->args);

					if(com[i] == '!')
					{
						pause = 1;
						i++;
					}
					while(isspace(com[i]) && (size_t)i < strlen(com))
						i++;

					if(strlen(com + i) == 0)
					{
						free(com);
						break;
					}

					if(cmd->background)
						start_background_job(com + i);
					else
					{
						if(shellout(com + i, pause ? 1 : (cfg.fast_run ? 0 : -1)) == 127 &&
								cfg.fast_run)
						{
							char *buf = fast_run_complete(com + i);
							if(buf == NULL)
								save_msg = 1;
							else
								shellout(buf, pause ? 1 : -1);
							free(buf);
						}
					}

					if(!cmd->background)
						free(com);
				}
				else
				{
					show_error_msg("Command Error",
						"The :! command requires an argument - :!command");
					save_msg = 1;
				}
			}
			break;
		case COM_APROPOS:
			{
				if(cmd->args != NULL)
				{
					show_apropos_menu(view, cmd->args);
				}
			}
			break;
		case COM_CHANGE:
			enter_permissions_mode(view);
			break;
		case COM_CD:
			comm_cd(view, cmd);
			break;
		case COM_CMAP:
			save_msg = do_map(cmd, "Command Line", "cmap", CMDLINE_MODE);
			break;
		case COM_COLORSCHEME:
		{
			if(cmd->args)
				load_color_scheme(cmd->args);
			else /* Show menu with colorschemes listed */
				show_colorschemes_menu(view);
			break;
		}
		case COM_COMMAND:
			{
				if(cmd->args)
				{
					if(cmd->args[0] == '!')
					{
						int x = 1;
						while(isspace(cmd->args[x]) && (size_t)x < strlen(cmd->args))
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
		case COM_CMDHISTORY:
			show_cmdhistory_menu(view);
			break;
		case COM_DELETE:
			if(select_files_in_range(view, cmd) != 0)
				break;
			save_msg = delete_file(view, DEFAULT_REG_NAME, 0, NULL, 1);
			break;
		case COM_DELCOMMAND:
			if(cmd->args == NULL)
			{
				show_error_msg("Command Error",
						"The :delcommand command requires an argument - :delcommand cmd");
				save_msg = 1;
				break;
			}
			remove_command(cmd->args);
			break;
		case COM_DIRS:
			show_dirstack_menu(view);
			break;
		case COM_DISPLAY:
		case COM_REGISTERS:
			show_register_menu(view);
			break;
		case COM_RENAME:
			if(select_files_in_range(view, cmd) != 0)
				break;
			rename_files(view);
			save_msg = 1; /* there are always some message */
			break;
		case COM_EDIT:
			{
				if(cmd->args != NULL)
				{
					char buf[PATH_MAX];
					snprintf(buf, sizeof(buf), "%s %s", cfg.vi_command, cmd->args);
					shellout(buf, -1);
					break;
				}
				if(!view->selected_files || !view->dir_entry[view->list_pos].selected)
				{
					char buf[PATH_MAX];
					if(view->dir_entry[view->list_pos].name != NULL)
					{
						char *escaped =
								escape_filename(view->dir_entry[view->list_pos].name, 0, 0);
						snprintf(buf, sizeof(buf), "%s %s/%s", cfg.vi_command,
								view->curr_dir, escaped);
						free(escaped);
						shellout(buf, -1);
					}
				}
				else
				{
					char *cmd = edit_cmd_selection(view);
					if(cmd == NULL)
					{
						show_error_msg("Unable to allocate enough memory",
								"Cannot load file");
						break;
					}
					shellout(cmd, -1);
					free(cmd);
				}
			}
			break;
		case COM_EMPTY:
			{
				char buf[24 + (strlen(cfg.escaped_trash_dir) + 1)*2 + 1];
				snprintf(buf, sizeof(buf), "rm -rf %s/* %s/.[!.]*",
						cfg.escaped_trash_dir, cfg.escaped_trash_dir);
				clean_regs_with_trash();
				start_background_job(buf);
				clean_cmds_with_trash();
			}
			break;
		case COM_FILTER:
			{
				int invert = 1;
				const char *args = cmd->args;
				if(args == NULL)
				{
					args = "";
				}
				else if(args[0] == '!')
				{
					invert = 0;
					args++;
				}

				args = skip_spaces(args);

				if(args[0] != '\0' && args[0] == '/' && args[strlen(args) - 1] == '/')
				{
					if(cmd->argc == 2 && cmd->argv[0][0] == '!')
					{
						invert = 0;
						args = cmd->argv[1];
					}
					else if(cmd->argc == 1)
					{
						args = cmd->argv[0];
					}
				}
				else if(args[0] == '\0' && !invert)
				{
					view->invert = !view->invert;
					load_dir_list(view, 1);
					moveto_list_pos(view, 0);
					curr_stats.setting_change = 1;
				}
				else
					set_view_filter(view, args, invert);
			}
			break;
		case COM_FILE:
			show_filetypes_menu(view, cmd->background);
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
						shellout(help_file, -1);
					}
					else
					{
						snprintf(help_file, sizeof(help_file),
								"%s -c \'help vifm\' -c only", cfg.vi_command);
						shellout(help_file, -1);
					}
				}
				else
				{
					snprintf(help_file, sizeof(help_file),
							"%s %s/vifm-help.txt",
							cfg.vi_command, cfg.config_dir);

					shellout(help_file, -1);
				}
			}
			break;
		case COM_HISTORY:
			save_msg = show_history_menu(view);
			break;
		case COM_INVERT:
			view->invert = !view->invert;
			load_dir_list(view, 1);
			moveto_list_pos(view, 0);
			curr_stats.setting_change = 1;
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
			if(!cfg.use_screen)
				break;
			my_system("screen -X eval 'windowlist'");
			break;
		case COM_MAP:
			if(cmd->args != NULL && cmd->args[0] == '!')
			{
				save_msg = do_map(cmd, "", "map", CMDLINE_MODE);
			}
			else
			{
				save_msg = do_map(cmd, "", "map", NORMAL_MODE);
				if(save_msg == 0)
					save_msg = do_map(cmd, "", "map", VISUAL_MODE);
			}
			break;
			break;
		case COM_MARKS:
			show_bookmarks_menu(view);
			break;
		case COM_NMAP:
			save_msg = do_map(cmd, "Normal", "nmap", NORMAL_MODE);
			break;
		case COM_NOHLSEARCH:
			{
				if(view->selected_files)
				{
					int y = 0;
					for(y = 0; y < view->list_rows; y++)
					{
						if(view->dir_entry[y].selected)
							view->dir_entry[y].selected = 0;
					}
					draw_dir_list(view, view->top_line);
					moveto_list_pos(view, view->list_pos);
				}
			}
			break;
		case COM_ONLY:
			comm_only();
			break;
		case COM_POPD:
			if(popd() != 0)
			{
				status_bar_message("Directory stack empty");
				save_msg = 1;
			}
			break;
		case COM_PUSHD:
			if(cmd->args == NULL)
			{
				show_error_msg("Command Error",
						"The :pushd command requires an argument - :pushd directory");
				break;
			}
			if(pushd() != 0)
			{
				status_bar_message("Not enough memory");
				save_msg = 1;
				break;
			}
			comm_cd(view, cmd);
			break;
		case COM_PWD:
			status_bar_message(view->curr_dir);
			save_msg = 1;
			break;
		case COM_X:
		case COM_QUIT:
			if(cmd->args && cmd->args[0] == '!')
				curr_stats.setting_change = 0;
			comm_quit();
			break;
		case COM_SORT:
			enter_sort_mode(view);
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
		case COM_SET:
			if(cmd->args == NULL)
			{
				show_error_msg("Command Error",
						"The :set command requires at least one argument");
				save_msg = 1;
				break;
			}
			save_msg = process_set_args(cmd->args);
			break;
		case COM_SHELL:
			shellout(NULL, 0);
			break;
		case COM_SPLIT:
			comm_split();
			break;
		case COM_SYNC:
			change_directory(other_view, view->curr_dir);
			load_dir_list(other_view, 0);
			break;
		case COM_UNDOLIST:
			show_undolist_menu(view, cmd->args && cmd->args[0] == '!');
			break;
		case COM_UNMAP:
			break;
		case COM_VIEW:
			{
				if(curr_stats.number_of_windows == 1)
				{
					show_error_msg("Cannot view files",
							"Cannot view files in one window mode");
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
					load_dir_list(other_view, 1);
				}
				else
				{
					curr_stats.view = 1;
					quick_view_file(view);
				}
			}
			break;
		case COM_VIFM:
			show_vifm_menu(view);
			break;
		case COM_YANK:
			if(select_files_in_range(view, cmd) != 0)
				break;
			save_msg = yank_files(view, DEFAULT_REG_NAME, 0, NULL);
			break;
		case COM_VMAP:
			save_msg = do_map(cmd, "Visual", "vmap", VISUAL_MODE);
			break;
		case COM_WRITE:
			{
				int tmp;

				tmp = curr_stats.setting_change;
				if(cmd->args && cmd->args[0] == '!')
					curr_stats.setting_change = 1;
				write_config_file();
				curr_stats.setting_change = tmp;
			}
			break;
		case COM_WQ:
			curr_stats.setting_change = 1;
			comm_quit();
			break;
		default:
			{
				char buf[48];
				snprintf(buf, sizeof(buf), "Builtin is %d", cmd->builtin);
				show_error_msg("Internal Error in " __FILE__, buf);
			}
			break;
	}

	if(view->selected_files)
	{
		clean_selected_files(view);
		draw_dir_list(view, view->top_line);
		moveto_list_pos(view, view->list_pos);
	}

	return save_msg;
}

static wchar_t  *
substitute_specs(const char *cmd)
{
	wchar_t *buf, *p;

	buf = malloc((strlen(cmd) + 1)*sizeof(wchar_t));
	if(buf == NULL)
	{
		return NULL;
	}

	p = buf;
	while(*cmd != '\0')
	{
		if(strncmp(cmd, "<cr>", 4) == 0)
		{
			*p++ = L'\r';
			cmd += 4;
		}
		else if(strncmp(cmd, "<space>", 7) == 0)
		{
			*p++ = L' ';
			cmd += 7;
		}
		else if(cmd[0] == '<' && cmd[1] == 'f' && isdigit(cmd[2]) && cmd[3] == '>')
		{
			*p++ = KEY_F0 + (cmd[2] - '0');
			cmd += 4;
		}
		else if(cmd[0] == '<' && cmd[1] == 'f' && isdigit(cmd[2]) && isdigit(cmd[3])
				&& cmd[4] == '>')
		{
			int num = (cmd[2] - '0')*10 + (cmd[3] - '0');
			if(num < 64)
			{
				*p++ = KEY_F0 + num;
				cmd += 5;
			}
		}
		else
		{
			*p++ = (wchar_t)*cmd++;
		}
	}
	*p = L'\0';

	return buf;
}

static const char *
skip_spaces(const char *cmd)
{
	while(isspace(*cmd) && *cmd != '\0')
	{
		cmd++;
	}
	return cmd;
}

static const char *
skip_word(const char *cmd)
{
	while(!isspace(*cmd) && *cmd != '\0')
	{
		cmd++;
	}
	return cmd;
}

static int
execute_user_command(FileView *view, cmd_params *cmd)
{
	char *expanded_com = NULL;
	int use_menu = 0;
	int split = 0;

	if(select_files_in_range(view, cmd) != 0)
		return 0;

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

	if(use_menu)
	{
		show_user_menu(view, expanded_com);

		if(!cmd->background)
			free(expanded_com);

		return 0;
	}

	if(split)
	{
		if(!cfg.use_screen)
		{
			free(expanded_com);
			return 0;
		}

		split_screen(view, expanded_com);
		free(expanded_com);
		return 0;
	}

	if(strncmp(expanded_com, "filter ", 7) == 0)
	{
		view->invert = 1;
		view->filename_filter = (char *)realloc(view->filename_filter,
				strlen(strchr(expanded_com, ' ')) + 1);
		snprintf(view->filename_filter,
				strlen(strchr(expanded_com, ' ')) + 1, "%s",
				strchr(expanded_com, ' ') + 1);

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
		return find_pattern(view, view->regexp, 0);
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
		free(expanded_com);

	if(view->selected_files)
	{
		free_selected_file_array(view);
		view->selected_files = 0;
		load_dir_list(view, 1);
		if(view == curr_view)
			moveto_list_pos(view, view->list_pos);
	}
	return 0;
}

int
execute_command(FileView *view, char *command)
{
	cmd_params cmd;
	int result;

	cmd.cmd_name = NULL;
	cmd.args = NULL;

	while(isspace(*command) && *command != '\0')
		++command;

	if(command[0] == '"')
		return 0;

	result = parse_command(view, command, &cmd);

	/* !! command  is already handled in parse_command() */
	if(result == 0)
		return 0;

	/* Invalid command or range. */
	if(result < 0)
		return 1;

	if(cmd.builtin > -1)
		result = execute_builtin_command(view, &cmd);
	else
		result = execute_user_command(view, &cmd);

	free(cmd.cmd_name);
	free(cmd.args);
	free_string_array(cmd.argv, cmd.argc);

	return result;
}

/*
 * Return value:
 *  - 0 not in arg
 *  - 1 skip next char
 *  - 2 in arg
 */
#ifndef TEST
static
#endif
int
line_pos(const char *begin, const char *end)
{
	int state;
	enum { BEGIN, NO_QUOTING, S_QUOTING, D_QUOTING, R_QUOTING };

	state = BEGIN;
	while(begin != end)
	{
		switch(state)
		{
			case BEGIN:
				if(*begin == '\'')
					state = S_QUOTING;
				else if(*begin == '"')
					state = D_QUOTING;
				else if(*begin == '/')
					state = R_QUOTING;
				else if(*begin != ' ')
					state = NO_QUOTING;
				break;
			case NO_QUOTING:
				if(*begin == ' ')
				{
					state = BEGIN;
				}
				else if(*begin == '\'')
				{
					state = S_QUOTING;
				}
				else if(*begin == '"')
				{
					state = D_QUOTING;
				}
				else if(*begin == '\\')
				{
					begin++;
					if(begin == end)
						return 1;
				}
				break;
			case S_QUOTING:
				if(*begin == '\'')
					state = BEGIN;
				break;
			case D_QUOTING:
				if(*begin == '"')
				{
					state = BEGIN;
				}
				else if(*begin == '\\')
				{
					begin++;
					if(begin == end)
						return 1;
				}
				break;
			case R_QUOTING:
				if(*begin == '/')
				{
					state = BEGIN;
				}
				else if(*begin == '\\')
				{
					begin++;
					if(begin == end)
						return 1;
				}
				break;
		}
		begin++;
	}
	if(state == NO_QUOTING)
		;
	else if(state != BEGIN)
		return 2; /* error: no closing quote */

	return 0;
}

int
exec_commands(char *cmd, FileView *view, int type, int save_hist)
{
	int save_msg = 0;
	char *p, *q;

	if((type == GET_COMMAND || type == GET_VISUAL_COMMAND) && save_hist)
		save_command_history(cmd);

	p = cmd;
	q = cmd;
	while(*cmd != '\0')
	{
		if(*p == '\\')
		{
			if(*(p + 1) == '|')
			{
				*q++ = '|';
				p += 2;
			}
			else
			{
				*q++ = *p++;
				*q++ = *p++;
			}
		}
		else if((*p == '|' && line_pos(cmd, q) == 0) || *p == '\0')
		{
			if(*p != '\0')
				p++;

			while(*cmd == ' ' || *cmd == ':')
				cmd++;
			if(*cmd == '!' || strncmp(cmd, "com", 3) == 0)
			{
				save_msg += exec_command(cmd, view, type);
				break;
			}

			*q = '\0';
			q = p;

			save_msg += exec_command(cmd, view, type);

			cmd = q;
		}
		else
			*q++ = *p++;
	}

	return save_msg;
}

static int
exec_command(char *cmd, FileView *view, int type)
{
	if(cmd == NULL)
	{
		if(type == GET_FSEARCH_PATTERN || type == GET_BSEARCH_PATTERN)
			return find_pattern(view, view->regexp, type == GET_BSEARCH_PATTERN);
		return 0;
	}

	if(type == GET_COMMAND || type == GET_VISUAL_COMMAND)
	{
		return execute_command(view, cmd);
	}
	else if(type == GET_FSEARCH_PATTERN || type == GET_BSEARCH_PATTERN)
	{
		strncpy(view->regexp, cmd, sizeof(view->regexp));
		save_search_history(cmd);
		return find_pattern(view, cmd, type == GET_BSEARCH_PATTERN);
	}
	return 0;
}

void _gnuc_noreturn
comm_quit(void)
{
	unmount_fuse();

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
	write_info_file();

	endwin();
	exit(0);
}

void
comm_only(void)
{
	curr_stats.number_of_windows = 1;
	redraw_window();
	/* TODO see what this code about and decide if it can be used
	my_system("screen -X eval \"only\"");
	*/
}

void
comm_split(void)
{
	curr_stats.number_of_windows = 2;
	redraw_window();
}

static void
unescape(char *s, int regexp)
{
	char *p;

	p = s;
	while(s[0] != '\0')
	{
		if(s[0] == '\\' && (!regexp || s[1] == '/'))
			s++;
		*p++ = *s++;
	}
	*p = '\0';
}

static void
replace_esc(char *s)
{
	static const char table[] =
						/* 00  01  02  03  04  05  06  07  08  09  0a  0b  0c  0d  0e  0f */
	/* 00 */	"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
	/* 10 */	"\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
	/* 20 */	"\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f"
	/* 30 */	"\x00\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f"
	/* 40 */	"\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f"
	/* 50 */	"\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5a\x5b\x5c\x5d\x5e\x5f"
	/* 60 */	"\x60\x07\x0b\x63\x64\x65\x0c\x67\x68\x69\x6a\x6b\x6c\x6d\x0a\x6f"
	/* 70 */	"\x70\x71\x0d\x73\x09\x75\x0b\x77\x78\x79\x7a\x7b\x7c\x7d\x7e\x7f"
	/* 80 */	"\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f"
	/* 90 */	"\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f"
	/* a0 */	"\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf"
	/* b0 */	"\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf"
	/* c0 */	"\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf"
	/* d0 */	"\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf"
	/* e0 */	"\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef"
	/* f0 */	"\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff";

	char *str = s;
	char *p;

	p = s;
	while(*s != '\0')
	{
		if(*s != '\\')
		{
			*p++ = *s++;
			continue;
		}
		s++;
		if(*s == '\0')
		{
			LOG_ERROR_MSG("Escaped eol in \"%s\"", str);
			break;
		}
		*p++ = table[(int)*s++];
	}
	*p = '\0';
}

static int
get_args_count(const char *cmdstr)
{
	int i, state;
	int result = 0;
	enum { BEGIN, NO_QUOTING, S_QUOTING, D_QUOTING, R_QUOTING };

	state = BEGIN;
	for(i = 0; cmdstr[i] != '\0'; i++)
		switch(state)
		{
			case BEGIN:
				if(cmdstr[i] == '\'')
					state = S_QUOTING;
				else if(cmdstr[i] == '"')
					state = D_QUOTING;
				else if(cmdstr[i] == '/')
					state = R_QUOTING;
				else if(cmdstr[i] != ' ')
					state = NO_QUOTING;
				break;
			case NO_QUOTING:
				if(cmdstr[i] == ' ')
				{
					result++;
					state = BEGIN;
				}
				else if(cmdstr[i] == '\\')
				{
					if(cmdstr[i + 1] != '\0')
						i++;
				}
				break;
			case S_QUOTING:
				if(cmdstr[i] == '\'')
				{
					result++;
					state = BEGIN;
				}
				break;
			case D_QUOTING:
				if(cmdstr[i] == '"')
				{
					result++;
					state = BEGIN;
				}
				else if(cmdstr[i] == '\\')
				{
					if(cmdstr[i + 1] != '\0')
						i++;
				}
				break;
			case R_QUOTING:
				if(cmdstr[i] == '/')
				{
					result++;
					state = BEGIN;
				}
				else if(cmdstr[i] == '\\')
				{
					if(cmdstr[i + 1] != '\0')
						i++;
				}
				break;
		}
	if(state == NO_QUOTING)
		result++;
	else if(state != BEGIN)
		return 0; /* error: no closing quote */

	return result;
}

#ifndef TEST
static
#endif
char **
dispatch_line(const char *args, int *count)
{
	char *cmdstr;
	int len;
	int i, j;
	int state, st;
	char** params;

	enum { BEGIN, NO_QUOTING, S_QUOTING, D_QUOTING, R_QUOTING, ARG };

	*count = get_args_count(args);
	if(*count == 0)
		return NULL;

	params = malloc(sizeof(char*)*(*count + 1));
	if(params == NULL)
		return NULL;

	while(args[0] == ' ')
		args++;
	cmdstr = strdup(args);
	len = strlen(cmdstr);
	for(i = 0, st, j = 0, state = BEGIN; i <= len; ++i)
	{
		int prev_state = state;
		switch(state)
		{
			case BEGIN:
				if(cmdstr[i] == '\'')
				{
					st = i + 1;
					state = S_QUOTING;
				}
				else if(cmdstr[i] == '"')
				{
					st = i + 1;
					state = D_QUOTING;
				}
				else if(cmdstr[i] == '/')
				{
					st = i + 1;
					state = R_QUOTING;
				}
				else if(cmdstr[i] != ' ')
				{
					st = i;
					state = NO_QUOTING;
				}
				break;
			case NO_QUOTING:
				if(!cmdstr[i] || cmdstr[i] == ' ')
					state = ARG;
				else if(cmdstr[i] == '\\')
				{
					if(cmdstr[i + 1] != '\0')
						i++;
				}
				break;
			case S_QUOTING:
				if(!cmdstr[i] || cmdstr[i] == '\'')
					state = ARG;
			case D_QUOTING:
				if(!cmdstr[i] || cmdstr[i] == '"')
					state = ARG;
				else if(cmdstr[i] == '\\')
				{
					if(cmdstr[i + 1] != '\0')
						i++;
				}
				break;
			case R_QUOTING:
				if(!cmdstr[i] || cmdstr[i] == '/')
					state = ARG;
				else if(cmdstr[i] == '\\')
				{
					if(cmdstr[i + 1] == '/')
						i++;
				}
				break;
		}
		if(state == ARG)
		{
			/* found another argument */
			cmdstr[i] = '\0';
			params[j] = strdup(&cmdstr[st]);
			if(prev_state == NO_QUOTING)
				unescape(params[j], 0);
			else if(prev_state == D_QUOTING)
				replace_esc(params[j]);
			else if(prev_state == R_QUOTING)
				unescape(params[j], 1);
			j++;
			state = BEGIN;
		}
	}

	params[*count] = NULL;

	free(cmdstr);
	return params;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
