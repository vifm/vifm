#include <stic.h>

#include <stddef.h> /* NULL */
#include <stdio.h> /* remove() */
#include <string.h> /* strdup() */

#include "../../src/compat/os.h"
#include "../../src/utils/fsdata.h"

TEST(freeing_null_fsdata_is_ok)
{
	fsdata_free(NULL);
}

TEST(freeing_new_fsdata_is_ok)
{
	fsdata_t *const fsd = fsdata_create(0, 0);
	assert_non_null(fsd);
	fsdata_free(fsd);
}

TEST(get_returns_error_for_unknown_path)
{
	tree_val_t data;
	fsdata_t *const fsd = fsdata_create(0, 0);
	assert_failure(fsdata_get(fsd, ".", &data));
	fsdata_free(fsd);
}

TEST(get_does_not_alter_data_on_unknown_path)
{
	tree_val_t data = 0;
	fsdata_t *const fsd = fsdata_create(0, 0);
	assert_failure(fsdata_get(fsd, ".", &data));
	assert_true(data == 0);
	fsdata_free(fsd);
}

TEST(get_returns_previously_set_value)
{
	tree_val_t data = 0;
	fsdata_t *const fsd = fsdata_create(0, 0);
	assert_success(fsdata_set(fsd, ".", data++));
	assert_success(fsdata_get(fsd, ".", &data));
	assert_true(data == 0);
	fsdata_free(fsd);
}

TEST(set_overwrites_previous_value)
{
	tree_val_t data = 0;
	fsdata_t *const fsd = fsdata_create(0, 0);
	assert_success(fsdata_set(fsd, ".", data++));
	assert_success(fsdata_set(fsd, ".", data++));
	assert_success(fsdata_get(fsd, ".", &data));
	assert_true(data == 1);
	fsdata_free(fsd);
}

TEST(siblings_are_independent)
{
	tree_val_t data = 0;
	fsdata_t *const fsd = fsdata_create(0, 0);
	assert_success(os_mkdir(SANDBOX_PATH "/dir1", 0700));
	assert_success(os_mkdir(SANDBOX_PATH "/dir2", 0700));

	assert_success(fsdata_set(fsd, SANDBOX_PATH "/dir1", data++));
	assert_success(fsdata_set(fsd, SANDBOX_PATH "/dir2", data++));

	assert_success(fsdata_get(fsd, SANDBOX_PATH "/dir1", &data));
	assert_true(data == 0);
	assert_success(fsdata_get(fsd, SANDBOX_PATH "/dir2", &data));
	assert_true(data == 1);

	assert_success(remove(SANDBOX_PATH "/dir1"));
	assert_success(remove(SANDBOX_PATH "/dir2"));
	fsdata_free(fsd);
}

TEST(set_does_not_work_for_path_that_do_not_exist)
{
	tree_val_t data = 0;
	fsdata_t *const fsd = fsdata_create(0, 0);
	assert_failure(fsdata_set(fsd, "no/path", data));
	fsdata_free(fsd);
}

TEST(set_does_not_work_for_path_that_do_not_exist_anymore)
{
	tree_val_t data = 0;
	fsdata_t *const fsd = fsdata_create(0, 0);
	assert_success(os_mkdir(SANDBOX_PATH "/dir", 0700));
	assert_success(fsdata_set(fsd, SANDBOX_PATH "/dir", data));
	assert_success(remove(SANDBOX_PATH "/dir"));
	assert_failure(fsdata_set(fsd, SANDBOX_PATH "/dir", data));
	fsdata_free(fsd);
}

TEST(memory_is_handled_by_fsdata_correctly_on_free)
{
	union
	{
		char *name;
		tree_val_t data;
	} u = { .name = strdup("str") };

	fsdata_t *const fsd = fsdata_create(0, 1);
	assert_non_null(u.name);
	assert_success(fsdata_set(fsd, ".", u.data));
	/* Freeing the structure should free the string (absence of leaks should be
	 * checked by external tools). */
	fsdata_free(fsd);
}

TEST(memory_is_handled_by_fsdata_correctly_on_set)
{
	union
	{
		char *name;
		tree_val_t data;
	} u = { .name = strdup("str1") };

	fsdata_t *const fsd = fsdata_create(0, 1);
	assert_non_null(u.name);
	assert_success(fsdata_set(fsd, ".", u.data));
	u.name = strdup("str2");
	/* Overwriting the value should replace the old one (absence of leaks should
	 * be checked by external tools). */
	assert_success(fsdata_set(fsd, ".", u.data));
	fsdata_free(fsd);
}

TEST(end_value_is_preferred_over_intermediate_value)
{
	tree_val_t data = 0;
	fsdata_t *const fsd = fsdata_create(1, 0);
	assert_success(os_mkdir(SANDBOX_PATH "/dir", 0700));

	assert_success(fsdata_set(fsd, SANDBOX_PATH, data++));
	assert_success(fsdata_set(fsd, SANDBOX_PATH "/dir", data++));
	assert_success(fsdata_get(fsd, SANDBOX_PATH "/dir", &data));
	assert_true(data == 1);

	assert_success(remove(SANDBOX_PATH "/dir"));
	fsdata_free(fsd);
}

TEST(intermediate_value_is_returned_if_end_value_is_not_found)
{
	tree_val_t data = 0;
	fsdata_t *const fsd = fsdata_create(1, 0);
	assert_success(os_mkdir(SANDBOX_PATH "/dir", 0700));

	assert_success(fsdata_set(fsd, SANDBOX_PATH, data++));
	assert_success(fsdata_get(fsd, SANDBOX_PATH "/dir", &data));
	assert_true(data == 0);

	assert_success(remove(SANDBOX_PATH "/dir"));
	fsdata_free(fsd);
}
