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
#include "utils/macros.h"
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

/* Details about a macro found by find_next_macro(). */
typedef struct
{
	MacroKind kind;     /* Type of the macro or invalid/none mark. */
	const char *begin;  /* Points at the start of the macro (percent sign). */
	const char *end;    /* Points past the end of the macro. */

	const char *mod;  /* Location of filename modifiers. */
	const reg_t *reg; /* Register to be used by MK_r. */
	int quoted;       /* Whether escaping via quotes should be used during macro
	                     expansion. */
}
macro_info_t;

/* File iteration function. */
typedef int (*iter_func)(view_t *view, dir_entry_t **entry);

static char * expand_macros(const char command[], const char args[],
		MacroFlags *flags, int for_shell, int for_op, int single_only);
static macro_info_t find_next_macro(const char str[]);
static void limit_to_single_only(macro_info_t *info, int ncurr, int nother);
TSTATIC char * append_selected_files(view_t *view, char expanded[],
		int under_cursor, int quotes, const char mod[], iter_func iter,
		int for_shell);
static char * append_entry(view_t *view, char expanded[], PathType type,
		dir_entry_t *entry, int quotes, const char mod[], int for_shell);
static char * expand_directory_path(view_t *view, char *expanded, int quotes,
		const char *mod, int for_shell);
static char * expand_register(const reg_t *reg, const char curr_dir[],
		char expanded[], int quotes, const char mod[], int for_shell);
static char * expand_preview(char expanded[], MacroKind kind);
static preview_area_t get_preview_area(view_t *view);
static char * append_path_to_expanded(char expanded[], int quotes,
		const char path[]);
static char * append_to_expanded(char expanded[], const char str[]);
static char * append_to_expanded_n(char expanded[], const char str[],
		size_t str_len);
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

	char *res =
		expand_macros(command, args, flags, for_shell, for_op, /*single_only=*/0);

	lwin.pending_marking = lpending_marking;
	rwin.pending_marking = rpending_marking;

	return res;
}

char *
ma_expand_single(const char command[])
{
	int lpending_marking = lwin.pending_marking;
	int rpending_marking = rwin.pending_marking;

	char *const res = expand_macros(command, NULL, NULL, /*for_shell=*/0,
			/*for_op=*/0, /*single_only=*/1);

	lwin.pending_marking = lpending_marking;
	rwin.pending_marking = rpending_marking;

	unescape(res, 0);
	return res;
}

/* Performs substitution of macros with their values.  args and flags
 * parameters can be NULL.  The string returned needs to be freed by the
 * caller.  After executing flags is one of MF_* values.  On error NULL is
 * returned. */
static char *
expand_macros(const char command[], const char args[], MacroFlags *flags,
		int for_shell, int for_op, int single_only)
{
	ma_flags_set(flags, MF_NONE);

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

	if(strstr(command, "%r") != NULL)
	{
		regs_sync_from_shared_memory();
	}

	char *expanded = strdup("");
	if(expanded == NULL)
	{
		return NULL;
	}

	const char *tail;
	macro_info_t info;
	for(tail = command; ; tail = info.end)
	{
		info = find_next_macro(tail);

		/* Append prefix until the current macro to result. */
		expanded = append_to_expanded_n(expanded, tail, info.begin - tail);
		if(expanded == NULL)
		{
			return NULL;
		}

		if(info.kind == MK_NONE)
		{
			break;
		}

		if(single_only)
		{
			limit_to_single_only(&info, ncurr, nother);
		}

		switch(info.kind)
		{
			case MK_NONE:           /* Handled above.  Pass-through. */
			case MK_INVALID: break; /* Just skip over it. */
			case MK_PERCENT:
				expanded = append_to_expanded(expanded, "%");
				break;

			case MK_a:
				if(args != NULL)
				{
					expanded = append_to_expanded(expanded, args);
				}
				break;

			case MK_b:
				expanded = append_selected_files(curr_view, expanded, 0, info.quoted,
						info.mod, iter, for_shell);
				expanded = append_to_expanded(expanded, " ");
				expanded = append_selected_files(other_view, expanded, 0, info.quoted,
						info.mod, iter, for_shell);
				break;

			case MK_c:
				expanded = append_selected_files(curr_view, expanded, 1, info.quoted,
						info.mod, iter, for_shell);
				break;
			case MK_C:
				expanded = append_selected_files(other_view, expanded, 1, info.quoted,
						info.mod, iter, for_shell);
				break;

			case MK_f:
				expanded = append_selected_files(curr_view, expanded, 0, info.quoted,
						info.mod, iter, for_shell);
				break;
			case MK_F:
				expanded = append_selected_files(other_view, expanded, 0, info.quoted,
						info.mod, iter, for_shell);
				break;

			case MK_l:
				expanded = append_selected_files(curr_view, expanded, 0, info.quoted,
						info.mod, &iter_selected_entries, for_shell);
				break;
			case MK_L:
				expanded = append_selected_files(other_view, expanded, 0, info.quoted,
						info.mod, &iter_selected_entries, for_shell);
				break;

			case MK_d:
				expanded = expand_directory_path(curr_view, expanded, info.quoted,
						info.mod, for_shell);
				break;
			case MK_D:
				expanded = expand_directory_path(other_view, expanded, info.quoted,
						info.mod, for_shell);
				break;

			case MK_n: ma_flags_set(flags, MF_NO_TERM_MUX); break;
			case MK_N: ma_flags_set(flags, MF_KEEP_IN_FG); break;
			case MK_m: ma_flags_set(flags, MF_MENU_OUTPUT); break;
			case MK_M: ma_flags_set(flags, MF_MENU_NAV_OUTPUT); break;
			case MK_S: ma_flags_set(flags, MF_STATUSBAR_OUTPUT); break;
			case MK_q: ma_flags_set(flags, MF_PREVIEW_OUTPUT); break;
			case MK_s: ma_flags_set(flags, MF_SPLIT); break;
			case MK_v: ma_flags_set(flags, MF_SPLIT_VERT); break;
			case MK_u: ma_flags_set(flags, MF_CUSTOMVIEW_OUTPUT); break;
			case MK_U: ma_flags_set(flags, MF_VERYCUSTOMVIEW_OUTPUT); break;
			case MK_i: ma_flags_set(flags, MF_IGNORE); break;

			case MK_r:
				expanded = expand_register(info.reg, flist_get_dir(curr_view), expanded,
						info.quoted, info.mod, for_shell);
				break;

			case MK_Iu: ma_flags_set(flags, MF_CUSTOMVIEW_IOUTPUT); break;
			case MK_IU: ma_flags_set(flags, MF_VERYCUSTOMVIEW_IOUTPUT); break;

			case MK_Pl: ma_flags_set(flags, MF_PIPE_FILE_LIST); break;
			case MK_Pz: ma_flags_set(flags, MF_PIPE_FILE_LIST_Z); break;

			case MK_pc:
				return expanded;
			case MK_pd:
				/* Just skip %pd. */
				break;
			case MK_pu:
				ma_flags_set(flags, MF_NO_CACHE);
				break;
			case MK_ph:
			case MK_pw:
			case MK_px:
			case MK_py:
				expanded = expand_preview(expanded, info.kind);
				break;
		}
	}

	return expanded;
}

/* Finds first macro in the string.  Returns information about the macro from
 * which absence of a macro or details about it can be derived. */
static macro_info_t
find_next_macro(const char str[])
{
	static const char MACROS_WITH_QUOTING[] = "cCfFlLbdDr";

	macro_info_t info = {};

	const char *p = until_first(str, '%');
	if(*p == '\0')
	{
		info.begin = p;
		info.end = p;
		info.kind = MK_NONE;
		return info;
	}

	info.begin = p++;
	if(*p == '"' && char_is_one_of(MACROS_WITH_QUOTING, *(p + 1)))
	{
		info.quoted = 1;
		++p;
	}

	info.kind = MK_INVALID;
	switch(*p)
	{
		case '%': info.kind = MK_PERCENT; break;

		case 'a': info.kind = MK_a; break;
		case 'b': info.kind = MK_b; break;

		case 'c': info.kind = MK_c; break;
		case 'C': info.kind = MK_C; break;
		case 'f': info.kind = MK_f; break;
		case 'F': info.kind = MK_F; break;
		case 'l': info.kind = MK_l; break;
		case 'L': info.kind = MK_L; break;
		case 'd': info.kind = MK_d; break;
		case 'D': info.kind = MK_D; break;

		case 'n': info.kind = MK_n; break;
		case 'N': info.kind = MK_N; break;
		case 'm': info.kind = MK_m; break;
		case 'M': info.kind = MK_M; break;
		case 'S': info.kind = MK_S; break;
		case 'q': info.kind = MK_q; break;
		case 's': info.kind = MK_s; break;
		case 'v': info.kind = MK_v; break;
		case 'u': info.kind = MK_u; break;
		case 'U': info.kind = MK_U; break;
		case 'i': info.kind = MK_i; break;

		case 'r':
			info.kind = MK_r;
			info.reg = regs_find(tolower(*++p));
			if(info.reg == NULL)
			{
				--p;
				info.reg = regs_find(DEFAULT_REG_NAME);
				assert(info.reg != NULL);
			}
			break;

		case 'I':
			switch(*++p)
			{
				case 'u': info.kind = MK_Iu; break;
				case 'U': info.kind = MK_IU; break;

				default: --p; break;
			}
			break;

		case 'P':
			switch(*++p)
			{
				case 'l': info.kind = MK_Pl; break;
				case 'z': info.kind = MK_Pz; break;

				default: --p; break;
			}
			break;

		case 'p':
			switch(*++p)
			{
				case 'c': info.kind = MK_pc; break;
				case 'd': info.kind = MK_pd; break;
				case 'u': info.kind = MK_pu; break;
				case 'h': info.kind = MK_ph; break;
				case 'w': info.kind = MK_pw; break;
				case 'x': info.kind = MK_px; break;
				case 'y': info.kind = MK_py; break;

				default: --p; break;
			}
			break;
	}

	/* In combination with pointer updates in the switch above, this ensures
	 * skipping at least a single character after percent sign. */
	info.end = (*p != '\0' ? p + 1 : p);

	if(info.kind != MK_INVALID)
	{
		/* XXX: it's weird that filename modifiers are processed for all macros. */
		info.mod = info.end;
		info.end += mods_length(info.end);
	}

	return info;
}

/* Updates information about a macro in place to filter out non-single element
 * macros. */
static void
limit_to_single_only(macro_info_t *info, int ncurr, int nother)
{
	if(ONE_OF(info->kind, MK_c, MK_C, MK_d, MK_D))
	{
		/* Single-entry macro expansion is used only for internal purposes. */
		info->quoted = 0;
		return;
	}

	if(info->kind == MK_f && ncurr <= 1)
	{
		return;
	}
	if(info->kind == MK_F && nother <= 1)
	{
		return;
	}

	if(info->kind == MK_r && info->reg->nfiles <= 1)
	{
		return;
	}

	info->kind = MK_INVALID;
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
		*flags = (*flags & ~0x000f) | flag;
	}
	else if(flag < MF_THIRD_SET_)
	{
		*flags = (*flags & ~0x00f0) | flag;
	}
	else if(flag < MF_FOURTH_SET_)
	{
		*flags = (*flags & ~0x0f00) | flag;
	}
	else
	{
		*flags = (*flags & ~0xf000) | flag;
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
		/* XXX: why update slashes in the whole result instead of only in the just
		 *      added path? */
		internal_to_system_slashes(result);
	}

	return result;
}

/* Expands content of a register specified considering filename-modifiers.
 * Reallocates the expanded string and returns result (possibly NULL). */
static char *
expand_register(const reg_t *reg, const char curr_dir[], char expanded[],
		int quotes, const char mod[], int for_shell)
{
	int i;
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

/* Expands preview parameter macros specified by the key argument.  Reallocates
 * the expanded string and returns result (possibly NULL). */
static char *
expand_preview(char expanded[], MacroKind kind)
{
	const preview_area_t parea = get_preview_area(curr_view);

	int param;
	switch(kind)
	{
		case MK_ph: param = parea.h; break;
		case MK_pw: param = parea.w; break;
		case MK_px: param = getbegx(parea.view->win) + parea.x; break;
		case MK_py: param = getbegy(parea.view->win) + parea.y; break;

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
		char *const escaped = posix_like_escape(path, /*type=*/0);
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
	return append_to_expanded_n(expanded, str, strlen(str));
}

/* Appends at most str_len characters at the beginning of str to expanded with
 * reallocation.  Returns address of the new string or NULL on reallocation
 * error. */
static char *
append_to_expanded_n(char expanded[], const char str[], size_t str_len)
{
	if(str_len == 0)
	{
		return expanded;
	}

	/* XXX: strlen() would be unnecessary if we cached it. */
	const size_t old_len = strlen(expanded);
	char *t = realloc(expanded, old_len + str_len + 1);
	if(t == NULL)
	{
		free(expanded);
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return NULL;
	}
	copy_str(t + old_len, str_len + 1, str);
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

int
ma_contains2(const char cmd[], MacroKind kind1, MacroKind kind2)
{
	macro_info_t info = find_next_macro(cmd);
	for( ; info.kind != MK_NONE; info = find_next_macro(info.end))
	{
		if(ONE_OF(info.kind, kind1, kind2))
		{
			return 1;
		}
	}
	return 0;
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
			cline_set_attr(&result, /*user_color=*/0);
			++*pattern;
		}
		else if(isdigit(pat[1]) && pat[2] == '*')
		{
			int user_color = pat[1] - '0';
			cline_set_attr(&result, user_color);
			*pattern += 2;
		}
		else if(isdigit(pat[1]) && isdigit(pat[2]) && pat[3] == '*')
		{
			int user_color = (pat[1] - '0')*10 + (pat[2] - '0');
			cline_set_attr(&result, user_color);
			*pattern += 3;
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
		return ((flags & 0x000f) == flag);
	}
	else if(flag < MF_THIRD_SET_)
	{
		return ((flags & 0x00f0) == flag);
	}
	else if(flag < MF_FOURTH_SET_)
	{
		return ((flags & 0x0f00) == flag);
	}
	else
	{
		return ((flags & 0xf000) == flag);
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
		case MF_FOURTH_SET_:
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

		case MF_KEEP_IN_FG: return "%N";

		case MF_PIPE_FILE_LIST: return "%Pl";
		case MF_PIPE_FILE_LIST_Z: return "%Pz";

		case MF_NO_CACHE: return "%pu";
	}

	assert(0 && "Unhandled MacroFlags item.");
	return "";
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
