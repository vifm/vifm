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

#include "utils_win.h"

#include <windows.h>
#include <winioctl.h>

#include <curses.h>

#include <fcntl.h>

#include <ctype.h> /* toupper() */
#include <stddef.h> /* size_t */
#include <string.h> /* strcat() strchr() strcpy() strlen() */
#include <stdio.h> /* snprintf() */

#include "../cfg/config.h"
#include "env.h"
#include "fs.h"
#include "fs_limits.h"
#include "log.h"
#include "macros.h"
#include "str.h"

static const char PATHEXT_EXT_DEF[] = ".bat;.exe;.com";

int
my_system_no_cls(char command[])
{
	char buf[strlen(cfg.shell) + 5 + strlen(command)*4 + 1 + 1];

	if(stroscmp(cfg.shell, "cmd") == 0)
	{
		snprintf(buf, sizeof(buf), "%s /C \"%s\"", cfg.shell, command);
		return system(buf);
	}
	else
	{
		char *p;
		int returned_exit_code;

		strcpy(buf, cfg.shell);
		strcat(buf, " -c '");

		p = buf + strlen(buf);
		while(*command != '\0')
		{
			if(*command == '\\')
				*p++ = '\\';
			*p++ = *command++;
		}
		*p = '\0';

		strcat(buf, "'");
		return win_exec_cmd(buf, &returned_exit_code);
	}
}

int
is_on_slow_fs(const char full_path[])
{
	return 0;
}

unsigned int
get_pid(void)
{
	return GetCurrentProcessId();
}

int
wcwidth(wchar_t c)
{
	return 1;
}

int
wcswidth(const wchar_t str[], size_t max_len)
{
	const size_t wcslen_result = wcslen(str);
	return MIN(max_len, wcslen_result);
}

int
win_exec_cmd(char cmd[], int *const returned_exit_code)
{
	BOOL ret;
	DWORD exit_code;
	STARTUPINFO startup = {};
	PROCESS_INFORMATION pinfo;

	*returned_exit_code = 0;

	ret = CreateProcessA(NULL, cmd, NULL, NULL, 0, 0, NULL, NULL, &startup,
			&pinfo);
	if(ret == 0)
	{
		const DWORD last_error = GetLastError();
		LOG_WERROR(last_error);
		return last_error;
	}

	CloseHandle(pinfo.hThread);

	if(WaitForSingleObject(pinfo.hProcess, INFINITE) != WAIT_OBJECT_0)
	{
		const DWORD last_error = GetLastError();
		LOG_WERROR(last_error);

		CloseHandle(pinfo.hProcess);
		return last_error;
	}
	if(GetExitCodeProcess(pinfo.hProcess, &exit_code) == 0)
	{
		const DWORD last_error = GetLastError();
		LOG_WERROR(last_error);

		CloseHandle(pinfo.hProcess);
		return last_error;
	}
	CloseHandle(pinfo.hProcess);
	*returned_exit_code = 1;
	return exit_code;
}

static void
strtoupper(char *s)
{
	while(*s != '\0')
	{
		*s = toupper(*s);
		s++;
	}
}

int
win_executable_exists(const char path[])
{
	const char *p;
	char path_buf[NAME_MAX];
	size_t pos;

	if(strchr(after_last(path, '/'), '.') != NULL)
	{
		return path_exists(path);
	}

	snprintf(path_buf, sizeof(path_buf), "%s", path);
	pos = strlen(path_buf);

	p = env_get_def("PATHEXT", PATHEXT_EXT_DEF);
	while((p = extract_part(p, ';', path_buf + pos)) != NULL)
	{
		if(path_exists(path_buf))
		{
			return 1;
		}
	}
	return 0;
}

int
is_win_executable(const char name[])
{
	const char *p;
	char name_buf[NAME_MAX];
	char ext_buf[16];

	snprintf(name_buf, sizeof(name_buf), "%s", name);
	strtoupper(name_buf);

	p = env_get_def("PATHEXT", PATHEXT_EXT_DEF);
	while((p = extract_part(p, ';', ext_buf)) != NULL)
	{
		strtoupper(ext_buf);
		if(ends_with(name_buf, ext_buf))
		{
			return 1;
		}
	}
	return 0;
}

int
is_vista_and_above(void)
{
	DWORD v = GetVersion();
	return ((v & 0xff) >= 6);
}

const char *
attr_str(DWORD attr)
{
	static char buf[5 + 1];
	buf[0] = '\0';
	if(attr & FILE_ATTRIBUTE_ARCHIVE)
		strcat(buf, "A");
	if(attr & FILE_ATTRIBUTE_HIDDEN)
		strcat(buf, "H");
	if(attr & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED)
		strcat(buf, "I");
	if(attr & FILE_ATTRIBUTE_READONLY)
		strcat(buf, "R");
	if(attr & FILE_ATTRIBUTE_SYSTEM)
		strcat(buf, "S");

	return buf;
}

const char *
attr_str_long(DWORD attr)
{
	static char buf[10 + 1];
	snprintf(buf, sizeof(buf), "ahirscdepz");
	buf[0] ^= ((attr & FILE_ATTRIBUTE_ARCHIVE) != 0)*0x20;
	buf[1] ^= ((attr & FILE_ATTRIBUTE_HIDDEN) != 0)*0x20;
	buf[2] ^= ((attr & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED) != 0)*0x20;
	buf[3] ^= ((attr & FILE_ATTRIBUTE_READONLY) != 0)*0x20;
	buf[4] ^= ((attr & FILE_ATTRIBUTE_SYSTEM) != 0)*0x20;
	buf[5] ^= ((attr & FILE_ATTRIBUTE_COMPRESSED) != 0)*0x20;
	buf[6] ^= ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0)*0x20;
	buf[7] ^= ((attr & FILE_ATTRIBUTE_ENCRYPTED) != 0)*0x20;
	buf[8] ^= ((attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0)*0x20;
	buf[9] ^= ((attr & FILE_ATTRIBUTE_SPARSE_FILE) != 0)*0x20;

	return buf;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
