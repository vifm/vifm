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

#include "../../engine/keys.h"
#include "../../utils/fs.h"
#include "../../utils/fs_limits.h"
#include "../../utils/log.h"
#include "../../utils/macros.h"
#include "../../utils/path.h"
#include "../../filelist.h"
#include "../../status.h"
#include "../../undo.h"
#include "../modes.h"

/* enumeration of properties */
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
static const char * get_title(void);
static int is_one_file_selected(int first_file_index);
static int get_first_file_index(void);
static int get_selection_size(int first_file_index);
static void leave_attr_mode(void);
static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info);
static void set_attrs(FileView *view, const int *attrs,
		const int *origin_attrs);
static void files_attrib(FileView *view, DWORD add, DWORD sub,
		int recurse_dirs);
static void attrib_file_in_list(FileView *view, int pos, DWORD add, DWORD sub,
		int recurse_dirs);
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

static int *mode;
static FileView *view;
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
	{L"\x03", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	/* return */
	{L"\x0d", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{L"\x0e", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"\x10", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	/* escape */
	{L"\x1b", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L" ", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_space}}},
	{L"G", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
	{L"ZQ", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"ZZ", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"gg", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
	{L"h", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_space}}},
	{L"j", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"k", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"l", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{L"q", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"t", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_space}}},
#ifdef ENABLE_EXTENDED_KEYS
	{{KEY_HOME}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
	{{KEY_END}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
	{{KEY_UP}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{{KEY_DOWN}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{{KEY_RIGHT}, {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
#endif /* ENABLE_EXTENDED_KEYS */
};

/* initializes properties change mode */
void
init_attr_dialog_mode(int *key_mode)
{
	int ret_code;

	assert(key_mode != NULL);

	mode = key_mode;

	ret_code = add_cmds(builtin_cmds, ARRAY_LEN(builtin_cmds), ATTR_MODE);
	assert(ret_code == 0);
}

/* enters properties change mode */
void
enter_attr_mode(FileView *active_view)
{
	if(curr_stats.load_stage < 2)
		return;

	view = active_view;
	*mode = ATTR_MODE;
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
	DWORD attributes;
	DWORD diff;
	int i;

	memset(attrs, 0, sizeof(attrs));

	attributes = 0;
	diff = 0;
	i = 0;
	while(i < view->list_rows && !view->dir_entry[i].selected)
		i++;
	file_is_dir = 0;
	if(i == view->list_rows)
	{
		i = view->list_pos;
		file_is_dir = is_dir(view->dir_entry[i].name);
	}
	attributes = GetFileAttributes(view->dir_entry[i].name);
	while(i < view->list_rows)
	{
		if(view->dir_entry[i].selected)
		{
			diff |= (GetFileAttributes(view->dir_entry[i].name) ^ attributes);
			file_is_dir = file_is_dir || is_dir(view->dir_entry[i].name);
		}

		i++;
	}
	/* TODO: set attributes recursively */
	file_is_dir = 0;

	for(i = 0; i < ATTR_COUNT; i++)
		attrs[i] = !(diff & attr_list[i]) ? (int)(attributes & attr_list[i]) : -1;
	memcpy(origin_attrs, attrs, sizeof(attrs));
}

/* Gets title of the permissions dialog.  Returns pointer to a temporary string
 * of file name in the view or to a statically allocated string. */
static const char *
get_title(void)
{
	static char title[64];

	const int first_file_index = get_first_file_index();
	if(is_one_file_selected(first_file_index))
	{
		return view->dir_entry[first_file_index].name;
	}

	snprintf(title, sizeof(title), "%d files",
			get_selection_size(first_file_index));
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

/* redraws properties change dialog */
void
redraw_attr_dialog(void)
{
	const char *title;
	int i;
	int x, y;
	size_t title_len;
	int need_ellipsis;

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
	box(change_win, ACS_VLINE, ACS_HLINE);

	x = getmaxx(change_win);
	title = get_title();
	title_len = strlen(title);
	need_ellipsis = (title_len > (size_t)x - 2);

	if(need_ellipsis)
	{
		x -= 3;
		title_len = x;
	}
	mvwaddnstr(change_win, 0, (getmaxx(change_win) - title_len)/2, title, x - 2);
	if(need_ellipsis)
	{
		waddstr(change_win, "...");
	}

	draw_curr();
}

/* leaves properties change dialog */
static void
leave_attr_mode(void)
{
	*mode = NORMAL_MODE;
	curr_stats.use_input_bar = 1;

	clean_selected_files(view);
	load_dir_list(view, 1);
	move_to_list_pos(view, view->list_pos);

	update_all_windows();
}

/* leaves properties change dialog */
static void
cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info)
{
	leave_attr_mode();
}

/* leaves properties change dialog after changing file attributes */
static void
cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info)
{
	char path[PATH_MAX];

	if(!changed)
		return;

	snprintf(path, sizeof(path), "%s/%s", view->curr_dir,
			view->dir_entry[view->list_pos].name);

	set_attrs(view, attrs, origin_attrs);

	leave_attr_mode();
}

/* sets file properties according to users input. forms attribute change mask */
static void
set_attrs(FileView *view, const int *attrs, const int *origin_attrs)
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

/* changes attributes of files in the view */
static void
files_attrib(FileView *view, DWORD add, DWORD sub, int recurse_dirs)
{
	int i;

	i = 0;
	while(i < view->list_rows && !view->dir_entry[i].selected)
		i++;

	if(i == view->list_rows)
	{
		char buf[COMMAND_GROUP_INFO_LEN];
		snprintf(buf, sizeof(buf), "chmod in %s: %s",
				replace_home_part(view->curr_dir),
				view->dir_entry[view->list_pos].name);
		cmd_group_begin(buf);
		attrib_file_in_list(view, view->list_pos, add, sub, recurse_dirs);
	}
	else
	{
		char buf[COMMAND_GROUP_INFO_LEN];
		size_t len;
		int j = i;
		len = snprintf(buf, sizeof(buf), "chmod in %s: ",
				replace_home_part(view->curr_dir));

		while(i < view->list_rows && len < sizeof(buf))
		{
			if(view->dir_entry[i].selected)
			{
				if(len >= 2 && buf[len - 2] != ':')
				{
					strncat(buf + len, ", ", sizeof(buf) - len - 1);
					len += strlen(buf + len);
				}
				strncat(buf + len, view->dir_entry[i].name, sizeof(buf) - len - 1);
				len += strlen(buf + len);
			}
			i++;
		}
		cmd_group_begin(buf);
		while(j < view->list_rows)
		{
			if(view->dir_entry[j].selected)
			{
				attrib_file_in_list(view, j, add, sub, recurse_dirs);
			}
			j++;
		}
	}
	cmd_group_end();
}

/* changes properties of single file */
static void attrib_file_in_list(FileView *view, int pos, DWORD add, DWORD sub,
		int recurse_dirs)
{
	char *filename;
	char path_buf[PATH_MAX];

	filename = view->dir_entry[pos].name;
	snprintf(path_buf, sizeof(path_buf), "%s/%s", view->curr_dir, filename);
	chosp(path_buf);
	file_attrib(path_buf, add, sub, recurse_dirs);
}

/* performs properties change with support of undoing */
static void
file_attrib(char *path, DWORD add, DWORD sub, int recurse_dirs)
{
	/* TODO: set attributes recursively */
	DWORD attrs = GetFileAttributes(path);
	if(attrs == INVALID_FILE_ATTRIBUTES)
	{
		LOG_WERROR(GetLastError());
		return;
	}
	if(add != 0)
	{
		const size_t wadd = add;
		if(perform_operation(OP_ADDATTR, (void *)wadd, path, NULL) == 0)
		{
			add_operation(OP_ADDATTR, (void *)wadd, (void *)(~attrs & wadd), path,
					"");
		}
	}
	if(sub != 0)
	{
		const size_t wsub = sub;
		if(perform_operation(OP_SUBATTR, (void *)wsub, path, NULL) == 0)
		{
			add_operation(OP_SUBATTR, (void *)wsub, (void *)(~attrs & wsub), path,
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
			c = '*';
			attrs[attr_num] = 1;
		}
		else
		{
			c = ' ';
			attrs[attr_num] = 0;
		}
	}
	else
	{
		c = attrs[attr_num] ? ' ' : '*';
		attrs[attr_num] = !attrs[attr_num];
	}
	mvwaddch(change_win, curr, 4, c);
	wrefresh(change_win);
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
	wrefresh(change_win);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
