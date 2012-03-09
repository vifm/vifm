#include <string.h>

#include "seatest.h"

#include "../../src/engine/cmds.h"
#include "../../src/commands.h"
#include "../../src/ui.h"

extern cmds_conf_t cmds_conf;

static void
setup_lwin(void)
{
	strcpy(lwin.curr_dir, "/lwin");

	lwin.list_rows = 5;
	lwin.list_pos = 2;
	lwin.dir_entry = calloc(lwin.list_rows, sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("../");
	lwin.dir_entry[1].name = strdup("lfile0");
	lwin.dir_entry[2].name = strdup("lfile1");
	lwin.dir_entry[3].name = strdup("lfile2");
	lwin.dir_entry[4].name = strdup("lfile3");

	lwin.dir_entry[1].selected = 1;
	lwin.dir_entry[3].selected = 1;
	lwin.selected_files = 2;
}

static void
setup_rwin(void)
{
	strcpy(rwin.curr_dir, "/rwin");

	rwin.list_rows = 7;
	rwin.list_pos = 5;
	rwin.dir_entry = calloc(rwin.list_rows, sizeof(*rwin.dir_entry));
	rwin.dir_entry[0].name = strdup("../");
	rwin.dir_entry[1].name = strdup("rfile0");
	rwin.dir_entry[2].name = strdup("rfile1");
	rwin.dir_entry[3].name = strdup("rfile2");
	rwin.dir_entry[4].name = strdup("rfile3");
	rwin.dir_entry[5].name = strdup("rfile4");
	rwin.dir_entry[6].name = strdup("rfile5");

	rwin.dir_entry[2].selected = 1;
	rwin.dir_entry[4].selected = 1;
	rwin.dir_entry[6].selected = 1;
	rwin.selected_files = 3;
}

static void
setup(void)
{
	setup_lwin();
	setup_rwin();
}

static void
teardown(void)
{
	int i;

	for(i = 0; i < lwin.list_rows; i++)
		free(lwin.dir_entry[i].name);
	free(lwin.dir_entry);

	for(i = 0; i < rwin.list_rows; i++)
		free(rwin.dir_entry[i].name);
	free(rwin.dir_entry);
}

static void
one_number_range_test(void)
{
	cmd_info_t cmd_info = {
		.begin = -1, .end = -1
	};

	curr_view = &lwin;
	cmds_conf.begin = 0;
	cmds_conf.current = curr_view->list_pos;
	cmds_conf.end = curr_view->list_rows - 1;
	select_range(-1, &cmd_info);
	assert_int_equal(2, lwin.selected_files);

	curr_view = &rwin;
	cmds_conf.begin = 0;
	cmds_conf.current = curr_view->list_pos;
	cmds_conf.end = curr_view->list_rows - 1;
	select_range(-1, &cmd_info);
	assert_int_equal(3, rwin.selected_files);
}

static void
test_one_in_the_range(void)
{
	cmd_info_t cmd_info = {
		.begin = 0, .end = 0
	};

	curr_view = &lwin;
	cmds_conf.begin = 0;
	cmds_conf.current = curr_view->list_pos;
	cmds_conf.end = curr_view->list_rows - 1;
	select_range(-1, &cmd_info);
	assert_int_equal(1, lwin.selected_files);
	assert_int_equal(1, lwin.dir_entry[0].selected);

	curr_view = &rwin;
	cmds_conf.begin = 0;
	cmds_conf.current = curr_view->list_pos;
	cmds_conf.end = curr_view->list_rows - 1;
	select_range(-1, &cmd_info);
	assert_int_equal(1, rwin.selected_files);
	assert_int_equal(1, rwin.dir_entry[0].selected);
}

void
one_number_range(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(one_number_range_test);
	run_test(test_one_in_the_range);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
