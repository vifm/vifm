/* vifm
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

#include "version.h"

#include <curses.h> /* COLORS COLOR_PAIRS */

#include <assert.h> /* assert() */
#include <stddef.h> /* NULL */
#include <string.h> /* strdup() */

#include "utils/str.h"
#include "utils/utils.h"
#include "status.h"
#include "vcache.h"

/* This variable is automatically updated during build. */
extern const char GIT_INFO[];

int
fill_version_info(char **list, int include_stats)
{
	const int LEN = 22;
	int x = 0;

	if(list == NULL)
	{
		return LEN;
	}

	list[x++] = strdup("Version: " VERSION);
	list[x++] = format_str("Git info: %s", GIT_INFO);
#ifdef WITH_BUILD_TIMESTAMP
	list[x++] = strdup("Compiled at: " __DATE__ " " __TIME__);
#endif
	list[x++] = strdup("");

#ifdef ENABLE_EXTENDED_KEYS
	list[x++] = strdup("Support of extended keys is on");
#else
	list[x++] = strdup("Support of extended keys is off");
#endif

#ifdef ENABLE_DESKTOP_FILES
	list[x++] = strdup("Parsing of .desktop files is enabled");
#else
	list[x++] = strdup("Parsing of .desktop files is disabled");
#endif

#ifdef HAVE_LIBGTK
	list[x++] = strdup("With GTK+ library");
#else
	list[x++] = strdup("Without GTK+ library");
#endif

#ifdef HAVE_LIBMAGIC
	list[x++] = strdup("With magic library");
#else
	list[x++] = strdup("Without magic library");
#endif

#ifdef HAVE_X11
	list[x++] = strdup("With X11 library");
#else
	list[x++] = strdup("Without X11 library");
#endif

#ifdef DYN_X11
	list[x++] = strdup("With dynamic loading of X11 library");
#else
	list[x++] = strdup("Without dynamic loading of X11 library");
#endif

#ifdef HAVE_FILE_PROG
	list[x++] = strdup("With file program");
#else
	list[x++] = strdup("Without file program");
#endif

#ifdef SUPPORT_NO_CLOBBER
	list[x++] = strdup("With -n option for cp and mv");
#else
#ifndef _WIN32
	list[x++] = strdup("With -n option for cp and mv");
#endif /* _WIN32 */
#endif

#ifdef ENABLE_REMOTE_CMDS
	list[x++] = strdup("With remote command execution");
#else
	list[x++] = strdup("Without remote command execution");
#endif

	if(include_stats)
	{
		char size[64];
		(void)friendly_size_notation(vcache_size(), sizeof(size), size);

		list[x++] = strdup("");
#ifndef _WIN32
		list[x++] = format_str("Terminal name: %s", curr_stats.term_name);
#endif
		list[x++] = format_str("Max colors: %d", COLORS);
		list[x++] = format_str("Max color pairs: %d", COLOR_PAIRS);
#ifdef HAVE_EXTENDED_COLORS
		list[x++] = strdup("Extended colors: present");
#else
		list[x++] = strdup("Extended colors: missing");
#endif
#ifndef _WIN32
		list[x++] = format_str("RGB: %d", tigetflag("RGB"));
#endif
		list[x++] = format_str("Direct color: %s",
				curr_stats.direct_color ? "yes" : "no");

		list[x++] = strdup("");
		list[x++] = format_str("Preview cache size: %s", size);
	}

	assert(x <= LEN);

	return x;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
