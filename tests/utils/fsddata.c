#include <stic.h>

#include <string.h> /* strdup() */

#include "../../src/utils/fsddata.h"

TEST(memory_is_handled_by_fsddata_correctly_on_free)
{
	fsddata_t *const fsd = fsddata_create(0);
	assert_success(fsddata_set(fsd, ".", strdup("str")));
	/* Freeing the structure should free the string (absence of leaks should be
	 * checked by external tools). */
	fsddata_free(fsd);
}

TEST(memory_is_handled_by_fsddata_correctly_on_set)
{
	fsddata_t *const fsd = fsddata_create(0);
	assert_success(fsddata_set(fsd, ".", strdup("str1")));
	/* Overwriting the value should replace the old one (absence of leaks should
	 * be checked by external tools). */
	assert_success(fsddata_set(fsd, ".", strdup("str2")));
	fsddata_free(fsd);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
