#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "filelist.h"
#include "options.h"
#include "ui.h"

#include "opt_handlers.h"

static void add_options(void);
static void load_options(void);
static void print_func(const char *msg, const char *description);
static void iec_handler(enum opt_op op, union optval_t val);
static void timefmt_handler(enum opt_op op, union optval_t val);

static int save_msg;

void
init_options(void)
{
	set_print_handler(&print_func);
	add_options();
	load_options();
}

static void
add_options(void)
{
	add_option("iec", OPT_BOOL, 0, NULL, &iec_handler);
	add_option("timefmt", OPT_STR, 0, NULL, &timefmt_handler);
}

static void
load_options(void)
{
	union optval_t val;

	val.int_val = cfg.use_iec_prefixes;
	set_option("iec", val);

	val.str_val = cfg.time_format + 1;
	set_option("timefmt", val);
}

int
process_set_args(const char *args)
{
	save_msg = 0;
	set_options(args);
	return save_msg;
}

static void
print_func(const char *msg, const char *description)
{
	if(*msg == '\0')
	{
		status_bar_message(description);
	}
	else
	{
		char buf[strlen(msg) + 2 + strlen(description) + 1];
		sprintf(buf, "%s: %s", msg, description);
		status_bar_message(buf);
	}
	save_msg = 1;
}

static void
iec_handler(enum opt_op op, union optval_t val)
{
	cfg.use_iec_prefixes = val.int_val;

	redraw_lists();
}

static void
timefmt_handler(enum opt_op op, union optval_t val)
{
	free(cfg.time_format);
	cfg.time_format = malloc(1 + strlen(val.str_val) + 1);
	strcpy(cfg.time_format, " ");
	strcat(cfg.time_format, val.str_val);

	redraw_lists();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
