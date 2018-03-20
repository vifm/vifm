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

/* The move argument specifies whether cursor in the view should be adjusted to
 * point to just found file in case of successful search.  Sets *found to
 * non-zero if pattern was found, otherwise it's assigned zero.  print_errors
 * means user needs feedback, otherwise it can be provided later using one of
 * print_*_msg() functions.  Returns non-zero when a message was printed (or
 * would have been printed with print_errors set) to a user, otherwise zero is
 * returned.  Returned value is negative for incorrect pattern. */
int find_pattern(struct view_t *view, const char pattern[], int backward,
		int move, int *found, int print_errors);

/* Looks for a search match in specified direction from current cursor position
 * taking search wrapping into account.  Returns non-zero if something was
 * found, otherwise zero is returned. */
int goto_search_match(struct view_t *view, int backward);

/* Auxiliary functions. */

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
