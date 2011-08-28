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

#include <fcntl.h>

#include <assert.h>
#include <string.h>

#include "../config.h"
#include "background.h"

#include "filelist.h"
#include "fileops.h"
#include "keys.h"
#include "menus.h"
#include "modes.h"
#include "status.h"
#include "undo.h"
#include "utils.h"

#include "permissions_dialog.h"

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

static void leave_permissions_mode(void);
static void cmd_ctrl_c(struct key_info, struct keys_info *);
static void cmd_ctrl_m(struct key_info, struct keys_info *);
static void set_perm_string(FileView *view, const int *perms,
		const int *origin_perms);
static void files_chmod(FileView *view, const char *mode, int recurse_dirs);
static void chmod_file_in_list(FileView *view, int pos, const char *mode,
		const char *inv_mode, int recurse_dirs);
static void file_chmod(char *path, const char *mode, const char *inv_mode,
		int recurse_dirs);
static void cmd_G(struct key_info, struct keys_info *);
static void cmd_gg(struct key_info, struct keys_info *);
static void cmd_space(struct key_info, struct keys_info *);
static void cmd_j(struct key_info, struct keys_info *);
static void cmd_k(struct key_info, struct keys_info *);
static void inc_curr(void);
static void dec_curr(void);

static struct keys_add_info builtin_cmds[] = {
	{L"\x03", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	/* return */
	{L"\x0d", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	/* escape */
	{L"\x1b", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L" ", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_space}}},
	{L"G", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_G}}},
	{L"gg", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_gg}}},
	{L"h", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_space}}},
	{L"j", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{L"k", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{L"l", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
	{L"q", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"t", {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_space}}},
#ifdef ENABLE_EXTENDED_KEYS
	{{KEY_UP}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_k}}},
	{{KEY_DOWN}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_j}}},
	{{KEY_RIGHT}, {BUILDIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_m}}},
#endif /* ENABLE_EXTENDED_KEYS */
};

void
init_permissions_dialog_mode(int *key_mode)
{
	int ret_code;

	assert(key_mode != NULL);

	mode = key_mode;

	ret_code = add_cmds(builtin_cmds, ARRAY_LEN(builtin_cmds), PERMISSIONS_MODE);
	assert(ret_code == 0);
}

void
enter_permissions_mode(FileView *active_view)
{
#ifndef _WIN32
	int i;
	mode_t fmode;
	mode_t diff;
	uid_t uid = geteuid();

	if(curr_stats.vifm_started < 2)
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

	*mode = PERMISSIONS_MODE;
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
		leave_permissions_mode();
		return;
	}

	col = 9;
	changed = 0;
	redraw_permissions_dialog();
#endif
}

void
redraw_permissions_dialog(void)
{
	char *filename;
	int x, y;

	werase(change_win);

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

	mvwaddstr(change_win, 10, 6, "	[ ] Execute");
	if(perms[6])
		mvwaddch(change_win, 10, 9, (perms[6] < 0) ? 'X' : '*');

	mvwaddstr(change_win, 11, 6, "	[ ] SetGID");
	if(perms[7])
		mvwaddch(change_win, 11, 9, (perms[7] < 0) ? 'X' : '*');

	mvwaddstr(change_win, 13, 2, "Other [ ] Read");
	if(perms[8])
		mvwaddch(change_win, 13, 9, (perms[8] < 0) ? 'X' : '*');

	mvwaddstr(change_win, 14, 6, "	[ ] Write");
	if(perms[9])
		mvwaddch(change_win, 14, 9, (perms[9] < 0) ? 'X' : '*');

	mvwaddstr(change_win, 15, 6, "	[ ] Execute");
	if(perms[10])
		mvwaddch(change_win, 15, 9, (perms[10] < 0) ? 'X' : '*');

	mvwaddstr(change_win, 16, 6, "	[ ] Sticky");
	if(perms[11])
		mvwaddch(change_win, 16, 9, (perms[11] < 0) ? 'X' : '*');

	if(file_is_dir)
	{
		wresize(change_win, 22, 30);
		mvwaddstr(change_win, 18, 6, "	[ ] Set Recursively");
	}
	else
	{
		wresize(change_win, 20, 30);
	}

	getmaxyx(stdscr, y, x);
	mvwin(change_win, (y - (20 + (file_is_dir != 0)*2))/2, (x - 30)/2);
	box(change_win, ACS_VLINE, ACS_HLINE);

	filename = get_current_file_name(view);
	if(strlen(filename) > (size_t)x - 2)
		filename[x - 4] = '\0';
	mvwaddnstr(change_win, 0, (getmaxx(change_win) - strlen(filename))/2,
			filename, x - 2);

	curs_set(1);
	wmove(change_win, curr, col);
	wrefresh(change_win);
}

static void
leave_permissions_mode(void)
{
	*mode = NORMAL_MODE;
	curs_set(0);
	curr_stats.use_input_bar = 1;

	clean_selected_files(view);

	update_all_windows();
}

static void
cmd_ctrl_c(struct key_info key_info, struct keys_info *keys_info)
{
	leave_permissions_mode();
}

static void
cmd_ctrl_m(struct key_info key_info, struct keys_info *keys_info)
{
	char path[PATH_MAX];

	if(!changed)
		return;

	snprintf(path, sizeof(path), "%s/%s", view->curr_dir,
			view->dir_entry[view->list_pos].name);

	set_perm_string(view, perms, origin_perms);
	load_dir_list(view, 1);
	moveto_list_pos(view, view->list_pos);

	leave_permissions_mode();
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
	char perm_string[64] = " ";

	if(adv_perms[0] && adv_perms[1] && adv_perms[2])
	{
		adv_perms[0] = -1;
		adv_perms[1] = -1;
		adv_perms[2] = -1;
	}

	strcat(perm_string, "a-x+X,");

	for(i = 0; i < 12; i++)
	{
		if(perms[i] == -1)
			continue;

		if(perms[i] == origin_perms[i] && !perms[12])
		{
			if((i != 2 && i != 6 && i != 10) || !adv_perms[i/4])
				continue;
		}

		if((i == 2 || i == 6 || i == 10) && adv_perms[i/4] < 0)
			continue;

		if(perms[i])
		{
			if((i == 2 || i == 6 || i == 10) && adv_perms[i/4])
				strcat(perm_string, add_adv_perm[i/4]);
			else
				strcat(perm_string, add_perm[i]);
		}
		else
		{
			strcat(perm_string, sub_perm[i]);
		}

		strcat(perm_string, ",");
	}
	perm_string[strlen(perm_string) - 1] = '\0'; /* Remove last , */

	files_chmod(view, perm_string, perms[12]);
}

static void
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
		snprintf(buf, sizeof(buf), "chmod in %s: ",
				replace_home_part(view->curr_dir));
		len = strlen(buf);

		while(i < view->list_rows && len < COMMAND_GROUP_INFO_LEN)
		{
			if(view->dir_entry[i].selected)
			{
				if(buf[len - 2] != ':')
				{
					strncat(buf, ", ", sizeof(buf));
					buf[sizeof(buf) - 1] = '\0';
				}
				strncat(buf, view->dir_entry[i].name, sizeof(buf));
				buf[sizeof(buf) - 1] = '\0';
				len = strlen(buf);
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
cmd_G(struct key_info key_info, struct keys_info *keys_info)
{
	while(curr < bottom)
	{
		inc_curr();
		permnum++;
	}

	wmove(change_win, curr, col);
	wrefresh(change_win);
}

static void
cmd_gg(struct key_info key_info, struct keys_info *keys_info)
{
	while(curr > 3)
	{
		dec_curr();
		permnum--;
	}

	wmove(change_win, curr, col);
	wrefresh(change_win);
}

static void
cmd_space(struct key_info key_info, struct keys_info *keys_info)
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

	wmove(change_win, curr, col);
	wrefresh(change_win);
}

static void
cmd_j(struct key_info key_info, struct keys_info *keys_info)
{
	inc_curr();
	permnum++;

	if(curr > bottom)
	{
		dec_curr();
		permnum--;
	}

	wmove(change_win, curr, col);
	wrefresh(change_win);
}

static void
cmd_k(struct key_info key_info, struct keys_info *keys_info)
{
	dec_curr();
	permnum--;

	if(curr < top)
	{
		inc_curr();
		permnum++;
	}

	wmove(change_win, curr, col);
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
