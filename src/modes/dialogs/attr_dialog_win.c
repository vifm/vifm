/* vifm
 * Copyright (C) 2012 xaizek.
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

#include "attr_dialog_win.h"

#include <windows.h>

#include <assert.h>
#include <stddef.h> /* NULL size_t */
#include <string.h> /* strncat() strlen() */

#include "../../compat/curses.h"
#include "../../compat/fs_limits.h"
#include "../../engine/keys.h"
#include "../../engine/mode.h"
#include "../../ui/cancellation.h"
#include "../../ui/fileview.h"
#include "../../ui/ui.h"
#include "../../utils/fs.h"
#include "../../utils/log.h"
#include "../../utils/macros.h"
#include "../../utils/path.h"
#include "../../utils/str.h"
#include "../../utils/utf8.h"
#include "../../utils/utils.h"
#include "../../filelist.h"
#include "../../flist_sel.h"
#include "../../status.h"
#include "../../undo.h"
#include "../modes.h"
#include "../wk.h"
#include "msg_dialog.h"

/* Enumeration of properties. */
enum
{
	ATTR_ARCHIVE,
	ATTR_HIDDEN,
	ATTR_NOT_INDEX,
	ATTR_READONLY,
	ATTR_SYSTEM,
	ATTR_COUNT,
	PSEUDO_ATTR_RECURSIVE = ATTR_COUNT,
	ALL_ATTRS,
};

static void init(void);
static void get_attrs(void);
static char * get_title(int max_width);
static int is_one_file_selected(int first_file_index);
static int get_first_file_index(void);
static int get_selection_size(int first_file_index);
static void leave_attr_mode(void);
static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_return(key_info_t key_info, keys_info_t *keys_info);
static void set_attrs(view_t *view, const int *attrs, const int *origin_attrs);
static void files_attrib(view_t *view, DWORD add, DWORD sub, int recurse_dirs);
static void file_attrib(char *path, DWORD add, DWORD sub, int recurse_dirs);
static void cmd_G(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gg(key_info_t key_info, keys_info_t *keys_info);
static void cmd_space(key_info_t key_info, keys_info_t *keys_info);
static void cmd_j(key_info_t key_info, keys_info_t *keys_info);
static void cmd_k(key_info_t key_info, keys_info_t *keys_info);
static void inc_curr(void);
static void dec_curr(void);
static void clear_curr(void);
static void draw_curr(void);

/* properties dialog width */
static const int WIDTH = 29;

static view_t *view;
static int top, bottom;
static int curr;
static int attr_num;
static int step;
static int col;
static int changed;
static int file_is_dir;
static int attrs[ALL_ATTRS];
static int origin_attrs[ALL_ATTRS];

static const DWORD attr_list[ATTR_COUNT] = {
	[ATTR_ARCHIVE] = FILE_ATTRIBUTE_ARCHIVE,
	[ATTR_HIDDEN] = FILE_ATTRIBUTE_HIDDEN,
	[ATTR_NOT_INDEX] = FILE_ATTRIBUTE_NOT_CONTENT_INDEXED,
	[ATTR_READONLY] = FILE_ATTRIBUTE_READONLY,
	[ATTR_SYSTEM] = FILE_ATTRIBUTE_SYSTEM,
};
ARRAY_GUARD(attr_list, ATTR_COUNT);

static const char *attr_strings[ATTR_COUNT] = {
	[ATTR_ARCHIVE] = "Archive",
	[ATTR_HIDDEN] = "Hidden",
	[ATTR_NOT_INDEX] = "No content indexing",
	[ATTR_READONLY] = "Readonly",
	[ATTR_SYSTEM] = "System",
};
ARRAY_GUARD(attr_strings, ATTR_COUNT);

static keys_add_info_t builtin_cmds[] = {
	{WK_C_c,    {{&cmd_ctrl_c}, .descr = "close the dialog"}},
	{WK_CR,     {{&cmd_return}, .descr = "update permissions"}},
	{WK_C_n,    {{&cmd_j},      .descr = "go to item below"}},
	{WK_C_p,    {{&cmd_k},      .descr = "go to item above"}},
	{WK_ESC,    {{&cmd_ctrl_c}, .descr = "close the dialog"}},
	{WK_SPACE,  {{&cmd_space},  .descr = "toggle current item"}},
	{WK_G,      {{&cmd_G},      .descr = "go to the last item"}},
	{WK_Z WK_Q, {{&cmd_ctrl_c}, .descr = "close the dialog"}},
	{WK_Z WK_Z, {{&cmd_ctrl_c}, .descr = "close the dialog"}},
	{WK_g WK_g, {{&cmd_gg},     .descr = "go to the first item"}},
	{WK_h,      {{&cmd_space},  .descr = "toggle current item"}},
	{WK_j,      {{&cmd_j},      .descr = "go to item below"}},
	{WK_k,      {{&cmd_k},      .descr = "go to item above"}},
	{WK_l,      {{&cmd_return}, .descr = "update permissions"}},
	{WK_q,      {{&cmd_ctrl_c}, .descr = "close the dialog"}},
	{WK_t,      {{&cmd_space},  .descr = "toggle current item"}},
#ifdef ENABLE_EXTENDED_KEYS
	{{K(KEY_HOME)},  {{&cmd_gg},     .descr = "go to the first item"}},
	{{K(KEY_END)},   {{&cmd_G},      .descr = "go to the last item"}},
	{{K(KEY_UP)},    {{&cmd_k},      .descr = "go to item above"}},
	{{K(KEY_DOWN)},  {{&cmd_j},      .descr = "go to item below"}},
	{{K(KEY_RIGHT)}, {{&cmd_return}, .descr = "update permissions"}},
#endif /* ENABLE_EXTENDED_KEYS */
};

/* initializes properties change mode */
void
init_attr_dialog_mode(void)
{
	int ret_code;

	ret_code = vle_keys_add(builtin_cmds, ARRAY_LEN(builtin_cmds), ATTR_MODE);
	assert(ret_code == 0);

	(void)ret_code;
}

/* enters properties change mode */
void
enter_attr_mode(view_t *active_view)
{
	flist_set_marking(active_view, 0);

	if(curr_stats.load_stage < 2)
		return;

	view = active_view;
	vle_mode_set(ATTR_MODE, VMT_SECONDARY);
	ui_hide_graphics();
	clear_input_bar();
	curr_stats.use_input_bar = 0;

	init();
	get_attrs();
	redraw_attr_dialog();
}

/* initializes inner state of the unit */
static void
init(void)
{
	top = 2;
	bottom = file_is_dir ? 8 : 6;
	curr = 2;
	attr_num = 0;
	step = 1;

	col = 2;
	changed = 0;
}

/* initializes file attributes array */
static void
get_attrs(void)
{
	int i;
	DWORD attributes;
	DWORD diff = 0;
	dir_entry_t *entry = NULL;
	int first = 1;

	file_is_dir = 0;
	while(iter_marked_entries(view, &entry))
	{
		if(first)
		{
			attributes = entry->attrs;
			first = 0;
		}

		diff |= (entry->attrs ^ attributes);
		file_is_dir |= fentry_is_dir(entry);
	}
	if(first)
	{
		show_error_msg("Attributes", "No files to process");
		return;
	}

	/* FIXME: allow setting attributes recursively. */
	file_is_dir = 0;

	memset(attrs, 0, sizeof(attrs));
	for(i = 0; i < ATTR_COUNT; ++i)
	{
		attrs[i] = !(diff & attr_list[i]) ? (int)(attributes & attr_list[i]) : -1;
	}
	memcpy(origin_attrs, attrs, sizeof(attrs));
}

/* Composes title for the dialog.  Returns pointer to a newly allocated
 * string. */
static char *
get_title(int max_width)
{
	const int first_file_index = get_first_file_index();

	if(!is_one_file_selected(first_file_index))
	{
		return format_str(" %d files ", get_selection_size(first_file_index));
	}

	char *escaped = escape_unreadable(view->dir_entry[first_file_index].name);

	char *ellipsed = right_ellipsis(escaped, max_width - 2, curr_stats.ellipsis);
	free(escaped);

	char *title = format_str(" %s ", ellipsed);
	free(ellipsed);

	return title;
}

/* Checks whether single file is selected.  Returns non-zero if so, otherwise
 * zero is returned. */
static int
is_one_file_selected(int first_file_index)
{
	int i;
	if(!view->dir_entry[first_file_index].selected)
	{
		return 1;
	}
	i = first_file_index + 1;
	while(i < view->list_rows && !view->dir_entry[i].selected)
	{
		i++;
	}
	return i >= view->list_rows;
}

/* Gets index of the first one on which chmod operation applies.  Returns index
 * of the first subjected file. */
static int
get_first_file_index(void)
{
	int i = 0;
	while(i < view->list_rows && !view->dir_entry[i].selected)
	{
		i++;
	}
	return (i == view->list_rows) ? view->list_pos : i;
}

/* Gets number of files, which will be affected by the chmod operation. */
static int
get_selection_size(int first_file_index)
{
	int selection_size = 1;
	int i = first_file_index + 1;
	while(i < view->list_rows)
	{
		if(view->dir_entry[i].selected)
		{
			selection_size++;
		}
		i++;
	}
	return selection_size;
}

void
redraw_attr_dialog(void)
{
	char *title;
	int i;
	int x, y;

	werase(change_win);
	if(file_is_dir)
		wresize(change_win, 11, WIDTH);
	else
		wresize(change_win, 9, WIDTH);

	y = 2;
	x = 2;
	for(i = 0; i < ATTR_COUNT; i++)
	{
		mvwaddstr(change_win, y, x, " [ ] ");
		mvwaddstr(change_win, y, x + 5, attr_strings[i]);
		if(attrs[i])
			mvwaddch(change_win, y, x + 2, (attrs[i] < 0) ? 'X' : '*');

		y += 1;
	}

	if(file_is_dir)
		mvwaddstr(change_win, y + 1, x, " [ ] Set Recursively");

	getmaxyx(stdscr, y, x);
	mvwin(change_win, (y - getmaxy(change_win))/2, (x - getmaxx(change_win))/2);
	box(change_win, 0, 0);

	title = get_title(getmaxx(change_win) - 2);
	mvwaddstr(change_win, 0, (getmaxx(change_win) - utf8_strsw(title))/2, title);
	free(title);

	draw_curr();
}

/* leaves properties change dialog */
static void
leave_attr_mode(void)
{
	vle_mode_set(NORMAL_MODE, VMT_PRIMARY);
	curr_stats.use_input_bar = 1;

	flist_sel_stash(view);
	ui_view_schedule_reload(view);
}

/* leaves properties change dialog */
static void
cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info)
{
	leave_attr_mode();
}

/* leaves properties change dialog after changing file attributes */
static void
cmd_return(key_info_t key_info, keys_info_t *keys_info)
{
	if(changed)
	{
		set_attrs(view, attrs, origin_attrs);
		leave_attr_mode();
	}
}

/* sets file properties according to users input. forms attribute change mask */
static void
set_attrs(view_t *view, const int *attrs, const int *origin_attrs)
{
	int i;
	DWORD add_mask = 0;
	DWORD sub_mask = 0;

	for(i = 0; i < ALL_ATTRS; i++)
	{
		if(attrs[i] == -1 || attrs[i] == origin_attrs[i])
			continue;

		if(attrs[i])
		{
			add_mask |= attr_list[i];
		}
		else
		{
			sub_mask |= attr_list[i];
		}
	}

	files_attrib(view, add_mask, sub_mask, attrs[PSEUDO_ATTR_RECURSIVE]);
}

/* Changes attributes of files in the view. */
static void
files_attrib(view_t *view, DWORD add, DWORD sub, int recurse_dirs)
{
	char undo_msg[COMMAND_GROUP_INFO_LEN];
	dir_entry_t *entry;
	size_t len;

	snprintf(undo_msg, sizeof(undo_msg), "chmod in %s: ",
			replace_home_part(flist_get_dir(view)));
	len = strlen(undo_msg);

	ui_cancellation_push_off();

	entry = NULL;
	while(iter_marked_entries(view, &entry))
	{
		if(len >= 2U && undo_msg[len - 2U] != ':')
		{
			strncat(undo_msg + len, ", ", sizeof(undo_msg) - len - 1);
			len += strlen(undo_msg + len);
		}
		strncat(undo_msg + len, entry->name, sizeof(undo_msg) - len - 1);
		len += strlen(undo_msg + len);
	}

	un_group_open(undo_msg);

	entry = NULL;
	while(iter_marked_entries(view, &entry) && !ui_cancellation_requested())
	{
		char path[PATH_MAX + 1];
		get_full_path_of(entry, sizeof(path), path);
		file_attrib(path, add, sub, recurse_dirs);
	}

	un_group_close();
	ui_cancellation_pop();
}

/* performs properties change with support of undoing */
static void
file_attrib(char *path, DWORD add, DWORD sub, int recurse_dirs)
{
	/* FIXME: set attributes recursively. */

	DWORD attrs = GetFileAttributes(path);
	if(attrs == INVALID_FILE_ATTRIBUTES)
	{
		LOG_WERROR(GetLastError());
		return;
	}
	if(add != 0)
	{
		const size_t wadd = add;
		if(perform_operation(OP_ADDATTR, NULL, (void *)wadd, path, NULL) ==
				OPS_SUCCEEDED)
		{
			un_group_add_op(OP_ADDATTR, (void *)wadd, (void *)(~attrs & wadd), path,
					"");
		}
	}
	if(sub != 0)
	{
		const size_t wsub = sub;
		if(perform_operation(OP_SUBATTR, NULL, (void *)wsub, path, NULL) ==
				OPS_SUCCEEDED)
		{
			un_group_add_op(OP_SUBATTR, (void *)wsub, (void *)(~attrs & wsub), path,
					"");
		}
	}
}

/* goes to the bottom of the properties dialog */
static void
cmd_G(key_info_t key_info, keys_info_t *keys_info)
{
	clear_curr();

	while(curr < bottom)
	{
		inc_curr();
		attr_num++;
	}

	draw_curr();
}

/* goes to the top of the properties dialog */
static void
cmd_gg(key_info_t key_info, keys_info_t *keys_info)
{
	clear_curr();

	while(curr > top)
	{
		dec_curr();
		attr_num--;
	}

	draw_curr();
}

/* changes selected flag's state */
static void
cmd_space(key_info_t key_info, keys_info_t *keys_info)
{
	char c;
	changed = 1;

	if(attrs[attr_num] < 0)
	{
		c = ' ';
		attrs[attr_num] = 0;
	}
	else if(curr == 7)
	{
		attrs[attr_num] = !attrs[attr_num];
		c = attrs[attr_num] ? '*' : ' ';
	}
	else if(origin_attrs[attr_num] < 0)
	{
		if(attrs[attr_num] > 0)
		{
			c = 'X';
			attrs[attr_num] = -1;
		}
		else if(attrs[attr_num] == 0)
		{
			/* attrs[attr_num] < 0 is handled above, so it must be 0 here. */
			c = '*';
			attrs[attr_num] = 1;
		}
	}
	else
	{
		c = attrs[attr_num] ? ' ' : '*';
		attrs[attr_num] = !attrs[attr_num];
	}
	mvwaddch(change_win, curr, 4, c);
	ui_refresh_win(change_win);
}

/* moves cursor down one or more times */
static void
cmd_j(key_info_t key_info, keys_info_t *keys_info)
{
	clear_curr();

	inc_curr();
	attr_num++;

	if(curr > bottom)
	{
		dec_curr();
		attr_num--;
	}

	draw_curr();
}

/* moves cursor up one or more times */
static void
cmd_k(key_info_t key_info, keys_info_t *keys_info)
{
	clear_curr();

	dec_curr();
	attr_num--;

	if(curr < top)
	{
		inc_curr();
		attr_num++;
	}

	draw_curr();
}

/* moves cursor one line up */
static void
inc_curr(void)
{
	curr += step;

	if(curr == 7)
		curr++;
}

/* moves cursor one down up */
static void
dec_curr(void)
{
	curr -= step;

	if(curr == 7)
		curr--;
}

/* clears cursor marker */
static void
clear_curr(void)
{
	mvwaddch(change_win, curr, col, ' ');
}

/* draws cursor marker */
static void
draw_curr(void)
{
	mvwaddch(change_win, curr, col, '>');
	ui_refresh_win(change_win);
	checked_wmove(change_win, curr, col);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
