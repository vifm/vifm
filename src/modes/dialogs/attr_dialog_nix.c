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

#include <curses.h>

#include <fcntl.h>
#include <sys/stat.h> /* S_* */
#include <sys/types.h> /* mode_t */
#include <unistd.h> /* uid_t geteuid() */

#include <assert.h> /* assert() */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <string.h> /* strncat() strlen() */

#include "../../compat/curses.h"
#include "../../compat/fs_limits.h"
#include "../../engine/keys.h"
#include "../../engine/mode.h"
#include "../../ui/cancellation.h"
#include "../../ui/fileview.h"
#include "../../ui/ui.h"
#include "../../utils/fs.h"
#include "../../utils/macros.h"
#include "../../utils/path.h"
#include "../../utils/str.h"
#include "../../utils/test_helpers.h"
#include "../../utils/utf8.h"
#include "../../utils/utils.h"
#include "../../filelist.h"
#include "../../flist_sel.h"
#include "../../ops.h"
#include "../../status.h"
#include "../../undo.h"
#include "../modes.h"
#include "../wk.h"
#include "msg_dialog.h"

static char get_perm_mark(int line);
static char * get_title(int max_width);
static int is_one_file_selected(int first_file_index);
static int get_first_file_index(void);
static int get_selection_size(int first_file_index);
static void leave_attr_mode(void);
static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_return(key_info_t key_info, keys_info_t *keys_info);
TSTATIC void set_perm_string(view_t *view, const int perms[13],
		const int origin_perms[13], int adv_perms[3]);
static void file_chmod(char *path, const char *mode, const char *inv_mode,
		int recurse_dirs);
static void cmd_G(key_info_t key_info, keys_info_t *keys_info);
static void cmd_gg(key_info_t key_info, keys_info_t *keys_info);
static void cmd_space(key_info_t key_info, keys_info_t *keys_info);
static void cmd_r(key_info_t key_info, keys_info_t *keys_info);
static void cmd_w(key_info_t key_info, keys_info_t *keys_info);
static void cmd_x(key_info_t key_info, keys_info_t *keys_info);
static void cmd_s(key_info_t key_info, keys_info_t *keys_info);
static void cmd_e(key_info_t key_info, keys_info_t *keys_info);
static void toggle_bit_class(int i);
static void cmd_j(key_info_t key_info, keys_info_t *keys_info);
static void cmd_k(key_info_t key_info, keys_info_t *keys_info);
static void inc_curr(void);
static void dec_curr(void);

static view_t *view;
static int top, bottom;
static int curr;
static int permnum;
static int step;
static int col;
static int changed;
static int file_is_dir;
static int perms[13];
static int origin_perms[13];
static int adv_perms[3];

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
	{WK_r,      {{&cmd_r},      .descr = "toggle read bits"}},
	{WK_w,      {{&cmd_w},      .descr = "toggle write bits"}},
	{WK_x,      {{&cmd_x},      .descr = "toggle execute bits"}},
	{WK_s,      {{&cmd_s},      .descr = "toggle special bits"}},
	{WK_e,      {{&cmd_e},      .descr = "toggle recursion"}},
#ifdef ENABLE_EXTENDED_KEYS
	{{K(KEY_HOME)},  {{&cmd_gg},     .descr = "go to the first item"}},
	{{K(KEY_END)},   {{&cmd_G},      .descr = "go to the last item"}},
	{{K(KEY_UP)},    {{&cmd_k},      .descr = "go to item above"}},
	{{K(KEY_DOWN)},  {{&cmd_j},      .descr = "go to item below"}},
	{{K(KEY_RIGHT)}, {{&cmd_return}, .descr = "update permissions"}},
#endif /* ENABLE_EXTENDED_KEYS */
};

void
init_attr_dialog_mode(void)
{
	int ret_code;

	ret_code = vle_keys_add(builtin_cmds, ARRAY_LEN(builtin_cmds), ATTR_MODE);
	assert(ret_code == 0);

	(void)ret_code;
}

void
enter_attr_mode(view_t *active_view)
{
	mode_t fmode = 0;
	mode_t diff;
	uid_t uid = geteuid();
	dir_entry_t *entry;
	int first;

	flist_set_marking(active_view, 0);

	if(curr_stats.load_stage < 2)
		return;

	view = active_view;
	memset(perms, 0, sizeof(perms));

	first = 1;
	file_is_dir = 0;
	diff = 0;
	entry = NULL;
	while(iter_marked_entries(view, &entry))
	{
		if(first)
		{
			fmode = entry->mode;
			first = 0;
		}

		diff |= (entry->mode ^ fmode);
		file_is_dir |= fentry_is_dir(entry);

		if(uid != 0 && entry->uid != uid)
		{
			show_error_msgf("Access error", "You are not owner of %s", entry->name);
			return;
		}
	}
	if(first)
	{
		show_error_msg("Permissions", "No files to process");
		return;
	}

	ui_hide_graphics();
	vle_mode_set(ATTR_MODE, VMT_SECONDARY);
	modes_input_bar_clear();
	curr_stats.use_input_bar = 0;

	/* Values:
	 *  - = 0 -- the bit is unset,
	 *  - > 0 -- the bit is set,
	 *  - < 0 -- value is different for different files. */
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

	/* These are Y positions from the top of the dialog, thus the topmost entry
	 * (namely Owner - Read) is at position 3. */
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

	col = 10;
	changed = 0;
	redraw_attr_dialog();
}

void
redraw_attr_dialog(void)
{
	char *title;
	int x, y;

	werase(change_win);
	if(file_is_dir)
		wresize(change_win, 22, 31);
	else
		wresize(change_win, 20, 31);

	mvwprintw(change_win,  3, 3, "Owner [%c] Read", get_perm_mark(0));
	mvwprintw(change_win,  4, 3, "      [%c] Write", get_perm_mark(1));
	mvwprintw(change_win,  5, 3, "      [%c] Execute", get_perm_mark(2));
	mvwprintw(change_win,  6, 3, "      [%c] SetUID", get_perm_mark(3));
	mvwprintw(change_win,  8, 3, "Group [%c] Read", get_perm_mark(4));
	mvwprintw(change_win,  9, 3, "      [%c] Write", get_perm_mark(5));
	mvwprintw(change_win, 10, 3, "      [%c] Execute", get_perm_mark(6));
	mvwprintw(change_win, 11, 3, "      [%c] SetGID", get_perm_mark(7));
	mvwprintw(change_win, 13, 3, "Other [%c] Read", get_perm_mark(8));
	mvwprintw(change_win, 14, 3, "      [%c] Write", get_perm_mark(9));
	mvwprintw(change_win, 15, 3, "      [%c] Execute",  get_perm_mark(10));
	mvwprintw(change_win, 16, 3, "      [%c] Sticky", get_perm_mark(11));

	if(file_is_dir)
	{
		mvwprintw(change_win, 18, 3, "      [%c] Set Recursively",
				get_perm_mark(12));
	}

	getmaxyx(stdscr, y, x);
	mvwin(change_win, (y - (20 + (file_is_dir != 0)*2))/2, (x - 31)/2);
	box(change_win, 0, 0);

	title = get_title(getmaxx(change_win) - 2);
	mvwaddstr(change_win, 0, (getmaxx(change_win) - utf8_strsw(title))/2, title);
	free(title);

	checked_wmove(change_win, curr, col);
	ui_set_cursor(/*visibility=*/1);
	ui_refresh_win(change_win);
}

/* Determines visual mark for a permission in its current state.  Returns the
 * mark. */
static char
get_perm_mark(int i)
{
	/* Execute bit. */
	if(i == 2 || i == 6 || i == 10)
	{
		if(perms[i] && adv_perms[i/4])
		{
			return 'd';
		}
	}

	if(perms[i] < 0)
	{
		return 'X';
	}

	return (perms[i] ? '*' : ' ');
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

static void
leave_attr_mode(void)
{
	vle_mode_set(NORMAL_MODE, VMT_PRIMARY);
	ui_set_cursor(/*visibility=*/0);
	curr_stats.use_input_bar = 1;

	flist_sel_stash(view);
	ui_view_schedule_reload(view);
}

static void
cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info)
{
	leave_attr_mode();
}

static void
cmd_return(key_info_t key_info, keys_info_t *keys_info)
{
	char path[PATH_MAX + 1];

	if(!changed)
	{
		return;
	}

	get_current_full_path(view, sizeof(path), path);
	set_perm_string(view, perms, origin_perms, adv_perms);

	leave_attr_mode();
}

TSTATIC void
set_perm_string(view_t *view, const int perms[13], const int origin_perms[13],
		int adv_perms[3])
{
	int i = 0;
	const char *add_perm[] = {
		"u+r", "u+w", "u+x", "u+s",
		"g+r", "g+w", "g+x", "g+s",
		"o+r", "o+w", "o+x", "o+t"
	};
	const char *sub_perm[] = {
		"u-r", "u-w", "u-x", "u-s",
		"g-r", "g-w", "g-x", "g-s",
		"o-r", "o-w", "o-x", "o-t"
	};
	const char *add_adv_perm[] = { "u-x+X", "g-x+X", "o-x+X" };
	char perm_str[64] = " ";
	size_t perm_str_len = strlen(perm_str);

	if(adv_perms[0] && adv_perms[1] && adv_perms[2])
	{
		adv_perms[0] = -1;
		adv_perms[1] = -1;
		adv_perms[2] = -1;

		snprintf(perm_str + perm_str_len, sizeof(perm_str) - perm_str_len,
				"a-x+X,");
		perm_str_len += strlen(perm_str + perm_str_len);
	}

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

		snprintf(perm_str + perm_str_len, sizeof(perm_str) - perm_str_len, "%s,",
				perm);
		perm_str_len += strlen(perm_str + perm_str_len);
	}
	perm_str[strlen(perm_str) - 1] = '\0'; /* Remove last comma (','). */

	files_chmod(view, perm_str, perms[12]);
}

void
files_chmod(view_t *view, const char mode[], int recurse_dirs)
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
		char inv_mode[16];
		snprintf(inv_mode, sizeof(inv_mode), "0%o", entry->mode & 0xff);

		char path[PATH_MAX + 1];
		get_full_path_of(entry, sizeof(path), path);
		file_chmod(path, mode, inv_mode, recurse_dirs);
	}

	un_group_close();

	ui_cancellation_pop();
}

static void
file_chmod(char *path, const char *mode, const char *inv_mode, int recurse_dirs)
{
	int op = recurse_dirs ? OP_CHMODR : OP_CHMOD;

	if(perform_operation(op, NULL, (void *)mode, path, NULL) == OPS_SUCCEEDED)
	{
		un_group_add_op(op, strdup(mode), strdup(inv_mode), path, "");
	}
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
	ui_refresh_win(change_win);
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
	ui_refresh_win(change_win);
}

/* Toggles all read bits. */
static void
cmd_r(key_info_t key_info, keys_info_t *keys_info)
{
	toggle_bit_class(0);
}

/* Toggles all write bits. */
static void
cmd_w(key_info_t key_info, keys_info_t *keys_info)
{
	toggle_bit_class(1);
}

/* Toggles all execute bits. */
static void
cmd_x(key_info_t key_info, keys_info_t *keys_info)
{
	toggle_bit_class(2);
}

/* Toggles all special bits. */
static void
cmd_s(key_info_t key_info, keys_info_t *keys_info)
{
	toggle_bit_class(3);
}

/* Toggles recursive bit. */
static void
cmd_e(key_info_t key_info, keys_info_t *keys_info)
{
	if(file_is_dir)
	{
		changed = 1;
		perms[12] = !perms[12];
		redraw_attr_dialog();
	}
}

/* Given i = {0,1,2,3} sets/unsets each of the three {r,w,x,s} bits. */
static void
toggle_bit_class(int i)
{
	changed = 1;

	if(perms[i] && perms[i + 4] && perms[i + 8])
	{
		/* Execute bit. */
		if(i == 2)
		{
			if(!adv_perms[0] && !adv_perms[1] && !adv_perms[2])
			{
				adv_perms[0] = adv_perms[1] = adv_perms[2] = 1;
			}
			else
			{
				adv_perms[0] = adv_perms[1] = adv_perms[2] = 0;
				perms[i] = perms[i + 4] = perms[i + 8] = 0;
			}
		}
		else
		{
			perms[i] = perms[i + 4] = perms[i + 8] = 0;
		}
	}
	else
	{
		perms[i] = perms[i + 4] = perms[i + 8] = 1;
	}

	redraw_attr_dialog();
}

static void
cmd_space(key_info_t key_info, keys_info_t *keys_info)
{
	changed = 1;

	if(perms[permnum] < 0)
	{
		perms[permnum] = 0;
	}
	/* Execute bit. */
	else if(curr == 5 || curr == 10 || curr == 15)
	{
		int i = curr/5 - 1;
		if(!perms[permnum])
		{
			perms[permnum] = 1;
		}
		else
		{
			if(adv_perms[i])
			{
				if(origin_perms[permnum] < 0)
				{
					perms[permnum] = -1;
				}
				else
				{
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
			perms[permnum] = -1;
		}
		else
		{
			/* perms[permnum] < 0 is handled above, so it must be 0 here. */
			perms[permnum] = 1;
		}
	}
	else
	{
		perms[permnum] = !perms[permnum];
	}

	redraw_attr_dialog();
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
	ui_refresh_win(change_win);
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
	ui_refresh_win(change_win);
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
/* vim: set cinoptions+=t0 filetype=c : */
