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
#include <objidl.h>
#include <shlguid.h>
#include <shobjidl.h>
#include <windows.h>

#include <curses.h>

#include <fcntl.h>
#include <unistd.h> /* _dup2() _pipe() _spawnvp() close() dup() pipe() */

#include <ctype.h> /* toupper() */
#include <stddef.h> /* NULL size_t */
#include <stdint.h> /* uint32_t */
#include <stdlib.h> /* EXIT_SUCCESS free() */
#include <string.h> /* strcat() strchr() strcpy() strdup() strlen() */
#include <stdio.h> /* FILE SEEK_SET fread() fclose() snprintf() */

#include "../cfg/config.h"
#include "../compat/fs_limits.h"
#include "../compat/mntent.h"
#include "../compat/os.h"
#include "../compat/wcwidth.h"
#include "../ui/ui.h"
#include "../cmd_completion.h"
#include "../status.h"
#include "cancellation.h"
#include "env.h"
#include "fs.h"
#include "log.h"
#include "macros.h"
#include "path.h"
#include "selector.h"
#include "str.h"
#include "test_helpers.h"
#include "utf8.h"

#define PE_HDR_SIGNATURE 0x00004550U
#define PE_HDR_OFFSET 0x3cU
#define PE_HDR_SUBSYSTEM_OFFSET 0x5cU
#define SUBSYSTEM_GUI 2

static const char PATHEXT_EXT_DEF[] = ".bat;.exe;.com";

static void process_cancel_request(pid_t pid,
		const cancellation_t *cancellation);
TSTATIC int should_wait_for_program(const char cmd[]);
static DWORD handle_process(const char cmd[], HANDLE proc, int *got_exit_code);
static char * base64_encode(const char str[]);
static int get_subsystem(const char filename[]);
static int get_stream_subsystem(FILE *fp);
static FILE * read_cmd_output_internal(const char cmd[], int out_pipe[2],
		int preserve_stdin);
static int get_set_owner_privilege(void);
static char * get_root_path(const char path[]);
static BOOL CALLBACK close_app_enum(HWND hwnd, LPARAM lParam);

void
pause_shell(void)
{
	if(curr_stats.shell_type == ST_CMD || curr_stats.shell_type == ST_YORI)
	{
		run_in_shell_no_cls("pause", SHELL_BY_APP);
	}
	else if(curr_stats.shell_type == ST_PS)
	{
		/* TODO: make pausing work. */
	}
	else
	{
		run_in_shell_no_cls(PAUSE_CMD, SHELL_BY_APP);
	}
}

int
run_in_shell_no_cls(char command[], ShellRequester by)
{
	char *const sh_cmd = win_make_sh_cmd(command, by);

	int returned_exit_code;
	int ret = win_exec_cmd(command, sh_cmd, &returned_exit_code);
	if(!returned_exit_code)
	{
		ret = -1;
	}

	free(sh_cmd);

	return ret;
}

int
run_with_input(char command[], FILE *input, ShellRequester by)
{
	rewind(input);

	PROCESS_INFORMATION pinfo;

	STARTUPINFOW startup = {
		.dwFlags = STARTF_USESTDHANDLES,
		.hStdInput = (HANDLE)_get_osfhandle(_fileno(input)),
		.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE),
		.hStdError = GetStdHandle(STD_ERROR_HANDLE),
	};
	SetHandleInformation(startup.hStdInput, HANDLE_FLAG_INHERIT, 1);
	SetHandleInformation(startup.hStdOutput, HANDLE_FLAG_INHERIT, 1);
	SetHandleInformation(startup.hStdError, HANDLE_FLAG_INHERIT, 1);

	char *sh_cmd = win_make_sh_cmd(command, by);
	wchar_t *wide_cmd = to_wide(sh_cmd);
	free(sh_cmd);

	int started = CreateProcessW(NULL, wide_cmd, NULL, NULL, 1, 0,
			NULL, NULL, &startup, &pinfo);
	free(wide_cmd);

	if(!started)
	{
		return -1;
	}

	CloseHandle(pinfo.hThread);

	int is_exit_code;
	DWORD code = handle_process(NULL, pinfo.hProcess, &is_exit_code);
	if(!is_exit_code && code != NO_ERROR)
	{
		LOG_WERROR(code);
		code = -1;
	}

	CloseHandle(pinfo.hProcess);
	return code;
}

void
recover_after_shellout(void)
{
	if(curr_stats.load_stage > 0)
	{
		reset_prog_mode();
		resize_term(cfg.lines, cfg.columns);
	}
}

void
wait_for_data_from(pid_t pid, FILE *f, int fd,
		const cancellation_t *cancellation)
{
	enum { DELAY_MS = 250 };

	selector_t *selector = selector_alloc();
	if(selector == NULL)
	{
		return;
	}

	fd = (f != NULL ? fileno(f) : fd);
	HANDLE h = (HANDLE)_get_osfhandle(fd);
	selector_add(selector, h);

	int has_data;
	do
	{
		process_cancel_request(pid, cancellation);
		has_data = selector_wait(selector, DELAY_MS);
	}
	while(!has_data);
	process_cancel_request(pid, cancellation);

	selector_free(selector);
}

/* Checks whether cancelling of current operation is requested and tries to
 * cancel the process specified by its id. */
static void
process_cancel_request(pid_t pid, const cancellation_t *cancellation)
{
	if(cancellation_requested(cancellation))
	{
		if(win_cancel_process(pid) != 0)
		{
			LOG_SERROR_MSG(errno, "Failed to cancel process with PID %" PRINTF_ULL,
					(unsigned long long)pid);
		}
	}
}

void
block_all_thread_signals(void)
{
	/* No normal signals on Windows. */
}

int
refers_to_slower_fs(const char from[], const char to[])
{
	return 0;
}

int
is_on_slow_fs(const char full_path[], const char slowfs_specs[])
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
win_exec_cmd(const char cmd[], char full_cmd[], int *returned_exit_code)
{
	wchar_t *utf16_full_cmd;
	BOOL ret;
	DWORD code;
	STARTUPINFOW startup = {};
	PROCESS_INFORMATION pinfo;

	*returned_exit_code = 0;

	utf16_full_cmd = utf8_to_utf16(full_cmd);
	ret = CreateProcessW(NULL, utf16_full_cmd, NULL, NULL, 0, 0, NULL, NULL,
			&startup, &pinfo);
	free(utf16_full_cmd);

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

char *
win_make_sh_cmd(const char cmd[], ShellRequester by)
{
	const char *sh_flag;
	char *free_me = NULL;

	const char *fmt;
	if(curr_stats.shell_type == ST_CMD)
	{
		sh_flag = (by == SHELL_BY_USER ? cfg.shell_cmd_flag : "/C");
		/* Documentation in `cmd /?` seems to LIE, can't make both spaces and
		 * special characters work at the same time. */
		fmt = (cmd[0] == '"') ? "%s %s \"%s\"" : "%s %s %s";
	}
	else if(curr_stats.shell_type == ST_YORI)
	{
		sh_flag = (by == SHELL_BY_USER ? cfg.shell_cmd_flag : "-c");
		fmt = "%s %s %s";
	}
	else if(curr_stats.shell_type == ST_PS)
	{
		fmt = "%s %s %s";
		sh_flag = "-encodedCommand";

		char *actual_cmd = format_str("& %s", cmd);

		free_me = base64_encode(actual_cmd);
		cmd = free_me;

		free(actual_cmd);
	}
	else
	{
		sh_flag = (by == SHELL_BY_USER ? cfg.shell_cmd_flag : "-c");
		fmt = "%s %s '%s'";

		free_me = malloc(strlen(cmd)*2 + 1);
		char *p = free_me;
		while(*cmd != '\0')
		{
			if(*cmd == '\\')
			{
				*p++ = '\\';
			}
			*p++ = *cmd++;
		}
		*p = '\0';

		cmd = free_me;
	}

	/* Size of format minus the size of the %s-s. */
	int buf_size = strlen(fmt) - 3*2
	             + strlen(cfg.shell)
	             + strlen(sh_flag)
	             + strlen(cmd)
	             + 1; /* Trailing '\0'. */
	char buf[buf_size];
	snprintf(buf, sizeof(buf), fmt, cfg.shell, sh_flag, cmd);
	free(free_me);
	return strdup(buf);
}

/* Base64-encodes the string after converting it to Unicode.  Returns the
 * encoded version. */
static char *
base64_encode(const char str[])
{
	const char *dict = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	                   "abcdefghijklmnopqrstuvwxyz"
	                   "0123456789+/";

	wchar_t *wstr = to_wide(str);
	int bytes = wcslen(wstr)*sizeof(wchar_t);

	str = (const char *)wstr;

	char *out = malloc(DIV_ROUND_UP(bytes, 3)*4 + 1);
	char *p = out;

	while(bytes > 0)
	{
		unsigned int x = 0;

		int i;
		for(i = 0; i < 3 && bytes > 0; ++i, --bytes)
		{
			x = (x << 8) | (unsigned char)*str++;
		}

		x <<= 8*(3 - i);

		while(i-- > -1)
		{
			*p++ = dict[(x >> 18) & 0x3f];
			x <<= 6;
		}
	}

	while((p - out)%4 != 0)
	{
		*p++ = '=';
	}

	*p = '\0';

	free(wstr);
	return out;
}

/* Handles process execution.  The cmd can be NULL.  Returns system error code
 * when sets *got_exit_code to 0 and exit code of the process otherwise. */
static DWORD
handle_process(const char cmd[], HANDLE proc, int *got_exit_code)
{
	DWORD exit_code;

	if(cmd != NULL && !should_wait_for_program(cmd))
	{
		/* Simulate program finishing normally. */
		*got_exit_code = 1;
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
TSTATIC int
should_wait_for_program(const char cmd[])
{
	char name[NAME_MAX + 1];
	char path[PATH_MAX + 1];

	(void)extract_cmd_name(cmd, 0, sizeof(name), name);
	system_to_internal_slashes(name);

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
	char name_buf[NAME_MAX + 1];
	char ext_buf[16];

	copy_str(name_buf, sizeof(name_buf), name);
	strtoupper(name_buf);

	p = env_get_def("PATHEXT", PATHEXT_EXT_DEF);
	while((p = extract_part(p, ";", ext_buf)) != NULL)
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
attr_str(uint32_t attr)
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
attr_str_long(uint32_t attr)
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
			struct mntent entry = {
				.mnt_dir = drive,
				.mnt_type = "",
			};
			if(client(&entry, arg))
			{
				break;
			}
		}
	}

	return 0;
}

int
executable_exists(const char path[])
{
	const char *p;
	char path_buf[NAME_MAX + 1];
	size_t pos;

	if(strchr(after_last(path, '/'), '.') != NULL)
	{
		return path_exists(path, DEREF) && !is_dir(path);
	}

	copy_str(path_buf, sizeof(path_buf), path);
	pos = strlen(path_buf);

	p = env_get_def("PATHEXT", PATHEXT_EXT_DEF);
	while((p = extract_part(p, ";", path_buf + pos)) != NULL)
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

	system_to_internal_slashes(dir_buf);
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
	char shell[NAME_MAX + 1];
	const char *shell_name;

	(void)extract_cmd_name(shell_cmd, 0, sizeof(shell), shell);
	shell_name = get_last_path_component(shell);

	if(stroscmp(shell_name, "cmd") == 0 || stroscmp(shell_name, "cmd.exe") == 0)
	{
		return ST_CMD;
	}
	if(stroscmp(shell_name, "yori") == 0 || stroscmp(shell_name, "yori.exe") == 0)
	{
		return ST_YORI;
	}
	if(stroscmp(shell_name, "powershell") == 0 ||
			stroscmp(shell_name, "powershell.exe") == 0 ||
			stroscmp(shell_name, "pwsh") == 0 ||
			stroscmp(shell_name, "pwsh.exe") == 0)
	{
		return ST_PS;
	}
	return ST_NORMAL;
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
	ui_shutdown();
	system("cls");
	if(os_system(cmd) != EXIT_SUCCESS)
	{
		system("pause");
	}
	update_screen(UT_FULL);
	update_screen(UT_REDRAW);
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
	/* We need ENABLE_WINDOW_INPUT flag to get terminal resize event. */
	SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), ENABLE_ECHO_INPUT |
			ENABLE_EXTENDED_FLAGS | ENABLE_INSERT_MODE | ENABLE_LINE_INPUT |
			ENABLE_MOUSE_INPUT | ENABLE_QUICK_EDIT_MODE | ENABLE_WINDOW_INPUT);
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
reopen_term_stdout(void)
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
			FILE_SHARE_READ | FILE_SHARE_WRITE, &sec_attr, OPEN_EXISTING, 0, 0);
	if(handle_out == INVALID_HANDLE_VALUE)
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

int
reopen_term_stdin(void)
{
	HANDLE handle_in;
	SECURITY_ATTRIBUTES sec_attr;

	/* Share this file handle with child processes so that they could use standard
	 * it too. */
	sec_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
	sec_attr.bInheritHandle = TRUE;
	sec_attr.lpSecurityDescriptor = NULL;

	handle_in = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE, &sec_attr, OPEN_EXISTING, 0, 0);
	if(handle_in == INVALID_HANDLE_VALUE)
	{
		fprintf(stderr, "Failed to open CONIN$.");
		return 1;
	}

	if(SetStdHandle(STD_INPUT_HANDLE, handle_in) == FALSE)
	{
		fprintf(stderr, "Failed to set CONIN$ as standard input.");
		return 1;
	}

	return 0;
}

FILE *
read_cmd_output(const char cmd[], int preserve_stdin)
{
	int in_fd, out_fd, err_fd;
	int out_pipe[2];
	FILE *result;

	if(_pipe(out_pipe, 512, O_NOINHERIT) != 0)
	{
		return NULL;
	}

	in_fd = dup(_fileno(stdin));
	out_fd = dup(_fileno(stdout));
	err_fd = dup(_fileno(stderr));

	result = read_cmd_output_internal(cmd, out_pipe, preserve_stdin);

	_dup2(in_fd, _fileno(stdin));
	_dup2(out_fd, _fileno(stdout));
	_dup2(err_fd, _fileno(stderr));
	close(in_fd);
	close(out_fd);
	close(err_fd);

	if(result == NULL)
	{
		close(out_pipe[0]);
	}
	close(out_pipe[1]);

	return result;
}

/* Performs redirection and execution of the command.  Returns file descriptor
 * bound to stdout of the command. */
static FILE *
read_cmd_output_internal(const char cmd[], int out_pipe[2], int preserve_stdin)
{
	if(!preserve_stdin)
	{
		HANDLE h = CreateFileA("\\\\.\\NUL", GENERIC_READ, 0, NULL, OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL, NULL);
		if(h == INVALID_HANDLE_VALUE)
		{
			return NULL;
		}

		const int fd = _open_osfhandle((intptr_t)h, _O_RDWR);
		if(fd == -1)
		{
			CloseHandle(h);
			return NULL;
		}

		if(_dup2(fd, _fileno(stdin)) != 0)
		{
			return NULL;
		}
		if(fd != _fileno(stdin))
		{
			close(fd);
		}
	}

	if(_dup2(out_pipe[1], _fileno(stdout)) != 0)
	{
		return NULL;
	}
	if(_dup2(out_pipe[1], _fileno(stderr)) != 0)
	{
		return NULL;
	}

	const char *args[] = { "cmd", "/C", cmd, NULL };
	const int retcode = _spawnvp(P_NOWAIT, args[0], (const char **)args);

	return (retcode == 0 ? NULL : _fdopen(out_pipe[0], "r"));
}

const char *
get_installed_data_dir(void)
{
	static char data_dir[PATH_MAX + 1];
	if(data_dir[0] == '\0')
	{
		char exe_dir[PATH_MAX + 1];
		(void)get_exe_dir(exe_dir, sizeof(exe_dir));
		snprintf(data_dir, sizeof(data_dir), "%s/data", exe_dir);
	}
	return data_dir;
}

const char *
get_sys_conf_dir(void)
{
	return get_installed_data_dir();
}

FILE *
win_tmpfile(void)
{
	char dir[PATH_MAX + 1];
	char file[PATH_MAX + 1];
	HANDLE h;
	int fd;
	FILE *f;

	if(GetTempPathA(sizeof(dir), dir) == 0)
	{
		return NULL;
	}

	if(GetTempFileNameA(dir, "dir-view", 0U, file) == 0)
	{
		return NULL;
	}

	h = CreateFileA(file, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
			FILE_FLAG_DELETE_ON_CLOSE, NULL);
	if(h == INVALID_HANDLE_VALUE)
	{
		return NULL;
	}

	fd = _open_osfhandle((intptr_t)h, _O_RDWR);
	if(fd == -1)
	{
		CloseHandle(h);
		return NULL;
	}

	f = fdopen(fd, "r+");
	if(f == NULL)
	{
		close(fd);
	}

	return f;
}

void
clone_attribs(const char path[], const char from[], const struct stat *st)
{
	wchar_t *const utf16_path = utf8_to_utf16(path);
	wchar_t *const utf16_from = utf8_to_utf16(from);

	if(get_set_owner_privilege())
	{
		SECURITY_INFORMATION info = GROUP_SECURITY_INFORMATION
		                          | OWNER_SECURITY_INFORMATION;
		DWORD size_needed = 0U;
		(void)GetFileSecurityW(utf16_from, info, NULL, 0, &size_needed);
		if(size_needed != 0U)
		{
			char sec_descr[size_needed];
			PSECURITY_DESCRIPTOR descr = (void *)&sec_descr;
			if(GetFileSecurityW(utf16_from, info, descr, size_needed,
						&size_needed) != FALSE)
			{
				SetFileSecurityW(utf16_path, info, descr);
			}
		}
	}

	HANDLE hfrom = CreateFileW(utf16_from, GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, NULL);
	HANDLE hto = CreateFileW(utf16_path, GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, NULL);

	if(hto != INVALID_HANDLE_VALUE && hfrom != INVALID_HANDLE_VALUE)
	{
		FILETIME creationTime, lastAccessTime, lastWriteTime;

		if(GetFileTime(hfrom, &creationTime, &lastAccessTime,
					&lastWriteTime) != FALSE)
		{
			SetFileTime(hto, &creationTime, &lastAccessTime, &lastWriteTime);
		}
	}

	if(hto != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hto);
	}
	if(hfrom != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hfrom);
	}
	free(utf16_from);
	free(utf16_path);
}

/* Requests privilege required to set file owner/group.  Returns non-zero if it
 * was granted and zero otherwise. */
static int
get_set_owner_privilege(void)
{
	static int granted = -1;
	if(granted >= 0)
	{
		return granted;
	}

	HANDLE token;
	if(!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token))
	{
		return (granted = 0);
	}

	LUID luid;
	if(!LookupPrivilegeValue(NULL, "SeRestorePrivilege", &luid))
	{
		CloseHandle(token);
		return (granted = 0);
	}

	TOKEN_PRIVILEGES tp;
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	if(!AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), NULL, NULL))
	{
		CloseHandle(token);
		return (granted = 0);
	}

	CloseHandle(token);
	granted = (GetLastError() != ERROR_NOT_ALL_ASSIGNED);
	return granted;
}

int
get_drive_info(const char at[], uint64_t *total_bytes, uint64_t *free_bytes)
{
	char *const root_path = get_root_path(at);
	DWORD sectors_in_cluster, bytes_in_sector, free_cluster_count;
	DWORD total_number_of_clusters;
	if(!GetDiskFreeSpaceA(root_path, &sectors_in_cluster, &bytes_in_sector,
				&free_cluster_count, &total_number_of_clusters))
	{
		free(root_path);
		return -1;
	}
	free(root_path);

	*total_bytes =
		(uint64_t)bytes_in_sector*sectors_in_cluster*total_number_of_clusters;
	*free_bytes = (uint64_t)bytes_in_sector*sectors_in_cluster*free_cluster_count;
	return 0;
}

/* Extracts root part of the path (drive name or UNC share name).  Returns newly
 * allocated string. */
static char *
get_root_path(const char path[])
{
	const char *slash;
	char *root;

	if(!is_unc_path(path))
	{
		const char root_path[] = { path[0], ':', '\\', '\0' };
		return strdup(root_path);
	}

	slash = until_first(after_first(path + 2, '/'), '/');
	root = format_str("%.*s\\", (int)(slash - path), path);

	system_to_internal_slashes(root);
	return root;
}

int
win_cancel_process(DWORD pid)
{
	return EnumWindows((WNDENUMPROC)&close_app_enum, (LPARAM)pid) == FALSE;
}

/* EnumWindows callback that sends WM_CLOSE message to all windows of a
 * process. */
static BOOL CALLBACK
close_app_enum(HWND hwnd, LPARAM lParam)
{
	DWORD dwID;
	GetWindowThreadProcessId(hwnd, &dwID);

	if(dwID == (DWORD)lParam)
	{
		PostMessage(hwnd, WM_CLOSE, 0, 0);
	}

	return TRUE;
}

time_t
win_to_unix_time(FILETIME ft)
{
	const uint64_t WINDOWS_TICK = 10000000;
	const uint64_t SEC_TO_UNIX_EPOCH = 11644473600LL;

	/* Be careful with types here, first line is important. */
	uint64_t win_time = ft.dwHighDateTime;
	win_time = (win_time << 32) | ft.dwLowDateTime;

	return win_time/WINDOWS_TICK - SEC_TO_UNIX_EPOCH;
}

DWORD
win_get_file_attrs(const char path[])
{
	DWORD attr;
	wchar_t *utf16_path;

	if(is_path_absolute(path) && !is_unc_path(path))
	{
		if(isalpha(path[0]) && !drive_exists(path[0]))
		{
			return INVALID_FILE_ATTRIBUTES;
		}
	}

	utf16_path = utf8_to_utf16(path);
	attr = GetFileAttributesW(utf16_path);
	free(utf16_path);

	return attr;
}

DWORD
win_get_reparse_point_type(const char path[])
{
	char path_copy[PATH_MAX + 1];
	DWORD attr;
	wchar_t *utf16_filename;
	HANDLE hfind;
	WIN32_FIND_DATAW ffd;

	attr = win_get_file_attrs(path);
	if(attr == INVALID_FILE_ATTRIBUTES)
	{
		LOG_WERROR(GetLastError());
		return 0;
	}

	if(!(attr & FILE_ATTRIBUTE_REPARSE_POINT))
	{
		return 0;
	}

	copy_str(path_copy, sizeof(path_copy), path);
	chosp(path_copy);

	utf16_filename = utf8_to_utf16(path_copy);
	hfind = FindFirstFileW(utf16_filename, &ffd);
	free(utf16_filename);

	if(hfind == INVALID_HANDLE_VALUE)
	{
		LOG_WERROR(GetLastError());
		return 0;
	}

	if(!FindClose(hfind))
	{
		LOG_WERROR(GetLastError());
	}

	return ffd.dwReserved0;
}

int
win_reparse_point_read(const char path[], char buf[], size_t buf_len)
{
	char path_copy[PATH_MAX + 1];
	copy_str(path_copy, sizeof(path_copy), path);
	chosp(path_copy);

	wchar_t *utf16_filename = to_wide(path_copy);
	HANDLE hfile = CreateFileW(utf16_filename, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
	free(utf16_filename);

	if(hfile == INVALID_HANDLE_VALUE)
	{
		LOG_WERROR(GetLastError());
		return -1;
	}

	DWORD attr;
	if(!DeviceIoControl(hfile, FSCTL_GET_REPARSE_POINT, NULL, 0, buf,
			buf_len, &attr, NULL))
	{
		LOG_WERROR(GetLastError());
		CloseHandle(hfile);
		return -1;
	}
	CloseHandle(hfile);

	return 0;
}

int
win_symlink_read(const char link[], char buf[], int buf_len)
{
	if(!is_symlink(link))
	{
		return -1;
	}

	char rdb[2048];
	if(win_reparse_point_read(link, rdb, sizeof(rdb)) != 0)
	{
		return -1;
	}

	REPARSE_DATA_BUFFER *sbuf = (REPARSE_DATA_BUFFER *)rdb;
	WCHAR *path = sbuf->SymbolicLinkReparseBuffer.PathBuffer;
	path[sbuf->SymbolicLinkReparseBuffer.PrintNameOffset/sizeof(WCHAR) +
			sbuf->SymbolicLinkReparseBuffer.PrintNameLength/sizeof(WCHAR)] = L'\0';
	char *mb = to_multibyte(path +
			sbuf->SymbolicLinkReparseBuffer.PrintNameOffset/sizeof(WCHAR));
	if(strncmp(mb, "\\??\\", 4) == 0)
		strncpy(buf, mb + 4, buf_len);
	else
		strncpy(buf, mb, buf_len);
	buf[buf_len - 1] = '\0';
	free(mb);
	system_to_internal_slashes(buf);
	return 0;
}

int
win_shortcut_read(const char shortcut[], char buf[], int buf_len)
{
	HRESULT hres;

	static int initialized;
	if(!initialized)
	{
		hres = CoInitializeEx(NULL, COINIT_MULTITHREADED);
		if(!SUCCEEDED(hres))
		{
			return 1;
		}
		initialized = 1;
	}

	static IShellLinkW *shlink;
	if(shlink == NULL)
	{
		hres = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
				&IID_IShellLinkW, (void **)&shlink);
		if(!SUCCEEDED(hres))
		{
			return 1;
		}
	}

	static IPersistFile *pfile;
	if(pfile == NULL)
	{
		hres = shlink->lpVtbl->QueryInterface(shlink, &IID_IPersistFile,
				(void **)&pfile);
		if(!SUCCEEDED(hres))
		{
			return 1;
		}
	}

	wchar_t *wpath = to_wide(shortcut);
	if(wpath == NULL)
	{
		return 1;
	}

	hres = pfile->lpVtbl->Load(pfile, wpath, STGM_READ);
	free(wpath);

	if(SUCCEEDED(hres))
	{
		hres = shlink->lpVtbl->Resolve(shlink, INVALID_HANDLE_VALUE, SLR_NO_UI);

		if(SUCCEEDED(hres))
		{
			wchar_t target[PATH_MAX + 1];
			hres = shlink->lpVtbl->GetPath(shlink, target, sizeof(target), NULL, 0);

			if(hres == S_OK)
			{
				char *mb = to_multibyte(target);
				if(mb == NULL)
				{
					return 1;
				}
				copy_str(buf, buf_len, mb);
				system_to_internal_slashes(buf);
				free(mb);
				return 0;
			}
		}
	}

	return 1;
}

uint64_t
get_true_inode(const struct dir_entry_t *entry)
{
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
