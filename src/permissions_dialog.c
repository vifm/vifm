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

#include "background.h"
#include "filelist.h"
#include "fileops.h"
#include "keys.h"
#include "modes.h"
#include "status.h"
#include "utils.h"

#include "permissions_dialog.h"

static int *mode;
static FileView *view;
static int top;
static int bottom;
static int curr;
static int permnum;
static int step;
static int col;
static int changed;
static int perms[] = {0,0,0,0,0,0,0,0,0,0,0,0,0};

static void init_extended_keys(void);
static void leave_permissions_mode(void);
static void keys_ctrl_c(struct key_info, struct keys_info *);
static void keys_ctrl_m(struct key_info, struct keys_info *);
static void set_perm_string(FileView *view, int *perms, char *file);
static void file_chmod(FileView *view, char *path, char *mode,
		int recurse_dirs);
static void keys_space(struct key_info, struct keys_info *);
static void keys_j(struct key_info, struct keys_info *);
static void keys_k(struct key_info, struct keys_info *);

void
init_permissions_dialog_mode(int *key_mode)
{
	struct key_t *curr;

	assert(key_mode != NULL);

	mode = key_mode;

	curr = add_keys(L"\x03", PERMISSIONS_MODE);
	curr->data.handler = keys_ctrl_c;

	/* return */
	curr = add_keys(L"\x0d", PERMISSIONS_MODE);
	curr->data.handler = keys_ctrl_m;

	/* escape */
	curr = add_keys(L"\x1b", PERMISSIONS_MODE);
	curr->data.handler = keys_ctrl_c;

	curr = add_keys(L" ", PERMISSIONS_MODE);
	curr->data.handler = keys_space;

	curr = add_keys(L"j", PERMISSIONS_MODE);
	curr->data.handler = keys_j;

	curr = add_keys(L"k", PERMISSIONS_MODE);
	curr->data.handler = keys_k;

	curr = add_keys(L"l", PERMISSIONS_MODE);
	curr->data.handler = keys_ctrl_m;

	curr = add_keys(L"t", PERMISSIONS_MODE);
	curr->data.handler = keys_space;

	init_extended_keys();
}

static void
init_extended_keys(void)
{
	struct key_t *curr;
	wchar_t buf[] = {L'\0', L'\0'};

	buf[0] = KEY_UP;
	curr = add_keys(buf, PERMISSIONS_MODE);
	curr->data.handler = keys_k;

	buf[0] = KEY_DOWN;
	curr = add_keys(buf, PERMISSIONS_MODE);
	curr->data.handler = keys_j;
}

void
enter_permissions_mode(FileView *active_view)
{
	int isdir;
	int x, y;
	char *filename;
	mode_t fmode;

	view = active_view;
	*mode = PERMISSIONS_MODE;
	curr_stats.use_input_bar = 0;
	memset(perms, 0, sizeof(perms));

	getmaxyx(stdscr, y, x);

	wclear(change_win);

	fmode = view->dir_entry[view->list_pos].mode;

	filename = get_current_file_name(view);
	if(strlen(filename) > x - 2)
		filename[x - 4] = '\0';

	mvwaddnstr(change_win, 1, (x - strlen(filename))/2, filename, x - 2);

	mvwaddstr(change_win, 3, 2, "Owner [ ] Read");
	if(fmode & S_IRUSR)
	{
		perms[0] = 1;
		mvwaddch(change_win, 3, 9, '*');
	}
	mvwaddstr(change_win, 4, 6, "  [ ] Write");

	if(fmode & S_IWUSR)
	{
		perms[1] = 1;
		mvwaddch(change_win, 4, 9, '*');
	}
	mvwaddstr(change_win, 5, 6, "  [ ] Execute");

	if(fmode & S_IXUSR)
	{
		perms[2] = 1;
		mvwaddch(change_win, 5, 9, '*');
	}

	mvwaddstr(change_win, 6, 6, "  [ ] SetUID");
	if(fmode & S_ISUID)
	{
		perms[3] = 1;
		mvwaddch(change_win, 6, 9, '*');
	}

	mvwaddstr(change_win, 8, 2, "Group [ ] Read");
	if(fmode & S_IRGRP)
	{
		perms[4] = 1;
		mvwaddch(change_win, 8, 9, '*');
	}

	mvwaddstr(change_win, 9, 6, "  [ ] Write");
	if(fmode & S_IWGRP)
	{
		perms[5] = 1;
		mvwaddch(change_win, 9, 9, '*');
	}

	mvwaddstr(change_win, 10, 6, "	[ ] Execute");
	if(fmode & S_IXGRP)
	{
		perms[6] = 1;
		mvwaddch(change_win, 10, 9, '*');
	}

	mvwaddstr(change_win, 11, 6, "	[ ] SetGID");
	if(fmode & S_ISGID)
	{
		perms[7] = 1;
		mvwaddch(change_win, 11, 9, '*');
	}

	mvwaddstr(change_win, 13, 2, "Other [ ] Read");
	if(fmode & S_IROTH)
	{
		perms[8] = 1;
		mvwaddch(change_win, 13, 9, '*');
	}

	mvwaddstr(change_win, 14, 6, "	[ ] Write");
	if(fmode & S_IWOTH)
	{
		perms[9] = 1;
		mvwaddch(change_win, 14, 9, '*');
	}

	mvwaddstr(change_win, 15, 6, "	[ ] Execute");
	if(fmode & S_IXOTH)
	{
		perms[10] = 1;
		mvwaddch(change_win, 15, 9, '*');
	}

	mvwaddstr(change_win, 16, 6, "	[ ] Sticky");
	if(fmode & S_ISVTX)
	{
		perms[11] = 1;
		mvwaddch(change_win, 16, 9, '*');
	}

	if(is_dir(filename))
	{
		mvwaddstr(change_win, 18, 6, "	[ ] Set Recursively");
		isdir = 1;
	}

	top = 3;
	bottom = isdir ? 17 : 16;
	curr = 3;
	permnum = 0;
	step = 1;
	col = 9;
	changed = 0;

	mvwin(change_win, (y - 20)/2, (x - 30)/2);
	box(change_win, ACS_VLINE, ACS_HLINE);
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
}

static void
keys_ctrl_c(struct key_info key_info, struct keys_info *keys_info)
{
	leave_permissions_mode();
}

static void
keys_ctrl_m(struct key_info key_info, struct keys_info *keys_info)
{
	char path[PATH_MAX];

	leave_permissions_mode();

	if(!changed)
	{
		return;
	}

	snprintf(path, sizeof(path), "%s/%s", view->curr_dir,
			view->dir_entry[view->list_pos].name);

	set_perm_string(view, perms, path);
	load_dir_list(view, 1);
	moveto_list_pos(view, view->curr_line);
}

static void
set_perm_string(FileView *view, int *perms, char *file)
{
	int i = 0;
	char *add_perm[] = {"u+r", "u+w", "u+x", "u+s", "g+r", "g+w", "g+x", "g+s",
											"o+r", "o+w", "o+x", "o+t"};
	char *sub_perm[] = { "u-r", "u-w", "u-x", "u-s", "g-r", "g-w", "g-x", "g-s",
											"o-r", "o-w", "o-x", "o-t"};
	char perm_string[64] = " ";

	for(i = 0; i < 12; i++)
	{
		if(perms[i])
			strcat(perm_string, add_perm[i]);
		else
			strcat(perm_string, sub_perm[i]);

		strcat(perm_string, ",");
	}
	perm_string[strlen(perm_string) - 1] = '\0'; /* Remove last , */

	file_chmod(view, file, perm_string, perms[12]);
}

static void
file_chmod(FileView *view, char *path, char *mode, int recurse_dirs)
{
  char cmd[PATH_MAX + 128] = " ";
  char *filename = escape_filename(path, strlen(path), 1);

	if(recurse_dirs)
	{
		snprintf(cmd, sizeof(cmd), "chmod -R %s %s", mode, filename);
		start_background_job(cmd);
	}
	else
	{
		snprintf(cmd, sizeof(cmd), "chmod %s %s", mode, filename);
		system_and_wait_for_errors(cmd);
	}

	free(filename);
}

static void
keys_space(struct key_info key_info, struct keys_info *keys_info)
{
	changed = 1;
	if(perms[permnum])
	{
		perms[permnum] = 0;
		mvwaddch(change_win, curr, col, ' ');
	}
	else
	{
		perms[permnum] = 1;
		mvwaddch(change_win, curr, col, '*');
	}

	wmove(change_win, curr, col);
	wrefresh(change_win);
}

static void
keys_j(struct key_info key_info, struct keys_info *keys_info)
{
	curr += step;
	permnum++;

	if(curr > bottom)
	{
		curr-= step;
		permnum--;
	}
	if(curr == 7 || curr == 12 || curr == 17)
		curr++;

	wmove(change_win, curr, col);
	wrefresh(change_win);
}

static void
keys_k(struct key_info key_info, struct keys_info *keys_info)
{
	curr -= step;
	permnum--;
	if(curr < top)
	{
		curr+= step;
		permnum++;
	}

	if(curr == 7 || curr == 12 || curr == 17)
		curr--;

	wmove(change_win, curr, col);
	wrefresh(change_win);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
