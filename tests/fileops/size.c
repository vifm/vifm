#include <stic.h>

#include <unistd.h> /* usleep() */

#include <string.h> /* strcpy() strdup() */

#include "../../src/cfg/config.h"
#include "../../src/utils/dynarray.h"
#include "../../src/filelist.h"
#include "../../src/fileops.h"
#include "../../src/status.h"

TEST(directory_size_is_calculated_in_bg)
{
	int i;
	uint64_t size, nitems;

	strcpy(lwin.curr_dir, TEST_DATA_PATH);

	init_status(&cfg);

	lwin.list_rows = 1;
	lwin.list_pos = 0;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("various-sizes");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[0].type = FT_DIR;
	lwin.dir_entry[0].selected = 1;

	calculate_size_bg(&lwin, 0);

	dcache_get_at(TEST_DATA_PATH "/various-sizes", &size, &nitems);
	while(size == DCACHE_UNKNOWN)
	{
		usleep(200);
		dcache_get_at(TEST_DATA_PATH "/various-sizes", &size, &nitems);
	}

	assert_int_equal(73728, size);

	for(i = 0; i < lwin.list_rows; ++i)
	{
		free_dir_entry(&lwin, &lwin.dir_entry[i]);
	}
	dynarray_free(lwin.dir_entry);

	reset_status(&cfg);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
