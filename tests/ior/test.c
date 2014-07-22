#include "seatest.h"

#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */

#include "../../src/cfg/config.h"

void chgrp_tests(void);
void chmod_tests(void);
void chown_tests(void);
void cp_tests(void);
void mv_tests(void);
void rm_tests(void);

static void
setup(void)
{
	cfg.shell = strdup("/bin/bash");
}

static void
teardown(void)
{
	free(cfg.shell);
}

void
all_tests(void)
{
	chgrp_tests();
	chmod_tests();
	chown_tests();
	cp_tests();
	mv_tests();
	rm_tests();
}

int
main(void)
{
	suite_setup(setup);
	suite_teardown(teardown);
	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
