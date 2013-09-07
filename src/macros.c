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

#include "macros.h"

#include <assert.h> /* assert() */
#include <ctype.h> /* tolower() */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* realloc() free() calloc() */
#include <string.h> /* strchr() strncat() strcpy() strlen() strcat() strdup() */

#include "cfg/config.h"
#include "menus/menus.h"
#include "utils/fs_limits.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/test_helpers.h"
#include "utils/utils.h"
#include "filename_modifiers.h"
#include "registers.h"

TSTATIC char * append_selected_files(FileView *view, char expanded[],
		int under_cursor, int quotes, const char mod[], int for_shell);
static char * append_selected_file(FileView *view, char *expanded,
		int dir_name_len, int pos, int quotes, const char *mod, int for_shell);
static char * expand_directory_path(FileView *view, char *expanded, int quotes,
		const char *mod, int for_shell);
static char * expand_register(const char curr_dir[], char expanded[],
		int quotes, const char mod[], int key, int *well_formed, int for_shell);
static char * append_path_to_expanded(char expanded[], int quotes,
		const char path[]);
static char * append_to_expanded(char *expanded, const char* str);
static char * add_missing_macros(char expanded[], size_t len, size_t nmacros,
		custom_macro_t macros[]);

char *
expand_macros(const char *command, const char *args, MacroFlags *flags,
		int for_shell)
{
	/* TODO: refactor this function expand_macros() */

	static const char MACROS_WITH_QUOTING[] = "cCfFbdDr";

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
						command + x + 1, for_shell);
				len = strlen(expanded);
				expanded = realloc(expanded, len + 1 + 1);
				strcat(expanded, " ");
				expanded = append_selected_files(other_view, expanded, 0, quotes,
						command + x + 1, for_shell);
				len = strlen(expanded);
				break;
			case 'c': /* current dir file under the cursor */
				expanded = append_selected_files(curr_view, expanded, 1, quotes,
						command + x + 1, for_shell);
				len = strlen(expanded);
				break;
			case 'C': /* other dir file under the cursor */
				expanded = append_selected_files(other_view, expanded, 1, quotes,
						command + x + 1, for_shell);
				len = strlen(expanded);
				break;
			case 'f': /* current dir selected files */
				expanded = append_selected_files(curr_view, expanded, 0, quotes,
						command + x + 1, for_shell);
				len = strlen(expanded);
				break;
			case 'F': /* other dir selected files */
				expanded = append_selected_files(other_view, expanded, 0, quotes,
						command + x + 1, for_shell);
				len = strlen(expanded);
				break;
			case 'd': /* current directory */
				expanded = expand_directory_path(curr_view, expanded, quotes,
						command + x + 1, for_shell);
				len = strlen(expanded);
				break;
			case 'D': /* other directory */
				expanded = expand_directory_path(other_view, expanded, quotes,
						command + x + 1, for_shell);
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
			case 'r': /* register's content */
				{
					int well_formed;
					expanded = expand_register(curr_view->curr_dir, expanded, quotes,
							command + x + 2, command[x + 1], &well_formed, for_shell);
					len = strlen(expanded);
					if(well_formed)
					{
						x++;
					}
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
		int quotes, const char mod[], int for_shell)
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
					mod, for_shell);

			if(++x != view->selected_files)
			{
				expanded = append_to_expanded(expanded, " ");
			}
		}
	}
	else
	{
		expanded = append_selected_file(view, expanded, dir_name_len,
				view->list_pos, quotes, mod, for_shell);
	}

#ifdef _WIN32
	if(for_shell && stroscmp(cfg.shell, "cmd") == 0)
		to_back_slash(expanded + old_len);
#endif

	return expanded;
}

static char *
append_selected_file(FileView *view, char *expanded, int dir_name_len, int pos,
		int quotes, const char *mod, int for_shell)
{
	char buf[PATH_MAX];
	const char *modified;

	snprintf(buf, sizeof(buf), "%s%s%s",
			(dir_name_len != 0) ? view->curr_dir : "", (dir_name_len != 0) ? "/" : "",
			view->dir_entry[pos].name);
	chosp(buf);

	modified = apply_mods(buf, view->curr_dir, mod, for_shell);
	expanded = append_path_to_expanded(expanded, quotes, modified);

	return expanded;
}

static char *
expand_directory_path(FileView *view, char *expanded, int quotes,
		const char *mod, int for_shell)
{
	const char *const modified = apply_mods(view->curr_dir, "/", mod, for_shell);
	char *const result = append_path_to_expanded(expanded, quotes, modified);

#ifdef _WIN32
	if(for_shell && stroscmp(cfg.shell, "cmd") == 0)
		to_back_slash(result);
#endif

	return result;
}

/* Expands content of a register specified by the key argument considering
 * filename-modifiers.  If key is unknown, fallbacks to the default register.
 * Sets *well_formed to non-zero for valid value of the key.  Reallocates the
 * expanded string and returns result (possibly NULL). */
static char *
expand_register(const char curr_dir[], char expanded[], int quotes,
		const char mod[], int key, int *well_formed, int for_shell)
{
	int i;

	*well_formed = 1;
	registers_t *reg = find_register(tolower(key));
	if(reg == NULL)
	{
		*well_formed = 0;
		reg = find_register(DEFAULT_REG_NAME);
		assert(reg != NULL);
		mod--;
	}

	for(i = 0; i < reg->num_files; i++)
	{
		const char *const modified = apply_mods(reg->files[i], curr_dir, mod,
				for_shell);
		expanded = append_path_to_expanded(expanded, quotes, modified);
		if(i != reg->num_files - 1)
		{
			expanded = append_to_expanded(expanded, " ");
		}
	}

#ifdef _WIN32
	if(for_shell && stroscmp(cfg.shell, "cmd") == 0)
		to_back_slash(expanded);
#endif

	return expanded;
}

/* Appends the path to the expanded string with either proper escaping or
 * quoting.  Returns NULL on not enough memory error. */
static char *
append_path_to_expanded(char expanded[], int quotes, const char path[])
{
	if(quotes)
	{
		const char *const dquoted = enclose_in_dquotes(path);
		expanded = append_to_expanded(expanded, dquoted);
	}
	else
	{
		char *const escaped = escape_filename(path, 0);
		if(escaped == NULL)
		{
			show_error_msg("Memory Error", "Unable to allocate enough memory");
			free(expanded);
			return NULL;
		}

		expanded = append_to_expanded(expanded, escaped);
		free(escaped);
	}

	return expanded;
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

char *
expand_custom_macros(const char pattern[], size_t nmacros,
		custom_macro_t macros[])
{
	char *expanded = strdup("");
	size_t len = 0;
	while(*pattern != '\0')
	{
		if(pattern[0] != '%')
		{
			const char single_char[] = { *pattern, '\0' };
			expanded = extend_string(expanded, single_char, &len);
		}
		else if(pattern[1] == '%' || pattern[1] == '\0')
		{
			expanded = extend_string(expanded, "%", &len);
			pattern += pattern[1] == '%';
		}
		else
		{
			int i = 0;
			pattern++;
			while(i < nmacros && macros[i].letter != *pattern)
			{
				i++;
			}
			if(i < nmacros)
			{
				expanded = extend_string(expanded, macros[i].value, &len);
				if(macros[i].uses_left > 0)
				{
					macros[i].uses_left--;
				}
			}
		}
		pattern++;
	}

	expanded = add_missing_macros(expanded, len, nmacros, macros);

	return expanded;
}

/* Ensures that the expanded string contains required number of mandatory
 * macros.  Returns reallocated expanded. */
static char *
add_missing_macros(char expanded[], size_t len, size_t nmacros,
		custom_macro_t macros[])
{
	int i;
	for(i = 0; i < nmacros; i++)
	{
		custom_macro_t *const macro = &macros[i];
		while(macro->uses_left > 0)
		{
			expanded = extend_string(expanded, " ", &len);
			expanded = extend_string(expanded, macro->value, &len);
			macro->uses_left--;
		}
	}
	return expanded;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
