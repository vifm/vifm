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

static void init_extended_keys(void);
static void leave_permissions_mode(void);
static void cmd_ctrl_c(struct key_info, struct keys_info *);
static void cmd_ctrl_m(struct key_info, struct keys_info *);
static void set_perm_string(FileView *view, const int *perms,
		const int *origin_perms, char *file);
static void files_chmod(FileView *view, char *mode, int recurse_dirs);
static char * add_file_to_list(char *list, const char *filename);
static void file_chmod(char *path, char *mode, int recurse_dirs);
static void cmd_space(struct key_info, struct keys_info *);
static void cmd_j(struct key_info, struct keys_info *);
static void cmd_k(struct key_info, struct keys_info *);
static void inc_curr(void);
static void dec_curr(void);

void
init_permissions_dialog_mode(int *key_mode)
{
	struct key_t *curr;

	assert(key_mode != NULL);

	mode = key_mode;

	curr = add_cmd(L"\x03", PERMISSIONS_MODE);
	curr->data.handler = cmd_ctrl_c;

	/* return */
	curr = add_cmd(L"\x0d", PERMISSIONS_MODE);
	curr->data.handler = cmd_ctrl_m;

	/* escape */
	curr = add_cmd(L"\x1b", PERMISSIONS_MODE);
	curr->data.handler = cmd_ctrl_c;

	curr = add_cmd(L" ", PERMISSIONS_MODE);
	curr->data.handler = cmd_space;

	curr = add_cmd(L"j", PERMISSIONS_MODE);
	curr->data.handler = cmd_j;

	curr = add_cmd(L"k", PERMISSIONS_MODE);
	curr->data.handler = cmd_k;

	curr = add_cmd(L"l", PERMISSIONS_MODE);
	curr->data.handler = cmd_ctrl_m;

	curr = add_cmd(L"t", PERMISSIONS_MODE);
	curr->data.handler = cmd_space;

	init_extended_keys();
}

static void
init_extended_keys(void)
{
#ifdef ENABLE_EXTENDED_KEYS
	struct key_t *curr;
	wchar_t buf[] = {L'\0', L'\0'};

	buf[0] = KEY_UP;
	curr = add_cmd(buf, PERMISSIONS_MODE);
	curr->data.handler = cmd_k;

	buf[0] = KEY_DOWN;
	curr = add_cmd(buf, PERMISSIONS_MODE);
	curr->data.handler = cmd_j;
#endif /* ENABLE_EXTENDED_KEYS */
}

void
enter_permissions_mode(FileView *active_view)
{
	int i;
	mode_t fmode;
	mode_t diff;

	view = active_view;
	*mode = PERMISSIONS_MODE;
	clear_input_bar();
	curr_stats.use_input_bar = 0;
	memset(perms, 0, sizeof(perms));

	diff = 0;
	i = 0;
	while(!view->dir_entry[i].selected && i < view->list_rows)
		i++;
	if(i == view->list_rows)
	{
		i = view->list_pos;
		file_is_dir = is_dir(view->dir_entry[i].name);
	}
	else
		file_is_dir = 0;
	fmode = view->dir_entry[i].mode;
	while(i < view->list_rows)
	{
		if(view->dir_entry[i].selected)
			diff |= (view->dir_entry[i].mode ^ fmode);
		i++;
	}

	perms[0] = !(diff & S_IRUSR) ? (fmode & S_IRUSR) : -1;
	perms[1] = !(diff & S_IWUSR) ? (fmode & S_IWUSR) : -1;
	perms[2] = !(diff & S_IXUSR) ? (fmode & S_IXUSR) : -1;
	perms[3] = !(diff & S_ISUID) ? (fmode & S_ISUID) : -1;
	perms[4] = !(diff & S_IRGRP) ? (fmode & S_IRGRP) : -1;
	perms[5] = !(diff & S_IWGRP) ? (fmode & S_IWGRP) : -1;
	perms[6] = !(diff & S_IXGRP) ? (fmode & S_IXGRP) : -1;
	perms[7] = !(diff & S_ISGID) ? (fmode & S_ISGID) : -1;
	perms[8] = !(diff & S_IROTH) ? (fmode & S_IROTH) : -1;
	perms[9] = !(diff & S_IWOTH) ? (fmode & S_IWOTH) : -1;
	perms[10] = !(diff & S_IXOTH) ? (fmode & S_IXOTH) : -1;
	perms[11] = !(diff & S_ISVTX) ? (fmode & S_ISVTX) : -1;
	memcpy(origin_perms, perms, sizeof(perms));

	top = 3;
	bottom = file_is_dir ? 18 : 16;
	curr = 3;
	permnum = 0;
	step = 1;
	while (perms[permnum] < 0 && curr <= bottom)
	{
		inc_curr();
		permnum++;
	}

	if (curr > bottom)
	{
		show_error_msg("Permissions change error",
				"Selected files have no common access state");
		leave_permissions_mode();
		return;
	}

	col = 9;
	changed = 0;
	redraw_permissions_dialog();
}

void
redraw_permissions_dialog(void)
{
	char *filename;
	int x, y;

	wclear(change_win);

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
		wresize(change_win, 20, 30);

	getmaxyx(stdscr, y, x);
	mvwin(change_win, (y - (20 + (file_is_dir != 0)*2))/2, (x - 30)/2);
	box(change_win, ACS_VLINE, ACS_HLINE);

	filename = get_current_file_name(view);
	if(strlen(filename) > x - 2)
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

	leave_permissions_mode();

	if(!changed)
	{
		return;
	}

	snprintf(path, sizeof(path), "%s/%s", view->curr_dir,
			view->dir_entry[view->list_pos].name);

	set_perm_string(view, perms, origin_perms, path);
	load_dir_list(view, 1);
	moveto_list_pos(view, view->curr_line);
	clean_selected_files(view);
	draw_dir_list(view, view->top_line, view->list_pos);
	moveto_list_pos(view, view->list_pos);
}

static void
set_perm_string(FileView *view, const int *perms, const int *origin_perms,
		char *file)
{
	int i = 0;
	char *add_perm[] = {"u+r", "u+w", "u+x", "u+s", "g+r", "g+w", "g+x", "g+s",
											"o+r", "o+w", "o+x", "o+t"};
	char *sub_perm[] = {"u-r", "u-w", "u-x", "u-s", "g-r", "g-w", "g-x", "g-s",
											"o-r", "o-w", "o-x", "o-t"};
	char perm_string[64] = " ";

	for(i = 0; i < 12; i++)
	{
		if(perms[i] == origin_perms[i])
			continue;

		if(perms[i])
			strcat(perm_string, add_perm[i]);
		else
			strcat(perm_string, sub_perm[i]);

		strcat(perm_string, ",");
	}
	perm_string[strlen(perm_string) - 1] = '\0'; /* Remove last , */

	files_chmod(view, perm_string, perms[12]);
}

static void
files_chmod(FileView *view, char *mode, int recurse_dirs)
{
	int i;
	char *file_list;

	file_list = malloc(1);
	if(file_list == NULL)
		return;
	*file_list = '\0';

	i = 0;
	while(!view->dir_entry[i].selected && i < view->list_rows)
		i++;
	if(i == view->list_rows)
	{
		char *p, *filename;

		filename = view->dir_entry[view->list_pos].name;
		p = add_file_to_list(file_list, filename);
		if(p == NULL)
		{
			free(file_list);
			return;
		}
		file_list = p;
	}
	else
	{
		while(i < view->list_rows)
		{
			if(view->dir_entry[i].selected)
			{
				char *p, *filename;

				filename = view->dir_entry[i].name;
				p = add_file_to_list(file_list, filename);
				if(p == NULL)
				{
					free(file_list);
					return;
				}
				file_list = p;
			}
			i++;
		}
	}
	file_chmod(file_list, mode, recurse_dirs);
	free(file_list);
}

static char *
add_file_to_list(char *list, const char *filename)
{
	char *p, *escaped;

	escaped = escape_filename(filename, strlen(filename), 1);
	if(escaped == NULL)
	{
		return NULL;
	}

	p = realloc(list, strlen(list) + strlen(escaped) + 2);
	if(p == NULL)
	{
		free(escaped);
		return NULL;
	}
	list = p;
	strcat(list, " ");
	strcat(list, escaped);
	free(escaped);
	return list;
}

static void
file_chmod(char *path, char *mode, int recurse_dirs)
{
  char cmd[128 + strlen(path)];

	if(recurse_dirs)
	{
		snprintf(cmd, sizeof(cmd), "chmod -R %s %s", mode, path);
		start_background_job(cmd);
	}
	else
	{
		snprintf(cmd, sizeof(cmd), "chmod %s %s", mode, path);
		system_and_wait_for_errors(cmd);
	}
}

static void
cmd_space(struct key_info key_info, struct keys_info *keys_info)
{
	changed = 1;

	mvwaddch(change_win, curr, col, perms[permnum] < 0 ? 'X' :
			(perms[permnum] ? ' ' : '*'));
	perms[permnum] = !perms[permnum];

	wmove(change_win, curr, col);
	wrefresh(change_win);
}

static void
cmd_j(struct key_info key_info, struct keys_info *keys_info)
{
	do
	{
		inc_curr();
		permnum++;
	} while (curr <= bottom && perms[permnum] < 0);

	if(curr > bottom)
	{
		do
		{
			dec_curr();
			permnum--;
		} while (perms[permnum] < 0);
	}

	wmove(change_win, curr, col);
	wrefresh(change_win);
}

static void
cmd_k(struct key_info key_info, struct keys_info *keys_info)
{
	do
	{
		dec_curr();
		permnum--;
	} while (curr >= top && perms[permnum] < 0);

	if(curr < top)
	{
		do
		{
			inc_curr();
			permnum++;
		} while (perms[permnum] < 0);
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
