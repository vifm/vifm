#include "utils.h"

#include <stddef.h> /* NULL */
#include <string.h> /* strcmp() */

#include "../../src/bmarks.h"

static void bmarks_cb(const char path[], const char tags[], time_t timestamp,
		void *arg);

static const char *result_tags;

const char *
get_tags(const char path[])
{
	result_tags = NULL;
	bmarks_list(&bmarks_cb, (void *)path);
	return result_tags;
}

static void
bmarks_cb(const char path[], const char tags[], time_t timestamp, void *arg)
{
	if(strcmp(path, arg) == 0)
	{
		result_tags = tags;
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
