#include <unistd.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "config.h"

#include "log.h"

static FILE *log;
static int verbosity;

static void log_time(void);

void
init_logger(int verbosity_level)
{
	verbosity = verbosity_level;
	if(verbosity <= 0)
		return;
	
	log = fopen(cfg.log_file, "a");
	setvbuf(log, NULL, _IONBF, 0);

	fprintf(log, "\n");
	log_time();
	fprintf(log, "\t\tStarted vifm\n");
}

void
log_error(const char *file, const char *func, int line)
{
	if(verbosity <= 0)
		return;

	log_time();
	fprintf(log, " at %s:%d (%s)\n", file, line, func);
}

void
log_serror(const char *file, const char *func, int line, int no)
{
	if(verbosity <= 0)
		return;

	log_time();
	fprintf(log, " at %s:%d (%s)\n", file, line, func);
	fprintf(log, "\t\t%s\n", strerror(no));
}

void
log_msg(const char *msg, ...)
{
	va_list ap;
	va_start(ap, msg);

	if(verbosity <= 0)
		return;

	fputc('\t', log);
	fputc('\t', log);
	vfprintf(log, msg, ap);
	fputc('\n', log);

	va_end(ap);
}

static void
log_time(void)
{
	time_t t;

	if(verbosity <= 0)
		return;

	t = time(NULL);
	fprintf(log, "%s", ctime(&t));
}

void
log_cwd(void)
{
	char buf[PATH_MAX];

	if(getcwd(buf, sizeof(buf)) == NULL)
		log_msg("%s", "getwd() error");
	else
		log_msg("getwd() returned \"%s\"", buf);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
