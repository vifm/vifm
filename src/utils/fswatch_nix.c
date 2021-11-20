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
#include <sys/types.h> /* dev_t ino_t */
#include <unistd.h> /* close() read() */

#include <errno.h> /* EAGAIN errno */
#include <stddef.h> /* NULL */
#include <stdint.h> /* uint32_t */
#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */
#include <time.h> /* time_t time() */

#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "trie.h"

/* TODO: consider implementation that could reuse already available descriptor
 *       by just removing old watch and then adding a new one. */

/* Watcher data. */
struct fswatch_t
{
	/* Path that's being watched. */
	char *path;
	/* File descriptor for inotify. */
	int fd;
	/* Watch descriptor. */
	int wd;
	/* Trie to keep track of per file frequency of notifications. */
	trie_t *stats;
	/* To monitor mount events, which aren't reported by inotify. */
	dev_t dev;
	ino_t inode;
};

/* Per file statistics information. */
typedef struct
{
	time_t last_update;  /* Time of the last change to the file. */
	time_t banned_until; /* Moment until notifications should be ignored. */
	uint32_t ban_mask;   /* Events right before the ban. */
	int count;           /* How many times file changed continuously in the last
	                        several seconds. */
}
notif_stat_t;

static FSWatchState poll_for_replacement(fswatch_t *w);
static int update_file_stats(fswatch_t *w, const struct inotify_event *e,
		time_t now);

/* Events we're interested in. */
static const uint32_t EVENTS_MASK = IN_ATTRIB | IN_MODIFY | IN_CLOSE_WRITE
                                  | IN_CREATE | IN_DELETE | IN_EXCL_UNLINK
                                  | IN_MOVED_FROM | IN_MOVED_TO;

fswatch_t *
fswatch_create(const char path[])
{
	struct stat st;
	if(os_stat(path, &st) != 0)
	{
		return NULL;
	}

	fswatch_t *const w = malloc(sizeof(*w));
	if(w == NULL)
	{
		return NULL;
	}

	w->dev = st.st_dev;
	w->inode = st.st_ino;

	/* Create tree to collect update frequency statistics. */
	w->stats = trie_create(&free);
	if(w->stats == NULL)
	{
		trie_free(w->stats);
		free(w);
		return NULL;
	}

	/* Create inotify instance. */
	w->fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
	if(w->fd == -1)
	{
		trie_free(w->stats);
		free(w);
		return NULL;
	}

	/* Add directory to watch. */
	w->wd = inotify_add_watch(w->fd, path, EVENTS_MASK);
	if(w->wd == -1)
	{
		close(w->fd);
		trie_free(w->stats);
		free(w);
		return NULL;
	}

	w->path = strdup(path);
	if(w->path == NULL)
	{
		fswatch_free(w);
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
		trie_free(w->stats);
		close(w->fd);
		free(w);
	}
}

FSWatchState
fswatch_poll(fswatch_t *w)
{
	enum { MAX_READS = 100 };
	enum { BUF_LEN = (10 * (sizeof(struct inotify_event) + NAME_MAX + 1)) };

	char buf[BUF_LEN];
	int nread;
	int changed = 0;
	int nreads = 0;
	const time_t now = time(NULL);

	do
	{
		char *p;
		struct inotify_event *e;

		/* Receive a package of events. */
		nread = read(w->fd, buf, BUF_LEN);
		if(nread < 0)
		{
			if(errno != EAGAIN)
			{
				return FSWS_ERRORED;
			}
			break;
		}

		/* And process each of them separately. */
		for(p = buf; p < buf + nread; p += sizeof(struct inotify_event) + e->len)
		{
			e = (struct inotify_event *)p;
			if((e->mask & IN_IGNORED) != 0 && e->wd == w->wd)
			{
				return poll_for_replacement(w);
			}

			if((e->mask & EVENTS_MASK) != 0 && update_file_stats(w, e, now))
			{
				changed = 1;
			}
		}

		/* Limit maximum number of reads to ensure that we won't spend all our time
		 * in this loop. */
		if(++nreads > MAX_READS)
		{
			break;
		}
	}
	while(nread != 0);

	return (changed ? FSWS_UPDATED : poll_for_replacement(w));
}

/* Detects replacement of path's target.  Returns watcher's state. */
static FSWatchState
poll_for_replacement(fswatch_t *w)
{
	struct stat st;
	if(os_stat(w->path, &st) != 0)
	{
		return FSWS_ERRORED;
	}

	if(w->dev == st.st_dev && w->inode == st.st_ino)
	{
		return FSWS_UNCHANGED;
	}

	w->dev = st.st_dev;
	w->inode = st.st_ino;

	int wd = inotify_add_watch(w->fd, w->path, EVENTS_MASK);
	if(wd == -1)
	{
		return FSWS_ERRORED;
	}

	/* We ignore error from this call because input should always be correct from
	 * our side, yet watch descriptor might have been killed by the kernel.  So
	 * checking for an error causes false positives. */
	(void)inotify_rm_watch(w->fd, w->wd);

	w->wd = wd;
	return FSWS_REPLACED;
}

/* Updates information about a file event is about.  Returns non-zero if this is
 * an interesting event that's worth attention (e.g. re-reading information from
 * file system), otherwise zero is returned. */
static int
update_file_stats(fswatch_t *w, const struct inotify_event *e, time_t now)
{
	enum { HITS_TO_BAN_AFTER = 5, BAN_SECS = 5 };

	const uint32_t IMPORTANT_EVENTS = IN_CREATE | IN_DELETE | IN_MOVED_FROM
	                                | IN_MOVED_TO | IN_Q_OVERFLOW;

	const char *const fname = (e->len == 0U) ? "." : e->name;
	void *data;
	notif_stat_t *stats;

	/* See if we already know this file and retrieve associated information if
	 * so. */
	if(trie_get(w->stats, fname, &data) != 0)
	{
		notif_stat_t *const stats = malloc(sizeof(*stats));
		if(stats != NULL)
		{
			stats->last_update = now;
			stats->banned_until = 0U;
			stats->count = 1;
			if(trie_set(w->stats, fname, stats) != 0)
			{
				free(stats);
			}
		}

		return 1;
	}

	stats = data;

	/* Unban entry on any of the "important" events. */
	if(e->mask & IMPORTANT_EVENTS)
	{
		stats->banned_until = 0U;
		stats->count = 1;
	}

	/* Ignore events during banned period, unless it's something new. */
	if(now < stats->banned_until && !(e->mask & ~stats->ban_mask))
	{
		return 0;
	}

	/* Treat events happened in the next second as a sequence. */
	stats->count = (now - stats->last_update <= 1) ? (stats->count + 1) : 1;

	/* Files that cause relatively long sequence of events are banned for a
	 * while.  Don't ban the directory itself, we don't want to miss changes of
	 * file list. */
	if(stats->count > HITS_TO_BAN_AFTER && e->len != 0U)
	{
		stats->ban_mask = e->mask;
		stats->banned_until = now + BAN_SECS;
	}

	stats->last_update = now;

	return 1;
}

#else

#include "filemon.h"

#include <string.h> /* strdup() */

/* Watcher data. */
struct fswatch_t
{
	filemon_t filemon; /* Stamp based monitoring. */
	char *path;        /* Path to the file being watched. */
};

fswatch_t *
fswatch_create(const char path[])
{
	fswatch_t *const w = malloc(sizeof(*w));
	if(w == NULL)
	{
		return NULL;
	}

	if(filemon_from_file(path, FMT_MODIFIED, &w->filemon) != 0)
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

FSWatchState
fswatch_poll(fswatch_t *w)
{
	filemon_t filemon;
	if(filemon_from_file(w->path, FMT_MODIFIED, &filemon) != 0)
	{
		return FSWS_ERRORED;
	}

	int changed = !filemon_equal(&w->filemon, &filemon);

	w->filemon = filemon;

	return (changed ? FSWS_UPDATED : FSWS_UNCHANGED);
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
