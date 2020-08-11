#include "stubs.h"

#include <stdlib.h> /* abort() */

#include "../../src/vifm.h"

struct view_t;

int vifm_tests_exited;

void
vifm_reexec_startup_commands(void)
{
	/* Do nothing. */
}

void
vifm_try_leave(int write_info, int cquit, int force)
{
	vifm_tests_exited = 1;
}

void
vifm_choose_files(struct view_t *view, int nfiles, char *files[])
{
	abort();
}

void
vifm_finish(const char message[])
{
	abort();
}

void
vifm_exit(int exit_code)
{
	abort();
}

int
vifm_testing(void)
{
	return 1;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
