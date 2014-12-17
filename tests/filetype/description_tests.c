#include <stdlib.h>

#include "seatest.h"

#include "../../src/filetype.h"
#include "../../src/status.h"

static void
test_one_pattern(void)
{
	assoc_records_t ft;

	ft_set_programs("*.tar", "{description} tar prog", 0, 0);

	ft = ft_get_all_programs("file.version.tar");
	assert_int_equal(1, ft.count);

	assert_true(ft.list[0].command != NULL);
	assert_string_equal("description", ft.list[0].description);

	free(ft.list);
}

static void
test_two_patterns(void)
{
	assoc_records_t ft;

	ft_set_programs("*.tar,*.zip", "{archives} prog", 0, 0);

	{
		ft = ft_get_all_programs("file.version.tar");
		assert_int_equal(1, ft.count);

		assert_true(ft.list[0].command != NULL);
		assert_string_equal("archives", ft.list[0].description);

		free(ft.list);
	}

	{
		ft = ft_get_all_programs("file.version.zip");
		assert_int_equal(1, ft.count);

		assert_true(ft.list[0].command != NULL);
		assert_string_equal("archives", ft.list[0].description);

		free(ft.list);
	}
}

static void
test_two_programs(void)
{
	assoc_records_t ft;

	ft_set_programs("*.tar", "{rar} rarprog, {zip} zipprog", 0, 0);

	ft = ft_get_all_programs("a.tar");

	assert_int_equal(2, ft.count);

	assert_string_equal(ft.list[0].command, "rarprog");
	assert_string_equal(ft.list[0].description, "rar");

	assert_string_equal(ft.list[1].command, "zipprog");
	assert_string_equal(ft.list[1].description, "zip");

	free(ft.list);
}

void
description_tests(void)
{
	test_fixture_start();

	run_test(test_one_pattern);
	run_test(test_two_patterns);
	run_test(test_two_programs);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
