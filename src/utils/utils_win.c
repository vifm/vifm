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

#include <ntdef.h>
#include <windows.h>
#include <winioctl.h>

#include <curses.h>

#include <fcntl.h>

#include <ctype.h> /* toupper() */
#include <stddef.h> /* NULL size_t */
#include <stdint.h> /* uint32_t */
#include <stdlib.h> /* EXIT_SUCCESS free() */
#include <string.h> /* strcat() strchr() strcpy() strlen() */
#include <stdio.h> /* FILE SEEK_SET fopen() fread() fclose() snprintf() */

#include "../cfg/config.h"
#include "../compat/os.h"
#include "../compat/wcwidth.h"
#include "../ui/ui.h"
#include "../commands_completion.h"
#include "../status.h"
#include "env.h"
#include "fs.h"
#include "fs_limits.h"
#include "log.h"
#include "macros.h"
#include "mntent.h"
#include "path.h"
#include "str.h"
#include "utf8.h"

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
	if(curr_stats.shell_type == ST_CMD)
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

	if(curr_stats.shell_type == ST_CMD)
	{
		/* See "cmd /?" for an "explanation" why extra double quotes are
		 * omitted. */
		snprintf(buf, sizeof(buf), "%s /C %s", cfg.shell, command);
		return os_system(buf);
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

void
wait_for_data_from(pid_t pid, FILE *f, int fd)
{
	/* Do nothing.  No need to wait for anything on this platform. */
}

int
set_sigchld(int block)
{
	return 0;
}

int
refers_to_slower_fs(const char from[], const char to[])
{
	return 0;
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
	return compat_wcwidth(c);
}

int
wcswidth(const wchar_t str[], size_t max_len)
{
	return compat_wcswidth(str, max_len);
}

int
win_exec_cmd(char cmd[], int *const returned_exit_code)
{
	wchar_t *utf16_cmd;
	BOOL ret;
	DWORD code;
	STARTUPINFOW startup = {};
	PROCESS_INFORMATION pinfo;

	*returned_exit_code = 0;

	utf16_cmd = utf8_to_utf16(cmd);
	ret = CreateProcessW(NULL, utf16_cmd, NULL, NULL, 0, 0, NULL, NULL, &startup,
			&pinfo);
	free(utf16_cmd);

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

	wchar_t *const utf16_filename = utf8_to_utf16(filename);
	FILE *const fp = _wfopen(utf16_filename, L"rb");
	free(utf16_filename);

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

const char *
escape_for_cd(const char str[])
{
	static char buf[PATH_MAX*2];
	char *p;

	p = buf;
	while(*str != '\0')
	{
		if(char_is_one_of("\\ $", *str))
			*p++ = '\\';
		else if(*str == '%')
			*p++ = '%';
		*p++ = *str;

		str++;
	}
	*p = '\0';
	return buf;
}

const char *
win_resolve_mount_points(const char path[])
{
	static char resolved_path[PATH_MAX];

	DWORD attr;
	wchar_t *utf16_path;
	HANDLE hfind;
	WIN32_FIND_DATAW ffd;
	HANDLE hfile;
	char rdb[2048];
	char *t;
	int offset;
	REPARSE_DATA_BUFFER *rdbp;

	utf16_path = utf8_to_utf16(path);
	attr = GetFileAttributesW(utf16_path);
	free(utf16_path);

	if(attr == INVALID_FILE_ATTRIBUTES)
	{
		return path;
	}

	if(!(attr & FILE_ATTRIBUTE_REPARSE_POINT))
	{
		return path;
	}

	copy_str(resolved_path, sizeof(resolved_path), path);
	chosp(resolved_path);

	utf16_path = utf8_to_utf16(resolved_path);
	hfind = FindFirstFileW(utf16_path, &ffd);

	if(hfind == INVALID_HANDLE_VALUE)
	{
		free(utf16_path);
		return path;
	}

	FindClose(hfind);

	if(ffd.dwReserved0 != IO_REPARSE_TAG_MOUNT_POINT)
	{
		free(utf16_path);
		return path;
	}

	hfile = CreateFileW(utf16_path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
			NULL);

	free(utf16_path);

	if(hfile == INVALID_HANDLE_VALUE)
	{
		return path;
	}

	if(!DeviceIoControl(hfile, FSCTL_GET_REPARSE_POINT, NULL, 0, rdb, sizeof(rdb),
			&attr, NULL))
	{
		CloseHandle(hfile);
		return path;
	}
	CloseHandle(hfile);

	rdbp = (REPARSE_DATA_BUFFER *)rdb;
	t = to_multibyte(rdbp->MountPointReparseBuffer.PathBuffer);

	offset = starts_with_lit(t, "\\??\\") ? 4 : 0;
	strcpy(resolved_path, t + offset);

	free(t);

	return resolved_path;
}

int
win_get_dir_mtime(const char dir_path[], FILETIME *ft)
{
	char selfref_path[PATH_MAX];
	wchar_t *utf16_path;
	HANDLE hfile;
	int r;

	snprintf(selfref_path, sizeof(selfref_path), "%s/.", dir_path);

	utf16_path = utf8_to_utf16(selfref_path);
	hfile = CreateFileW(utf16_path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	free(utf16_path);

	if(hfile == INVALID_HANDLE_VALUE)
	{
		return 1;
	}

	r = !GetFileTime(hfile, NULL, NULL, ft);
	CloseHandle(hfile);
	return r;
}

int
get_mount_point(const char path[], size_t buf_len, char buf[])
{
	snprintf(buf, buf_len, "%c:/", path[0]);
	return 0;
}

int
traverse_mount_points(mptraverser client, void *arg)
{
	char c;

	for(c = 'a'; c < 'z'; c++)
	{
		if(drive_exists(c))
		{
			const char drive[] = { c, ':', '/', '\0' };
			struct mntent entry =
			{
				.mnt_dir = drive,
				.mnt_type = "",
			};
			client(&entry, arg);
		}
	}

	return 0;
}

int
executable_exists(const char path[])
{
	const char *p;
	char path_buf[NAME_MAX];
	size_t pos;

	if(strchr(after_last(path, '/'), '.') != NULL)
	{
		return path_exists(path, DEREF) && !is_dir(path);
	}

	copy_str(path_buf, sizeof(path_buf), path);
	pos = strlen(path_buf);

	p = env_get_def("PATHEXT", PATHEXT_EXT_DEF);
	while((p = extract_part(p, ';', path_buf + pos)) != NULL)
	{
		if(path_exists(path_buf, DEREF) && !is_dir(path_buf))
		{
			return 1;
		}
	}
	return 0;
}

int
get_exe_dir(char dir_buf[], size_t dir_buf_len)
{
	if(GetModuleFileNameA(NULL, dir_buf, dir_buf_len) == 0)
	{
		return 1;
	}

	to_forward_slash(dir_buf);
	break_atr(dir_buf, '/');
	return 0;
}

EnvType
get_env_type(void)
{
	return ET_WIN;
}

int
format_help_cmd(char cmd[], size_t cmd_size)
{
	int bg;
	snprintf(cmd, cmd_size, "%s \"%s/" VIFM_HELP "\"", get_vicmd(&bg),
			cfg.config_dir);
	return bg;
}

void
display_help(const char cmd[])
{
	def_prog_mode();
	endwin();
	system("cls");
	if(os_system(cmd) != EXIT_SUCCESS)
	{
		system("pause");
	}
	update_screen(UT_FULL);
	update_screen(UT_REDRAW);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
