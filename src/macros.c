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
#include <ctype.h> /* isdigit() tolower() */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* realloc() free() calloc() */
#include <string.h> /* memset() strncat() strcpy() strlen() strdup() */

#include "cfg/config.h"
#include "compat/fs_limits.h"
#include "modes/dialogs/msg_dialog.h"
#include "ui/colored_line.h"
#include "ui/quickview.h"
#include "ui/ui.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/test_helpers.h"
#include "utils/utf8.h"
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

/* Should return the same character if processing of the macro is allowed or
 * '\0' if it's not allowed. */
typedef char (*macro_filter_func)(int *quoted, char c, char data,
		int ncurr, int nother);

/* File iteration function. */
typedef int (*iter_func)(view_t *view, dir_entry_t **entry);

static char filter_all(int *quoted, char c, char data, int ncurr, int nother);
static char filter_single(int *quoted, char c, char data,
		int ncurr, int nother);
static char * expand_macros_i(const char command[], const char args[],
		MacroFlags *flags, int for_shell, int for_op, macro_filter_func filter);
TSTATIC char * append_selected_files(view_t *view, char expanded[],
		int under_cursor, int quotes, const char mod[], iter_func iter,
		int for_shell);
static char * append_entry(view_t *view, char expanded[], PathType type,
		dir_entry_t *entry, int quotes, const char mod[], int for_shell);
static char * expand_directory_path(view_t *view, char *expanded, int quotes,
		const char *mod, int for_shell);
static char * expand_register(const char curr_dir[], char expanded[],
		int quotes, const char mod[], int key, int *well_formed, int for_shell);
static char * expand_preview(char expanded[], int key, int *well_formed);
static preview_area_t get_preview_area(view_t *view);
static char * append_path_to_expanded(char expanded[], int quotes,
		const char path[]);
static char * append_to_expanded(char expanded[], const char str[]);
static cline_t expand_custom(const char **pattern, size_t nmacros,
		custom_macro_t macros[], int with_opt, int in_opt);
static char * add_missing_macros(char expanded[], size_t len, size_t nmacros,
		custom_macro_t macros[]);

char *
ma_expand(const char command[], const char args[], MacroFlags *flags,
		MacroExpandReason reason)
{
	int for_shell = (reason == MER_SHELL || reason == MER_SHELL_OP);
	int for_op = (reason == MER_OP || reason == MER_SHELL_OP);

	int lpending_marking = lwin.pending_marking;
	int rpending_marking = rwin.pending_marking;

	char *res = expand_macros_i(command, args, flags, for_shell, for_op,
			&filter_all);

	lwin.pending_marking = lpending_marking;
	rwin.pending_marking = rpending_marking;

	return res;
}

/* macro_filter_func instantiation that allows all macros.  Returns the
 * argument. */
static char
filter_all(int *quoted, char c, char data, int ncurr, int nother)
{
	return c;
}

char *
ma_expand_single(const char command[])
{
	int lpending_marking = lwin.pending_marking;
	int rpending_marking = rwin.pending_marking;

	char *const res = expand_macros_i(command, NULL, NULL, /*for_shell=*/0,
			/*for_op=*/0, &filter_single);

	lwin.pending_marking = lpending_marking;
	rwin.pending_marking = rpending_marking;

	unescape(res, 0);
	return res;
}

/* macro_filter_func instantiation that filters out non-single element macros.
 * Returns the argument on allowed macro and '\0' otherwise. */
static char
filter_single(int *quoted, char c, char data, int ncurr, int nother)
{
	if(strchr("cCdD", c) != NULL)
	{
		*quoted = 0;
		return c;
	}

	if(c == 'f' && ncurr <= 1)
	{
		return c;
	}

	if(c == 'F' && nother <= 1)
	{
		return c;
	}

	if(c == 'r')
	{
		reg_t *reg = regs_find(tolower(data));
		if(reg != NULL && reg->nfiles == 1)
		{
			return c;
		}
	}

	return '\0';
}

/* args and flags parameters can equal NULL. The string returned needs to be
 * freed in the calling function. After executing flags is one of MF_*
 * values. */
static char *
expand_macros_i(const char command[], const char args[], MacroFlags *flags,
		int for_shell, int for_op, macro_filter_func filter)
{
	/* TODO: refactor this function expand_macros_i() */
	/* FIXME: repetitive len = strlen(expanded) could be optimized. */

	static const char MACROS_WITH_QUOTING[] = "cCfFlLbdDr";

	size_t cmd_len;
	char *expanded;
	size_t x;
	int len = 0;

	ma_flags_set(flags, MF_NONE);

	cmd_len = strlen(command);

	for(x = 0; x < cmd_len; x++)
		if(command[x] == '%')
			break;

	if(x >= cmd_len)
	{
		return strdup(command);
	}

	iter_func iter;
	int ncurr, nother;

	if(for_op)
	{
		iter = &iter_marked_entries;

		/* Turn selection into marking. */
		flist_set_marking(&lwin, 0);
		flist_set_marking(&rwin, 0);

		ncurr = flist_count_marked(curr_view);
		nother = flist_count_marked(other_view);
	}
	else
	{
		iter = &iter_selection_or_current;
		ncurr = curr_view->selected_files;
		nother = other_view->selected_files;
	}

	if(strstr(command + x, "%r") != NULL)
	{
		regs_sync_from_shared_memory();
	}

	expanded = calloc(cmd_len + 1, sizeof(char));
	strncat(expanded, command, x);
	x++;
	len = strlen(expanded);

	do
	{
		size_t y;
		char *p;

		int quotes = 0;
		if(command[x] == '"' && char_is_one_of(MACROS_WITH_QUOTING, command[x + 1]))
		{
			quotes = 1;
			++x;
		}
		switch(filter(&quotes, command[x],
					command[x] == '\0' ? '\0' : command[x + 1], ncurr, nother))
		{
			int well_formed;
			char key;

			case 'a': /* user arguments */
				if(args != NULL)
				{
					expanded = append_to_expanded(expanded, args);
					len = strlen(expanded);
				}
				break;
			case 'b': /* selected files of both dirs */
				expanded = append_selected_files(curr_view, expanded, 0, quotes,
						command + x + 1, iter, for_shell);
				expanded = append_to_expanded(expanded, " ");
				expanded = append_selected_files(other_view, expanded, 0, quotes,
						command + x + 1, iter, for_shell);
				len = strlen(expanded);
				break;
			case 'c': /* current dir file under the cursor */
				expanded = append_selected_files(curr_view, expanded, 1, quotes,
						command + x + 1, iter, for_shell);
				len = strlen(expanded);
				break;
			case 'C': /* other dir file under the cursor */
				expanded = append_selected_files(other_view, expanded, 1, quotes,
						command + x + 1, iter, for_shell);
				len = strlen(expanded);
				break;
			case 'f': /* current dir selected files */
				expanded = append_selected_files(curr_view, expanded, 0, quotes,
						command + x + 1, iter, for_shell);
				len = strlen(expanded);
				break;
			case 'F': /* other dir selected files */
				expanded = append_selected_files(other_view, expanded, 0, quotes,
						command + x + 1, iter, for_shell);
				len = strlen(expanded);
				break;
			case 'l': /* current dir selected files or nothing if no selection */
				expanded = append_selected_files(curr_view, expanded, 0, quotes,
						command + x + 1, &iter_selected_entries, for_shell);
				len = strlen(expanded);
				break;
			case 'L': /* other dir selected files or nothing if no selection */
				expanded = append_selected_files(other_view, expanded, 0, quotes,
						command + x + 1, &iter_selected_entries, for_shell);
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
				ma_flags_set(flags, MF_NO_TERM_MUX);
				break;
			case 'N': /* Do not run command in a separate terminal session. */
				ma_flags_set(flags, MF_KEEP_SESSION);
				break;
			case 'm': /* Use menu. */
				ma_flags_set(flags, MF_MENU_OUTPUT);
				break;
			case 'M': /* Use menu like with :locate and :find. */
				ma_flags_set(flags, MF_MENU_NAV_OUTPUT);
				break;
			case 'S': /* Show command output in the status bar */
				ma_flags_set(flags, MF_STATUSBAR_OUTPUT);
				break;
			case 'q': /* Show command output in the preview */
				ma_flags_set(flags, MF_PREVIEW_OUTPUT);
				break;
			case 's': /* Execute command in a new horizontal split. */
				ma_flags_set(flags, MF_SPLIT);
				break;
			case 'v': /* Execute command in a new vertical split. */
				ma_flags_set(flags, MF_SPLIT_VERT);
				break;
			case 'u': /* Parse output as list of files and compose custom view. */
				ma_flags_set(flags, MF_CUSTOMVIEW_OUTPUT);
				break;
			case 'U': /* Parse output as list of files and compose unsorted view. */
				ma_flags_set(flags, MF_VERYCUSTOMVIEW_OUTPUT);
				break;
			case 'i': /* Ignore output. */
				ma_flags_set(flags, MF_IGNORE);
				break;
			case 'I': /* Interactive custom views. */
				switch(command[x + 1])
				{
					case 'u':
						++x;
						ma_flags_set(flags, MF_CUSTOMVIEW_IOUTPUT);
						break;
					case 'U':
						++x;
						ma_flags_set(flags, MF_VERYCUSTOMVIEW_IOUTPUT);
						break;
				}
				break;
			case 'r': /* Registers' content. */
				expanded = expand_register(flist_get_dir(curr_view), expanded, quotes,
						command + x + 2, command[x + 1], &well_formed, for_shell);
				len = strlen(expanded);
				if(well_formed)
				{
					++x;
				}
				break;
			case 'p': /* Preview pane properties. */
				key = command[x + 1];
				if(key == 'c')
				{
					return expanded;
				}
				if(key == 'u') /* Do not cache preview result. */
				{
					ma_flags_set(flags, MF_NO_CACHE);
					++x;
					break;
				}
				/* Just skip %pd. */
				if(key == 'd')
				{
					++x;
					break;
				}

				expanded = expand_preview(expanded, key, &well_formed);
				len = strlen(expanded);
				if(well_formed)
				{
					++x;
				}
				break;
			case 'P': /* Pipe file list. */
				switch(command[x + 1])
				{
					case 'l':
						++x;
						ma_flags_set(flags, MF_PIPE_FILE_LIST);
						break;
					case 'z':
						++x;
						ma_flags_set(flags, MF_PIPE_FILE_LIST_Z);
						break;
				}
				break;
			case '%':
				expanded = append_to_expanded(expanded, "%");
				len = strlen(expanded);
				break;

			case '\0':
				if(char_is_one_of("pr", command[x]) && command[x + 1] != '\0')
				{
					++x;
				}
				break;
		}
		if(command[x] != '\0')
			x++;

		x += mods_length(command + x);

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

		p = realloc(expanded, len + (x - y) + 1);
		if(p == NULL)
		{
			free(expanded);
			return NULL;
		}
		expanded = p;
		strncat(expanded, command + y, x - y);
		len = strlen(expanded);

		++x;
	}
	while(x < cmd_len);

	return expanded;
}

void
ma_flags_set(MacroFlags *flags, MacroFlags flag)
{
	if(flags == NULL)
	{
		return;
	}

	if(flag == MF_NONE)
	{
		*flags = MF_NONE;
	}
	else if(flag < MF_SECOND_SET_)
	{
		*flags = (*flags & ~0x00f) | flag;
	}
	else if(flag < MF_THIRD_SET_)
	{
		*flags = (*flags & ~0x0f0) | flag;
	}
	else
	{
		*flags = (*flags & ~0xf00) | flag;
	}
}

TSTATIC char *
append_selected_files(view_t *view, char expanded[], int under_cursor,
		int quotes, const char mod[], iter_func iter, int for_shell)
{
	const PathType type = (view == other_view)
	                    ? PT_FULL
	                    : (flist_custom_active(view) ? PT_REL : PT_NAME);
#ifdef _WIN32
	size_t old_len = strlen(expanded);
#endif

	if(!under_cursor)
	{
		int first = 1;
		dir_entry_t *entry = NULL;
		while(iter(view, &entry))
		{
			if(!first)
			{
				expanded = append_to_expanded(expanded, " ");
			}

			expanded = append_entry(view, expanded, type, entry, quotes, mod,
					for_shell);
			first = 0;
		}
	}
	else
	{
		dir_entry_t *const curr = get_current_entry(view);
		if(!fentry_is_fake(curr))
		{
			expanded = append_entry(view, expanded, type, curr, quotes, mod,
					for_shell);
		}
	}

	if(for_shell && curr_stats.shell_type == ST_CMD)
	{
		internal_to_system_slashes(expanded + old_len);
	}

	return expanded;
}

/* Appends path to the entry to the expanded string.  Returns new value of
 * expanded string. */
static char *
append_entry(view_t *view, char expanded[], PathType type, dir_entry_t *entry,
		int quotes, const char mod[], int for_shell)
{
	char path[PATH_MAX + 1];
	const char *modified;

	switch(type)
	{
		case PT_NAME:
			copy_str(path, sizeof(path), entry->name);
			break;
		case PT_REL:
			get_short_path_of(view, entry, NF_NONE, 0, sizeof(path), path);
			break;
		case PT_FULL:
			get_full_path_of(entry, sizeof(path), path);
			break;

		default:
			assert(0 && "Unexpected path type");
			path[0] = '\0';
			break;
	}

	modified = mods_apply(path, flist_get_dir(view), mod, for_shell);
	expanded = append_path_to_expanded(expanded, quotes, modified);

	return expanded;
}

static char *
expand_directory_path(view_t *view, char *expanded, int quotes, const char *mod,
		int for_shell)
{
	const char *modified = mods_apply(flist_get_dir(view), "/", mod, for_shell);
	char *const result = append_path_to_expanded(expanded, quotes, modified);

	if(for_shell && curr_stats.shell_type == ST_CMD)
	{
		internal_to_system_slashes(result);
	}

	return result;
}

/* Expands content of a register specified by the key argument considering
 * filename-modifiers.  If key is unknown, falls back to the default register.
 * Sets *well_formed to non-zero for valid value of the key.  Reallocates the
 * expanded string and returns result (possibly NULL). */
static char *
expand_register(const char curr_dir[], char expanded[], int quotes,
		const char mod[], int key, int *well_formed, int for_shell)
{
	int i;
	reg_t *reg;

	*well_formed = 1;
	reg = regs_find(tolower(key));
	if(reg == NULL)
	{
		*well_formed = 0;
		reg = regs_find(DEFAULT_REG_NAME);
		assert(reg != NULL);
		mod--;
	}

	for(i = 0; i < reg->nfiles; ++i)
	{
		const char *const modified = mods_apply(reg->files[i], curr_dir, mod,
				for_shell);
		expanded = append_path_to_expanded(expanded, quotes, modified);
		if(expanded == NULL)
		{
			return NULL;
		}

		if(i != reg->nfiles - 1)
		{
			expanded = append_to_expanded(expanded, " ");
		}
	}

	if(for_shell && curr_stats.shell_type == ST_CMD)
	{
		internal_to_system_slashes(expanded);
	}

	return expanded;
}

/* Expands preview parameter macros specified by the key argument.  If key is
 * unknown, skips the macro.  Sets *well_formed to non-zero for valid value of
 * the key.  Reallocates the expanded string and returns result (possibly
 * NULL). */
static char *
expand_preview(char expanded[], int key, int *well_formed)
{
	*well_formed = char_is_one_of("hwxy", key);
	if(!*well_formed)
	{
		*well_formed = 0;
		return expanded;
	}

	const preview_area_t parea = get_preview_area(curr_view);

	int param;
	switch(key)
	{
		case 'h': param = parea.h; break;
		case 'w': param = parea.w; break;
		case 'x': param = getbegx(parea.view->win) + parea.x; break;
		case 'y': param = getbegy(parea.view->win) + parea.y; break;

		default:
			assert(0 && "Unhandled preview property type");
			param = 0;
			break;
	}

	char num_str[32];
	snprintf(num_str, sizeof(num_str), "%d", param);

	return append_to_expanded(expanded, num_str);
}

/* Applies heuristics to determine area that is going to be used for preview.
 * Returns the area. */
static preview_area_t
get_preview_area(view_t *view)
{
	view_t *const other = (view == curr_view) ? other_view : curr_view;

	if(curr_stats.preview_hint != NULL)
	{
		return *(const preview_area_t *)curr_stats.preview_hint;
	}

	view_t *source = view;
	if(curr_stats.preview.on || (!view->explore_mode && other->explore_mode))
	{
		view = other;
	}

	const preview_area_t parea = {
		.source = source,
		.view = view,
		.x = ui_qv_left(view),
		.y = ui_qv_top(view),
		.w = ui_qv_width(view),
		.h = ui_qv_height(view),
	};
	return parea;
}

/* Appends the path to the expanded string with either proper escaping or
 * quoting.  Returns NULL on not enough memory error. */
static char *
append_path_to_expanded(char expanded[], int quotes, const char path[])
{
	if(quotes)
	{
		const char *const dquoted = enclose_in_dquotes(path, curr_stats.shell_type);
		expanded = append_to_expanded(expanded, dquoted);
	}
	else
	{
		char *const escaped = shell_like_escape(path, 0);
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

/* Appends str to expanded with reallocation.  Returns address of the new
 * string or NULL on reallocation error. */
static char *
append_to_expanded(char expanded[], const char str[])
{
	char *t;
	const size_t len = strlen(expanded);

	t = realloc(expanded, len + strlen(str) + 1);
	if(t == NULL)
	{
		free(expanded);
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return NULL;
	}
	strcpy(t + len, str);
	return t;
}

const char *
ma_get_clear_cmd(const char cmd[])
{
	const char *clear_cmd;
	const char *const break_point = strstr(cmd, "%pc");
	if(break_point == NULL)
	{
		return NULL;
	}

	clear_cmd = skip_whitespace(break_point + 3);
	return is_null_or_empty(clear_cmd) ? NULL : clear_cmd;
}

char *
ma_expand_custom(const char pattern[], size_t nmacros, custom_macro_t macros[],
		int with_opt)
{
	cline_t result = ma_expand_colored_custom(pattern, nmacros, macros, with_opt);
	free(result.attrs);
	return result.line;
}

cline_t
ma_expand_colored_custom(const char pattern[], size_t nmacros,
		custom_macro_t macros[], int with_opt)
{
	cline_t result = expand_custom(&pattern, nmacros, macros, with_opt, 0);
	assert(strlen(result.attrs) == utf8_strsw(result.line) && "Broken attrs!");
	return result;
}

/* Expands macros of form %x in the pattern (%% is expanded to %) according to
 * macros specification and accounting for nesting inside %[ and %].  Updates
 * *pattern pointer.  Returns colored line. */
static cline_t
expand_custom(const char **pattern, size_t nmacros, custom_macro_t macros[],
		int with_opt, int in_opt)
{
	cline_t result = cline_make();

	int nexpansions = 0;
	while(**pattern != '\0')
	{
		const char *pat = (*pattern)++;
		if(pat[0] != '%')
		{
			const char single_char[] = { *pat, '\0' };
			result.line = extend_string(result.line, single_char, &result.line_len);
		}
		else if(pat[1] == '%' || pat[1] == '\0')
		{
			result.line = extend_string(result.line, "%", &result.line_len);
			*pattern += (pat[1] == '%');
		}
		else if(pat[1] == '*')
		{
			cline_set_attr(&result, '0');
			++*pattern;
		}
		else if(isdigit(pat[1]) && pat[2] == '*')
		{
			cline_set_attr(&result, pat[1]);
			*pattern += 2;
		}
		else if(with_opt && pat[1] == '[')
		{
			++*pattern;
			cline_t opt = expand_custom(pattern, nmacros, macros, with_opt, 1);
			cline_splice_attrs(&result, &opt);
			result.line = extend_string(result.line, opt.line, &result.line_len);
			nexpansions += (opt.line[0] != '\0');
			free(opt.line);
			continue;
		}
		else if(in_opt && pat[1] == ']')
		{
			++*pattern;
			if(nexpansions == 0 && result.line != NULL)
			{
				cline_clear(&result);
			}
			return result;
		}
		else
		{
			size_t i = 0U;
			while(i < nmacros && macros[i].letter != **pattern)
			{
				++i;
			}
			++*pattern;
			if(i < nmacros)
			{
				const char *value = macros[i].value;
				if(macros[i].expand_mods)
				{
					assert(is_path_absolute(macros[i].parent) &&
							"Invalid parent for mods.");
					value = mods_apply(value, macros[i].parent, *pattern, 0);

					*pattern += mods_length(*pattern);
				}

				if(!macros[i].flag)
				{
					result.line = extend_string(result.line, value, &result.line_len);
				}
				--macros[i].uses_left;
				macros[i].explicit_use = 1;
				nexpansions += (value[0] != '\0');
			}
		}
	}

	if(in_opt)
	{
		/* Unmatched %[. */
		(void)strprepend(&result.line, &result.line_len, "%[");
		(void)strprepend(&result.attrs, &result.attrs_len, "  ");
	}
	else
	{
		result.line = add_missing_macros(result.line, result.line_len, nmacros,
				macros);
		cline_finish(&result);
	}

	return result;
}

/* Ensures that the expanded string contains required number of mandatory
 * macros.  Returns reallocated expanded. */
static char *
add_missing_macros(char expanded[], size_t len, size_t nmacros,
		custom_macro_t macros[])
{
	int groups[nmacros == 0 ? 1 : nmacros];
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
			/* Make sure we don't add spaces for nothing. */
			if(macro->value[0] != '\0')
			{
				expanded = extend_string(expanded, " ", &len);
				expanded = extend_string(expanded, macro->value, &len);
			}
			--*uses_left;
		}
	}
	return expanded;
}

int
ma_flags_present(MacroFlags flags, MacroFlags flag)
{
	if(flag == MF_NONE)
	{
		return (flags == MF_NONE);
	}
	else if(flag < MF_SECOND_SET_)
	{
		return ((flags & 0x00f) == flag);
	}
	else if(flag < MF_THIRD_SET_)
	{
		return ((flags & 0x0f0) == flag);
	}
	else
	{
		return ((flags & 0xf00) == flag);
	}
}

int
ma_flags_missing(MacroFlags flags, MacroFlags flag)
{
	return !ma_flags_present(flags, flag);
}

const char *
ma_flags_to_str(MacroFlags flags)
{
	switch(flags)
	{
		case MF_FIRST_SET_:
		case MF_SECOND_SET_:
		case MF_THIRD_SET_:
		case MF_NONE: return "";

		case MF_MENU_OUTPUT: return "%m";
		case MF_MENU_NAV_OUTPUT: return "%M";
		case MF_STATUSBAR_OUTPUT: return "%S";
		case MF_PREVIEW_OUTPUT: return "%q";
		case MF_CUSTOMVIEW_OUTPUT: return "%u";
		case MF_VERYCUSTOMVIEW_OUTPUT: return "%U";
		case MF_CUSTOMVIEW_IOUTPUT: return "%Iu";
		case MF_VERYCUSTOMVIEW_IOUTPUT: return "%IU";

		case MF_SPLIT: return "%s";
		case MF_SPLIT_VERT: return "%v";
		case MF_IGNORE: return "%i";
		case MF_NO_TERM_MUX: return "%n";

		case MF_KEEP_SESSION: return "%N";

		case MF_PIPE_FILE_LIST: return "%Pl";
		case MF_PIPE_FILE_LIST_Z: return "%Pz";

		case MF_NO_CACHE: return "%pu";
	}

	assert(0 && "Unhandled MacroFlags item.");
	return "";
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
