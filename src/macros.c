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

#include <assert.h> /* assert() */
#include <limits.h> /* PATH_MAX */
#include <stdlib.h> /* realloc() free() calloc() */
#include <string.h> /* strchr() strncat() strcpy() strlen() strcat() */

#include "cfg/config.h"
#include "menus/menus.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/test_helpers.h"
#include "utils/utils.h"
#include "filename_modifiers.h"

#include "macros.h"

TSTATIC char * append_selected_files(FileView *view, char expanded[],
		int under_cursor, int quotes, const char mod[]);
static char * append_selected_file(FileView *view, char *expanded,
		int dir_name_len, int pos, int quotes, const char *mod);
static char * expand_directory_path(FileView *view, char *expanded, int quotes,
		const char *mod);
static char * append_to_expanded(char *expanded, const char* str);

char *
expand_macros(FileView *view, const char *command, const char *args,
		MacroFlags *flags)
{
	/* TODO: refactor this function expand_macros() */

	static const char MACROS_WITH_QUOTING[] = "cCfFbdD";

	size_t cmd_len;
	char *expanded;
	size_t x;
	int y = 0;
	int len = 0;

	if(flags != NULL)
	{
		*flags = MACRO_NONE;
	}

	cmd_len = strlen(command);

	for(x = 0; x < cmd_len; x++)
		if(command[x] == '%')
			break;

	if(x >= cmd_len)
	{
		return strdup(command);
	}

	expanded = calloc(cmd_len + 1, sizeof(char));
	strncat(expanded, command, x);
	x++;
	len = strlen(expanded);

	do
	{
		int quotes = 0;
		if(command[x] == '"' && char_is_one_of(MACROS_WITH_QUOTING, command[x + 1]))
		{
			quotes = 1;
			x++;
		}
		switch(command[x])
		{
			case 'a': /* user arguments */
				if(args != NULL)
				{
					char arg_buf[strlen(args) + 2];

					expanded = (char *)realloc(expanded,
							strlen(expanded) + strlen(args) + 3);
					snprintf(arg_buf, sizeof(arg_buf), "%s", args);
					strcat(expanded, arg_buf);
					len = strlen(expanded);
				}
				break;
			case 'b': /* selected files of both dirs */
				expanded = append_selected_files(curr_view, expanded, 0, quotes,
						command + x + 1);
				len = strlen(expanded);
				expanded = realloc(expanded, len + 1 + 1);
				strcat(expanded, " ");
				expanded = append_selected_files(other_view, expanded, 0, quotes,
						command + x + 1);
				len = strlen(expanded);
				break;
			case 'c': /* current dir file under the cursor */
				expanded = append_selected_files(curr_view, expanded, 1, quotes,
						command + x + 1);
				len = strlen(expanded);
				break;
			case 'C': /* other dir file under the cursor */
				expanded = append_selected_files(other_view, expanded, 1, quotes,
						command + x + 1);
				len = strlen(expanded);
				break;
			case 'f': /* current dir selected files */
				expanded = append_selected_files(curr_view, expanded, 0, quotes,
						command + x + 1);
				len = strlen(expanded);
				break;
			case 'F': /* other dir selected files */
				expanded = append_selected_files(other_view, expanded, 0, quotes,
						command + x + 1);
				len = strlen(expanded);
				break;
			case 'd': /* current directory */
				expanded = expand_directory_path(curr_view, expanded, quotes,
						command + x + 1);
				len = strlen(expanded);
				break;
			case 'D': /* other directory */
				expanded = expand_directory_path(other_view, expanded, quotes,
						command + x + 1);
				len = strlen(expanded);
				break;
			case 'm': /* use menu */
				if(flags != NULL)
				{
					*flags = MACRO_MENU_OUTPUT;
				}
				break;
			case 'M': /* use menu like with :locate and :find */
				if(flags != NULL)
				{
					*flags = MACRO_MENU_NAV_OUTPUT;
				}
				break;
			case 'S': /* show command output in the status bar */
				if(flags != NULL)
				{
					*flags = MACRO_STATUSBAR_OUTPUT;
				}
				break;
			case 's': /* split in new screen region */
				if(flags != NULL)
				{
					*flags = MACRO_SPLIT;
				}
				break;
			case 'i': /* ignore output */
				if(flags != NULL)
				{
					*flags = MACRO_IGNORE;
				}
				break;
			case '%':
				expanded = (char *)realloc(expanded, len + 2);
				strcat(expanded, "%");
				len++;
				break;

			default:
				break;
		}
		if(command[x] != '\0')
			x++;

		while(x < cmd_len)
		{
			size_t len = get_mods_len(command + x);
			if(len == 0)
			{
				break;
			}
			x += len;
		}

		y = x;

		while(x < cmd_len)
		{
			if(command[x] == '%')
				break;
			if(command[x] != '\0')
				x++;
		}
		assert(x >= y);
		assert(y <= cmd_len);
		expanded = realloc(expanded, len + (x - y) + 1);
		strncat(expanded, command + y, x - y);
		len = strlen(expanded);
		x++;
	}
	while(x < cmd_len);

	if(len > cfg.max_args/2)
		show_error_msg("Argument is too long", " FIXME ");

	return expanded;
}

TSTATIC char *
append_selected_files(FileView *view, char expanded[], int under_cursor,
		int quotes, const char mod[])
{
	int dir_name_len = 0;
#ifdef _WIN32
	size_t old_len = strlen(expanded);
#endif

	if(view == other_view)
		dir_name_len = strlen(other_view->curr_dir) + 1;

	if(view->selected_files && !under_cursor)
	{
		int y, x = 0;
		for(y = 0; y < view->list_rows; y++)
		{
			if(!view->dir_entry[y].selected)
				continue;

			expanded = append_selected_file(view, expanded, dir_name_len, y, quotes,
					mod);

			if(++x != view->selected_files)
				strcat(expanded, " ");
		}
	}
	else
	{
		expanded = append_selected_file(view, expanded, dir_name_len,
				view->list_pos, quotes, mod);
	}

#ifdef _WIN32
	if(stroscmp(cfg.shell, "cmd") == 0)
		to_back_slash(expanded + old_len);
#endif

	return expanded;
}

static char *
append_selected_file(FileView *view, char *expanded, int dir_name_len, int pos,
		int quotes, const char *mod)
{
	char buf[PATH_MAX] = "";

	if(dir_name_len != 0)
		strcat(strcpy(buf, view->curr_dir), "/");
	strcat(buf, view->dir_entry[pos].name);
	chosp(buf);

	if(quotes)
	{
		const char *s = enclose_in_dquotes(apply_mods(buf, view->curr_dir, mod));
		expanded = realloc(expanded, strlen(expanded) + strlen(s) + 1 + 1);
		strcat(expanded, s);
	}
	else
	{
		char *temp;

		temp = escape_filename(apply_mods(buf, view->curr_dir, mod), 0);
		expanded = realloc(expanded, strlen(expanded) + strlen(temp) + 1 + 1);
		strcat(expanded, temp);
		free(temp);
	}

	return expanded;
}

static char *
expand_directory_path(FileView *view, char *expanded, int quotes,
		const char *mod)
{
	char *result;
	if(quotes)
	{
		const char *s = enclose_in_dquotes(apply_mods(view->curr_dir, "/", mod));
		result = append_to_expanded(expanded, s);
	}
	else
	{
		char *escaped;

		escaped = escape_filename(apply_mods(view->curr_dir, "/", mod), 0);
		if(escaped == NULL)
		{
			show_error_msg("Memory Error", "Unable to allocate enough memory");
			free(expanded);
			return NULL;
		}

		result = append_to_expanded(expanded, escaped);
		free(escaped);
	}

#ifdef _WIN32
	if(stroscmp(cfg.shell, "cmd") == 0)
		to_back_slash(result);
#endif

	return result;
}

static char *
append_to_expanded(char *expanded, const char* str)
{
	char *t;

	t = realloc(expanded, strlen(expanded) + strlen(str) + 1);
	if(t == NULL)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		free(expanded);
		return NULL;
	}
	strcat(t, str);
	return t;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
