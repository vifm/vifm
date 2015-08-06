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

#include "utils.h"
#include "utils_int.h"

#include <ntdef.h>
#include <windows.h>
#include <winioctl.h>

#include <curses.h>

#include <fcntl.h>
#include <unistd.h> /* _dup2() _pipe() _spawnvp() close() dup() pipe() */

#include <ctype.h> /* toupper() */
#include <stddef.h> /* NULL size_t */
#include <stdint.h> /* uint32_t */
#include <stdlib.h> /* EXIT_SUCCESS free() */
#include <string.h> /* strcat() strchr() strcpy() strlen() */
#include <stdio.h> /* FILE SEEK_SET fread() fclose() snprintf() */

#include "../cfg/config.h"
#include "../compat/fs_limits.h"
#include "../compat/mntent.h"
#include "../compat/os.h"
#include "../compat/wcwidth.h"
#include "../ui/ui.h"
#include "../commands_completion.h"
#include "../status.h"
#include "env.h"
#include "fs.h"
#include "log.h"
#include "macros.h"
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
static int win_get_dir_mtime(const char dir_path[], FILETIME *ft);
static FILE * use_info_prog_internal(const char cmd[], int out_pipe[2]);

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
		/* Documentation in `cmd /?` seems to LIE, can't make both spaces and
		 * special characters work at the same time. */
		const char *const fmt = (command[0] == '"') ? "%s /C \"%s\"" : "%s /C %s";
		snprintf(buf, sizeof(buf), fmt, cfg.shell, command);
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
	if(curr_stats.load_stage != 0)
	{
		reset_prog_mode();
		resize_term(cfg.lines, cfg.columns);
	}
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
win_check_dir_changed(FileView *view)
{
	FILETIME ft;
	int r;

	if(stroscmp(view->watched_dir, view->curr_dir) != 0)
	{
		wchar_t *utf16_cwd;

		FindCloseChangeNotification(view->dir_watcher);
		strcpy(view->watched_dir, view->curr_dir);

		utf16_cwd = utf8_to_utf16(view->curr_dir);

		view->dir_watcher = FindFirstChangeNotificationW(utf16_cwd, 1,
				FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
				FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
				FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SECURITY);

		free(utf16_cwd);

		if(view->dir_watcher == NULL || view->dir_watcher == INVALID_HANDLE_VALUE)
		{
			log_msg("ha%s", "d");
		}
	}

	if(WaitForSingleObject(view->dir_watcher, 0) == WAIT_OBJECT_0)
	{
		FindNextChangeNotification(view->dir_watcher);
		return 1;
	}

	if(win_get_dir_mtime(view->curr_dir, &ft) != 0)
	{
		return -1;
	}

	r = CompareFileTime(&view->dir_mtime, &ft);
	view->dir_mtime = ft;

	return r != 0;
}

/* Gets last directory modification time.  Returns non-zero on error, otherwise
 * zero is returned. */
static int
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

ExecEnvType
get_exec_env_type(void)
{
	return EET_EMULATOR_WITH_X;
}

ShellType
get_shell_type(const char shell_cmd[])
{
	char shell[NAME_MAX];
	const char *shell_name;

	(void)extract_cmd_name(shell_cmd, 0, sizeof(shell), shell);
	shell_name = get_last_path_component(shell);

	if(stroscmp(shell_name, "cmd") == 0 || stroscmp(shell_name, "cmd.exe") == 0)
	{
		return ST_CMD;
	}
	else
	{
		return ST_NORMAL;
	}
}

int
format_help_cmd(char cmd[], size_t cmd_size)
{
	int bg;
	snprintf(cmd, cmd_size, "%s \"%s/" VIFM_HELP "\"", cfg_get_vicmd(&bg),
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

int
update_dir_mtime(FileView *view)
{
	if(win_get_dir_mtime(view->curr_dir, &view->dir_mtime) != 0)
	{
		return -1;
	}

	/* Skip all directory change events accumulated so far. */
	while(win_check_dir_changed(view) > 0)
	{
		/* Do nothing. */
	}

	return 0;
}

void
wait_for_signal(void)
{
	/* Do nothing. */
}

void
stop_process(void)
{
	/* Do nothing. */
}

void
update_terminal_settings(void)
{
	SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), ENABLE_ECHO_INPUT |
			ENABLE_EXTENDED_FLAGS | ENABLE_INSERT_MODE | ENABLE_LINE_INPUT |
			ENABLE_MOUSE_INPUT | ENABLE_QUICK_EDIT_MODE);
}

void
get_uid_string(const dir_entry_t *entry, int as_num, size_t buf_len, char buf[])
{
	/* Simply return empty buffer. */
	buf[0] = '\0';
}

void
get_gid_string(const dir_entry_t *entry, int as_num, size_t buf_len, char buf[])
{
	/* Simply return empty buffer. */
	buf[0] = '\0';
}

FILE *
reopen_terminal(void)
{
	int outfd;
	FILE *fp;
	HANDLE handle_out;
	SECURITY_ATTRIBUTES sec_attr;

	outfd = dup(STDOUT_FILENO);
	if(outfd == -1)
	{
		fprintf(stderr, "Failed to store original output stream.");
		return NULL;
	}

	fp = fdopen(outfd, "w");
	if(fp == NULL)
	{
		fprintf(stderr, "Failed to open original output stream.");
		return NULL;
	}

	/* Share this file handle with child processes so that could use standard
	 * output. */
	sec_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
	sec_attr.bInheritHandle = TRUE;
	sec_attr.lpSecurityDescriptor = NULL;

	handle_out = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE, &sec_attr, 0, 0, 0);
	if(handle_out == NULL)
	{
		fclose(fp);
		fprintf(stderr, "Failed to open CONOUT$.");
		return NULL;
	}

	if(SetStdHandle(STD_OUTPUT_HANDLE, handle_out) == FALSE)
	{
		fclose(fp);
		fprintf(stderr, "Failed to set CONOUT$ as standard output.");
		return NULL;
	}

	return fp;
}

FILE *
read_cmd_output(const char cmd[])
{
	int out_fd, err_fd;
	int out_pipe[2];
	FILE *result;

	if(_pipe(out_pipe, 512, O_NOINHERIT) != 0)
	{
		return NULL;
	}

	out_fd = dup(_fileno(stdout));
	err_fd = dup(_fileno(stderr));

	result = use_info_prog_internal(cmd, out_pipe);

	_dup2(out_fd, _fileno(stdout));
	_dup2(err_fd, _fileno(stderr));

	if(result == NULL)
		close(out_pipe[0]);
	close(out_pipe[1]);

	return result;
}

const char *
get_installed_data_dir(void)
{
	static char exe_dir[PATH_MAX];
	(void)get_exe_dir(exe_dir, sizeof(exe_dir));
	return exe_dir;
}

static FILE *
use_info_prog_internal(const char cmd[], int out_pipe[2])
{
	char *args[4];
	int retcode;

	if(_dup2(out_pipe[1], _fileno(stdout)) != 0)
	{
		return NULL;
	}
	if(_dup2(out_pipe[1], _fileno(stderr)) != 0)
	{
		return NULL;
	}

	args[0] = "cmd";
	args[1] = "/C";
	args[2] = (char *)cmd;
	args[3] = NULL;

	retcode = _spawnvp(P_NOWAIT, args[0], (const char **)args);

	return (retcode == 0) ? NULL : _fdopen(out_pipe[0], "r");
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
