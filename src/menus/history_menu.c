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
#include "../engine/mode.h"
#include "../modes/cmdline.h"
#include "../modes/menu.h"
#include "../modes/modes.h"
#include "../modes/normal.h"
#include "../ui/ui.h"
#include "../utils/hist.h"
#include "../utils/string_array.h"
#include "../cmd_core.h"
#include "menus.h"

/* Concrete type of displayed history. */
typedef enum
{
	CMDHISTORY,     /* Command history. */
	MENUCMDHISTORY, /* Command history of menus. */
	FSEARCHHISTORY, /* Forward search history. */
	BSEARCHHISTORY, /* Backward search history. */
	PROMPTHISTORY,  /* Prompt input history. */
	FILTERHISTORY,  /* Local filter history. */
	EXPRREGHISTORY, /* Expression register history. */
}
HistoryType;

static int show_history(view_t *view, HistoryType type, hist_t *hist,
		const char title[]);
static int execute_history_cb(view_t *view, menu_data_t *m);
static KHandlerResponse history_khandler(view_t *view, menu_data_t *m,
		const wchar_t keys[]);

int
show_cmdhistory_menu(view_t *view)
{
	return show_history(view, CMDHISTORY, &curr_stats.cmd_hist,
			"Command Line History");
}

int
show_menucmdhistory_menu(view_t *view)
{
	return show_history(view, MENUCMDHISTORY, &curr_stats.menucmd_hist,
			"Menu Command History");
}

int
show_fsearchhistory_menu(view_t *view)
{
	return show_history(view, FSEARCHHISTORY, &curr_stats.search_hist,
			"Search History");
}

int
show_bsearchhistory_menu(view_t *view)
{
	return show_history(view, BSEARCHHISTORY, &curr_stats.search_hist,
			"Search History");
}

int
show_prompthistory_menu(view_t *view)
{
	return show_history(view, PROMPTHISTORY, &curr_stats.prompt_hist,
			"Prompt History");
}

int
show_filterhistory_menu(view_t *view)
{
	return show_history(view, FILTERHISTORY, &curr_stats.filter_hist,
			"Filter History");
}

int
show_exprreghistory_menu(view_t *view)
{
	return show_history(view, EXPRREGHISTORY, &curr_stats.exprreg_hist,
			"Expression Register History");
}

/* Returns non-zero if status bar message should be saved. */
static int
show_history(view_t *view, HistoryType type, hist_t *hist, const char title[])
{
	static menu_data_t m;

	menus_init_data(&m, view, strdup(title), strdup("History disabled or empty"));
	m.execute_handler = &execute_history_cb;
	m.key_handler = &history_khandler;
	m.extra_data = type;

	int i;
	for(i = 0; i < hist->size; ++i)
	{
		m.len = add_to_string_array(&m.items, m.len, hist->items[i].text);
	}

	return menus_enter(m.state, view);
}

/* Callback that is invoked when menu item is selected.  Should return non-zero
 * to stay in menu mode. */
static int
execute_history_cb(view_t *view, menu_data_t *m)
{
	const char *const line = m->items[m->pos];

	switch((HistoryType)m->extra_data)
	{
		case MENUCMDHISTORY:
			hists_menucmd_save(line);
			/* This callback is called with mode set to "normal". */
			vle_mode_set(MENU_MODE, VMT_PRIMARY);
			curr_stats.save_msg = cmds_dispatch(line, view, CIT_MENU_COMMAND);
			/* Request staying in the menu only if the command hasn't left it. */
			return vle_mode_is(MENU_MODE);

		case CMDHISTORY:
			hists_commands_save(line);
			cmds_dispatch(line, view, CIT_COMMAND);
			break;
		case FSEARCHHISTORY:
			hists_search_save(line);
			modnorm_set_search_attrs(/*count=*/1, /*last_search_backward=*/0);
			cmds_dispatch1(line, view, CIT_FSEARCH_PATTERN);
			break;
		case BSEARCHHISTORY:
			hists_search_save(line);
			modnorm_set_search_attrs(/*count=*/1, /*last_search_backward=*/1);
			cmds_dispatch1(line, view, CIT_BSEARCH_PATTERN);
			break;
		case FILTERHISTORY:
			hists_filter_save(line);
			cmds_dispatch1(line, view, CIT_FILTER_PATTERN);
			break;

		case EXPRREGHISTORY:
		case PROMPTHISTORY:
			/* Can't replay prompt input. */
			break;
	}

	return 0;
}

/* Menu-specific shortcut handler.  Returns code that specifies both taken
 * actions and what should be done next. */
static KHandlerResponse
history_khandler(view_t *view, menu_data_t *m, const wchar_t keys[])
{
	if(wcscmp(keys, L"c") == 0)
	{
		/* Initialize to prevent possible compiler warnings. */
		CmdLineSubmode submode = CLS_COMMAND;
		switch((HistoryType)m->extra_data)
		{
			case MENUCMDHISTORY:
				submode = CLS_MENU_COMMAND;
				break;

			case CMDHISTORY:
				submode = CLS_COMMAND;
				break;
			case FSEARCHHISTORY:
				submode = CLS_FSEARCH;
				modnorm_set_search_attrs(/*count=*/1, /*last_search_backward=*/0);
				break;
			case BSEARCHHISTORY:
				submode = CLS_BSEARCH;
				modnorm_set_search_attrs(/*count=*/1, /*last_search_backward=*/1);
				break;
			case FILTERHISTORY:
				submode = CLS_FILTER;
				break;

			case EXPRREGHISTORY:
			case PROMPTHISTORY:
				/* Can't edit prompt input. */
				return KHR_UNHANDLED;
		}

		modmenu_morph_into_cline(submode, m->items[m->pos], 0);
		return KHR_MORPHED_MENU;
	}
	return KHR_UNHANDLED;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
