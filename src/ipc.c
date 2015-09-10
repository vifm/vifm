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
ipc_init(ipc_callback callback_func)
{
}

void
ipc_check(void)
{
}

int
ipc_send(char *data[])
{
	return 1;
}

#else

#ifndef _WIN32
#include <sys/types.h>
#include <sys/select.h> /* FD_* select() */
#else
#define O_NONBLOCK 0
#define REQUIRED_WINVER 0x0600 /* To get PIPE_REJECT_REMOTE_CLIENTS. */
#include "utils/windefs.h"
#ifndef PIPE_REJECT_REMOTE_CLIENTS
#define PIPE_REJECT_REMOTE_CLIENTS 1
#endif
#include <windows.h>
#endif
#include <sys/stat.h> /* mkfifo() stat() */
#include <dirent.h> /* DIR closedir() opendir() readdir() */
#include <fcntl.h>
#include <unistd.h> /* close() open() select() unlink() */

#include <assert.h> /* assert() */
#include <errno.h> /* EEXIST ENXIO errno */
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

/* Holds list information for add_to_list(). */
typedef struct
{
	char **lst;          /* List of strings. */
	size_t len;          /* Number of items. */
	const char *ipc_dir; /* Root of IPC objects. */
}
list_data_t;

static void clean_at_exit(void);
static FILE * create_pipe(char path_buf[], size_t len);
static char * receive_pkg(void);
static FILE * try_use_pipe(const char path[]);
static void handle_pkg(const char pkg[]);
static int send_pkg(const char whom[], const char what[], size_t len);
static char * get_the_only_target(void);
static int add_to_list(const char name[], const void *data, void *param);
#ifndef _WIN32
static int pipe_is_in_use(const char path[]);
#endif
static const char * get_ipc_dir(void);
static int sorter(const void *first, const void *second);

/* Stores callback to report received messages. */
static ipc_callback callback;
/* Whether unit was initialized and what's the result (-1 is error, 1 is
 * success). */
static int initialized;
/* Path to the pipe used by this instance. */
static char pipe_path[PATH_MAX];
/* Opened file of the pipe. */
static FILE *pipe_file;

int
ipc_enabled(void)
{
	return 1;
}

void
ipc_init(ipc_callback callback_func)
{
	assert(!initialized && "Repeated initialization?");

	callback = callback_func;

	pipe_file = create_pipe(pipe_path, sizeof(pipe_path));
	if(pipe_file == NULL)
	{
		initialized = -1;
		return;
	}

	atexit(&clean_at_exit);
	initialized = 1;
}

/* Frees resources used by IPC. */
static void
clean_at_exit(void)
{
	fclose(pipe_file);
	unlink(pipe_path);
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
	uint32_t size;
	char *pkg;
	char *p;

#ifndef _WIN32
	fd_set ready;
	int max_fd;
	struct timeval ts = { .tv_sec = 0, .tv_usec = 10000 };
#endif

	if(fread(&size, sizeof(size), 1U, pipe_file) != 1U)
	{
		return NULL;
	}

	pkg = malloc(size + 2U);
	if(pkg == NULL)
	{
		return NULL;
	}

#ifndef _WIN32
	max_fd = fileno(pipe_file);
	FD_ZERO(&ready);
	FD_SET(max_fd, &ready);
#endif

	p = pkg;
	while(size != 0U
#ifndef _WIN32
			&& select(max_fd + 1, &ready, NULL, NULL, &ts) > 0
#endif
			)
	{
		size_t read;

#ifdef _WIN32
		/* TODO: maybe use OVERLAPPED I/O on Windows instead, it's just so
		 *       inconvenient... */
		usleep(10000);
#endif

		read = fread(p, 1U, size, pipe_file);
		size -= read;
		p += read;

		if(read == 0U)
		{
			break;
		}

#ifndef _WIN32
		ts.tv_sec = 0;
		ts.tv_usec = 10000;
#endif
	}

#ifdef _WIN32
	{
		/* Weird requirement for named pipes, need to break and set connection every
		 * time. */
		const HANDLE pipe_handle = (HANDLE)_get_osfhandle(fileno(pipe_file));
		DisconnectNamedPipe(pipe_handle);
		ConnectNamedPipe(pipe_handle, NULL);
	}
#endif

	if(size != 0U)
	{
		free(pkg);
		return NULL;
	}

	/* Make sure we have two trailing zeroes. */
	*p++ = '\0';
	*p = '\0';

	return pkg;
}

/* Tries to open a pipe for communication.  Returns NULL on error or opened file
 * descriptor otherwise. */
static FILE *
create_pipe(char path_buf[], size_t len)
{
	int id = 0;
	FILE *f;

	do
	{
		snprintf(path_buf, len, "%s/" PREFIX "%03d", get_ipc_dir(), ++id);

		if(id == 0)
		{
			return NULL;
		}

		f = try_use_pipe(path_buf);
	}
	while(f == NULL);

	return f;
}

/* Either creates a pipe or reused previously abandoned one.  Returns NULL on
 * failure or valid file descriptor otherwise. */
static FILE *
try_use_pipe(const char path[])
{
	FILE *f;

#ifndef _WIN32
	int fd;

	/* Try to create a pipe. */
	if(mkfifo(path, 0600) == 0)
	{
		/* Open if created. */
		int fd = open(path, O_RDONLY | O_NONBLOCK);
		if(fd == -1)
		{
			return NULL;
		}
		return fdopen(fd, "r");
	}

	/* Fail fast if the error is not related to existence of the file or pipe
	 * exists and is in use. */
	if(errno != EEXIST || pipe_is_in_use(path))
	{
		return NULL;
	}

	/* Use this file if we're the only one willing to read from it. */
	fd = open(path, O_RDONLY | O_NONBLOCK);
#else
	HANDLE h;
	int fd;

	h = CreateNamedPipeA(path,
			PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE,
			PIPE_TYPE_BYTE | PIPE_NOWAIT | PIPE_REJECT_REMOTE_CLIENTS,
			PIPE_UNLIMITED_INSTANCES, 4096, 4096, 10, NULL);
	if(h == INVALID_HANDLE_VALUE)
	{
		return NULL;
	}

	fd = _open_osfhandle((intptr_t)h, _O_APPEND | _O_RDONLY);
	if(fd == -1)
	{
		CloseHandle(h);
		return NULL;
	}
#endif

	f = fdopen(fd, "r");
	if(f == NULL)
	{
		close(fd);
	}
	return f;
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
ipc_send(char *data[])
{
	/* FIXME: this shouldn't have fixed size. */
	char pkg[8192];
	size_t len;
	char *name;

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
		strcpy(pkg + len, *data);
		len += strlen(*data) + 1;
		data++;
	}
	pkg[len++] = '\0';

	name = get_the_only_target();
	if(name == NULL)
	{
		return 1;
	}

	return send_pkg(name, pkg, len);
}

/* Performs actual sending of package to another instance.  Returns zero on
 * success and non-zero otherwise. */
static int
send_pkg(const char whom[], const char what[], size_t len)
{
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

	if(enum_dir_content(data.ipc_dir, &add_to_list, &data) != 0)
	{
		*len = 0;
		return NULL;
	}

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
	if(strcmp(name, get_last_path_component(pipe_path)) == 0)
	{
		return 0;
	}

	/* On Windows it's guaranteed to be a valid pipe. */
#ifndef _WIN32
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

#ifndef _WIN32

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

/* Retrieves directory where FIFO objects are created.  Returns the path. */
static const char *
get_ipc_dir(void)
{
#ifndef _WIN32
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

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
