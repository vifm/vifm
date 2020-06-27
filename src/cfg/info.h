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

#ifndef VIFM__CFG__INFO_H__
#define VIFM__CFG__INFO_H__

#include "../utils/test_helpers.h"

/* Type of callback invoked after changing active session. */
typedef void (*sessions_changed)(const char new_session[]);

/* Reads vifminfo file populating internal structures with information it
 * contains.  Reread should be set to non-zero value when vifminfo is read not
 * during startup process. */
void state_load(int reread);

/* Stores state of the application.  Always writes vifminfo and stores session
 * if any is active. */
void state_store(void);

/* Sets callback to be invoked when active session has changed.  The parameter
 * can be NULL. */
void sessions_set_callback(sessions_changed callback);

/* Starts a new session, its creation is completed when its stored, until then
 * it exists only in memory.  Returns zero if it was started successfully,
 * otherwise non-zero is returned (when session with such name already
 * exists). */
int sessions_create(const char name[]);

/* Detaches from a session.  Returns zero after detaching and non-zero if no
 * session was active. */
int sessions_stop(void);

/* Retrieves name of the current session.  Returns the name or empty string. */
const char * sessions_current(void);

/* Checks whether session specified by the name is the current one.  Returns
 * non-zero if so, otherwise zero is returned. */
int sessions_current_is(const char name[]);

/* Checks whether a session is currently active.  Returns non-zero if so,
 * otherwise zero is returned. */
int sessions_active(void);

/* Checks whether a session exists.  Returns non-zero if so, otherwise zero is
 * returned. */
int sessions_exists(const char name[]);

/* Loads session.  This includes loading of vifminfo, so call either this one
 * or state_load().  Returns zero on success, otherwise non-zero is returned. */
int sessions_load(const char name[]);

/* Deletes a session.  Returns zero on success and non-zero if something went
 * wrong. */
int sessions_remove(const char name[]);

/* Performs completion of session names. */
void sessions_complete(const char prefix[]);

#ifdef TEST
#include "../utils/parson.h"
#endif
TSTATIC_DEFS(
	void write_info_file(void);
	char * drop_locale(void);
	void restore_locale(char locale[]);
	JSON_Value * serialize_state(int vinfo);
	void merge_states(int vinfo, int session_load, JSON_Object *current,
		const JSON_Object *admixture);
)

#endif /* VIFM__CFG__INFO_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
