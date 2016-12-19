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

#include "ipc.h"

#ifndef ENABLE_REMOTE_CMDS

#include <stddef.h> /* NULL */

int
ipc_enabled(void)
{
	return 0;
}

char **
ipc_list(int *len)
{
	*len = 0;
	return NULL;
}

void
ipc_init(const char name[], ipc_callback callback_func)
{
}

const char *
ipc_get_name(void)
{
	return "";
}

void
ipc_check(void)
{
}

int
ipc_send(const char whom[], char *data[])
{
	return 1;
}

#else

#if defined(_WIN32) || defined(__CYGWIN__)
/* Named pipes don't work very well on Cygwin neither directly nor indirectly
 * (using //./pipe/ paths with POSIX API), so use Win32 API on Cygwin. */
# define WIN32_PIPE_READ
#endif

#ifndef WIN32_PIPE_READ
# include <sys/types.h>
# include <sys/select.h> /* FD_* select() */
#else
# define O_NONBLOCK 0
# define REQUIRED_WINVER 0x0600 /* To get PIPE_REJECT_REMOTE_CLIENTS. */
# include "utils/windefs.h"
# include <windows.h>
# ifndef PIPE_REJECT_REMOTE_CLIENTS
#  define PIPE_REJECT_REMOTE_CLIENTS 1
# endif
# ifdef KEY_EVENT
#  undef KEY_EVENT /* curses also defines a macro with such name. */
# endif
#endif

#include <sys/stat.h> /* mkfifo() stat() */
#include <dirent.h> /* DIR closedir() opendir() readdir() */
#include <fcntl.h>
#include <unistd.h> /* close() open() select() unlink() */

#include <assert.h> /* assert() */
#include <errno.h> /* EACCES EEXIST EDQUOT ENOSPC ENXIO errno */
#include <stddef.h> /* NULL size_t ssize_t */
#include <stdio.h> /* FILE fclose() fdopen() fread() fwrite() */
#include <stdlib.h> /* atexit() free() malloc() qsort() snprintf() */
#include <string.h> /* strcmp() strcpy() strlen() */

#include "utils/fs.h"
#include "utils/log.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/utils.h"
#include "status.h"

/* Prefix for names of all pipes to distinguish them from other pipes. */
#define PREFIX "vifm-ipc-"

#ifndef WIN32_PIPE_READ
typedef FILE *read_pipe_t;
#define NULL_READ_PIPE NULL
#else
typedef HANDLE read_pipe_t;
#define NULL_READ_PIPE INVALID_HANDLE_VALUE
#endif

/* Holds list information for add_to_list(). */
typedef struct
{
	char **lst;          /* List of strings. */
	size_t len;          /* Number of items. */
	const char *ipc_dir; /* Root of IPC objects. */
}
list_data_t;

static void cleanup_at_exit(void);
static read_pipe_t create_pipe(const char name[], char path_buf[], size_t len);
static char * receive_pkg(void);
static read_pipe_t try_use_pipe(const char path[], int *fatal);
static void handle_pkg(const char pkg[]);
static int send_pkg(const char whom[], const char what[], size_t len);
static char * get_the_only_target(void);
static int add_to_list(const char name[], const void *data, void *param);
static const char * get_ipc_dir(void);
static int sorter(const void *first, const void *second);
#ifndef WIN32_PIPE_READ
static int pipe_is_in_use(const char path[]);
#endif

/* Stores callback to report received messages. */
static ipc_callback callback;
/* Whether unit was initialized and what's the result (-1 is error, 1 is
 * success). */
static int initialized;
/* Path to the pipe used by this instance. */
static char pipe_path[PATH_MAX];
/* Opened file of the pipe. */
static read_pipe_t pipe_file;

int
ipc_enabled(void)
{
	return (initialized > 0);
}

void
ipc_init(const char name[], ipc_callback callback_func)
{
	assert(!initialized && "Repeated initialization?");

	callback = callback_func;

	if(name == NULL)
	{
		name = "vifm";
	}

	pipe_file = create_pipe(name, pipe_path, sizeof(pipe_path));
	if(pipe_file == NULL_READ_PIPE)
	{
		initialized = -1;
		return;
	}

	atexit(&cleanup_at_exit);
	initialized = 1;
}

/* Frees resources used by IPC. */
static void
cleanup_at_exit(void)
{
#ifndef WIN32_PIPE_READ
	fclose(pipe_file);
	unlink(pipe_path);
#else
	CloseHandle(pipe_file);
#endif
}

const char *
ipc_get_name(void)
{
	if(initialized < 0)
	{
		return "";
	}

	return get_last_path_component(pipe_path) + (sizeof(PREFIX) - 1U);
}

void
ipc_check(void)
{
	char *pkg;

	assert(initialized != 0 && "Wrong IPC unit state.");
	if(initialized < 0)
	{
		return;
	}

	pkg = receive_pkg();
	if(pkg == NULL)
	{
		return;
	}

	handle_pkg(pkg);
	free(pkg);
}

/* Receives message addressed to this instance.  Returns NULL if there was no
 * message or on failure to read it, otherwise newly allocated string is
 * returned. */
static char *
receive_pkg(void)
{
#ifndef WIN32_PIPE_READ
	uint32_t size;
	char *pkg;
	char *p;

	fd_set ready;
	int max_fd;
	struct timeval ts = { .tv_sec = 0, .tv_usec = 10000 };

	if(fread(&size, sizeof(size), 1U, pipe_file) != 1U || size >= 4294967294U)
	{
		return NULL;
	}

	pkg = malloc(size + 2U);
	if(pkg == NULL)
	{
		return NULL;
	}

	max_fd = fileno(pipe_file);
	FD_ZERO(&ready);
	FD_SET(max_fd, &ready);

	p = pkg;
	while(size != 0U && select(max_fd + 1, &ready, NULL, NULL, &ts) > 0)
	{
		const size_t nread = fread(p, 1U, size, pipe_file);
		size -= nread;
		p += nread;

		if(nread == 0U)
		{
			break;
		}

		ts.tv_sec = 0;
		ts.tv_usec = 10000;
	}

	if(size != 0U)
	{
		free(pkg);
		return NULL;
	}

	/* Make sure we have two trailing zeroes. */
	*p++ = '\0';
	*p = '\0';

	return pkg;
#else
	uint32_t size;
	char *pkg;
	char *p;
	DWORD nread;

	if(ReadFile(pipe_file, &size, sizeof(size), &nread, NULL) == FALSE ||
			size >= 4294967294U)
	{
		return NULL;
	}

	pkg = malloc(size + 2U);
	if(pkg == NULL)
	{
		return NULL;
	}

	p = pkg;
	while(size != 0U)
	{
		/* TODO: maybe use OVERLAPPED I/O on Windows instead, it's just so
		 *       inconvenient... */
		usleep(10000);

		if(ReadFile(pipe_file, p, size, &nread, NULL) == FALSE || nread == 0U)
		{
			break;
		}

		size -= nread;
		p += nread;
	}

	/* Weird requirement for named pipes, need to break and set connection every
	 * time. */
	DisconnectNamedPipe(pipe_file);
	ConnectNamedPipe(pipe_file, NULL);

	if(size != 0U)
	{
		free(pkg);
		return NULL;
	}

	/* Make sure we have two trailing zeroes. */
	*p++ = '\0';
	*p = '\0';

	return pkg;
#endif
}

/* Tries to open a pipe for communication.  Returns NULL_READ_PIPE on error or
 * opened file descriptor otherwise. */
static read_pipe_t
create_pipe(const char name[], char path_buf[], size_t len)
{
	int id = 0;
	read_pipe_t rp;
	int fatal;

	/* Try to use name as is at first. */
	snprintf(path_buf, len, "%s/" PREFIX "%s", get_ipc_dir(), name);
	rp = try_use_pipe(path_buf, &fatal);
	while(rp == NULL_READ_PIPE && !fatal)
	{
		snprintf(path_buf, len, "%s/" PREFIX "%s%d", get_ipc_dir(), name, ++id);

		if(id == 0)
		{
			return NULL_READ_PIPE;
		}

		rp = try_use_pipe(path_buf, &fatal);
	}

	return rp;
}

/* Either creates a pipe or reused previously abandoned one.  Returns
 * NULL_READ_PIPE on failure (with *fatal set to non-zero if further tries don't
 * make any sense) or valid file descriptor otherwise. */
static read_pipe_t
try_use_pipe(const char path[], int *fatal)
{
#ifndef WIN32_PIPE_READ
	FILE *f;
	int fd;

	*fatal = 0;

	/* Try to create a pipe. */
	if(mkfifo(path, 0600) == 0)
	{
		/* Open if created. */
		const int fd = open(path, O_RDONLY | O_NONBLOCK);
		if(fd == -1)
		{
			if(errno == EACCES)
			{
				/* If we created a file that we can't open, remove it and assume that
				 * something is fundamentally wrong with the system setup. */
				(void)unlink(path);
				*fatal = 1;
			}
			return NULL_READ_PIPE;
		}
		return fdopen(fd, "r");
	}

	/* Fail fast if the error is not related to existence of the file or pipe
	 * exists and is in use. */
	if(errno != EEXIST || pipe_is_in_use(path))
	{
		/* No retries if file-system is unable to create more files. */
		*fatal = (errno == EDQUOT || errno == ENOSPC);
		return NULL_READ_PIPE;
	}

	/* Use this file if we're the only one willing to read from it. */
	fd = open(path, O_RDONLY | O_NONBLOCK);

	if(fd == -1)
	{
		return NULL_READ_PIPE;
	}

	f = fdopen(fd, "r");
	if(f == NULL_READ_PIPE)
	{
		close(fd);
	}
	return f;
#else
	*fatal = 0;
	return CreateNamedPipeA(path,
			PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE,
			PIPE_TYPE_BYTE | PIPE_NOWAIT | PIPE_REJECT_REMOTE_CLIENTS,
			PIPE_UNLIMITED_INSTANCES, 4096, 4096, 10, NULL);
#endif
}

/* Parses pkg into array of strings and invokes callback. */
static void
handle_pkg(const char pkg[])
{
	char **array = NULL;
	size_t len = 0U;

	while(*pkg != '\0')
	{
		len = add_to_string_array(&array, len, 1, pkg);
		pkg += strlen(pkg) + 1;
	}
	len = put_into_string_array(&array, len, NULL);

	if(len != 0U)
	{
		callback(array);
	}

	free_string_array(array, len);
}

int
ipc_send(const char whom[], char *data[])
{
	/* FIXME: this shouldn't have fixed size.  Or maybe it should be PIPE_BUF to
	 * guarantee atomic operation. */
	char pkg[8192];
	size_t len;
	char *name = NULL;
	int ret;

	assert(initialized != 0 && "Wrong IPC unit state.");
	if(initialized < 0)
	{
		return 1;
	}

	if(get_cwd(pkg, sizeof(pkg)) == NULL)
	{
		LOG_ERROR_MSG("Can't get working directory");
		return 1;
	}
	len = strlen(pkg) + 1;

	while(*data != NULL)
	{
		len += copy_str(pkg + len, sizeof(pkg) - len, *data);
		++data;
	}
	pkg[len++] = '\0';

	if(whom == NULL)
	{
		name = get_the_only_target();
		if(name == NULL)
		{
			return 1;
		}
		whom = name;
	}

	ret = send_pkg(whom, pkg, len);

	free(name);
	return ret;
}

/* Performs actual sending of package to another instance.  Returns zero on
 * success and non-zero otherwise. */
static int
send_pkg(const char whom[], const char what[], size_t len)
{
#ifndef WIN32_PIPE_READ
	char path[PATH_MAX];
	int fd;
	FILE *dst;
	uint32_t size;

	snprintf(path, sizeof(path), "%s/" PREFIX "%s", get_ipc_dir(), whom);

	fd = open(path, O_WRONLY | O_NONBLOCK);
	if(fd == -1)
	{
		return 1;
	}

	dst = fdopen(fd, "w");
	if(dst == NULL)
	{
		close(fd);
		return 1;
	}

	size = len;
	if(fwrite(&size, sizeof(size), 1U, dst) != 1U ||
			fwrite(what, len, 1U, dst) != 1U)
	{
		fclose(dst);
		return 1;
	}

	fclose(dst);
	return 0;
#else
	char path[PATH_MAX];
	HANDLE h;
	uint32_t size;
	DWORD nwritten;

	snprintf(path, sizeof(path), "%s/" PREFIX "%s", get_ipc_dir(), whom);

	h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
			0, NULL);
	if(h == INVALID_HANDLE_VALUE)
	{
		return 1;
	}

	size = len;
	if(WriteFile(h, &size, sizeof(size), &nwritten, NULL) == FALSE ||
			nwritten != sizeof(size) ||
			WriteFile(h, what, len, &nwritten, NULL) == FALSE || nwritten != len)
	{
		CloseHandle(h);
		return 1;
	}

	CloseHandle(h);
	return 0;
#endif
}

/* Automatically picks target instance to send data to.  Returns newly allocated
 * string or NULL on error (no other instances or memory allocation failure). */
static char *
get_the_only_target(void)
{
	int len;
	char *name;
	char **list = ipc_list(&len);

	if(len == 0)
	{
		return NULL;
	}

	name = list[0];
	list[0] = NULL;
	free_string_array(list, len);

	return name;
}

char **
ipc_list(int *len)
{
	list_data_t data = { .ipc_dir = get_ipc_dir() };

#ifndef WIN32_PIPE_READ
	if(enum_dir_content(data.ipc_dir, &add_to_list, &data) != 0)
	{
		*len = 0;
		return NULL;
	}
#else
	{
		char find_pat[PATH_MAX];
		HANDLE hfind;
		WIN32_FIND_DATAA ffd;

		snprintf(find_pat, sizeof(find_pat), "%s/*", data.ipc_dir);
		hfind = FindFirstFileA(find_pat, &ffd);

		if(hfind == INVALID_HANDLE_VALUE)
		{
			*len = 0;
			return NULL;
		}

		do
		{
			if(add_to_list(ffd.cFileName, &ffd, &data) != 0)
			{
				break;
			}
		}
		while(FindNextFileA(hfind, &ffd));
		FindClose(hfind);
	}
#endif

	qsort(data.lst, data.len, sizeof(*data.lst), &sorter);

	*len = data.len;
	return data.lst;
}

/* Analyzes pipe and adds it to the list of pipes.  Returns zero on success or
 * non-zero on error. */
static int
add_to_list(const char name[], const void *data, void *param)
{
	list_data_t *const list_data = param;

	if(!starts_with_lit(name, PREFIX))
	{
		return 0;
	}

	/* Skip ourself. */
	if(stroscmp(name, get_last_path_component(pipe_path)) == 0)
	{
		return 0;
	}

	/* On Windows it's guaranteed to be a valid pipe. */
#ifndef WIN32_PIPE_READ
	{
		char path[PATH_MAX];
		struct stat statbuf;
		snprintf(path, sizeof(path), "%s/%s", list_data->ipc_dir, name);
		if(stat(path, &statbuf) != 0 || !S_ISFIFO(statbuf.st_mode) ||
				!pipe_is_in_use(path))
		{
			return 0;
		}
	}
#endif

	list_data->len = add_to_string_array(&list_data->lst, list_data->len, 1,
			name + strlen(PREFIX));
	return 0;
}

/* Retrieves directory where FIFO objects are created.  Returns the path. */
static const char *
get_ipc_dir(void)
{
#ifndef WIN32_PIPE_READ
	return get_tmpdir();
#else
	return "//./pipe";
#endif
}

/* Wraps strcmp() for use with qsort(). */
static int
sorter(const void *first, const void *second)
{
	const char *const *const a = first;
	const char *const *const b = second;
	return strcmp(*a, *b);
}

#ifndef WIN32_PIPE_READ

/* Tries to open a pipe to check whether it has any readers or it's
 * abandoned.  Returns non-zero if somebody is reading from the pipe and zero
 * otherwise. */
static int
pipe_is_in_use(const char path[])
{
	const int fd = open(path, O_WRONLY | O_NONBLOCK);
	if(fd != -1 || errno != ENXIO)
	{
		if(fd != -1)
		{
			close(fd);
		}
		return 1;
	}
	return 0;
}

#endif

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
