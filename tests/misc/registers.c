#include <stic.h>

#include <unistd.h> /* chdir() */

#include <stddef.h> /* wchar_t */

#include "../../src/utils/string_array.h"
#include "../../src/registers.h"

static void suggest_cb(const wchar_t text[], const wchar_t value[],
		const char d[]);

static const char *descr;
static int nlines;

SETUP()
{
	regs_init();
	descr = NULL;
	nlines = 0;
}

TEARDOWN()
{
	regs_reset();
}

TEST(regs_set_can_clear)
{
	const reg_t *reg = regs_find('a');

	regs_append('a', "a");
	assert_int_equal(1, reg->nfiles);
	regs_set('a', /*files=*/NULL, /*nfiles=*/0);
	assert_int_equal(0, reg->nfiles);
}

TEST(regs_set_handles_invalid_reg_name)
{
	char file[] = "file";
	char *files[] = { file };
	regs_set('#', files, /*nfiles=*/1);
}

TEST(suggestion_does_not_print_empty_lines)
{
	assert_success(chdir(TEST_DATA_PATH "/existing-files"));

	regs_append('a', "a");
	regs_append('a', "b");
	regs_append('a', "c");

	regs_suggest(&suggest_cb, 5);

	assert_int_equal(3, nlines);
	assert_string_equal("a", descr);
}

TEST(suggestion_trims_extra_lines_single_reg)
{
	assert_success(chdir(TEST_DATA_PATH "/existing-files"));

	regs_append('a', "a");
	regs_append('a', "b");
	regs_append('a', "c");

	regs_suggest(&suggest_cb, 2);

	assert_int_equal(2, nlines);
	assert_string_equal("b", descr);
}

TEST(suggestion_trims_extra_lines_multiple_regs)
{
	assert_success(chdir(TEST_DATA_PATH "/existing-files"));

	regs_append('a', "a");
	regs_append('a', "b");
	regs_append('a', "c");

	regs_append('"', "a");
	regs_append('"', "b");
	regs_append('"', "c");

	regs_suggest(&suggest_cb, 2);

	assert_int_equal(4, nlines);
	assert_string_equal("b", descr);
}

TEST(empty_list_of_registers_is_not_listed)
{
	char **list = regs_list("");
	int len = count_strings(list);
	assert_int_equal(0, len);
	free_string_array(list, len);
}

TEST(wrong_registers_are_not_listed)
{
	char **list = regs_list("!@");
	int len = count_strings(list);
	assert_int_equal(0, len);
	free_string_array(list, len);
}

TEST(empty_registers_are_not_listed)
{
	char **list = regs_list("abc");
	int len = count_strings(list);
	assert_int_equal(0, len);
	free_string_array(list, len);
}

TEST(registers_are_listed)
{
	regs_append('a', "a1");
	regs_append('a', "a2");

	regs_append('b', "b1");

	char **list = regs_list("ba");
	int len = count_strings(list);
	assert_int_equal(5, len);
	assert_string_equal("\"b", list[0]);
	assert_string_equal("b1", list[1]);
	assert_string_equal("\"a", list[2]);
	assert_string_equal("a1", list[3]);
	assert_string_equal("a2", list[4]);
	free_string_array(list, len);
}

static void
suggest_cb(const wchar_t text[], const wchar_t value[], const char d[])
{
	descr = d;
	++nlines;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
