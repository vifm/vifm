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
	fsdata_t *const fsd = fsdata_create(0);
	assert_non_null(fsd);
	fsdata_free(fsd);
}

TEST(get_returns_error_for_unknown_path)
{
	int data;
	fsdata_t *const fsd = fsdata_create(0);
	assert_failure(fsdata_get(fsd, ".", &data, sizeof(data)));
	fsdata_free(fsd);
}

TEST(get_returns_error_for_wrong_path)
{
	int data;
	fsdata_t *const fsd = fsdata_create(0);
	assert_failure(fsdata_get(fsd, "no/path", &data, sizeof(data)));
	fsdata_free(fsd);
}

TEST(get_does_not_alter_data_on_unknown_path)
{
	int data = 0;
	fsdata_t *const fsd = fsdata_create(0);
	assert_failure(fsdata_get(fsd, ".", &data, sizeof(data)));
	assert_true(data == 0);
	fsdata_free(fsd);
}

TEST(get_returns_previously_set_value)
{
	int data = 0;
	fsdata_t *const fsd = fsdata_create(0);
	assert_success(fsdata_set(fsd, ".", &data, sizeof(data)));
	++data;
	assert_success(fsdata_get(fsd, ".", &data, sizeof(data)));
	assert_true(data == 0);
	fsdata_free(fsd);
}

TEST(set_overwrites_previous_value)
{
	int data = 0;
	fsdata_t *const fsd = fsdata_create(0);
	assert_success(fsdata_set(fsd, ".", &data, sizeof(data)));
	++data;
	assert_success(fsdata_set(fsd, ".", &data, sizeof(data)));
	++data;
	assert_success(fsdata_get(fsd, ".", &data, sizeof(data)));
	assert_true(data == 1);
	fsdata_free(fsd);
}

TEST(siblings_are_independent)
{
	int data = 0;
	fsdata_t *const fsd = fsdata_create(0);
	assert_success(os_mkdir(SANDBOX_PATH "/dir1", 0700));
	assert_success(os_mkdir(SANDBOX_PATH "/dir2", 0700));

	assert_success(fsdata_set(fsd, SANDBOX_PATH "/dir1", &data, sizeof(data)));
	++data;
	assert_success(fsdata_set(fsd, SANDBOX_PATH "/dir2", &data, sizeof(data)));
	++data;

	assert_success(fsdata_get(fsd, SANDBOX_PATH "/dir1", &data, sizeof(data)));
	assert_true(data == 0);
	assert_success(fsdata_get(fsd, SANDBOX_PATH "/dir2", &data, sizeof(data)));
	assert_true(data == 1);

	assert_success(remove(SANDBOX_PATH "/dir1"));
	assert_success(remove(SANDBOX_PATH "/dir2"));
	fsdata_free(fsd);
}

TEST(set_does_not_work_for_paths_that_do_not_exist)
{
	int data = 0;
	fsdata_t *const fsd = fsdata_create(0);
	assert_failure(fsdata_set(fsd, "no/path", &data, sizeof(data)));
	fsdata_free(fsd);
}

TEST(set_does_not_work_for_path_that_do_not_exist_anymore)
{
	int data = 0;
	fsdata_t *const fsd = fsdata_create(0);
	assert_success(os_mkdir(SANDBOX_PATH "/dir", 0700));
	assert_success(fsdata_set(fsd, SANDBOX_PATH "/dir", &data, sizeof(data)));
	assert_success(remove(SANDBOX_PATH "/dir"));
	assert_failure(fsdata_set(fsd, SANDBOX_PATH "/dir", &data, sizeof(data)));
	fsdata_free(fsd);
}

TEST(end_value_is_preferred_over_intermediate_value)
{
	int data = 0;
	fsdata_t *const fsd = fsdata_create(1);
	assert_success(os_mkdir(SANDBOX_PATH "/dir", 0700));

	assert_success(fsdata_set(fsd, SANDBOX_PATH, &data, sizeof(data)));
	++data;
	assert_success(fsdata_set(fsd, SANDBOX_PATH "/dir", &data, sizeof(data)));
	++data;
	assert_success(fsdata_get(fsd, SANDBOX_PATH "/dir", &data, sizeof(data)));
	assert_true(data == 1);

	assert_success(remove(SANDBOX_PATH "/dir"));
	fsdata_free(fsd);
}

TEST(intermediate_value_is_returned_if_end_value_is_not_found)
{
	int data = 0;
	fsdata_t *const fsd = fsdata_create(1);
	assert_success(os_mkdir(SANDBOX_PATH "/dir", 0700));

	assert_success(fsdata_set(fsd, SANDBOX_PATH, &data, sizeof(data)));
	++data;
	assert_success(fsdata_get(fsd, SANDBOX_PATH "/dir", &data, sizeof(data)));
	assert_true(data == 0);

	assert_success(remove(SANDBOX_PATH "/dir"));
	fsdata_free(fsd);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
