#include <stic.h>

#include <stdlib.h>

#include "../../src/undo.h"

#include "test.h"

TEST(detail)
{
	char **list;
	char **p;

	list = un_get_list(1);

	assert_false(list == NULL);
	assert_true(list[11] == NULL);

	assert_string_equal(" msg3", list[0]);
	assert_string_equal("  do: mv do_msg3 to undo_msg3", list[1]);
	assert_string_equal("  undo: mv undo_msg3 to do_msg3", list[2]);

	assert_string_equal(" msg2", list[3]);
	assert_string_equal("  do: mv do_msg2_cmd2 to undo_msg2_cmd2", list[4]);
	assert_string_equal("  undo: mv undo_msg2_cmd2 to do_msg2_cmd2", list[5]);
	assert_string_equal("  do: mv do_msg2_cmd1 to undo_msg2_cmd1", list[6]);
	assert_string_equal("  undo: mv undo_msg2_cmd1 to do_msg2_cmd1", list[7]);

	assert_string_equal(" msg1", list[8]);
	assert_string_equal("  do: mv do_msg1 to undo_msg1", list[9]);
	assert_string_equal("  undo: mv undo_msg1 to do_msg1", list[10]);

	p = list;
	while(*p != NULL)
		free(*p++);
	free(list);
}

TEST(detail_after_reset)
{
	un_reset();

	char **list = un_get_list(1);
	assert_true(list[0] == NULL);
	free(list);
}

static OpsResult
exec_func(OPS op, void *data, const char src[], const char dst[])
{
	return OPS_SUCCEEDED;
}

TEST(detail_smaller_limit)
{
	static int undo_levels = 2;

	char **list;
	char **p;

	init_undo_list_for_tests(&exec_func, &undo_levels);

	list = un_get_list(1);

	assert_false(list == NULL);
	assert_true(list[6] == NULL);

	assert_string_equal(" msg3", list[0]);
	assert_string_equal("  do: mv do_msg3 to undo_msg3", list[1]);
	assert_string_equal("  undo: mv undo_msg3 to do_msg3", list[2]);

	assert_string_equal(" msg2", list[3]);
	assert_string_equal("  do: mv do_msg2_cmd2 to undo_msg2_cmd2", list[4]);
	assert_string_equal("  undo: mv undo_msg2_cmd2 to do_msg2_cmd2", list[5]);

	p = list;
	while(*p != NULL)
		free(*p++);
	free(list);
}

TEST(nondetail)
{
	char **list;
	char **p;

	list = un_get_list(0);

	assert_false(list == NULL);
	assert_true(list[3] == NULL);

	assert_string_equal(" msg3", list[0]);
	assert_string_equal(" msg2", list[1]);
	assert_string_equal(" msg1", list[2]);

	p = list;
	while(*p != NULL)
		free(*p++);
	free(list);
}

TEST(nondetail_after_reset)
{
	un_reset();

	char **list = un_get_list(0);
	assert_true(list[0] == NULL);
	free(list);
}

TEST(nondetail_smaller_limit)
{
	static int undo_levels = 2;

	char **list;
	char **p;

	init_undo_list_for_tests(exec_func, &undo_levels);

	list = un_get_list(0);

	assert_false(list == NULL);
	assert_true(list[2] == NULL);

	assert_string_equal(" msg3", list[0]);
	assert_string_equal(" msg2", list[1]);

	p = list;
	while(*p != NULL)
		free(*p++);
	free(list);
}

TEST(get_pos)
{
	assert_int_equal(0, un_get_list_pos(0));
	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());
	assert_int_equal(1, un_get_list_pos(0));
	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());
	assert_int_equal(2, un_get_list_pos(0));
	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());
	assert_int_equal(3, un_get_list_pos(0));
	assert_int_equal(UN_ERR_NONE, un_group_undo());
	assert_int_equal(3, un_get_list_pos(0));
	assert_int_equal(UN_ERR_NONE, un_group_undo());
	assert_int_equal(3, un_get_list_pos(0));
	assert_int_equal(UN_ERR_NONE, un_group_undo());
	assert_int_equal(3, un_get_list_pos(0));
	assert_int_equal(UN_ERR_NONE, un_group_undo());
}

TEST(set_pos_no_detail)
{
	assert_int_equal(0, un_get_list_pos(0));
	un_set_pos(3, 0);
	assert_int_equal(3, un_get_list_pos(0));

	assert_int_equal(UN_ERR_NONE, un_group_undo());
	assert_int_equal(UN_ERR_BALANCE, un_group_redo());
	assert_int_equal(2, un_get_list_pos(0));
	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());
	assert_int_equal(3, un_get_list_pos(0));

	un_set_pos(1, 0);
	assert_int_equal(1, un_get_list_pos(0));

	assert_int_equal(UN_ERR_SUCCESS, un_group_undo());
	assert_int_equal(2, un_get_list_pos(0));
	assert_int_equal(UN_ERR_SUCCESS, un_group_redo());
	assert_int_equal(1, un_get_list_pos(0));
}

TEST(set_pos_detail)
{
	un_set_pos(0, 1);
	assert_int_equal(0, un_get_list_pos(1));
	un_set_pos(1, 1);
	assert_int_equal(0, un_get_list_pos(1));
	un_set_pos(2, 1);
	assert_int_equal(0, un_get_list_pos(1));

	un_set_pos(3, 1);
	assert_int_equal(3, un_get_list_pos(1));
	un_set_pos(4, 1);
	assert_int_equal(3, un_get_list_pos(1));
	un_set_pos(5, 1);
	assert_int_equal(3, un_get_list_pos(1));
	un_set_pos(6, 1);
	assert_int_equal(3, un_get_list_pos(1));
	un_set_pos(7, 1);
	assert_int_equal(3, un_get_list_pos(1));

	un_set_pos(8, 1);
	assert_int_equal(8, un_get_list_pos(1));
	un_set_pos(9, 1);
	assert_int_equal(8, un_get_list_pos(1));
	un_set_pos(10, 1);
	assert_int_equal(8, un_get_list_pos(1));

	un_set_pos(11, 1);
	assert_int_equal(11, un_get_list_pos(1));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
