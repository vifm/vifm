#include <stic.h>

#include <unistd.h> /* chdir() unlink() */

#include <locale.h> /* LC_ALL setlocale() */
#include <string.h> /* memset() */

#include "../../src/cfg/config.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/utils/str.h"
#include "../../src/sort.h"

#define SIGN(n) ({__typeof(n) _n = (n); (_n < 0) ? -1 : (_n > 0);})
#define ASSERT_STRCMP_EQUAL(a, b) \
		do { assert_int_equal(SIGN(a), SIGN(b)); } while(0)

static void free_view(FileView *view);

SETUP_ONCE()
{
	(void)setlocale(LC_ALL, "");
}

SETUP()
{
	cfg.sort_numbers = 1;

	lwin.list_rows = 3;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("a");
	lwin.dir_entry[0].type = FT_REG;
	lwin.dir_entry[1].name = strdup("_");
	lwin.dir_entry[1].type = FT_REG;
	lwin.dir_entry[2].name = strdup("A");
	lwin.dir_entry[2].type = FT_REG;

	rwin.list_rows = 2;
	rwin.dir_entry = dynarray_cextend(NULL,
			rwin.list_rows*sizeof(*rwin.dir_entry));
	rwin.dir_entry[0].name = strdup("аааааааааа");
	rwin.dir_entry[0].type = FT_REG;
	rwin.dir_entry[1].name = strdup("АААААААААА");
	rwin.dir_entry[1].type = FT_REG;
}

TEARDOWN()
{
	free_view(&lwin);
	free_view(&rwin);
}

static void
free_view(FileView *view)
{
	int i;

	for(i = 0; i < view->list_rows; ++i)
	{
		free(view->dir_entry[i].name);
	}
	dynarray_free(view->dir_entry);

	view->list_rows = 0;
}

TEST(special_chars_ignore_case_sort)
{
	lwin.sort[0] = SK_BY_INAME;
	memset(&lwin.sort[1], SK_NONE, sizeof(lwin.sort) - 1);

	sort_view(&lwin);

	assert_string_equal("_", lwin.dir_entry[0].name);
}

/* Windows is really bad at handling links. */
#ifndef _WIN32

TEST(symlink_to_dir)
{
	assert_success(chdir(SANDBOX_PATH));
	assert_success(symlink(".", "self"));

	lwin.sort[0] = SK_BY_INAME;
	memset(&lwin.sort[1], SK_NONE, sizeof(lwin.sort) - 1);

	replace_string(&lwin.dir_entry[2].name, "self");
	lwin.dir_entry[2].type = FT_LINK;

	cfg.slow_fs_list = strdup("");

	sort_view(&lwin);

	free(cfg.slow_fs_list);
	cfg.slow_fs_list = NULL;

	assert_string_equal("self", lwin.dir_entry[0].name);

	assert_int_equal(0, unlink("self"));
	assert_int_equal(0, chdir("../.."));
}

#endif

TEST(versort_without_numbers)
{
	const char *s, *t;

	s = "abc";
	t = "abcdef";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "abcdef";
	t = "abc";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "";
	t = "abc";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "abc";
	t = "";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "";
	t = "";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "abcdef";
	t = "abcdef";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "abcdef";
	t = "abcdeg";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "abcdeg";
	t = "abcdef";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));
}

TEST(versort_with_numbers)
{
	const char *s, *t;

	s = "abcdef0";
	t = "abcdef0";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "abcdef0";
	t = "abcdef1";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "abcdef1";
	t = "abcdef0";
	ASSERT_STRCMP_EQUAL(strcmp(s, t), strnumcmp(s, t));

	s = "abcdef9";
	t = "abcdef10";
	assert_true(strnumcmp(s, t) < 0);

	s = "abcdef10";
	t = "abcdef10";
	assert_true(strnumcmp(s, t) == 0);

	s = "abcdef10";
	t = "abcdef9";
	assert_true(strnumcmp(s, t) > 0);

	s = "abcdef1.20.0";
	t = "abcdef1.5.1";
	assert_true(strnumcmp(s, t) > 0);

	s = "x001";
	t = "x1";
	assert_true(strnumcmp(s, t) < 0);

	s = "x1";
	t = "x001";
	assert_true(strnumcmp(s, t) > 0);
}

TEST(versort_numbers_only)
{
	const char *s, *t;

	s = "00";
	t = "01";
	assert_true(strnumcmp(s, t) < 0);

	s = "01";
	t = "10";
	assert_true(strnumcmp(s, t) < 0);

	s = "10";
	t = "11";
	assert_true(strnumcmp(s, t) < 0);

	s = "01";
	t = "00";
	assert_true(strnumcmp(s, t) > 0);

	s = "10";
	t = "01";
	assert_true(strnumcmp(s, t) > 0);

	s = "11";
	t = "10";
	assert_true(strnumcmp(s, t) > 0);

	s = "11";
	t = "10";
	assert_true(strnumcmp(s, t) > 0);

	s = "13";
	t = "100";
	assert_true(strnumcmp(s, t) < 0);

	s = "100";
	t = "13";
	assert_true(strnumcmp(s, t) > 0);

	s = "09";
	t = "3";
	assert_true(strnumcmp(s, t) > 0);

	s = "3";
	t = "10";
	assert_true(strnumcmp(s, t) < 0);
}

TEST(versort_numbers_only_and_letters_only)
{
	const char *s, *t;

	s = "A";
	t = "10";
	assert_true(strnumcmp(s, t) > 0);

	s = "10";
	t = "A";
	assert_true(strnumcmp(s, t) < 0);
}

TEST(versort_zero_and_zerox)
{
	const char *s, *t;

	s = "0";
	t = "01";
	assert_true(strnumcmp(s, t) < 0);
}

TEST(versort_zerox_and_one)
{
	const char *s, *t;

	s = "00_";
	t = "01";
	assert_true(strnumcmp(s, t) < 0);
}

TEST(versort_man_page_example)
{
	/* According to the `man strverscmp`:
	 *   000 < 00 < 01 < 010 < 09 < 0 < 1 < 9 < 10.
	 * In Vifm these conditions differ:
	 *  - 000 == 00;
	 *  - 010 == 09;
	 *  - 09  == 0.
	 */

	assert_true(strnumcmp("000", "00") == 0);
	assert_true(strnumcmp("00", "01") < 0);
	assert_true(strnumcmp("01", "010") < 0);
	assert_true(strnumcmp("010", "09") > 0);
	assert_true(strnumcmp("09", "0") > 0);
	assert_true(strnumcmp("0", "1") < 0);
	assert_true(strnumcmp("1", "9") < 0);
	assert_true(strnumcmp("9", "10") < 0);
}

TEST(ignore_case_name_sort_breaks_ties_deterministically)
{
	/* If normalized names are equal, byte-by-byte comparison should be used to
	 * break ties. */

	rwin.sort[0] = SK_BY_INAME;
	memset(&rwin.sort[1], SK_NONE, sizeof(rwin.sort) - 1);

	sort_view(&rwin);

	assert_string_equal("АААААААААА", rwin.dir_entry[0].name);
	assert_string_equal("аааааааааа", rwin.dir_entry[1].name);
}

TEST(extensions_of_dot_files_are_sorted_correctly)
{
	free_view(&lwin);

	lwin.list_rows = 3;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("disown.c");
	lwin.dir_entry[0].type = FT_REG;
	lwin.dir_entry[1].name = strdup(".cdargsresult");
	lwin.dir_entry[1].type = FT_REG;
	lwin.dir_entry[2].name = strdup(".tmux.conf");
	lwin.dir_entry[2].type = FT_REG;

	lwin.sort[0] = SK_BY_EXTENSION;
	memset(&lwin.sort[1], SK_NONE, sizeof(lwin.sort) - 1);

	sort_view(&lwin);

	assert_string_equal(".cdargsresult", lwin.dir_entry[0].name);
	assert_string_equal("disown.c", lwin.dir_entry[1].name);
	assert_string_equal(".tmux.conf", lwin.dir_entry[2].name);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
