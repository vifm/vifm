#include <curses.h>

#include <assert.h>
#include <ctype.h>
#include <string.h>

#include "bookmarks.h"
#include "commands.h"
#include "color_scheme.h"
#include "config.h"
#include "filelist.h"
#include "fileops.h"
#include "keys.h"
#include "keys_buildin_c.h"
#include "menus.h"
#include "modes.h"
#include "status.h"
#include "ui.h"

#include "keys_buildin_m.h"

static int *mode;
static FileView *view;
int top, bottom, curr, col;

static void init_extended_keys(void);
static void leave_sort_mode(void);
static void keys_ctrl_c(struct key_info, struct keys_info *);
static void keys_ctrl_m(struct key_info, struct keys_info *);
static void keys_j(struct key_info, struct keys_info *);
static void keys_k(struct key_info, struct keys_info *);

void
init_buildin_s_keys(int *key_mode)
{
	struct key_t *curr;

	assert(key_mode != NULL);

	mode = key_mode;

	curr = add_keys(L"\x03", SORT_MODE);
	curr->data.handler = keys_ctrl_c;

	/* return */
	curr = add_keys(L"\x0d", SORT_MODE);
	curr->data.handler = keys_ctrl_m;

	/* escape */
	curr = add_keys(L"\x1b", SORT_MODE);
	curr->data.handler = keys_ctrl_c;

	curr = add_keys(L"j", SORT_MODE);
	curr->data.handler = keys_j;

	curr = add_keys(L"k", SORT_MODE);
	curr->data.handler = keys_k;

	curr = add_keys(L"l", SORT_MODE);
	curr->data.handler = keys_ctrl_m;

	init_extended_keys();
}

static void
init_extended_keys(void)
{
	struct key_t *curr;
	wchar_t buf[] = {L'\0', L'\0'};

	buf[0] = KEY_UP;
	curr = add_keys(buf, VISUAL_MODE);
	curr->data.handler = keys_k;

	buf[0] = KEY_DOWN;
	curr = add_keys(buf, VISUAL_MODE);
	curr->data.handler = keys_j;
}

void
enter_sort_mode(FileView *active_view)
{
	int x, y;

	view = active_view;
	*mode = SORT_MODE;

	wattroff(view->win, COLOR_PAIR(CURR_LINE_COLOR) | A_BOLD);
	curs_set(0);
	update_all_windows();
	werase(sort_win);
	box(sort_win, ACS_VLINE, ACS_HLINE);

	getmaxyx(sort_win, y, x);
	mvwaddstr(sort_win, 0, (x - 6)/2, " Sort ");
	mvwaddstr(sort_win, 1, 2, " Sort files by:");
	mvwaddstr(sort_win, 2, 4, " [ ] File Extenstion");
	mvwaddstr(sort_win, 3, 4, " [ ] File Name");
	mvwaddstr(sort_win, 4, 4, " [ ] Group ID");
	mvwaddstr(sort_win, 5, 4, " [ ] Group Name");
	mvwaddstr(sort_win, 6, 4, " [ ] Mode");
	mvwaddstr(sort_win, 7, 4, " [ ] Owner ID");
	mvwaddstr(sort_win, 8, 4, " [ ] Owner Name");
	mvwaddstr(sort_win, 9, 4, " [ ] Size (Ascending)");
	mvwaddstr(sort_win, 10, 4, " [ ] Size (Descending)");
	mvwaddstr(sort_win, 11, 4, " [ ] Time Accessed");
	mvwaddstr(sort_win, 12, 4, " [ ] Time Changed");
	mvwaddstr(sort_win, 13, 4, " [ ] Time Modified");
	mvwaddch(sort_win, view->sort_type + 2, 6, '*');

	top = 2;
	bottom = top + NUM_SORT_OPTIONS - 1;
	curr = view->sort_type + 2;
	col = 6;

	wmove(sort_win, curr, col);
	wrefresh(sort_win);
}

static void
leave_sort_mode(void)
{
	*mode = NORMAL_MODE;
}

static void
keys_ctrl_c(struct key_info key_info, struct keys_info *keys_info)
{
	leave_sort_mode();
}

static void
keys_ctrl_m(struct key_info key_info, struct keys_info *keys_info)
{
	char filename[NAME_MAX];

	leave_sort_mode();

	view->sort_type = curr - 2;
	curr_stats.setting_change = 1;

	snprintf(filename, sizeof(filename), "%s",
			view->dir_entry[view->list_pos].name);
	load_dir_list(view, 1);
	moveto_list_pos(view, find_file_pos_in_list(view, filename));
}

static void
keys_j(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	mvwaddch(sort_win, curr, col, ' ');
	curr++;
	if(curr > bottom)
		curr--;

	mvwaddch(sort_win, curr, col, '*');
	wmove(sort_win, curr, col);
	wrefresh(sort_win);
}

static void
keys_k(struct key_info key_info, struct keys_info *keys_info)
{
	if(key_info.count == NO_COUNT_GIVEN)
		key_info.count = 1;

	mvwaddch(sort_win, curr, col, ' ');
	curr--;
	if(curr < top)
		curr++;

	mvwaddch(sort_win, curr, col, '*');
	wmove(sort_win, curr, col);
	wrefresh(sort_win);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
