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

int
ipc_enabled(void)
{
	return 0;
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

#include <sys/stat.h> /* mkfifo */
#include <sys/types.h>
#include <sys/select.h> /* FD_* select */
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

static void clean_at_exit(void);
static FILE * create_pipe(char path_buf[], size_t len);
static char * receive(void);
static FILE * try_use_pipe(const char path[]);
static void handle_pkg(const char pkg[]);
static int send(const char whom[], const char what[], size_t len);
static char * get_the_only_target(void);
static char ** list_pipes(int *len);
static int pipe_is_in_use(const char path[]);
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

	pkg = receive();
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
receive(void)
{
	uint32_t size;
	char *pkg;
	char *p;

	fd_set ready;
	int max_fd;
	struct timeval ts = { .tv_sec = 0, .tv_usec = 10000 };

	if(fread(&size, sizeof(size), 1U, pipe_file) != 1U)
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
		const size_t read = fread(p, 1U, size, pipe_file);
		size -= read;
		p += read;

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
		snprintf(path_buf, len, "%s/" PREFIX "%03d", get_tmpdir(), ++id);

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
	return fdopen(fd, "r");
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

	return send(name, pkg, len);
}

/* Performs actual sending of package to another instance.  Returns zero on
 * success and non-zero otherwise. */
static int
send(const char whom[], const char what[], size_t len)
{
	char path[PATH_MAX];
	int fd;
	FILE *dst;
	uint32_t size;

	snprintf(path, sizeof(path), "%s/" PREFIX "%s", get_tmpdir(), whom);

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
	char **list = list_pipes(&len);

	if(len == 0)
	{
		return NULL;
	}

	name = list[0];
	list[0] = NULL;
	free_string_array(list, len);

	return name;
}

/* Enumerates identifiers of other instances.  Returns pointer to the list of
 * the length *len. */
static char **
list_pipes(int *len)
{
	DIR *dir;
	struct dirent *d;
	const char *tmp_dir = get_tmpdir();

	char **list = NULL;
	*len = 0;

	if((dir = opendir(tmp_dir)) == NULL)
	{
		return NULL;
	}

	while((d = readdir(dir)) != NULL)
	{
		char path[PATH_MAX];
		struct stat statbuf;

		if(!starts_with_lit(d->d_name, PREFIX))
		{
			continue;
		}

		/* Skip ourself. */
		if(strcmp(d->d_name, get_last_path_component(pipe_path)) == 0)
		{
			continue;
		}

		snprintf(path, sizeof(path), "%s/%s", tmp_dir, d->d_name);

		if(stat(path, &statbuf) != 0 || !S_ISFIFO(statbuf.st_mode) ||
				!pipe_is_in_use(path))
		{
			continue;
		}

		*len = add_to_string_array(&list, *len, 1, d->d_name + strlen(PREFIX));
	}
	closedir(dir);

	qsort(list, *len, sizeof(*list), &sorter);

	return list;
}

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
