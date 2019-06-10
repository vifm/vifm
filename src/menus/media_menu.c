/* vifm
 * Copyright (C) 2018 xaizek.
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

#include "media_menu.h"

#include <stddef.h> /* NULL */
#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */
#include <wchar.h> /* wcscmp() */

#include "../cfg/config.h"
#include "../compat/fs_limits.h"
#include "../compat/reallocarray.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../ui/cancellation.h"
#include "../ui/statusbar.h"
#include "../utils/fs.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../utils/utils.h"
#include "../background.h"
#include "menus.h"

/* Example state of the menu:
 *
 *    Items                              Data            Action
 *
 *    /dev/sdc
 *     -- not mounted                    m/dev/sda3      mount /dev/sda3
 *
 *    /dev/sdd [label]
 *     -- /                              u/              unmount /
 *     -- /root/tmp                      u/root/tmp      unmount /root/tmp
 *
 * Additional keys:
 *  - r -- reload list
 *  - m -- mount/unmount
 *
 * Behaviour on Enter:
 *  - for lines with mount-point: navigate inside it
 *  - for all other lines: just close the menu
 */

/* Description of a single device. */
typedef struct
{
	char *device;   /* Device name in the system. */
	char *label;    /* Label of the device (can be NULL). */
	char **paths;   /* List of paths at which the device is mounted. */
	int path_count; /* Number of elements in the paths array. */
}
media_info_t;

static int execute_media_cb(struct view_t *view, menu_data_t *m);
static KHandlerResponse media_khandler(struct view_t *view, menu_data_t *m,
		const wchar_t keys[]);
static int reload_list(menu_data_t *m);
static void output_handler(const char line[], void *arg);
static void free_info_array(void);
static void position_cursor(menu_data_t *m);

/* List of media devices. */
static media_info_t *infos;
/* Number of elements in infos array. */
static int info_count;

int
show_media_menu(struct view_t *view)
{
	static menu_data_t m;
	menus_init_data(&m, view, strdup("Media"), strdup("No media found"));
	m.execute_handler = &execute_media_cb;
	m.key_handler = &media_khandler;

	if(reload_list(&m) != 0)
	{
		return 0;
	}

	position_cursor(&m);

	return menus_enter(m.state, view);
}

/* Callback that is called when menu item is selected.  Return non-zero to stay
 * in menu mode and zero otherwise. */
static int
execute_media_cb(struct view_t *view, menu_data_t *m)
{
	const char *data = m->data[m->pos];
	if(data != NULL && *data == 'u')
	{
		const char *path = m->data[m->pos] + 1;
		menus_goto_dir(view, path);
	}
	return 0;
}

/* Menu-specific shortcut handler.  Returns code that specifies both taken
 * actions and what should be done next. */
static KHandlerResponse
media_khandler(struct view_t *view, menu_data_t *m, const wchar_t keys[])
{
	if(wcscmp(keys, L"r") == 0)
	{
		reload_list(m);
		return KHR_REFRESH_WINDOW;
	}
	else if(wcscmp(keys, L"m") == 0)
	{
		const char *data = m->data[m->pos];
		if(is_null_or_empty(data) || (*data != 'm' && *data != 'u') ||
				cfg.media_prg[0] == '\0')
		{
			return KHR_REFRESH_WINDOW;
		}

		int mount = (*data == 'm');
		const char *path = data + 1;

		if(!mount)
		{
			/* Try to get out of mount point before trying to unmount it. */
			char cwd[PATH_MAX + 1];
			if(get_cwd(cwd, sizeof(cwd)) == cwd && is_in_subtree(cwd, path, 1))
			{
				char out_of_mount_path[PATH_MAX + 1];
				snprintf(out_of_mount_path, sizeof(out_of_mount_path), "%s/..", path);
				(void)vifm_chdir(out_of_mount_path);
			}
		}

		const char *action = (mount ? "mount" : "unmount");
		const char *description = (mount ? "Mounting" : "Unmounting");

		ui_sb_msgf("%s %s...", description, path);

		char *escaped_path = shell_like_escape(path, 0);
		char *cmd = format_str("%s %s %s", cfg.media_prg, action, escaped_path);
		if(bg_and_wait_for_errors(cmd, &ui_cancellation_info) == 0)
		{
			reload_list(m);
		}
		else
		{
			show_error_msgf("Media operation error", "%s has failed", description);
		}
		free(escaped_path);
		free(cmd);

		ui_sb_clear();

		menus_partial_redraw(m->state);
		return KHR_REFRESH_WINDOW;
	}
	return KHR_UNHANDLED;
}

/* (Re)loads list of media devices into the menu.  Returns zero on success,
 * otherwise non-zero is returned (empty menu is not considered an error). */
static int
reload_list(menu_data_t *m)
{
	free_string_array(m->data, m->len);
	free_string_array(m->items, m->len);

	m->data = NULL;
	m->items = NULL;
	m->len = 0;

	if(cfg.media_prg[0] == '\0')
	{
		show_error_msg("Listing media devices",
				"'mediaprg' option must not be empty");
		return 1;
	}

	char *cmd = format_str("%s list", cfg.media_prg);
	if(process_cmd_output("Listing media", cmd, 0, 0, &output_handler, &m) != 0)
	{
		free(cmd);
		show_error_msgf("Listing media devices", "Unable to run: %s", cmd);
		return 1;
	}
	free(cmd);

	int i;
	for(i = 0; i < info_count; ++i)
	{
		media_info_t *info = &infos[i];

		put_into_string_array(&m->data, m->len, NULL);
		if(is_null_or_empty(info->label))
		{
			m->len = add_to_string_array(&m->items, m->len, 1, info->device);
		}
		else
		{
			m->len = put_into_string_array(&m->items, m->len,
					format_str("%s [%s]", info->device, info->label));
		}

		if(info->path_count == 0)
		{
			put_into_string_array(&m->data, m->len, format_str("m%s", info->device));
			m->len = add_to_string_array(&m->items, m->len, 1, "  -- not mounted");
		}
		else
		{
			int j;
			for(j = 0; j < info->path_count; ++j)
			{
				put_into_string_array(&m->data, m->len,
						format_str("u%s", info->paths[j]));
				m->len = put_into_string_array(&m->items, m->len,
						format_str("  -- %s", info->paths[j]));
			}
		}

		if(i != info_count - 1)
		{
			put_into_string_array(&m->data, m->len, NULL);
			m->len = add_to_string_array(&m->items, m->len, 1, "");
		}
	}

	free_info_array();

	return 0;
}

/* Implements process_cmd_output() callback that builds array of devices. */
static void
output_handler(const char line[], void *arg)
{
	if(skip_prefix(&line, "device="))
	{
		infos = reallocarray(infos, ++info_count, sizeof(*infos));
		media_info_t *info = &infos[info_count - 1];
		info->device = strdup(line);
		info->label = NULL;
		info->paths = NULL;
		info->path_count = 0;
	}
	else if(skip_prefix(&line, "label=") && info_count != 0)
	{
		replace_string(&infos[info_count - 1].label, line);
	}
	else if(skip_prefix(&line, "mount-point=") && info_count != 0)
	{
		media_info_t *info = &infos[info_count - 1];
		info->path_count = add_to_string_array(&info->paths, info->path_count, 1,
				line);
	}
}

/* Releases global data that collects information about media. */
static void
free_info_array(void)
{
	int i;
	for(i = 0; i < info_count; ++i)
	{
		media_info_t *info = &infos[i];
		free(info->device);
		free(info->label);
		free_string_array(info->paths, info->path_count);
	}
	free(infos);
	infos = NULL;
	info_count = 0;
}

/* Puts cursor position to a mount point that corresponds to current path if
 * possible. */
static void
position_cursor(menu_data_t *m)
{
	int i;
	for(i = 0; i < m->len; ++i)
	{
		if(m->data[i] != NULL && *m->data[i] == 'u' &&
				is_in_subtree(m->cwd, m->data[i] + 1, 1))
		{
			m->pos = i;
			break;
		}
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
