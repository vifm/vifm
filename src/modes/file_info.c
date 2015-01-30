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

#include "file_info.h"

#include <curses.h>

#ifndef _WIN32
#include <grp.h>
#include <pwd.h>
#endif
#include <sys/stat.h>

#include <assert.h> /* assert() */
#include <inttypes.h> /* PRId64 */
#include <stdint.h> /* uint64_t */
#include <stdio.h>
#include <string.h> /* strlen() */
#include <time.h>

#include "../engine/keys.h"
#include "../engine/mode.h"
#include "../menus/menus.h"
#include "../ui/ui.h"
#include "../utils/fs.h"
#include "../utils/fs_limits.h"
#include "../utils/macros.h"
#include "../utils/str.h"
#include "../utils/tree.h"
#include "../utils/utils.h"
#include "../filelist.h"
#include "../file_magic.h"
#include "../status.h"
#include "../types.h"
#include "modes.h"

static void leave_file_info_mode(void);
static int show_file_type(FileView *view, int curr_y);
static int show_mime_type(FileView *view, int curr_y);
static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info);

static FileView *view;
static int was_redraw;

static keys_add_info_t builtin_cmds[] = {
	{L"\x03", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"\x0c", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_l}}},
	/* return */
	{L"\x0d", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	/* escape */
	{L"\x1b", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"ZQ", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"ZZ", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
	{L"q", {BUILTIN_KEYS, FOLLOWED_BY_NONE, {.handler = cmd_ctrl_c}}},
};

void
init_file_info_mode(void)
{
	int ret_code;

	ret_code = add_cmds(builtin_cmds, ARRAY_LEN(builtin_cmds), FILE_INFO_MODE);
	assert(ret_code == 0);

	(void)ret_code;
}

void
enter_file_info_mode(FileView *v)
{
	vle_mode_set(FILE_INFO_MODE, VMT_PRIMARY);
	view = v;
	setup_menu();
	redraw_file_info_dialog();

	was_redraw = 0;
}

static void
leave_file_info_mode(void)
{
	vle_mode_set(NORMAL_MODE, VMT_PRIMARY);

	if(was_redraw)
	{
		update_screen(UT_FULL);
	}
	else
	{
		update_all_windows();
	}
}

void
redraw_file_info_dialog(void)
{
	char path_buf[PATH_MAX];
	const dir_entry_t *entry;
	char perm_buf[26];
	char size_buf[56];
	char buf[256];
#ifndef _WIN32
	char id_buf[26];
	struct passwd *pwd_buf;
	struct group *grp_buf;
#endif
	struct tm *tm_ptr;
	int curr_y;
	uint64_t size;
	int size_not_precise;

	assert(view != NULL);

	resize_for_menu_like();

	werase(menu_win);

	entry = &view->dir_entry[view->list_pos];

	size = 0;
	if(entry->type == FT_DIR)
	{
		char full_path[PATH_MAX];
		get_current_full_path(view, sizeof(full_path), full_path);
		tree_get_data(curr_stats.dirsize_cache, full_path, &size);
	}

	if(size == 0)
	{
		size = entry->size;
	}

	size_not_precise = friendly_size_notation(size, sizeof(size_buf), size_buf);

#ifndef _WIN32
	pwd_buf = getpwuid(entry->uid);
	if(pwd_buf == NULL)
	{
		snprintf(id_buf, sizeof(id_buf), "%d", (int)entry->uid);
	}
	else
	{
		copy_str(id_buf, sizeof(id_buf), pwd_buf->pw_name);
	}
	get_perm_string(perm_buf, sizeof(perm_buf), entry->mode);
#else
	copy_str(perm_buf, sizeof(perm_buf), attr_str_long(entry->attrs));
#endif

	curr_y = 2;

	mvwaddstr(menu_win, curr_y, 2, "Path: ");
	copy_str(path_buf, sizeof(path_buf), entry->origin);
	path_buf[getmaxx(menu_win) - 8] = '\0';
	checked_wmove(menu_win, curr_y, 8);
	wprint(menu_win, path_buf);
	curr_y += 2;

	mvwaddstr(menu_win, curr_y, 2, "Name: ");
	copy_str(path_buf, sizeof(path_buf), entry->name);
	path_buf[getmaxx(menu_win) - 8] = '\0';
	checked_wmove(menu_win, curr_y, 8);
	wprint(menu_win, path_buf);
	curr_y += 2;

	mvwaddstr(menu_win, curr_y, 2, "Size: ");
	mvwaddstr(menu_win, curr_y, 8, size_buf);
	if(size_not_precise)
	{
		snprintf(size_buf, sizeof(size_buf), " (%" PRId64 " bytes)", size);
		waddstr(menu_win, size_buf);
	}
	curr_y += 2;

	curr_y += show_file_type(view, curr_y);
	curr_y += show_mime_type(view, curr_y);

#ifndef _WIN32
	mvwaddstr(menu_win, curr_y, 2, "Permissions: ");
#else
	mvwaddstr(menu_win, curr_y, 2, "Attributes: ");
#endif
	mvwaddstr(menu_win, curr_y, 15, perm_buf);
	curr_y += 2;

	mvwaddstr(menu_win, curr_y, 2, "Modified: ");
	tm_ptr = localtime(&entry->mtime);
	strftime(buf, sizeof (buf), "%a %b %d %Y %I:%M %p", tm_ptr);
	checked_wmove(menu_win, curr_y, 13);
	wprint(menu_win, buf);
	curr_y += 2;

	mvwaddstr(menu_win, curr_y, 2, "Accessed: ");
	tm_ptr = localtime(&entry->atime);
	strftime(buf, sizeof (buf), "%a %b %d %Y %I:%M %p", tm_ptr);
	checked_wmove(menu_win, curr_y, 13);
	wprint(menu_win, buf);
	curr_y += 2;

#ifndef _WIN32
	mvwaddstr(menu_win, curr_y, 2, "Changed: ");
#else
	mvwaddstr(menu_win, curr_y, 2, "Created: ");
#endif
	tm_ptr = localtime(&entry->ctime);
	strftime(buf, sizeof (buf), "%a %b %d %Y %I:%M %p", tm_ptr);
	checked_wmove(menu_win, curr_y, 13);
	wprint(menu_win, buf);
	curr_y += 2;

#ifndef _WIN32
	mvwaddstr(menu_win, curr_y, 2, "Owner: ");
	mvwaddstr(menu_win, curr_y, 10, id_buf);
	curr_y += 2;

	mvwaddstr(menu_win, curr_y, 2, "Group: ");
	grp_buf = getgrgid(entry->gid);
	if(grp_buf == NULL)
	{
		snprintf(id_buf, sizeof(id_buf), "%d", (int)entry->gid);
	}
	else
	{
		copy_str(id_buf, sizeof(id_buf), grp_buf->gr_name);
	}
	mvwaddstr(menu_win, curr_y, 10, id_buf);
#endif

	box(menu_win, 0, 0);
	checked_wmove(menu_win, 0, 3);
	wprint(menu_win, " File Information ");
	wrefresh(menu_win);

	was_redraw = 1;
}

/* Returns increment for curr_y */
static int
show_file_type(FileView *view, int curr_y)
{
	const dir_entry_t *entry;
	int x;
	int old_curr_y = curr_y;
	x = getmaxx(menu_win);

	entry = &view->dir_entry[view->list_pos];

	mvwaddstr(menu_win, curr_y, 2, "Type: ");
	if(entry->type == FT_LINK)
	{
		char full_path[PATH_MAX];
		char linkto[PATH_MAX + NAME_MAX];

		get_current_full_path(view, sizeof(full_path), full_path);

		mvwaddstr(menu_win, curr_y, 8, "Link");
		curr_y += 2;
		mvwaddstr(menu_win, curr_y, 2, "Link To: ");

		if(get_link_target(full_path, linkto, sizeof(linkto)) == 0)
		{
			mvwaddnstr(menu_win, curr_y, 11, linkto, x - 11);

			if(!path_exists(linkto, DEREF))
			{
				mvwaddstr(menu_win, curr_y - 2, 12, " (BROKEN)");
			}
		}
		else
		{
			mvwaddstr(menu_win, curr_y, 11, "Couldn't Resolve Link");
		}
	}
	else if(entry->type == FT_EXEC || entry->type == FT_REG)
	{
#ifdef HAVE_FILE_PROG
		char full_path[PATH_MAX];
		FILE *pipe;
		char command[1024];
		char buf[NAME_MAX];

		get_current_full_path(view, sizeof(full_path), full_path);

		/* Use the file command to get file information. */
		snprintf(command, sizeof(command), "file \"%s\" -b", full_path);

		if((pipe = popen(command, "r")) == NULL)
		{
			mvwaddstr(menu_win, curr_y, 8, "Unable to open pipe to read file");
			return 2;
		}

		if(fgets(buf, sizeof(buf), pipe) != buf)
			strcpy(buf, "Pipe read error");

		pclose(pipe);

		mvwaddnstr(menu_win, curr_y, 8, buf, x - 9);
		if(x > 9 && strlen(buf) > x - 9)
		{
			mvwaddnstr(menu_win, curr_y + 1, 8, buf + x - 9, x - 9);
		}
#else /* #ifdef HAVE_FILE_PROG */
		if(entry->type == FT_EXEC)
			mvwaddstr(menu_win, curr_y, 8, "Executable");
		else
			mvwaddstr(menu_win, curr_y, 8, "Regular File");
#endif /* #ifdef HAVE_FILE_PROG */
	}
	else if(entry->type == FT_DIR)
	{
	  mvwaddstr(menu_win, curr_y, 8, "Directory");
	}
#ifndef _WIN32
	else if(S_ISCHR(entry->mode))
	{
	  mvwaddstr(menu_win, curr_y, 8, "Character Device");
	}
	else if(S_ISBLK(entry->mode))
	{
	  mvwaddstr(menu_win, curr_y, 8, "Block Device");
	}
	else if(entry->type == FT_FIFO)
	{
	  mvwaddstr(menu_win, curr_y, 8, "Fifo Pipe");
	}
	else if(S_ISSOCK(entry->mode))
	{
	  mvwaddstr(menu_win, curr_y, 8, "Socket");
	}
#endif
	else
	{
	  mvwaddstr(menu_win, curr_y, 8, "Unknown");
	}
	curr_y += 2;

	return curr_y - old_curr_y;
}

/* Returns increment for curr_y */
static int
show_mime_type(FileView *view, int curr_y)
{
	char full_path[PATH_MAX];
	const char* mimetype = NULL;

	get_current_full_path(view, sizeof(full_path), full_path);
	mimetype = get_mimetype(full_path);

	mvwaddstr(menu_win, curr_y, 2, "Mime Type: ");

	if(mimetype == NULL)
		mimetype = "Unknown";

	mvwaddstr(menu_win, curr_y, 13, mimetype);

	return 2;
}

static void
cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info)
{
	leave_file_info_mode();
}

static void
cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info)
{
	redraw_file_info_dialog();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
