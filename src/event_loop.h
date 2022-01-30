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

#ifndef VIFM__EVENT_LOOP_H__
#define VIFM__EVENT_LOOP_H__

#include "utils/test_helpers.h"

/* Everything is driven from this function with the exception of signals which
 * are handled in signals.c.  It is reentrant so remote pieces of code can run
 * nested event loops. */
void event_loop(const int *quit, int manage_marking);

void update_input_buf(void);

int is_input_buf_empty(void);

TSTATIC_DEFS(
	struct view_t;
	int process_scheduled_updates_of_view(struct view_t *view);
	void feed_keys(const wchar_t input[]);
)

#endif /* VIFM__EVENT_LOOP_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
