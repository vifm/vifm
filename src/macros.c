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
#include <string.h> /* memset() strchr() strncat() strcpy() strlen() strcat()
                       strdup() */

#include "cfg/config.h"
#include "modes/dialogs/msg_dialog.h"
#include "ui/ui.h"
#include "utils/fs_limits.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/test_helpers.h"
#include "utils/utils.h"
#include "filelist.h"
#include "filename_modifiers.h"
#include "registers.h"
#include "status.h"

/* Type of path to use during entry path expansion. */
typedef enum
{
	PT_NAME, /* Last path component, i.e. file name. */
	PT_REL,  /* Relative path. */
	PT_FULL, /* Full path. */
}
PathType;

static void set_flags(MacroFlags *flags, MacroFlags value);
TSTATIC char * append_selected_files(FileView *view, char expanded[],
		int under_cursor, int quotes, const char mod[], int for_shell);
static char * append_entry(FileView *view, char expanded[], PathType type,
		dir_entry_t *entry, int quotes, const char mod[], int for_shell);
static char * expand_directory_path(FileView *view, char *expanded, int quotes,
		const char *mod, int for_shell);
static char * expand_register(const char curr_dir[], char expanded[],
		int quotes, const char mod[], int key, int *well_formed, int for_shell);
static char * expand_preview(char expanded[], int key, int *well_formed);
static FileView * get_preview_view(FileView *view);
static char * append_path_to_expanded(char expanded[], int quotes,
		const char path[]);
static char * append_to_expanded(char *expanded, const char* str);
static char * add_missing_macros(char expanded[], size_t len, size_t nmacros,
		custom_macro_t macros[]);

char *
expand_macros(const char command[], const char args[], MacroFlags *flags,
		int for_shell)
{
	/* TODO: refactor this function expand_macros() */

	static const char MACROS_WITH_QUOTING[] = "cCfFbdDr";

	size_t cmd_len;
	char *expanded;
	size_t x;
	size_t y = 0U;
	int len = 0;

	set_flags(flags, MF_NONE);

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
			case 'D': /* Directory of the other view. */
				expanded = expand_directory_path(other_view, expanded, quotes,
						command + x + 1, for_shell);
				len = strlen(expanded);
				break;
			case 'n': /* Forbid using of terminal multiplexer, even if active. */
				set_flags(flags, MF_NO_TERM_MUX);
				break;
			case 'm': /* Use menu. */
				set_flags(flags, MF_MENU_OUTPUT);
				break;
			case 'M': /* Use menu like with :locate and :find. */
				set_flags(flags, MF_MENU_NAV_OUTPUT);
				break;
			case 'S': /* Show command output in the status bar */
				set_flags(flags, MF_STATUSBAR_OUTPUT);
				break;
			case 's': /* Split in new screen region and execute command there. */
				set_flags(flags, MF_SPLIT);
				break;
			case 'u': /* Parse output as list of files and compose custom view. */
				set_flags(flags, MF_CUSTOMVIEW_OUTPUT);
				break;
			case 'i': /* Ignore output. */
				set_flags(flags, MF_IGNORE);
				break;
			case 'r': /* Registers' content. */
				{
					int well_formed;
					expanded = expand_register(flist_get_dir(curr_view), expanded, quotes,
							command + x + 2, command[x + 1], &well_formed, for_shell);
					len = strlen(expanded);
					if(well_formed)
					{
						x++;
					}
				}
				break;
			case 'p': /* preview pane properties */
				{
					int well_formed;
					expanded = expand_preview(expanded, command[x + 1], &well_formed);
					len = strlen(expanded);
					if(well_formed)
					{
						++x;
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

	return expanded;
}

/* Sets *flags to the value, if flags isn't NULL. */
static void
set_flags(MacroFlags *flags, MacroFlags value)
{
	if(flags != NULL)
	{
		*flags = value;
	}
}

TSTATIC char *
append_selected_files(FileView *view, char expanded[], int under_cursor,
		int quotes, const char mod[], int for_shell)
{
	const PathType type = (view == other_view)
	                    ? PT_FULL
	                    : (flist_custom_active(view) ? PT_REL : PT_NAME);
#ifdef _WIN32
	size_t old_len = strlen(expanded);
#endif

	if(view->selected_files && !under_cursor)
	{
		int n = 0;
		dir_entry_t *entry = NULL;
		while(iter_selected_entries(view, &entry))
		{
			expanded = append_entry(view, expanded, type, entry, quotes, mod,
					for_shell);

			if(++n != view->selected_files)
			{
				expanded = append_to_expanded(expanded, " ");
			}
		}
	}
	else
	{
		expanded = append_entry(view, expanded, type, get_current_entry(view),
				quotes, mod, for_shell);
	}

#ifdef _WIN32
	if(for_shell && curr_stats.shell_type == ST_CMD)
	{
		to_back_slash(expanded + old_len);
	}
#endif

	return expanded;
}

/* Appends path to the entry to the expanded string.  Returns new value of
 * expanded string. */
static char *
append_entry(FileView *view, char expanded[], PathType type, dir_entry_t *entry,
		int quotes, const char mod[], int for_shell)
{
	char path[PATH_MAX];
	const char *modified;

	switch(type)
	{
		case PT_NAME:
			copy_str(path, sizeof(path), entry->name);
			break;
		case PT_REL:
			get_short_path_of(view, entry, 0, sizeof(path), path);
			break;
		case PT_FULL:
			get_full_path_of(entry, sizeof(path), path);
			break;

		default:
			assert(0 && "Unexpected path type");
			path[0] = '\0';
			break;
	}

	modified = apply_mods(path, flist_get_dir(view), mod, for_shell);
	expanded = append_path_to_expanded(expanded, quotes, modified);

	return expanded;
}

static char *
expand_directory_path(FileView *view, char *expanded, int quotes,
		const char *mod, int for_shell)
{
	const char *const modified = apply_mods(flist_get_dir(view), "/", mod, for_shell);
	char *const result = append_path_to_expanded(expanded, quotes, modified);

#ifdef _WIN32
	if(for_shell && curr_stats.shell_type == ST_CMD)
	{
		to_back_slash(result);
	}
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
	if(for_shell && curr_stats.shell_type == ST_CMD)
	{
		to_back_slash(expanded);
	}
#endif

	return expanded;
}

/* Expands preview parameter macros specified by the key argument.  If key is
 * unknown, skips the macro.  Sets *well_formed to non-zero for valid value of
 * the key.  Reallocates the expanded string and returns result (possibly
 * NULL). */
static char *
expand_preview(char expanded[], int key, int *well_formed)
{
	FileView *view;
	char num_str[32];
	int h, w, x, y;
	int param;

	if(!char_is_one_of("hwxy", key))
	{
		*well_formed = 0;
		return expanded;
	}

	*well_formed = 1;

	view = get_preview_view(curr_view);

	getbegyx(view->win, y, x);
	getmaxyx(view->win, h, w);

	switch(key)
	{
		case 'h': param = h - 2; break;
		case 'w': param = w - 2; break;
		case 'x': param = x + 1; break;
		case 'y': param = y + 1; break;

		default:
			assert(0 && "Unhandled preview property type");
			param = 0;
			break;
	}

	snprintf(num_str, sizeof(num_str), "%d", param);

	return append_to_expanded(expanded, num_str);
}

/* Applies heuristics to determine which view is going to be used for preview.
 * Returns the view. */
static FileView *
get_preview_view(FileView *view)
{
	FileView *const other = (view == curr_view) ? other_view : curr_view;

	if(curr_stats.preview_hint != NULL)
	{
		return curr_stats.preview_hint;
	}

	if(curr_stats.view || (!view->explore_mode && other->explore_mode))
	{
		return other;
	}
	else
	{
		return view;
	}
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
			size_t i = 0U;
			++pattern;
			while(i < nmacros && macros[i].letter != *pattern)
			{
				++i;
			}
			if(i < nmacros)
			{
				expanded = extend_string(expanded, macros[i].value, &len);
				--macros[i].uses_left;
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
	int groups[nmacros];
	size_t i;

	memset(&groups, 0, sizeof(groups));
	for(i = 0U; i < nmacros; ++i)
	{
		custom_macro_t *const macro = &macros[i];
		const int group = macro->group;
		if(group >= 0)
		{
			groups[group] += macro->uses_left;
		}
	}

	for(i = 0U; i < nmacros; ++i)
	{
		custom_macro_t *const macro = &macros[i];
		const int group = macro->group;
		int *const uses_left = (group >= 0) ? &groups[group] : &macro->uses_left;
		while(*uses_left > 0)
		{
			expanded = extend_string(expanded, " ", &len);
			expanded = extend_string(expanded, macro->value, &len);
			--*uses_left;
		}
	}
	return expanded;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
