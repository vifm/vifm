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
#include <sys/types.h>

#ifdef HAVE_SYS_SYSMACROS_H
#include <sys/sysmacros.h>
#endif

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
#include "../utils/string_array.h"
#include "../utils/utf8.h"
#include "../utils/utils.h"
#include "../filelist.h"
#include "../status.h"
#include "../types.h"
#include "dialogs/msg_dialog.h"
#include "modes.h"
#include "wk.h"

/* Width of the first column of the output. */
#define LHS_WIDTH 13

/* Data necessary for drawing pieces of information. */
typedef struct
{
	strlist_t lines; /* File info items. */
}
draw_ctx_t;

static void leave_file_info_mode(void);
static void fill_items(view_t *view, draw_ctx_t *ctx);
static void draw_items(const draw_ctx_t *ctx);
static void print_item(const char label[], const char path[], draw_ctx_t *ctx);
static void append_item(const char text[], draw_ctx_t *ctx);
static void show_file_type(view_t *view, draw_ctx_t *ctx);
static void print_link_info(const dir_entry_t *curr, draw_ctx_t *ctx);
static void show_mime_type(view_t *view, draw_ctx_t *ctx);
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

	ui_hide_graphics();

	term_title_update("File Information");
	vle_mode_set(FILE_INFO_MODE, VMT_PRIMARY);
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
	assert(view != NULL);

	if(resize_for_menu_like() != 0)
	{
		return;
	}

	draw_ctx_t ctx = { };
	fill_items(view, &ctx);

	ui_set_attr(menu_win, &cfg.cs.color[WIN_COLOR], cfg.cs.pair[WIN_COLOR]);
	werase(menu_win);

	draw_items(&ctx);

	box(menu_win, 0, 0);
	checked_wmove(menu_win, 0, 3);
	wprint(menu_win, " File Information ");
	ui_refresh_win(menu_win);
	checked_wmove(menu_win, 2, 2);

	free_string_array(ctx.lines.items, ctx.lines.nitems);
}

/* Generates list of strings with information about the file. */
static void
fill_items(view_t *view, draw_ctx_t *ctx)
{
	char buf[256];
	const dir_entry_t *curr = get_current_entry(view);

	char *escaped = escape_unreadable(curr->origin);
	print_item("Path", escaped, ctx);
	free(escaped);
	escaped = escape_unreadable(curr->name);
	print_item("Name", escaped, ctx);
	free(escaped);

	uint64_t size = fentry_get_size(view, curr);
	int size_not_precise =
		friendly_size_notation(size, sizeof(buf), buf);

	print_item("Size", buf, ctx);
	if(size_not_precise)
	{
		snprintf(buf, sizeof(buf), " (%" PRId64 " bytes)", size);
		append_item(buf, ctx);
	}

	show_file_type(view, ctx);
	show_mime_type(view, ctx);

#ifndef _WIN32
	snprintf(buf, sizeof(buf), "%d", curr->nlinks);
	print_item("Hard Links", buf, ctx);
#endif

	format_iso_time(curr->mtime, buf, sizeof(buf));
	print_item("Modified", buf, ctx);

	format_iso_time(curr->atime, buf, sizeof(buf));
	print_item("Accessed", buf, ctx);

	format_iso_time(curr->ctime, buf, sizeof(buf));
#ifndef _WIN32
	print_item("Changed", buf, ctx);
#else
	print_item("Created", buf, ctx);
#endif

	char perm_buf[26];

#ifndef _WIN32
	get_perm_string(perm_buf, sizeof(perm_buf), curr->mode);
	snprintf(buf, sizeof(buf), "%s (%03o)", perm_buf, curr->mode & 0777);
	print_item("Permissions", buf, ctx);

	char id_buf[26];

	get_uid_string(curr, 0, sizeof(id_buf), id_buf);
	if(isdigit(id_buf[0]))
	{
		copy_str(buf, sizeof(buf), id_buf);
	}
	else
	{
		snprintf(buf, sizeof(buf), "%s (%lu)", id_buf, (unsigned long)curr->uid);
	}
	print_item("Owner", buf, ctx);

	get_gid_string(curr, 0, sizeof(id_buf), id_buf);
	if(isdigit(id_buf[0]))
	{
		copy_str(buf, sizeof(buf), id_buf);
	}
	else
	{
		snprintf(buf, sizeof(buf), "%s (%lu)", id_buf, (unsigned long)curr->gid);
	}
	print_item("Group", buf, ctx);
#else
	copy_str(perm_buf, sizeof(perm_buf), attr_str_long(curr->attrs));
	print_item("Attributes", perm_buf, ctx);
#endif
}

/* Draws list of items on the screen. */
static void
draw_items(const draw_ctx_t *ctx)
{
	const int menu_width = getmaxx(menu_win);

	int i;
	int curr_y = 2;
	for(i = 0; i < ctx->lines.nitems; ++i)
	{
		const char *text = ctx->lines.items[i];
		const char *rhs = text + LHS_WIDTH;

		int x = 2 + (rhs - text);
		int max_width = menu_width - 2 - x;

		char part[1000];
		copy_str(part, MIN(sizeof(part), (size_t)(rhs - text + 1)), text);

		checked_wmove(menu_win, curr_y, 2);
		wprint(menu_win, part);

		text = rhs;

		do
		{
			const size_t print_len = utf8_nstrsnlen(text, max_width);

			copy_str(part, MIN(sizeof(part), print_len + 1), text);

			checked_wmove(menu_win, curr_y, x);
			wprint(menu_win, part);

			++curr_y;
			text += print_len;
		}
		while(text[0] != '\0');
	}
}

/* Prints item prefixed with a label wrapping the item if it's too long. */
static void
print_item(const char label[], const char text[], draw_ctx_t *ctx)
{
	char prefix[LHS_WIDTH + 1];
	snprintf(prefix, sizeof(prefix), "%s:", label);

	char *line = format_str("%-*s%s", LHS_WIDTH, prefix, text);
	ctx->lines.nitems =
		put_into_string_array(&ctx->lines.items, ctx->lines.nitems, line);
}

/* Adds suffix to the last item. */
static void
append_item(const char suffix[], draw_ctx_t *ctx)
{
	assert(ctx->lines.nitems > 0 && "Can't append to a nonexistent element!");

	char *old = ctx->lines.items[ctx->lines.nitems - 1];
	ctx->lines.items[ctx->lines.nitems - 1] = format_str("%s%s", old, suffix);
	free(old);
}

/* Prints type of the file and possibly some extra information about it. */
static void
show_file_type(view_t *view, draw_ctx_t *ctx)
{
	const dir_entry_t *curr;

	curr = get_current_entry(view);

	if(curr->type == FT_LINK || is_shortcut(curr->name))
	{
		print_link_info(curr, ctx);
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
		ShellType shell_type = (get_env_type() == ET_UNIX ? ST_POSIX : ST_CMD);
		escaped_full_path = shell_arg_escape(full_path, shell_type);
		snprintf(command, sizeof(command), "file %s -b", escaped_full_path);
		free(escaped_full_path);

		if((pipe = popen(command, "r")) == NULL)
		{
			print_item("Type", "Unable to open pipe to read file", ctx);
			return;
		}

		if(fgets(buf, sizeof(buf), pipe) != buf)
			strcpy(buf, "Pipe read error");

		pclose(pipe);

		print_item("Type", buf, ctx);
#else /* #ifdef HAVE_FILE_PROG */
		print_item("Type", (curr->type == FT_EXEC) ? "Executable" : "Regular File",
				ctx);
#endif /* #ifdef HAVE_FILE_PROG */
	}
	else if(curr->type == FT_DIR)
	{
		print_item("Type", "Directory", ctx);
	}
#ifndef _WIN32
	else if(curr->type == FT_CHAR_DEV || curr->type == FT_BLOCK_DEV)
	{
		const char *const type = (curr->type == FT_CHAR_DEV)
		                       ? "Character Device"
		                       : "Block Device";
		print_item("Type", type, ctx);

#if defined(major) && defined(minor)
		{
			char full_path[PATH_MAX + 1];
			struct stat st;
			get_current_full_path(view, sizeof(full_path), full_path);
			if(os_stat(full_path, &st) == 0)
			{
				char info[64];
				snprintf(info, sizeof(info), "0x%x:0x%x", major(st.st_rdev),
						minor(st.st_rdev));

				print_item("Device Id", info, ctx);
			}
		}
#endif
	}
	else if(curr->type == FT_SOCK)
	{
		print_item("Type", "Socket", ctx);
	}
#endif
	else if(curr->type == FT_FIFO)
	{
		print_item("Type", "Fifo Pipe", ctx);
	}
	else
	{
		print_item("Type", "Unknown", ctx);
	}
}

/* Prints information about a link entry. */
static void
print_link_info(const dir_entry_t *curr, draw_ctx_t *ctx)
{
	char full_path[PATH_MAX + 1];
	char linkto[PATH_MAX + NAME_MAX];

	get_full_path_of(curr, sizeof(full_path), full_path);

	const char *label;
	if(curr->type == FT_LINK)
	{
		print_item("Type", "Link", ctx);
		label = "Link To";
	}
	else
	{
		print_item("Type", "Shortcut", ctx);
		label = "Shortcut To";
	}

	if(get_link_target(full_path, linkto, sizeof(linkto)) == 0)
	{
		print_item(label, linkto, ctx);
		if(!path_exists(linkto, DEREF))
		{
			append_item(" (BROKEN)", ctx);
		}
	}
	else
	{
		print_item("Link", "Couldn't Resolve Link", ctx);
	}

	if(curr->type == FT_LINK)
	{
		char real[PATH_MAX + 1];
		if(os_realpath(full_path, real) == real)
		{
			print_item("Real Path", real, ctx);
		}
		else
		{
			print_item("Real Path", "Couldn't Resolve Path", ctx);
		}
	}
}

/* Prints mime-type of the file. */
static void
show_mime_type(view_t *view, draw_ctx_t *ctx)
{
	char full_path[PATH_MAX + 1];
	get_current_full_path(view, sizeof(full_path), full_path);

	const char *mimetype = get_mimetype(full_path, 0);
	if(mimetype == NULL)
	{
		mimetype = "Unknown";
	}

	print_item("Mime Type", mimetype, ctx);
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
