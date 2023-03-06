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

#ifndef VIFM__SEARCH_H__
#define VIFM__SEARCH_H__

struct view_t;

/* Search and navigation functions. */

/* Callback for moving cursor to pos and redrawing both the cursor and a
 * view. */
typedef void (*move_cursor_and_redraw_cb)(int pos);

/* Searches pattern in view, moves cursor to count's match using cb and
 * optionally (if print_errors is set) shows a message.  stash_selection and
 * select_matches are passed to search_pattern().  Sets *found to non-zero if
 * pattern was found, otherwise it's assigned zero.  Returns non-zero when a
 * message was printed (or would have been printed with print_errors set) to a
 * user, otherwise zero is returned.  Returned value is negative for invalid
 * pattern. */
int search_find(struct view_t *view, const char pattern[], int backward,
		int stash_selection, int select_matches, int count,
		move_cursor_and_redraw_cb cb, int print_errors, int *found);

/* Moves cursor to count's match using cb and shows a message.  Searches for
 * the last pattern from the search history, if there are no matches in view.
 * stash_selection and select_matches are passed to search_pattern().  Returns
 * non-zero when a message was printed to a user, otherwise zero is returned. */
int search_next(struct view_t *view, int backward, int stash_selection,
		int select_matches, int count, move_cursor_and_redraw_cb cb);

/* Searches pattern in view.  Does nothing in case pattern is empty.
 * stash_selection stashes selection before the search.  select_matches selects
 * found matches.  Returns non-zero for invalid pattern, otherwise zero is
 * returned. */
int search_pattern(struct view_t *view, const char pattern[],
		int stash_selection, int select_matches);

/* Looks for a count's search match in specified direction from current cursor
 * position taking search wrapping into account.  Returns non-zero if something
 * was found, otherwise zero is returned. */
int goto_search_match(struct view_t *view, int backward, int count,
		move_cursor_and_redraw_cb cb);

/* Looks for a count's search match in specified direction from current cursor
 * position taking search wrapping into account.  Returns index of a match, or
 * -1 if no matches were found. */
int find_search_match(struct view_t *view, int backward, int count);

/* Auxiliary functions. */

/* Callback for showing a message about search operation. */
typedef void (*print_search_msg_cb)(const struct view_t *view,
		int backward);

/* Prints success or error message, determined by the found argument.  Supposed
 * to be called after search_pattern() and the cursor positioned over a match.
 * The success message is prtined by cb.  Takes search highlighting, wrapping
 * and visual mode into account.  Returns non-zero if something was printed,
 * otherwise zero is returned. */
int print_search_result(const struct view_t *view, int found, int backward,
		print_search_msg_cb cb);

/* Prints results or error message about search operation to the user. */
void print_search_msg(const struct view_t *view, int backward);

/* Prints error message about failed search to the user. */
void print_search_fail_msg(const struct view_t *view, int backward);

/* Resets information about last search match. */
void reset_search_results(struct view_t *view);

/* Prints the search messages for the n or N commands. */
void print_search_next_msg(const struct view_t *view, int backward);

#endif /* VIFM__SEARCH_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
