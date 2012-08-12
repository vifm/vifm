#include "seatest.h"

#include "../../src/engine/cmds.h"
#include "../../src/utils/utils.h"

static char **
dispatch(const char cmd[], int *count, char separator, int regexp, int quotes)
{
	return dispatch_line(cmd, count, separator, regexp, quotes, NULL, NULL, NULL);
}

static void
test_no_quotes(void)
{
	int count;
	char **args;

	args = dispatch("abc def ghi", &count, ' ', 0, 1);
	assert_int_equal(3, count);
	if(count != 3)
		return;
	assert_string_equal("abc", args[0]);
	assert_string_equal("def", args[1]);
	assert_string_equal("ghi", args[2]);
	free_string_array(args, count);
}

static void
test_single_quotes(void)
{
	int count;
	char **args;

	args = dispatch("'abc' 'def' 'ghi'", &count, ' ', 0, 1);
	assert_int_equal(3, count);
	if(count != 3)
		return;
	assert_string_equal("abc", args[0]);
	assert_string_equal("def", args[1]);
	assert_string_equal("ghi", args[2]);
	free_string_array(args, count);

	args = dispatch("'     '     '", &count, ' ', 0, 1);
	assert_true(args == NULL);

	args = dispatch("'     \\'     '", &count, ' ', 0, 1);
	assert_true(args == NULL);
}

static void
test_double_quotes(void)
{
	int count;
	char **args;

	args = dispatch("\"abc\" \"def\" \"ghi\"", &count, ' ', 0, 1);
	assert_int_equal(3, count);
	if(count != 3)
		return;
	assert_string_equal("abc", args[0]);
	assert_string_equal("def", args[1]);
	assert_string_equal("ghi", args[2]);
	free_string_array(args, count);

	args = dispatch("\"     \"     \"", &count, ' ', 0, 1);
	assert_true(args == NULL);
}

static void
test_regexp_quotes(void)
{
	int count;
	char **args;

	args = dispatch("/abc/ /def/ /ghi/", &count, ' ', 1, 1);
	assert_int_equal(3, count);
	if(count != 3)
		return;
	assert_string_equal("abc", args[0]);
	assert_string_equal("def", args[1]);
	assert_string_equal("ghi", args[2]);
	free_string_array(args, count);

	args = dispatch("/     /     /", &count, ' ', 1, 1);
	assert_true(args == NULL);
}

static void
test_no_quotes_escaping(void)
{
	int count;
	char **args;

	args = dispatch("abc\\ def g\\ h\\ i\\ ", &count, ' ', 0, 1);
	assert_int_equal(2, count);
	if(count != 2)
		return;
	assert_string_equal("abc def", args[0]);
	assert_string_equal("g h i ", args[1]);
	free_string_array(args, count);
}

static void
test_single_quotes_escaping(void)
{
	int count;
	char **args;

	args = dispatch("'abc\\ def' '\\g\\h\\\\i'", &count, ' ', 0, 1);
	assert_int_equal(2, count);
	if(count != 2)
		return;
	assert_string_equal("abc\\ def", args[0]);
	assert_string_equal("\\g\\h\\\\i", args[1]);
	free_string_array(args, count);
}

static void
test_double_quotes_escaping(void)
{
	int count;
	char **args;

	args = dispatch("\" \\\" \"", &count, ' ', 0, 1);
	assert_int_equal(1, count);
	if(count != 1)
		return;
	assert_string_equal(" \" ", args[0]);
	free_string_array(args, count);
}

static void
test_regexp_quotes_escaping(void)
{
	int count;
	char **args;

	args = dispatch("/ \\( \\/ \\) /", &count, '/', 1, 0);
	assert_int_equal(1, count);
	if(count != 1)
		return;
	assert_string_equal(" \\( / \\) ", args[0]);
	free_string_array(args, count);

	args = dispatch("/\\.c$/", &count, ' ', 1, 1);
	assert_int_equal(1, count);
	if(count != 1)
		return;
	assert_string_equal("\\.c$", args[0]);
	free_string_array(args, count);
}

static void
test_start_and_end_spaces(void)
{
	int count;
	char **args;

	args = dispatch(" a", &count, ' ', 0, 1);
	assert_int_equal(1, count);
	if(count != 1)
		return;
	assert_string_equal("a", args[0]);
	free_string_array(args, count);

	args = dispatch("a ", &count, ' ', 0, 1);
	assert_int_equal(1, count);
	if(count != 1)
		return;
	assert_string_equal("a", args[0]);
	free_string_array(args, count);

	args = dispatch(" a ", &count, ' ', 0, 1);
	assert_int_equal(1, count);
	if(count != 1)
		return;
	assert_string_equal("a", args[0]);
	free_string_array(args, count);
}

static void
test_no_quoting(void)
{
	int count;
	char **args;

	args = dispatch("\"a", &count, ' ', 0, 0);
	assert_int_equal(1, count);
	if(count != 1)
		return;
	assert_string_equal("\"a", args[0]);
	free_string_array(args, count);

	args = dispatch("a\"", &count, ' ', 0, 0);
	assert_int_equal(1, count);
	if(count != 1)
		return;
	assert_string_equal("a\"", args[0]);
	free_string_array(args, count);

	args = dispatch("\"a\"", &count, ' ', 0, 0);
	assert_int_equal(1, count);
	if(count != 1)
		return;
	assert_string_equal("\"a\"", args[0]);
	free_string_array(args, count);
}

static void
test_cust_sep(void)
{
	int count;
	char **args;

	args = dispatch("//abc/", &count, '/', 1, 0);
	assert_int_equal(2, count);
	if(count != 2)
		return;
	assert_string_equal("", args[0]);
	assert_string_equal("abc", args[1]);
	free_string_array(args, count);

	args = dispatch("/abc//", &count, '/', 1, 0);
	assert_int_equal(2, count);
	if(count != 2)
		return;
	assert_string_equal("abc", args[0]);
	assert_string_equal("", args[1]);
	free_string_array(args, count);

	args = dispatch("!!abc!", &count, '!', 1, 0);
	assert_int_equal(2, count);
	if(count != 2)
		return;
	assert_string_equal("", args[0]);
	assert_string_equal("abc", args[1]);
	free_string_array(args, count);
}

void
test_dispatch_line(void)
{
	test_fixture_start();

	run_test(test_no_quotes);
	run_test(test_single_quotes);
	run_test(test_double_quotes);
	run_test(test_regexp_quotes);

	run_test(test_no_quotes_escaping);
	run_test(test_single_quotes_escaping);
	run_test(test_double_quotes_escaping);
	run_test(test_regexp_quotes_escaping);

	run_test(test_start_and_end_spaces);

	run_test(test_no_quoting);

	run_test(test_cust_sep);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
