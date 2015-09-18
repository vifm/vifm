#include <stic.h>

#include <stddef.h> /* NULL */
#include <string.h> /* memset() */

#include "../../src/utils/dynarray.h"

TEST(shrinked_dynarray_can_be_extended_safely)
{
	/* Allocate some initial space. */
	void *darray = dynarray_extend(NULL, 16);

	/* Extend it. */
	darray = dynarray_extend(darray, 1024);
	/* Now capacity should be strictly bigger than 16 + 1024. */
	darray = dynarray_shrink(darray);
	/* Now capacity should be equal to 16 + 1024. */

	/* Reserve more space, in case of bug, this can think that we have enough
	 * space. */
	darray = dynarray_extend(darray, 256);

	/* Make sure that memory is usable (if it's not, crash should follow). */
	memset(darray, -1, 16 + 1024 + 256);

	dynarray_free(darray);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
