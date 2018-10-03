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

#include "log.h"

#include <unistd.h>

#include <errno.h> /* errno */
#include <stdarg.h>
#include <stdio.h> /* stderr fprintf() */
#include <string.h> /* strerror() */
#include <time.h>

#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "../status.h"
#include "str.h"
#include "utils.h"

static FILE *log;
static int verbosity;

static void init(const char log_path[]);
static void log_time(void);

void
init_logger(int verbosity_level, const char log_path[])
{
	verbosity = verbosity_level;
	if(verbosity > 0)
	{
		init(log_path);
	}
}

void
reinit_logger(const char log_path[])
{
	if(verbosity <= 0)
	{
		return;
	}

	if(log != NULL)
	{
		log_time();
		fprintf(log, " ===== Logger reinitialization to '%s' =====\n", log_path);
		fclose(log);
	}

	init(log_path);
}

static void
init(const char log_path[])
{
	if(is_null_or_empty(log_path))
	{
		return;
	}

	log = os_fopen(log_path, "a");
	if(log == NULL)
	{
		if(curr_stats.load_stage == 0)
		{
			fprintf(stderr, "Failed to open log file (%s): %s\n", log_path,
					strerror(errno));
		}
		return;
	}
	setvbuf(log, NULL, _IONBF, 0);

	fprintf(log, "\n");
	log_time();
	fprintf(log, " ===== Started vifm =====\n");
}

void
log_prefix(const char *file, const char *func, int line)
{
	if(verbosity <= 0 || log == NULL)
		return;

	log_time();
	fprintf(log, " at %s:%d (%s) in process #%lu\n", file, line, func,
			(long unsigned int)get_pid());
}

void
log_vifm_state(void)
{
	if(verbosity <= 0 || log == NULL)
		return;

	fprintf(log, "               Load stage: %d\n", curr_stats.load_stage);
}

void
log_serror(const char *file, const char *func, int line, int no)
{
	if(verbosity <= 0 || log == NULL)
		return;

	log_prefix(file, func, line);
	fprintf(log, "               errno: %s\n", strerror(no));
}

#ifdef _WIN32
void
log_werror(const char *file, const char *func, int line, int no)
{
	if(verbosity <= 0 || log == NULL)
		return;

	log_prefix(file, func, line);
	fprintf(log, "               Windows error code: %d\n", no);
}
#endif

void
log_msg(const char *msg, ...)
{
	va_list ap;

	if(verbosity <= 0 || log == NULL)
	{
		return;
	}

	va_start(ap, msg);

	fprintf(log, "               ");
	vfprintf(log, msg, ap);
	fputc('\n', log);

	va_end(ap);
}

static void
log_time(void)
{
	time_t t;
	struct tm *tm_ptr;
	char buf[128];

	t = time(NULL);
	tm_ptr = localtime(&t);
	strftime(buf, sizeof(buf), "%y.%m.%d %H:%M", tm_ptr);
	fprintf(log, "%s", buf);
}

void
log_cwd(void)
{
	char buf[PATH_MAX + 1];

	if(verbosity <= 0 || log == NULL)
		return;

	if(os_getcwd(buf, sizeof(buf)) == NULL)
		log_msg("%s", "getwd() error");
	else
		log_msg("getwd() returned \"%s\"", buf);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
