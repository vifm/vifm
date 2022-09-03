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
#include "../ui/ui.h"
#include "../utils/fs.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../utils/utils.h"
#include "../running.h"
#include "menus.h"

/* Example state of the menu:
 *
 *    Items                              Data            Action
 *
 *    /dev/sdc                                           mount /dev/sda3
 *    `-- (not mounted)                  m/dev/sda3      mount /dev/sda3
 *
 *    /dev/sdd {text}
 *    |-- /                              u/              unmount /
 *    `-- /root/tmp                      u/root/tmp      unmount /root/tmp
 *
 *    /dev/sde                                           unmount /media
 *    `-- /media                         u/media         unmount /media
 *
 * Additional keys:
 *  - r -- reload list
 *  - [ -- previous device
 *  - ] -- next device
 *  - m -- mount/unmount
 *
 * Behaviour on Enter:
 *  - navigate inside mount-point or,
 *  - mount device if there are no mount-points or,
 *  - do nothing and keep menu open
 */

/* Description of a single device. */
typedef struct
{
	char *device;   /* Device name in the system. */
	char *text;     /* Arbitrary text next to device name (can be NULL). */
	char **paths;   /* List of paths at which the device is mounted. */
	int path_count; /* Number of elements in the paths array. */
	int has_info;   /* Set when text is set via info=. */
}
media_info_t;

static int execute_media_cb(struct view_t *view, menu_data_t *m);
static KHandlerResponse media_khandler(struct view_t *view, menu_data_t *m,
		const wchar_t keys[]);
static int reload_list(menu_data_t *m);
static void output_handler(const char line[], void *arg);
static void free_info_array(void);
static void position_cursor(menu_data_t *m);
static const char * get_selected_data(menu_data_t *m);
static void path_get_decors(const char path[], FileType type,
		const char **prefix, const char **suffix);
static int mediaprg_mount(const char data[], menu_data_t *m);

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
	const char *data = get_selected_data(m);
	if(data != NULL)
	{
		if(*data == 'u')
		{
			const char *path = data + 1;
			menus_goto_dir(view, path);
			return 0;
		}
		else if(*data == 'm')
		{
			(void)mediaprg_mount(data, m);
		}
	}
	return 1;
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
	else if(wcscmp(keys, L"[") == 0)
	{
		int i = m->pos;
		while(i-- > 0)
		{
			if(m->data[i] == NULL && m->data[i + 1] != NULL)
			{
				menus_set_pos(m->state, i);
				menus_partial_redraw(m->state);
				break;
			}
		}
		return KHR_REFRESH_WINDOW;
	}
	else if(wcscmp(keys, L"]") == 0)
	{
		int i = m->pos;
		while(++i < m->len - 1)
		{
			if(m->data[i] == NULL && m->data[i + 1] != NULL)
			{
				menus_set_pos(m->state, i);
				menus_partial_redraw(m->state);
				break;
			}
		}
		return KHR_REFRESH_WINDOW;
	}
	else if(wcscmp(keys, L"m") == 0)
	{
		const char *data = get_selected_data(m);
		(void)mediaprg_mount(data, m);
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
	if(process_cmd_output("Listing media", cmd, NULL, 0, 0, &output_handler,
				&m) != 0)
	{
		show_error_msgf("Listing media devices", "Unable to run: %s", cmd);
		free(cmd);
		return 1;
	}
	free(cmd);

	int i;
	/* The NULL check is for tests. */
	const int menu_rows = (menu_win != NULL ? (getmaxy(menu_win) + 1) - 3 : 100);
	int max_rows_with_blanks = info_count       /* Device lines. */
		                       + (info_count - 1) /* Blank lines. */;
	for(i = 0; i < info_count; ++i)
	{
		max_rows_with_blanks += infos[i].path_count > 0
		                      ? infos[i].path_count /* Mount-points. */
		                      : 1;                  /* "Not mounted" line. */
	}

	for(i = 0; i < info_count; ++i)
	{
		const char *prefix, *suffix;
		media_info_t *info = &infos[i];

		put_into_string_array(&m->data, m->len, NULL);

		path_get_decors(info->device, FT_BLOCK_DEV, &prefix, &suffix);
		m->len = put_into_string_array(&m->items, m->len,
				format_str("%s%s%s %s", prefix, info->device, suffix,
						info->text ? info->text : ""));

		if(info->path_count == 0)
		{
			put_into_string_array(&m->data, m->len, format_str("m%s", info->device));
			m->len = add_to_string_array(&m->items, m->len, "`-- (not mounted)");
		}
		else
		{
			int j;
			for(j = 0; j < info->path_count; ++j)
			{
				put_into_string_array(&m->data, m->len,
						format_str("u%s", info->paths[j]));

				path_get_decors(info->paths[j], FT_DIR, &prefix, &suffix);
				const char *path = (is_root_dir(info->paths[j]) && suffix[0] == '/')
				                 ? ""
				                 : info->paths[j];
				m->len = put_into_string_array(&m->items, m->len,
						format_str("%c-- %s%s%s", j + 1 == info->path_count ? '`' : '|',
								prefix, path, suffix));
			}
		}

		if(i != info_count - 1 && max_rows_with_blanks <= menu_rows)
		{
			put_into_string_array(&m->data, m->len, NULL);
			m->len = add_to_string_array(&m->items, m->len, "");
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
		media_info_t *new_infos = reallocarray(infos, ++info_count, sizeof(*infos));
		if(new_infos != NULL)
		{
			infos = new_infos;
			media_info_t *info = &infos[info_count - 1];
			info->device = strdup(line);
			info->text = NULL;
			info->paths = NULL;
			info->path_count = 0;
			info->has_info = 0;
		}
	}
	else if(info_count > 0)
	{
		media_info_t *info = &infos[info_count - 1];
		if(skip_prefix(&line, "info="))
		{
			replace_string(&info->text, line);
			info->has_info = 1;
		}
		else if(skip_prefix(&line, "label=") && !info->has_info)
		{
			put_string(&info->text, format_str("[%s]", line));
		}
		else if(skip_prefix(&line, "mount-point="))
		{
			info->path_count = add_to_string_array(&info->paths, info->path_count,
					line);
		}
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
		free(info->text);
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

/* Retrieves decorations for path.  See ui_get_decors(). */
static void
path_get_decors(const char path[], FileType type, const char **prefix,
		const char **suffix)
{
	dir_entry_t entry;

	entry.name = get_last_path_component(path);
	/* Avoid memory allocation. */
	if(entry.name != path)
	{
		entry.name[-1] = '\0';
		entry.origin = entry.name;
	}
	else
	{
		entry.origin = (type == FT_BLOCK_DEV ? "/dev" : "/");
	}
	entry.type = type;
	entry.name_dec_num = -1;

	ui_get_decors(&entry, prefix, suffix);

	/* Restore original path. */
	if(entry.name != path)
	{
		entry.name[-1] = '/';
	}
}

/* Retrieves appropriate data for selected item in a human-friendly way. */
static const char *
get_selected_data(menu_data_t *m)
{
	const char *data = m->data[m->pos];
	if(data == NULL)
	{
		char *next_data      = (m->pos + 1 < m->len ? m->data[m->pos + 1] : NULL);
		char *next_next_data = (m->pos + 2 < m->len ? m->data[m->pos + 2] : NULL);
		if(next_data != NULL && next_next_data == NULL)
		{
			data = next_data;
		}
	}
	return data;
}

/* Executes "mount" or "unmount" command on data.  Returns non-zero if program
 * executed successfully and zero otherwise. */
static int
mediaprg_mount(const char data[], menu_data_t *m)
{
	if(data == NULL || (*data != 'm' && *data != 'u') || cfg.media_prg[0] == '\0')
	{
		return 1;
	}

	int error = 0;
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

	char *escaped_path = shell_like_escape(path, 0);
	char *cmd = format_str("%s %s %s", cfg.media_prg, action, escaped_path);
	if(rn_shell(cmd, PAUSE_NEVER, 0, SHELL_BY_APP) == 0)
	{
		reload_list(m);
	}
	else
	{
		error = 1;
		show_error_msgf("Media operation error", "%s has failed", description);
	}
	free(escaped_path);
	free(cmd);

	menus_partial_redraw(m->state);
	return error;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
