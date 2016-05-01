#include <stic.h>

#include <unistd.h> /* chdir() */

#include <stddef.h> /* wchar_t */

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

static void
suggest_cb(const wchar_t text[], const wchar_t value[], const char d[])
{
	descr = d;
	++nlines;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
