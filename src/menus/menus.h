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

#ifndef VIFM__MENUS__MENUS_H__
#define VIFM__MENUS__MENUS_H__

#include <stddef.h> /* wchar_t */

#include "../utils/test_helpers.h"
#include "../macros.h"

struct view_t;

/* Result of handling key sequence by menu-specific shortcut handler. */
typedef enum
{
	KHR_REFRESH_WINDOW, /* Menu window refresh is needed. */
	KHR_CLOSE_MENU,     /* Menu mode should be left. */
	KHR_MORPHED_MENU,   /* Menu was morphed modmenu_morph_into_cline. */
	KHR_UNHANDLED,      /* Passed key wasn't handled. */
}
KHandlerResponse;

/* Opaque declaration of structure describing menu state. */
typedef struct menu_state_t menu_state_t;

/* Menu data related to specific menu rather than to state of menu mode or its
 * UI. */
typedef struct menu_data_t
{
	int top;      /* Index of first visible item. */
	int len;      /* Number of menu items. */
	int pos;      /* Menu item under the cursor. */
	int hor_pos;  /* Horizontal offset. */

	char *title;  /* Title of the menu. */
	char **items; /* Contains titles of all menu items. */

	/* Contains additional string data, associated with each of menu items, can be
	 * NULL. */
	char **data;

	/* Contains additional pointers for each menu entry, can be NULL. */
	void **void_data;

	/* Menu-specific shortcut handler, can be NULL.  Returns code that specifies
	 * both taken actions and what should be done next. */
	KHandlerResponse (*key_handler)(struct view_t *view, struct menu_data_t *m,
			const wchar_t keys[]);

	/* Callback that is called when menu item is selected.  Should return non-zero
	 * to stay in menu mode. */
	int (*execute_handler)(struct view_t *view, struct menu_data_t *m);

	/* Text displayed by menus_enter() function in case menu is empty, it can be
	 * NULL if this cannot happen. */
	char *empty_msg;

	/* Base for relative paths for navigation. */
	char *cwd;

	/* For filetype background, mime flags and such. */
	int extra_data;

	/* Whether this menu when non-empty should be saved for future use on closing
	 * menu. */
	int stashable;
	/* Whether selecting an item should keep menu mode active while running
	 * execute_handler. */
	int menu_context;

	menu_state_t *state; /* Opaque pointer to menu mode state. */
	int initialized;     /* Marker that shows whether menu data needs freeing. */
}
menu_data_t;

/* Menu data management. */

/* Fills fields of menu_data_t structure with some safe values.  empty_msg is
 * text displayed by menus_enter() function in case menu is empty, it can be
 * NULL if this cannot happen and will be freed by menus_reset_data(). */
void menus_init_data(menu_data_t *m, struct view_t *view, char title[],
		char empty_msg[]);

/* Frees resources associated with the menu and clears menu window. */
void menus_reset_data(menu_data_t *m);

/* Menu entering/reentering and transformation. */

/* Prepares menu, draws it and switches to the menu mode.  If a menu mode is
 * already active, switch to the new menu stashing the current one if necessary.
 * Returns non-zero if status bar message should be saved. */
int menus_enter(menu_data_t *m, struct view_t *view);

/* Replaces menu of the menu mode. */
void menus_switch_to(menu_data_t *m);

/* Restore previously saved menu.  Returns non-zero if status bar message should
 * be saved. */
int menus_unstash(struct view_t *view);

/* Loads an older stash after possibly stashing current menu.  Returns zero on
 * sucess or non-zero if there is no older stash. */
int menus_unstash_older(menu_state_t *ms);

/* Loads a newer menu stash.  Returns zero on sucess or non-zero if there is no
 * such stash. */
int menus_unstash_newer(menu_state_t *ms);

/* Loads menu from a stash by its index. */
void menus_unstash_at(menu_state_t *ms, int index);

/* Retrieves stashed menu by its index (from 0).  Sets *current if that's the
 * last viewed menu.  Returns NULL on invalid index. */
const menu_data_t * menus_get_stash(int index, int *current);

/* Moves menu items into custom view.  Returns zero on success, otherwise
 * non-zero is returned. */
int menus_to_custom_view(menu_state_t *ms, struct view_t *view, int very);

/* Either makes a menu or custom view out of command output.  Returns non-zero
 * if status bar message should be saved. */
int menus_capture(struct view_t *view, const char cmd[], int user_sh,
		menu_data_t *m, MacroFlags flags);

/* Menu drawing. */

/* Erases current menu item in menu window. */
void menus_erase_current(menu_state_t *ms);

/* Redraws all screen elements used by menus. */
void menus_full_redraw(menu_state_t *ms);

/* Redraws only menu list itself. */
void menus_partial_redraw(menu_state_t *ms);

/* Menu operations. */

/* Updates current position in the menu. */
void menus_set_pos(menu_state_t *ms, int pos);

/* Removes current menu item and redraws the menu. */
void menus_remove_current(menu_state_t *ms);

/* Navigates to/open path specification.  Specification can contain colon
 * followed by a line number when try_open is not zero.  Returns zero on
 * successful parsing and performed try to handle the file otherwise non-zero is
 * returned. */
int menus_goto_file(menu_data_t *m, struct view_t *view, const char spec[],
		int try_open);

/* Navigates to directory from a menu. */
void menus_goto_dir(struct view_t *view, const char path[]);

/* Menu search. */

/* Performs search of pattern among menu items.  NULL pattern requests use of
 * the last used pattern.  Returns new value for save_msg flag, but when
 * print_errors isn't requested can return -1 to indicate issues with the
 * pattern. */
int menus_search(const char pattern[], menu_data_t *m, int print_errors);

/* Resets search state of the menu according to specified parameters. */
void menus_search_reset(menu_state_t *ms, int backward, int new_repeat_count);

/* Reset search highlight of a menu. */
void menus_search_reset_hilight(menu_state_t *ms);

/* Performs search in requested direction.  Either continues the previous one or
 * restarts it. */
void menus_search_repeat(menu_state_t *ms, int backward);

/* Prints results or error message about search operation to the user. */
void menus_search_print_msg(const menu_data_t *m);

/* Retrieves number of search matches in the menu.  Returns the number. */
int menus_search_matched(const menu_data_t *m);

/* Auxiliary functions related to menus. */

/* Forms list of target files/directories in the current view and possibly
 * changes working directory to use relative paths.  On success returns newly
 * allocated string, which should be freed by the caller, otherwise NULL is
 * returned. */
char * menus_get_targets(struct view_t *view);

/* Predefined key handler for processing keys on elements of file lists.
 * Returns code that specifies both taken actions and what should be done
 * next. */
KHandlerResponse menus_def_khandler(struct view_t *view, menu_data_t *m,
		const wchar_t keys[]);

/* Formats menu's title as it's drawn on the frame.  Returns newly allocated
 * string. */
char * menus_format_title(const menu_data_t *m, struct view_t *view);

TSTATIC_DEFS(
	void menus_drop_stash(void);
	void menus_set_active(menu_data_t *m);
)

#endif /* VIFM__MENUS__MENUS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
