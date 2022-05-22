#include <stic.h>

#include <unistd.h> /* chdir() */

#include <stddef.h> /* NULL */
#include <stdlib.h> /* fclose() fopen() free() remove() */
#include <string.h> /* strcpy() */

#include "../../src/cfg/config.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/str.h"
#include "../../src/bmarks.h"
#include "../../src/cmd_core.h"
#include "../../src/ops.h"

static void bmarks_cb(const char p[], const char t[], time_t timestamp,
		void *arg);

static char *path;

SETUP_ONCE()
{
	cfg.use_system_calls = 1;
}

SETUP()
{
	assert_success(chdir(SANDBOX_PATH));

	init_commands();
	lwin.selected_files = 0;
	strcpy(lwin.curr_dir, ".");
	path = NULL;

	curr_view = &lwin;
}

TEARDOWN()
{
	assert_success(exec_commands("delbmarks!", &lwin, CIT_COMMAND));
	free(path);
}

TEST(rename_triggers_bmark_update)
{
	FILE *const f = fopen("old", "w");
	if(f != NULL)
	{
		fclose(f);
	}

	assert_success(exec_commands("bmark! old tag", &lwin, CIT_COMMAND));

	bmarks_list(&bmarks_cb, NULL);
	assert_string_equal("./old", path);

	assert_int_equal(OPS_SUCCEEDED, perform_operation(OP_MOVE, NULL, NULL,
				"./old", "./new"));

	bmarks_list(&bmarks_cb, NULL);
	assert_string_equal("./new", path);

	assert_success(remove("new"));
}

static void
bmarks_cb(const char p[], const char t[], time_t timestamp, void *arg)
{
	replace_string(&path, p);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
