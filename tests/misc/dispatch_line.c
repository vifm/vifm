#include <stic.h>

#include "../../src/engine/cmds.h"
#include "../../src/utils/string_array.h"
#include "../../src/utils/utils.h"

static char **
dispatch(const char cmd[], int *count, char separator, int regexp, int quotes)
{
	return dispatch_line(cmd, count, separator, regexp, quotes, NULL, NULL, NULL);
}

TEST(no_quotes)
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

TEST(no_quotes_start_at_the_start)
{
	int count;
	char **args;

	args = dispatch("\\ non-space", &count, ' ', 0, 1);
	assert_int_equal(1, count);
	if(count == 1)
	{
		assert_string_equal(" non-space", args[0]);
	}

	free_string_array(args, count);
}

TEST(single_quotes)
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

TEST(double_quotes)
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

TEST(regexp_quotes)
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

TEST(no_quotes_escaping)
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

TEST(single_quotes_escaping)
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

TEST(double_quotes_escaping)
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

TEST(regexp_quotes_escaping)
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

TEST(start_and_end_spaces)
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

TEST(no_quoting)
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

TEST(cust_sep)
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
