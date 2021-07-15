#include <stic.h>

#include <ctype.h> /* isspace() */
#include <stdlib.h> /* free() */
#include <string.h> /* strdup() */
#include <wchar.h> /* wcsdup() wcslen() */

#include "../../src/cfg/config.h"
#include "../../src/engine/abbrevs.h"
#include "../../src/modes/cmdline.h"

static void prepare_line(const wchar_t str[]);

static line_stats_t stats;

SETUP()
{
	int i;
	for(i = 0; i < 255; ++i)
	{
		cfg.word_chars[i] = !isspace(i);
	}

	assert_int_equal(0, vle_abbr_add_no_remap(L"x", L"y"));
	assert_int_equal(0, vle_abbr_add_no_remap(L"aa", L"bb"));
}

TEARDOWN()
{
	free(stats.line);
	stats.line = NULL;

	vle_abbr_reset();
}

TEST(single_character)
{
	int pos = -1, no_remap = -1;
	const wchar_t *abbrev_rhs;

	prepare_line(L"x");
	abbrev_rhs = extract_abbrev(&stats, &pos, &no_remap);
	assert_wstring_equal(L"y", abbrev_rhs);
	assert_int_equal(0, pos);
}

TEST(at_beginning)
{
	int pos = -1, no_remap = -1;
	const wchar_t *abbrev_rhs;

	prepare_line(L"aa");
	abbrev_rhs = extract_abbrev(&stats, &pos, &no_remap);
	assert_wstring_equal(L"bb", abbrev_rhs);
	assert_int_equal(0, pos);
}

TEST(after_prefix)
{
	int pos = -1, no_remap = -1;
	const wchar_t *abbrev_rhs;

	prepare_line(L"prefix aa");
	abbrev_rhs = extract_abbrev(&stats, &pos, &no_remap);
	assert_wstring_equal(L"bb", abbrev_rhs);
	assert_int_equal(7, pos);
}

TEST(before_suffix)
{
	int pos = -1, no_remap = -1;
	const wchar_t *abbrev_rhs;

	prepare_line(L"aa suffix");
	stats.index = 2;
	abbrev_rhs = extract_abbrev(&stats, &pos, &no_remap);
	assert_wstring_equal(L"bb", abbrev_rhs);
	assert_int_equal(0, pos);
}

TEST(in_the_middle)
{
	int pos = -1, no_remap = -1;
	const wchar_t *abbrev_rhs;

	prepare_line(L"prefix aa suffix");
	stats.index = 9;
	abbrev_rhs = extract_abbrev(&stats, &pos, &no_remap);
	assert_wstring_equal(L"bb", abbrev_rhs);
	assert_int_equal(7, pos);
}

static void
prepare_line(const wchar_t str[])
{
	free(stats.line);
	stats.line = wcsdup(str);
	stats.len = wcslen(str);
	stats.index = stats.len;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
