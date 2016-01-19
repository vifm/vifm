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

#include "history_menu.h"

#include <string.h> /* strdup() */

#include "../cfg/config.h"
#include "../cfg/hist.h"
#include "../modes/menu.h"
#include "../ui/ui.h"
#include "../utils/string_array.h"
#include "../cmd_core.h"
#include "menus.h"

/* Concrete type of displayed history. */
typedef enum
{
	CMDHISTORY,     /* Command history. */
	FSEARCHHISTORY, /* Forward search history. */
	BSEARCHHISTORY, /* Backward search history. */
	PROMPTHISTORY,  /* Prompt input history. */
	FILTERHISTORY,  /* Local filter history. */
}
HistoryType;

static int show_history(FileView *view, HistoryType type, hist_t *hist,
		const char title[]);
static int execute_history_cb(FileView *view, menu_info *m);

int
show_cmdhistory_menu(FileView *view)
{
	return show_history(view, CMDHISTORY, &cfg.cmd_hist, "Command Line History");
}

int
show_fsearchhistory_menu(FileView *view)
{
	return show_history(view, FSEARCHHISTORY, &cfg.search_hist, "Search History");
}

int
show_bsearchhistory_menu(FileView *view)
{
	return show_history(view, BSEARCHHISTORY, &cfg.search_hist, "Search History");
}

int
show_prompthistory_menu(FileView *view)
{
	return show_history(view, PROMPTHISTORY, &cfg.prompt_hist, "Prompt History");
}

int
show_filterhistory_menu(FileView *view)
{
	return show_history(view, FILTERHISTORY, &cfg.filter_hist, "Filter History");
}

/* Returns non-zero if status bar message should be saved. */
static int
show_history(FileView *view, HistoryType type, hist_t *hist, const char title[])
{
	int i;
	static menu_info m;

	init_menu_info(&m, strdup(title), strdup("History disabled or empty"));
	m.execute_handler = &execute_history_cb;
	m.extra_data = type;

	for(i = 0; i <= hist->pos; ++i)
	{
		m.len = add_to_string_array(&m.items, m.len, 1, hist->items[i]);
	}

	return display_menu(&m, view);
}

/* Callback that is invoked when menu item is selected.  Should return non-zero
 * to stay in menu mode. */
static int
execute_history_cb(FileView *view, menu_info *m)
{
	const char *const line = m->items[m->pos];

	switch((HistoryType)m->extra_data)
	{
		case CMDHISTORY:
			cfg_save_command_history(line);
			exec_commands(line, view, CIT_COMMAND);
			break;
		case FSEARCHHISTORY:
			cfg_save_search_history(line);
			exec_commands(line, view, CIT_FSEARCH_PATTERN);
			break;
		case BSEARCHHISTORY:
			cfg_save_search_history(line);
			exec_commands(line, view, CIT_BSEARCH_PATTERN);
			break;
		case FILTERHISTORY:
			cfg_save_filter_history(line);
			exec_commands(line, view, CIT_FILTER_PATTERN);
			break;
		case PROMPTHISTORY:
			/* Can't replay prompt input. */
			break;
	}

	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
