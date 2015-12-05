#include <stic.h>

#include <stdlib.h>

#include "../../src/filetype.h"
#include "../../src/status.h"
#include "test.h"

TEST(one_pattern)
{
	assoc_records_t ft;

	set_programs("*.tar", "{description} tar prog", 0, 0);

	ft = ft_get_all_programs("file.version.tar");
	assert_int_equal(1, ft.count);

	assert_true(ft.list[0].command != NULL);
	assert_string_equal("description", ft.list[0].description);

	ft_assoc_records_free(&ft);
}

TEST(double_comma_in_description)
{
	assoc_records_t ft;

	set_programs("*.tar", "{description,,is,,here} tar prog", 0, 0);

	ft = ft_get_all_programs("file.version.tar");
	assert_int_equal(1, ft.count);
	assert_string_equal("description,is,here", ft.list[0].description);

	ft_assoc_records_free(&ft);
}

TEST(two_patterns)
{
	assoc_records_t ft;

	set_programs("*.tar,*.zip", "{archives} prog", 0, 0);

	{
		ft = ft_get_all_programs("file.version.tar");
		assert_int_equal(1, ft.count);

		assert_true(ft.list[0].command != NULL);
		assert_string_equal("archives", ft.list[0].description);

		ft_assoc_records_free(&ft);
	}

	{
		ft = ft_get_all_programs("file.version.zip");
		assert_int_equal(1, ft.count);

		assert_true(ft.list[0].command != NULL);
		assert_string_equal("archives", ft.list[0].description);

		ft_assoc_records_free(&ft);
	}
}

TEST(two_programs)
{
	assoc_records_t ft;

	set_programs("*.tar", "{rar} rarprog, {zip} zipprog", 0, 0);

	ft = ft_get_all_programs("a.tar");

	assert_int_equal(2, ft.count);

	assert_string_equal(ft.list[0].command, "rarprog");
	assert_string_equal(ft.list[0].description, "rar");

	assert_string_equal(ft.list[1].command, "zipprog");
	assert_string_equal(ft.list[1].description, "zip");

	ft_assoc_records_free(&ft);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
