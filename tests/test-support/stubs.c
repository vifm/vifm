#include <stdlib.h> /* abort() */

#include "../../src/ui/ui.h"

int vifm_tests_exited;

void
vifm_restart(void)
{
	/* Do nothing. */
}

void
vifm_try_leave(int write_info, int cquit, int force)
{
	vifm_tests_exited = 1;
}

void
vifm_choose_files(view_t *view, int nfiles, char *files[])
{
	abort();
}

void
vifm_finish(const view_t *view, int nfiles, char *files[])
{
	/* Do nothing. */
}

void
vifm_exit(int exit_code)
{
	/* Do nothing. */
}

int
vifm_testing(void)
{
	return 1;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
