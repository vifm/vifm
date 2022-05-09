#include <stic.h>

#include "../../src/io/private/ionotif.h"
#include "../../src/fops_common.h"

SETUP_ONCE()
{
	/* To register I/O notification callback. */
	fops_init(NULL, NULL);
}

TEST(empty_estimation_is_ok_for_progress_handler)
{
	ops_t *ops = fops_get_ops(OP_NONE, "descr", "/base/dir", "/target/dir");

	ionotif_notify(IO_PS_ESTIMATING, ops->estim);

	fops_free_ops(ops);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
