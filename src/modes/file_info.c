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

#include <sys/stat.h>

#include <assert.h> /* assert() */
#include <ctype.h> /* isdigit() */
#include <inttypes.h> /* PRId64 */
#include <stddef.h> /* size_t */
#include <stdint.h> /* uint64_t */
#include <stdio.h>
#include <stdlib.h> /* free() */
#include <string.h> /* strlen() */

#include "../cfg/config.h"
#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "../engine/keys.h"
#include "../engine/mode.h"
#include "../int/file_magic.h"
#include "../int/term_title.h"
#include "../menus/menus.h"
#include "../ui/ui.h"
#include "../utils/fs.h"
#include "../utils/fsdata.h"
#include "../utils/macros.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/utf8.h"
#include "../utils/utils.h"
#include "../filelist.h"
#include "../status.h"
#include "../types.h"
#include "dialogs/msg_dialog.h"
#include "modes.h"
#include "wk.h"

static void leave_file_info_mode(void);
static int print_item(const char label[], const char path[], int curr_y);
static int show_file_type(view_t *view, int curr_y);
static int print_link_info(const dir_entry_t *curr, int curr_y);
static int show_mime_type(view_t *view, int curr_y);
static void cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info);
static void cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info);

static view_t *view;

static keys_add_info_t builtin_cmds[] = {
	{WK_C_c,    {{&cmd_ctrl_c}, .descr = "hide file info"}},
	{WK_C_l,    {{&cmd_ctrl_l}, .descr = "redraw"}},
	{WK_CR,     {{&cmd_ctrl_c}, .descr = "hide file info"}},
	{WK_ESC,    {{&cmd_ctrl_c}, .descr = "hide file info"}},
	{WK_Z WK_Q, {{&cmd_ctrl_c}, .descr = "hide file info"}},
	{WK_Z WK_Z, {{&cmd_ctrl_c}, .descr = "hide file info"}},
	{WK_q,      {{&cmd_ctrl_c}, .descr = "hide file info"}},
};

void
modfinfo_init(void)
{
	int ret_code;

	ret_code = vle_keys_add(builtin_cmds, ARRAY_LEN(builtin_cmds),
			FILE_INFO_MODE);
	assert(ret_code == 0);

	(void)ret_code;
}

void
modfinfo_enter(view_t *v)
{
	if(fentry_is_fake(get_current_entry(v)))
	{
		show_error_msg("File info", "Entry doesn't correspond to a file.");
		return;
	}

	term_title_update("File Information");
	vle_mode_set(FILE_INFO_MODE, VMT_PRIMARY);
	ui_qv_cleanup_if_needed();
	view = v;
	ui_setup_for_menu_like();
	modfinfo_redraw();
}

void
modfinfo_abort(void)
{
	leave_file_info_mode();
}

static void
leave_file_info_mode(void)
{
	vle_mode_set(NORMAL_MODE, VMT_PRIMARY);
	stats_redraw_later();
}

void
modfinfo_redraw(void)
{
	const dir_entry_t *curr;
	char perm_buf[26];
	char size_buf[64];
	char buf[256];
#ifndef _WIN32
	char id_buf[26];
#endif
	int curr_y;
	uint64_t size;
	int size_not_precise;

	assert(view != NULL);

	if(resize_for_menu_like() != 0)
	{
		return;
	}

	ui_set_attr(menu_win, &cfg.cs.color[WIN_COLOR], cfg.cs.pair[WIN_COLOR]);
	werase(menu_win);

	curr = get_current_entry(view);

	size = fentry_get_size(view, curr);
	size_not_precise = friendly_size_notation(size, sizeof(size_buf), size_buf);

	curr_y = 2;

	char *escaped = escape_unreadable(curr->origin);
	curr_y += print_item("Path: ", escaped, curr_y);
	free(escaped);
	escaped = escape_unreadable(curr->name);
	curr_y += print_item("Name: ", escaped, curr_y);
	free(escaped);

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
	get_perm_string(perm_buf, sizeof(perm_buf), curr->mode);
	curr_y += print_item("Permissions: ", perm_buf, curr_y);
#else
	copy_str(perm_buf, sizeof(perm_buf), attr_str_long(curr->attrs));
	curr_y += print_item("Attributes: ", perm_buf, curr_y);
#endif

#ifndef _WIN32
	snprintf(buf, sizeof(buf), "%d", curr->nlinks);
	curr_y += print_item("Hard Links: ", buf, curr_y);
#endif

	format_iso_time(curr->mtime, buf, sizeof(buf));
	curr_y += print_item("Modified: ", buf, curr_y);

	format_iso_time(curr->atime, buf, sizeof(buf));
	curr_y += print_item("Accessed: ", buf, curr_y);

	format_iso_time(curr->ctime, buf, sizeof(buf));
#ifndef _WIN32
	curr_y += print_item("Changed: ", buf, curr_y);
#else
	curr_y += print_item("Created: ", buf, curr_y);
#endif

#ifndef _WIN32
	get_uid_string(curr, 0, sizeof(id_buf), id_buf);
	if(isdigit(id_buf[0]))
	{
		copy_str(buf, sizeof(buf), id_buf);
	}
	else
	{
		snprintf(buf, sizeof(buf), "%s (%lu)", id_buf, (unsigned long)curr->uid);
	}
	curr_y += print_item("Owner: ", buf, curr_y);

	get_gid_string(curr, 0, sizeof(id_buf), id_buf);
	if(isdigit(id_buf[0]))
	{
		copy_str(buf, sizeof(buf), id_buf);
	}
	else
	{
		snprintf(buf, sizeof(buf), "%s (%lu)", id_buf, (unsigned long)curr->gid);
	}
	curr_y += print_item("Group: ", buf, curr_y);
#endif

	/* Fake use after last assignment. */
	(void)curr_y;

	box(menu_win, 0, 0);
	checked_wmove(menu_win, 0, 3);
	wprint(menu_win, " File Information ");
	ui_refresh_win(menu_win);
	checked_wmove(menu_win, 2, 2);
}

/* Prints item prefixed with a label wrapping the item if it's too long.
 * Returns increment for curr_y. */
static int
print_item(const char label[], const char text[], int curr_y)
{
	mvwaddstr(menu_win, curr_y, 2, label);
	int x = getcurx(menu_win);
	int max_width = getmaxx(menu_win) - 2 - x;
	int dy = 0;

	do
	{
		const size_t print_len = utf8_nstrsnlen(text, max_width);

		char part[1000];
		copy_str(part, MIN(sizeof(part), print_len + 1), text);
		wprint(menu_win, part);

		text += print_len;
		++dy;
		++curr_y;
		checked_wmove(menu_win, curr_y, x);
	}
	while(text[0] != '\0');

	return dy + 1;
}

/* Returns increment for curr_y. */
static int
show_file_type(view_t *view, int curr_y)
{
	const dir_entry_t *curr;
	int old_curr_y = curr_y;

	curr = get_current_entry(view);

	mvwaddstr(menu_win, curr_y, 2, "Type: ");
	if(curr->type == FT_LINK || is_shortcut(curr->name))
	{
		curr_y += print_link_info(curr, curr_y);
	}
	else if(curr->type == FT_EXEC || curr->type == FT_REG)
	{
#ifdef HAVE_FILE_PROG
		char full_path[PATH_MAX + 1];
		FILE *pipe;
		char command[1024];
		char buf[NAME_MAX + 1];
		char *escaped_full_path;

		get_current_full_path(view, sizeof(full_path), full_path);

		/* Use the file command to get file information. */
		escaped_full_path = shell_like_escape(full_path, 0);
		snprintf(command, sizeof(command), "file %s -b", escaped_full_path);
		free(escaped_full_path);

		if((pipe = popen(command, "r")) == NULL)
		{
			mvwaddstr(menu_win, curr_y, 8, "Unable to open pipe to read file");
			return 2;
		}

		if(fgets(buf, sizeof(buf), pipe) != buf)
			strcpy(buf, "Pipe read error");

		pclose(pipe);

		int max_x = getmaxx(menu_win);
		mvwaddnstr(menu_win, curr_y, 8, buf, max_x - 9);
		if(max_x > 9 && strlen(buf) > (size_t)(max_x - 9))
		{
			mvwaddnstr(menu_win, curr_y + 1, 8, buf + max_x - 9, max_x - 9);
		}
#else /* #ifdef HAVE_FILE_PROG */
		if(curr->type == FT_EXEC)
			mvwaddstr(menu_win, curr_y, 8, "Executable");
		else
			mvwaddstr(menu_win, curr_y, 8, "Regular File");
#endif /* #ifdef HAVE_FILE_PROG */
	}
	else if(curr->type == FT_DIR)
	{
		mvwaddstr(menu_win, curr_y, 8, "Directory");
	}
#ifndef _WIN32
	else if(curr->type == FT_CHAR_DEV || curr->type == FT_BLOCK_DEV)
	{
		const char *const type = (curr->type == FT_CHAR_DEV)
		                       ? "Character Device"
		                       : "Block Device";

		mvwaddstr(menu_win, curr_y, 8, type);

#if defined(major) && defined(minor)
		{
			char full_path[PATH_MAX + 1];
			struct stat st;
			get_current_full_path(view, sizeof(full_path), full_path);
			if(os_stat(full_path, &st) == 0)
			{
				char info[64];

				snprintf(info, sizeof(info), "Device Id: 0x%x:0x%x", major(st.st_rdev),
						minor(st.st_rdev));

				curr_y += 2;
				mvwaddstr(menu_win, curr_y, 2, info);
			}
		}
#endif
	}
	else if(curr->type == FT_SOCK)
	{
		mvwaddstr(menu_win, curr_y, 8, "Socket");
	}
#endif
	else if(curr->type == FT_FIFO)
	{
		mvwaddstr(menu_win, curr_y, 8, "Fifo Pipe");
	}
	else
	{
		mvwaddstr(menu_win, curr_y, 8, "Unknown");
	}
	curr_y += 2;

	return curr_y - old_curr_y;
}

/* Prints information about a link entry.  Returns increment for curr_y. */
static int
print_link_info(const dir_entry_t *curr, int curr_y)
{
	int old_curr_y = curr_y;
	int max_x = getmaxx(menu_win);

	char full_path[PATH_MAX + 1];
	char linkto[PATH_MAX + NAME_MAX];

	get_full_path_of(curr, sizeof(full_path), full_path);

	int broken_offset;
	int target_offset;
	if(curr->type == FT_LINK)
	{
		mvwaddstr(menu_win, curr_y, 8, "Link");
		curr_y += 2;
		mvwaddstr(menu_win, curr_y, 2, "Link To: ");
		broken_offset = 12;
		target_offset = 11;
	}
	else
	{
		mvwaddstr(menu_win, curr_y, 8, "Shortcut");
		curr_y += 2;
		mvwaddstr(menu_win, curr_y, 2, "Shortcut To: ");
		broken_offset = 16;
		target_offset = 15;
	}

	if(get_link_target(full_path, linkto, sizeof(linkto)) == 0)
	{
		mvwaddnstr(menu_win, curr_y, target_offset, linkto, max_x - target_offset);

		if(!path_exists(linkto, DEREF))
		{
			mvwaddstr(menu_win, curr_y - 2, broken_offset, " (BROKEN)");
		}
	}
	else
	{
		mvwaddstr(menu_win, curr_y, target_offset, "Couldn't Resolve Link");
	}

	if(curr->type == FT_LINK)
	{
		curr_y += 2;
		mvwaddstr(menu_win, curr_y, 2, "Real Path: ");

		char real[PATH_MAX + 1];
		if(os_realpath(full_path, real) == real)
		{
			mvwaddnstr(menu_win, curr_y, 13, real, max_x - 13);
		}
		else
		{
			waddstr(menu_win, "Couldn't Resolve Path");
		}
	}

	return curr_y - old_curr_y;
}

/* Returns increment for curr_y. */
static int
show_mime_type(view_t *view, int curr_y)
{
	char full_path[PATH_MAX + 1];
	get_current_full_path(view, sizeof(full_path), full_path);

	const char *mimetype = get_mimetype(full_path, 0);
	if(mimetype == NULL)
	{
		mimetype = "Unknown";
	}

	return print_item("Mime Type: ", mimetype, curr_y);
}

static void
cmd_ctrl_c(key_info_t key_info, keys_info_t *keys_info)
{
	leave_file_info_mode();
}

static void
cmd_ctrl_l(key_info_t key_info, keys_info_t *keys_info)
{
	modfinfo_redraw();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
