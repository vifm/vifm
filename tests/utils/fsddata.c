#include <stic.h>

#include <unistd.h> /* rmdir() */

#include <string.h> /* strdup() */

#include "../../src/compat/os.h"
#include "../../src/utils/fsddata.h"

TEST(freeing_null_fsddata_is_ok)
{
	fsddata_free(NULL);
}

TEST(memory_is_handled_by_fsddata_correctly_on_free)
{
	fsddata_t *const fsdd = fsddata_create(0, 1);
	assert_success(fsddata_set(fsdd, ".", strdup("str")));
	/* Freeing the structure should free the string (absence of leaks should be
	 * checked by external tools). */
	fsddata_free(fsdd);
}

TEST(memory_is_handled_by_fsddata_correctly_on_set)
{
	fsddata_t *const fsdd = fsddata_create(0, 1);
	assert_success(fsddata_set(fsdd, ".", strdup("str1")));
	/* Overwriting the value should replace the old one (absence of leaks should
	 * be checked by external tools). */
	assert_success(fsddata_set(fsdd, ".", strdup("str2")));
	fsddata_free(fsdd);
}

TEST(pointer_to_the_same_memory_is_returned)
{
	fsddata_t *const fsdd = fsddata_create(0, 1);
	char *const str = strdup("str");
	void *ptr;
	assert_success(fsddata_set(fsdd, ".", str));
	assert_success(fsddata_get(fsdd, ".", &ptr));
	assert_true(ptr == str);
	fsddata_free(fsdd);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
