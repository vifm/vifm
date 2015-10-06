/* vifm
 * Copyright (C) 2015 xaizek.
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

#include "fswatch.h"

#include <stdlib.h> /* free() malloc() */

#ifdef HAVE_INOTIFY

#include <sys/inotify.h> /* IN_* inotify_* */
#include <unistd.h> /* close() read() */

#include <errno.h> /* EAGAIN errno */
#include <stddef.h> /* NULL */

#include "../compat/fs_limits.h"

/* TODO: consider implementation that could reuse already available descriptor
 *       by just removing old watch and then adding a new one. */

/* Watcher data. */
struct fswatch_t
{
	int fd; /* File descriptor for inotify. */
};

fswatch_t *
fswatch_create(const char path[])
{
	int wd;

	fswatch_t *const w = malloc(sizeof(*w));
	if(w == NULL)
	{
		return NULL;
	}

	/* Create inotify instance. */
	w->fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
	if(w->fd == -1)
	{
		free(w);
		return NULL;
	}

	/* Add directory to watch. */
	wd = inotify_add_watch(w->fd, path, IN_ATTRIB | IN_MODIFY | IN_CREATE |
			IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_EXCL_UNLINK);
	if(wd == -1)
	{
		close(w->fd);
		free(w);
		return NULL;
	}

	return w;
}

void
fswatch_free(fswatch_t *w)
{
	if(w != NULL)
	{
		close(w->fd);
		free(w);
	}
}

int
fswatch_changed(fswatch_t *w, int *error)
{
  enum { BUF_LEN = (10 * (sizeof(struct inotify_event) + NAME_MAX + 1)) };

	char buf[BUF_LEN];
	int read_total;
	int nread;

	read_total = 0;
	*error = 0;
	do
	{
		nread = read(w->fd, buf, BUF_LEN);
		if(nread < 0)
		{
			if(errno == EAGAIN)
			{
				break;
			}

			*error = 1;
			break;
		}

		read_total += nread;
	}
	while(nread != 0);

	return read_total != 0;
}

#else

#include "filemon.h"

#include <string.h> /* strdup() */

/* Watcher data. */
struct fswatch_t
{
	filemon_t filemon; /* Stamp based monitoring. */
	char *path;
};

fswatch_t *
fswatch_create(const char path[])
{
	fswatch_t *const w = malloc(sizeof(*w));
	if(w == NULL)
	{
		return NULL;
	}

	if(filemon_from_file(path, &w->filemon) != 0)
	{
		free(w);
		return NULL;
	}

	w->path = strdup(path);
	if(w->path == NULL)
	{
		free(w);
		return NULL;
	}

	return w;
}

void
fswatch_free(fswatch_t *w)
{
	if(w != NULL)
	{
		free(w->path);
		free(w);
	}
}

int
fswatch_changed(fswatch_t *w, int *error)
{
	int changed;

	filemon_t filemon;
	if(filemon_from_file(w->path, &filemon) != 0)
	{
		*error = 1;
		return 1;
	}

	*error = 0;
	changed = !filemon_equal(&w->filemon, &filemon);

	filemon_assign(&w->filemon, &filemon);

	return changed;
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
