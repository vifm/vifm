#include <stic.h>

#include <unistd.h> /* rmdir() */

#include <stddef.h> /* NULL */

#include "../../src/compat/os.h"
#include "../../src/utils/fsdata.h"

#ifndef _WIN32
#define ROOT "/"
#else
#define ROOT "C:/"
#endif

static void visitor(void *data, void *arg);
static int traverser(const char name[], int valid, const void *parent_data,
		void *data, void *arg);

static int nnodes;

TEST(freeing_null_fsdata_is_ok)
{
	fsdata_free(NULL);
}

TEST(freeing_new_fsdata_is_ok)
{
	fsdata_t *const fsd = fsdata_create(0, 1);
	assert_non_null(fsd);
	fsdata_free(fsd);
}

TEST(get_returns_error_for_unknown_path)
{
	int data;
	fsdata_t *const fsd = fsdata_create(0, 1);
	assert_failure(fsdata_get(fsd, ".", &data, sizeof(data)));
	fsdata_free(fsd);
}

TEST(get_returns_error_for_wrong_path)
{
	int data;
	fsdata_t *const fsd = fsdata_create(0, 1);
	assert_failure(fsdata_get(fsd, "no/path", &data, sizeof(data)));
	fsdata_free(fsd);
}

TEST(get_does_not_alter_data_on_unknown_path)
{
	int data = 0;
	fsdata_t *const fsd = fsdata_create(0, 1);
	assert_failure(fsdata_get(fsd, ".", &data, sizeof(data)));
	assert_true(data == 0);
	fsdata_free(fsd);
}

TEST(get_returns_previously_set_value)
{
	int data = 0;
	fsdata_t *const fsd = fsdata_create(0, 1);
	assert_success(fsdata_set(fsd, ".", &data, sizeof(data)));
	++data;
	assert_success(fsdata_get(fsd, ".", &data, sizeof(data)));
	assert_true(data == 0);
	fsdata_free(fsd);
}

TEST(set_overwrites_previous_value)
{
	int data = 0;
	fsdata_t *const fsd = fsdata_create(0, 1);
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
	fsdata_t *const fsd = fsdata_create(0, 1);
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

	assert_success(rmdir(SANDBOX_PATH "/dir1"));
	assert_success(rmdir(SANDBOX_PATH "/dir2"));
	fsdata_free(fsd);
}

TEST(set_does_not_work_for_paths_that_do_not_exist)
{
	int data = 0;
	fsdata_t *const fsd = fsdata_create(0, 1);
	assert_failure(fsdata_set(fsd, "no/path", &data, sizeof(data)));
	fsdata_free(fsd);
}

TEST(set_does_not_work_for_path_that_do_not_exist_anymore)
{
	int data = 0;
	fsdata_t *const fsd = fsdata_create(0, 1);
	assert_success(os_mkdir(SANDBOX_PATH "/dir", 0700));
	assert_success(fsdata_set(fsd, SANDBOX_PATH "/dir", &data, sizeof(data)));
	assert_success(rmdir(SANDBOX_PATH "/dir"));
	assert_failure(fsdata_set(fsd, SANDBOX_PATH "/dir", &data, sizeof(data)));
	fsdata_free(fsd);
}

TEST(paths_resolution_can_be_disabled)
{
	int data = 0;
	fsdata_t *const fsd = fsdata_create(0, 0);
	assert_success(fsdata_set(fsd, "no/path", &data, sizeof(data)));
	assert_success(fsdata_get(fsd, "no/path", &data, sizeof(data)));
	fsdata_free(fsd);
}

TEST(end_value_is_preferred_over_intermediate_value)
{
	int data = 0;
	fsdata_t *const fsd = fsdata_create(1, 1);
	assert_success(os_mkdir(SANDBOX_PATH "/dir", 0700));

	assert_success(fsdata_set(fsd, SANDBOX_PATH, &data, sizeof(data)));
	++data;
	assert_success(fsdata_set(fsd, SANDBOX_PATH "/dir", &data, sizeof(data)));
	++data;
	assert_success(fsdata_get(fsd, SANDBOX_PATH "/dir", &data, sizeof(data)));
	assert_true(data == 1);

	assert_success(rmdir(SANDBOX_PATH "/dir"));
	fsdata_free(fsd);
}

TEST(intermediate_value_is_returned_if_end_value_is_not_found)
{
	int data = 0;
	fsdata_t *const fsd = fsdata_create(1, 1);
	assert_success(os_mkdir(SANDBOX_PATH "/dir", 0700));

	assert_success(fsdata_set(fsd, SANDBOX_PATH, &data, sizeof(data)));
	++data;
	assert_success(fsdata_get(fsd, SANDBOX_PATH "/dir", &data, sizeof(data)));
	assert_true(data == 0);

	assert_success(rmdir(SANDBOX_PATH "/dir"));
	fsdata_free(fsd);
}

TEST(parents_are_mapped_in_fsdata_on_match)
{
	char ch = '5';
	fsdata_t *const fsd = fsdata_create(0, 1);
	assert_success(os_mkdir(SANDBOX_PATH "/dir", 0700));

	assert_success(fsdata_set(fsd, SANDBOX_PATH, &ch, sizeof(ch)));
	assert_success(fsdata_set(fsd, SANDBOX_PATH "/dir", &ch, sizeof(ch)));

	assert_success(fsdata_map_parents(fsd, SANDBOX_PATH "/dir", visitor, NULL));

	assert_success(fsdata_get(fsd, SANDBOX_PATH, &ch, sizeof(ch)));
	assert_int_equal('6', ch);
	assert_success(fsdata_get(fsd, SANDBOX_PATH "/dir", &ch, sizeof(ch)));
	assert_int_equal('5', ch);

	assert_success(rmdir(SANDBOX_PATH "/dir"));
	fsdata_free(fsd);
}

TEST(parents_are_unchanged_in_fsdata_on_mismatch)
{
	char ch = '5';
	fsdata_t *const fsd = fsdata_create(0, 1);
	assert_success(os_mkdir(SANDBOX_PATH "/dir", 0700));

	assert_success(fsdata_set(fsd, SANDBOX_PATH, &ch, sizeof(ch)));
	assert_success(fsdata_set(fsd, SANDBOX_PATH "/dir", &ch, sizeof(ch)));

	assert_failure(fsdata_map_parents(fsd, SANDBOX_PATH "/wrong", visitor, NULL));

	assert_success(fsdata_get(fsd, SANDBOX_PATH, &ch, sizeof(ch)));
	assert_int_equal('5', ch);
	assert_success(fsdata_get(fsd, SANDBOX_PATH "/dir", &ch, sizeof(ch)));
	assert_int_equal('5', ch);

	assert_success(rmdir(SANDBOX_PATH "/dir"));
	fsdata_free(fsd);
}

TEST(root_can_carry_data)
{
	/* Big buffer that might overwrite some data. */
	char big_data[128];
	fsdata_t *const fsd = fsdata_create(0, 1);

	assert_success(fsdata_set(fsd, ROOT, big_data, sizeof(big_data)));

	/* This can try to use overwriten pointers. */
	assert_success(fsdata_map_parents(fsd, ROOT, visitor, NULL));

	fsdata_free(fsd);
}

TEST(data_size_can_change)
{
	char small_data[1];
	/* Big buffer that might overwrite some data. */
	char big_data[128];
	fsdata_t *const fsd = fsdata_create(0, 1);

	assert_success(fsdata_set(fsd, ROOT, small_data, sizeof(small_data)));
	assert_success(fsdata_set(fsd, ROOT, big_data, sizeof(big_data)));

	/* This can try to use overwriten pointers. */
	assert_success(fsdata_map_parents(fsd, ROOT, visitor, NULL));

	fsdata_free(fsd);
}

TEST(empty_tree_is_not_traversed)
{
	fsdata_t *const fsd = fsdata_create(0, 0);

	nnodes = 0;
	fsdata_traverse(fsd, &traverser, NULL);
	assert_int_equal(0, nnodes);

	fsdata_free(fsd);
}

TEST(tree_can_be_traversed)
{
	int data = 0;
	fsdata_t *const fsd = fsdata_create(0, 0);
	assert_success(fsdata_set(fsd, "no/such/path", &data, sizeof(data)));

	nnodes = 0;
	fsdata_traverse(fsd, &traverser, NULL);
	assert_int_equal(3, nnodes);

	fsdata_free(fsd);
}

TEST(cancellation)
{
	int data = 0;
	fsdata_t *const fsd = fsdata_create(0, 0);
	assert_success(fsdata_set(fsd, "no/such/path", &data, sizeof(data)));

	nnodes = -1;
	fsdata_traverse(fsd, &traverser, NULL);
	assert_int_equal(0, nnodes);

	nnodes = -2;
	fsdata_traverse(fsd, &traverser, NULL);
	assert_int_equal(0, nnodes);

	fsdata_free(fsd);
}

static void
visitor(void *data, void *arg)
{
	char *d = data;
	d[0] = '6';
}

static int
traverser(const char name[], int valid, const void *parent_data, void *data,
		void *arg)
{
	return (++nnodes == 0);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
