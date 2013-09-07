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

#include "attr_dialog_nix.h"

#include <fcntl.h>

#include <assert.h> /* assert() */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <string.h> /* strncat() strlen() */

#include "../../engine/keys.h"
#include "../../menus/menus.h"
#include "../../utils/fs.h"
#include "../../utils/fs_limits.h"
#include "../../utils/macros.h"
#include "../../utils/path.h"
#include "../../background.h"
#include "../../filelist.h"
#include "../../fileops.h"
#include "../../status.h"
#include "../../undo.h"
#include "../modes.h"

static const char * get_title(void);
static int is_one_file_selected(int first_file_index);
static int get_first_file_index(void);
static int get_selection_size(int first_file_index);
static void leave_attr_mode(void);
static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info);
static void set_perm_string(FileView *view, const int *perms,
		const int *origin_perms);
static void chmod_file_in_list(FileView *view, int pos, const char *mode,
		const char *inv_mode, int recurse_dirs);
static void file_chmod(char *path, const char *mode, const char *inv_mode,
		int recurse_dirs);
static void cmd_G(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gg(key_info_t key_info, keys_info_t *keys_info);
static void cmd_space(key_info_t key_info, keys_info_t *keys_info);
static void cmd_j(key_info_t key_info, keys_info_t *keys_info);
static void cmd_k(key_info_t key_info, keys_info_t *keys_info);
static void inc_curr(void);
static void dec_curr(void);

static int *mode;
static FileView *view;
static int top, bottom;
static int curr;
static int permnum;
static int step;
static int col;
static int changed;
static int file_is_dir;
static int perms[13] = {0,0,0,0,0,0,0,0,0,0,0,0,0};
static int origin_perms[13];
static int adv_perms[3];

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

void
init_attr_dialog_mode(int *key_mode)
{
	int ret_code;

	assert(key_mode != NULL);

	mode = key_mode;

	ret_code = add_cmds(builtin_cmds, ARRAY_LEN(builtin_cmds), ATTR_MODE);
	assert(ret_code == 0);
}

void
enter_attr_mode(FileView *active_view)
{
	int i;
	mode_t fmode;
	mode_t diff;
	uid_t uid = geteuid();

	if(curr_stats.load_stage < 2)
		return;

	view = active_view;
	memset(perms, 0, sizeof(perms));

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
	fmode = view->dir_entry[i].mode;
	if(uid != 0 && view->dir_entry[i].uid != uid)
	{
		show_error_msgf("Access error", "You are not owner of %s",
				view->dir_entry[i].name);
		clean_selected_files(view);
		load_dir_list(view, 1);
		move_to_list_pos(view, view->list_pos);
		return;
	}
	while(i < view->list_rows)
	{
		if(view->dir_entry[i].selected)
		{
			diff |= (view->dir_entry[i].mode ^ fmode);
			file_is_dir = file_is_dir || is_dir(view->dir_entry[i].name);

			if(uid != 0 && view->dir_entry[i].uid != uid)
			{
				show_error_msgf("Access error", "You are not owner of %s",
						view->dir_entry[i].name);
				return;
			}
		}

		i++;
	}

	*mode = ATTR_MODE;
	clear_input_bar();
	curr_stats.use_input_bar = 0;

	perms[0] = !(diff & S_IRUSR) ? (int)(fmode & S_IRUSR) : -1;
	perms[1] = !(diff & S_IWUSR) ? (int)(fmode & S_IWUSR) : -1;
	perms[2] = !(diff & S_IXUSR) ? (int)(fmode & S_IXUSR) : -1;
	perms[3] = !(diff & S_ISUID) ? (int)(fmode & S_ISUID) : -1;
	perms[4] = !(diff & S_IRGRP) ? (int)(fmode & S_IRGRP) : -1;
	perms[5] = !(diff & S_IWGRP) ? (int)(fmode & S_IWGRP) : -1;
	perms[6] = !(diff & S_IXGRP) ? (int)(fmode & S_IXGRP) : -1;
	perms[7] = !(diff & S_ISGID) ? (int)(fmode & S_ISGID) : -1;
	perms[8] = !(diff & S_IROTH) ? (int)(fmode & S_IROTH) : -1;
	perms[9] = !(diff & S_IWOTH) ? (int)(fmode & S_IWOTH) : -1;
	perms[10] = !(diff & S_IXOTH) ? (int)(fmode & S_IXOTH) : -1;
	perms[11] = !(diff & S_ISVTX) ? (int)(fmode & S_ISVTX) : -1;
	adv_perms[0] = 0;
	adv_perms[1] = 0;
	adv_perms[2] = 0;
	memcpy(origin_perms, perms, sizeof(perms));

	top = 3;
	bottom = file_is_dir ? 18 : 16;
	curr = 3;
	permnum = 0;
	step = 1;
	while(perms[permnum] < 0 && curr <= bottom)
	{
		inc_curr();
		permnum++;
	}

	if(curr > bottom)
	{
		show_error_msg("Permissions change error",
				"Selected files have no common access state");
		leave_attr_mode();
		return;
	}

	col = 9;
	changed = 0;
	redraw_attr_dialog();
}

void
redraw_attr_dialog(void)
{
	const char *title;
	int x, y;
	size_t title_len;
	int need_ellipsis;

	werase(change_win);
	if(file_is_dir)
		wresize(change_win, 22, 30);
	else
		wresize(change_win, 20, 30);

	mvwaddstr(change_win, 3, 2, "Owner [ ] Read");
	if(perms[0])
		mvwaddch(change_win, 3, 9, (perms[0] < 0) ? 'X' : '*');
	mvwaddstr(change_win, 4, 6, "  [ ] Write");

	if(perms[1])
		mvwaddch(change_win, 4, 9, (perms[1] < 0) ? 'X' : '*');
	mvwaddstr(change_win, 5, 6, "  [ ] Execute");

	if(perms[2])
		mvwaddch(change_win, 5, 9, (perms[2] < 0) ? 'X' : '*');

	mvwaddstr(change_win, 6, 6, "  [ ] SetUID");
	if(perms[3])
		mvwaddch(change_win, 6, 9, (perms[3] < 0) ? 'X' : '*');

	mvwaddstr(change_win, 8, 2, "Group [ ] Read");
	if(perms[4])
		mvwaddch(change_win, 8, 9, (perms[4] < 0) ? 'X' : '*');

	mvwaddstr(change_win, 9, 6, "  [ ] Write");
	if(perms[5])
		mvwaddch(change_win, 9, 9, (perms[5] < 0) ? 'X' : '*');

	mvwaddstr(change_win, 10, 6, "  [ ] Execute");
	if(perms[6])
		mvwaddch(change_win, 10, 9, (perms[6] < 0) ? 'X' : '*');

	mvwaddstr(change_win, 11, 6, "  [ ] SetGID");
	if(perms[7])
		mvwaddch(change_win, 11, 9, (perms[7] < 0) ? 'X' : '*');

	mvwaddstr(change_win, 13, 2, "Other [ ] Read");
	if(perms[8])
		mvwaddch(change_win, 13, 9, (perms[8] < 0) ? 'X' : '*');

	mvwaddstr(change_win, 14, 6, "  [ ] Write");
	if(perms[9])
		mvwaddch(change_win, 14, 9, (perms[9] < 0) ? 'X' : '*');

	mvwaddstr(change_win, 15, 6, "  [ ] Execute");
	if(perms[10])
		mvwaddch(change_win, 15, 9, (perms[10] < 0) ? 'X' : '*');

	mvwaddstr(change_win, 16, 6, "  [ ] Sticky");
	if(perms[11])
		mvwaddch(change_win, 16, 9, (perms[11] < 0) ? 'X' : '*');

	if(file_is_dir)
		mvwaddstr(change_win, 18, 6, "  [ ] Set Recursively");

	getmaxyx(stdscr, y, x);
	mvwin(change_win, (y - (20 + (file_is_dir != 0)*2))/2, (x - 30)/2);
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

	curs_set(TRUE);
	checked_wmove(change_win, curr, col);
	wrefresh(change_win);
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

static void
leave_attr_mode(void)
{
	*mode = NORMAL_MODE;
	curs_set(FALSE);
	curr_stats.use_input_bar = 1;

	clean_selected_files(view);
	load_dir_list(view, 1);
	move_to_list_pos(view, view->list_pos);

	update_all_windows();
}

static void
cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info)
{
	leave_attr_mode();
}

static void
cmd_ctrl_m(key_info_t key_info, keys_info_t *keys_info)
{
	char path[PATH_MAX];

	if(!changed)
		return;

	snprintf(path, sizeof(path), "%s/%s", view->curr_dir,
			view->dir_entry[view->list_pos].name);

	set_perm_string(view, perms, origin_perms);

	leave_attr_mode();
}

static void
set_perm_string(FileView *view, const int *perms, const int *origin_perms)
{
	int i = 0;
	char *add_perm[] = {"u+r", "u+w", "u+x", "u+s", "g+r", "g+w", "g+x", "g+s",
											"o+r", "o+w", "o+x", "o+t"};
	char *sub_perm[] = {"u-r", "u-w", "u-x", "u-s", "g-r", "g-w", "g-x", "g-s",
											"o-r", "o-w", "o-x", "o-t"};
	char *add_adv_perm[] = {"u-x+X", "g-x+X", "o-x+X"};
	char perm_str[64] = " ";
	size_t perm_str_len = strlen(perm_str);

	if(adv_perms[0] && adv_perms[1] && adv_perms[2])
	{
		adv_perms[0] = -1;
		adv_perms[1] = -1;
		adv_perms[2] = -1;
	}

	perm_str_len += snprintf(perm_str + perm_str_len,
			sizeof(perm_str) - perm_str_len, "a-x+X,");

	for(i = 0; i < 12; i++)
	{
		const char *perm;

		if(perms[i] == -1)
			continue;

		if(perms[i] == origin_perms[i] && !perms[12])
		{
			if((i != 2 && i != 6 && i != 10) || !adv_perms[i/4])
				continue;
		}

		if((i == 2 || i == 6 || i == 10) && adv_perms[i/4] < 0)
			continue;

		if(!perms[i])
		{
			perm = sub_perm[i];
		}
		else if((i == 2 || i == 6 || i == 10) && adv_perms[i/4])
		{
			perm = add_adv_perm[i/4];
		}
		else
		{
			perm = add_perm[i];
		}

		perm_str_len += snprintf(perm_str + perm_str_len,
				sizeof(perm_str) - perm_str_len, "%s,", perm);
	}
	perm_str[strlen(perm_str) - 1] = '\0'; /* Remove last comma (','). */

	files_chmod(view, perm_str, perms[12]);
}

void
files_chmod(FileView *view, const char *mode, int recurse_dirs)
{
	int i;

	i = 0;
	while(i < view->list_rows && !view->dir_entry[i].selected)
		i++;

	if(i == view->list_rows)
	{
		char buf[COMMAND_GROUP_INFO_LEN];
		char inv[16];
		snprintf(buf, sizeof(buf), "chmod in %s: %s",
				replace_home_part(view->curr_dir),
				view->dir_entry[view->list_pos].name);
		cmd_group_begin(buf);
		snprintf(inv, sizeof(inv), "0%o",
				view->dir_entry[view->list_pos].mode & 0xff);
		chmod_file_in_list(view, view->list_pos, mode, inv, recurse_dirs);
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
				char inv[16];
				snprintf(inv, sizeof(inv), "0%o", view->dir_entry[j].mode & 0xff);
				chmod_file_in_list(view, j, mode, inv, recurse_dirs);
			}
			j++;
		}
	}
	cmd_group_end();
}

static void
chmod_file_in_list(FileView *view, int pos, const char *mode,
		const char *inv_mode, int recurse_dirs)
{
	char *filename;
	char path_buf[PATH_MAX];

	filename = view->dir_entry[pos].name;
	snprintf(path_buf, sizeof(path_buf), "%s/%s", view->curr_dir, filename);
	chosp(path_buf);
	file_chmod(path_buf, mode, inv_mode, recurse_dirs);
}

static void
file_chmod(char *path, const char *mode, const char *inv_mode, int recurse_dirs)
{
	int op = recurse_dirs ? OP_CHMODR : OP_CHMOD;

	if(perform_operation(op, (void *)mode, path, NULL) == 0)
		add_operation(op, strdup(mode), strdup(inv_mode), path, "");
}

static void
cmd_G(key_info_t key_info, keys_info_t *keys_info)
{
	while(curr < bottom)
	{
		inc_curr();
		permnum++;
	}

	checked_wmove(change_win, curr, col);
	wrefresh(change_win);
}

static void
cmd_gg(key_info_t key_info, keys_info_t *keys_info)
{
	while(curr > 3)
	{
		dec_curr();
		permnum--;
	}

	checked_wmove(change_win, curr, col);
	wrefresh(change_win);
}

static void
cmd_space(key_info_t key_info, keys_info_t *keys_info)
{
	char c;
	changed = 1;

	if(perms[permnum] < 0)
	{
		c = ' ';
		perms[permnum] = 0;
	}
	else if(curr == 5 || curr == 10 || curr == 15)
	{
		int i = curr/5 - 1;
		if(!perms[permnum])
		{
			c = '*';
			perms[permnum] = 1;
		}
		else
		{
			if(!adv_perms[i])
			{
				c = 'd';
			}
			else
			{
				if(origin_perms[permnum] < 0)
				{
					c = 'X';
					perms[permnum] = -1;
				}
				else
				{
					c = ' ';
					perms[permnum] = 0;
				}
			}
			adv_perms[i] = !adv_perms[i];
		}
	}
	else if(origin_perms[permnum] < 0)
	{
		if(perms[permnum] > 0)
		{
			c = 'X';
			perms[permnum] = -1;
		}
		else if(perms[permnum] == 0)
		{
			c = '*';
			perms[permnum] = 1;
		}
		else
		{
			c = ' ';
			perms[permnum] = 0;
		}
	}
	else
	{
		c = perms[permnum] ? ' ' : '*';
		perms[permnum] = !perms[permnum];
	}
	mvwaddch(change_win, curr, col, c);

	checked_wmove(change_win, curr, col);
	wrefresh(change_win);
}

static void
cmd_j(key_info_t key_info, keys_info_t *keys_info)
{
	inc_curr();
	permnum++;

	if(curr > bottom)
	{
		dec_curr();
		permnum--;
	}

	checked_wmove(change_win, curr, col);
	wrefresh(change_win);
}

static void
cmd_k(key_info_t key_info, keys_info_t *keys_info)
{
	dec_curr();
	permnum--;

	if(curr < top)
	{
		inc_curr();
		permnum++;
	}

	checked_wmove(change_win, curr, col);
	wrefresh(change_win);
}

static void
inc_curr(void)
{
	curr += step;

	if(curr == 7 || curr == 12 || curr == 17)
		curr++;
}

static void
dec_curr(void)
{
	curr -= step;

	if(curr == 7 || curr == 12 || curr == 17)
		curr--;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
