#include <stic.h>

#include <unistd.h> /* usleep() */

#include <string.h> /* strcpy() strdup() */

#include "../../src/cfg/config.h"
#include "../../src/utils/dynarray.h"
#include "../../src/filelist.h"
#include "../../src/fops_misc.h"
#include "../../src/status.h"

#include "utils.h"

static void setup_single_entry(view_t *view, const char name[]);
static uint64_t wait_for_size(const char path[]);

SETUP()
{
	init_status(&cfg);
}

TEARDOWN()
{
	view_teardown(&lwin);

	reset_status(&cfg);
}

TEST(directory_size_is_calculated_in_bg)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH);
	setup_single_entry(&lwin, "various-sizes");
	lwin.dir_entry[0].selected = 1;

	fops_size_bg(&lwin, 0);
	assert_int_equal(73728, wait_for_size(TEST_DATA_PATH "/various-sizes"));
}

TEST(parent_dir_entry_triggers_calculation_of_current_dir)
{
	strcpy(lwin.curr_dir, TEST_DATA_PATH "/various-sizes");
	setup_single_entry(&lwin, "..");
	lwin.user_selection = 1;

	fops_size_bg(&lwin, 0);
	assert_int_equal(73728, wait_for_size(TEST_DATA_PATH "/various-sizes"));
}

static void
setup_single_entry(view_t *view, const char name[])
{
	view->user_selection = 1;
	view->list_rows = 1;
	view->list_pos = 0;
	view->dir_entry = dynarray_cextend(NULL,
			view->list_rows*sizeof(*view->dir_entry));
	view->dir_entry[0].name = strdup(name);
	view->dir_entry[0].origin = &view->curr_dir[0];
	view->dir_entry[0].type = FT_DIR;
}

static uint64_t
wait_for_size(const char path[])
{
	uint64_t size, nitems;
	int counter;

	dcache_get_at(path, &size, &nitems);
	counter = 0;
	while(size == DCACHE_UNKNOWN)
	{
		usleep(2000);
		dcache_get_at(path, &size, &nitems);
		if(++counter > 100)
		{
			assert_fail("Waiting for too long.");
			break;
		}
	}

	return size;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
