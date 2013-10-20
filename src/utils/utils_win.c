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
#include "utils_int.h"

#include <windows.h>
#include <winioctl.h>

#include <curses.h>

#include <fcntl.h>

#include <ctype.h> /* toupper() */
#include <stddef.h> /* size_t */
#include <stdint.h> /* uint32_t */
#include <string.h> /* strcat() strchr() strcpy() strlen() */
#include <stdio.h> /* FILE SEEK_SET fopen() fread() fclose() snprintf() */

#include "../cfg/config.h"
#include "../commands_completion.h"
#include "env.h"
#include "fs.h"
#include "fs_limits.h"
#include "log.h"
#include "macros.h"
#include "str.h"

#define PE_HDR_SIGNATURE 0x00004550U
#define PE_HDR_OFFSET 0x3cU
#define PE_HDR_SUBSYSTEM_OFFSET 0x5cU
#define SUBSYSTEM_GUI 2

static const char PATHEXT_EXT_DEF[] = ".bat;.exe;.com";

static int should_wait_for_program(const char cmd[]);
static DWORD handle_process(const char cmd[], HANDLE proc, int *got_exit_code);
static int get_subsystem(const char filename[]);
static int get_stream_subsystem(FILE *fp);

void
pause_shell(void)
{
	if(stroscmp(cfg.shell, "cmd") == 0)
	{
		run_in_shell_no_cls("pause");
	}
	else
	{
		run_in_shell_no_cls(PAUSE_CMD);
	}
}

int
run_in_shell_no_cls(char command[])
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

void
recover_after_shellout(void)
{
	reset_prog_mode();
	resize_term(cfg.lines, cfg.columns);
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
	DWORD code;
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

	code = handle_process(cmd, pinfo.hProcess, returned_exit_code);
	if(!*returned_exit_code && code != NO_ERROR)
	{
		LOG_WERROR(code);
	}

	CloseHandle(pinfo.hProcess);
	return code;
}

/* Handles process execution.  Returns system error code when sets
 * *got_exit_code to 0 and exit code of the process otherwise. */
static DWORD
handle_process(const char cmd[], HANDLE proc, int *got_exit_code)
{
	DWORD exit_code;

	if(!should_wait_for_program(cmd))
	{
		return 0;
	}

	if(WaitForSingleObject(proc, INFINITE) != WAIT_OBJECT_0)
	{
		return GetLastError();
	}

	if(GetExitCodeProcess(proc, &exit_code) == 0)
	{
		return GetLastError();
	}

	*got_exit_code = 1;
	return exit_code;
}

/* Checks whether execution should be paused until command is finished.  Returns
 * non-zero when such synchronization is required, otherwise zero is
 * returned. */
static int
should_wait_for_program(const char cmd[])
{
	char name[NAME_MAX];
	char path[PATH_MAX];

	(void)extract_cmd_name(cmd, 0, sizeof(name), name);

	if(get_cmd_path(name, sizeof(path), path) == 0)
	{
		return get_subsystem(path) != SUBSYSTEM_GUI;
	}
	return 1;
}

/* Gets subsystem of the executable pointed to by the filename.  Returns
 * subsystem or -1 on error. */
static int
get_subsystem(const char filename[])
{
	int subsystem;

	FILE *const fp = fopen(filename, "rb");
	if(fp == NULL)
	{
		return -1;
	}

	subsystem = get_stream_subsystem(fp);

	fclose(fp);

	return subsystem;
}

/* Gets subsystem of the executable contained in the fp stream.  Assumed that fp
 * is at the beginning of the stream.  Returns subsystem or -1 on error. */
static int
get_stream_subsystem(FILE *fp)
{
	uint32_t pe_offset, pe_signature;
	uint16_t subsystem;

	if(fgetc(fp) != 'M' || fgetc(fp) != 'Z')
	{
		return -1;
	}

	if(fseek(fp, PE_HDR_OFFSET, SEEK_SET) != 0)
	{
		return -1;
	}
	if(fread(&pe_offset, sizeof(pe_offset), 1U, fp) != 1U)
	{
		return -1;
	}

	if(fseek(fp, pe_offset, SEEK_SET) != 0)
	{
		return -1;
	}
	if(fread(&pe_signature, sizeof(pe_signature), 1U, fp) != 1U)
	{
		return -1;
	}
	if(pe_signature != PE_HDR_SIGNATURE)
	{
		return -1;
	}

	if(fseek(fp, pe_offset + PE_HDR_SUBSYSTEM_OFFSET, SEEK_SET) != 0)
	{
		return -1;
	}
	if(fread(&subsystem, sizeof(subsystem), 1U, fp) != 1U)
	{
		return -1;
	}
	return subsystem;
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
