#include <stic.h>

#include "../../src/filetype.h"

TEST(cannot_add_single_duplicate)
{
	assoc_records_t assocs = {};
	ft_assoc_record_add(&assocs, "cmd", "descr");
	ft_assoc_record_add(&assocs, "cmd", "descr");
	assert_int_equal(1, assocs.count);
	ft_assoc_records_free(&assocs);
}

TEST(cannot_add_multiple_duplicates)
{
	assoc_records_t assocs = {};
	ft_assoc_record_add(&assocs, "cmd1", "descr");
	ft_assoc_record_add(&assocs, "cmd2", "descr");
	ft_assoc_record_add(&assocs, "cmd3", "descr1");
	ft_assoc_record_add(&assocs, "cmd3", "descr2");
	ft_assoc_record_add_all(&assocs, &assocs);
	assert_int_equal(4, assocs.count);
	ft_assoc_records_free(&assocs);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
