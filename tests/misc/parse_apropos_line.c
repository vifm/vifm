#include <stic.h>

#include "../../src/menus/apropos_menu.h"

TEST(groff_format)
{
	int section;
	char topic[64];
	const int err = parse_apropos_line("vifm    (1)  - vi file manager", &section,
			topic, sizeof(topic));

	assert_success(err);
	assert_int_equal(1, section);
	assert_string_equal("vifm", topic);
}

TEST(mandoc_format)
{
	int section;
	char topic[64];
	const int err = parse_apropos_line("vifm(1)  - vi file manager", &section,
			topic, sizeof(topic));

	assert_success(err);
	assert_int_equal(1, section);
	assert_string_equal("vifm", topic);
}

/* Topics shouldn't contain parenthesis or we won't be able to parse output
 * reliably. */
TEST(bad_format)
{
	int section;
	char topic[64];
	const int err = parse_apropos_line("v(1)fm (1) - vi file manager", &section,
			topic, sizeof(topic));

	assert_success(err);
	assert_int_equal(1, section);
	assert_string_equal("v", topic);
}

TEST(section_number_validation)
{
	int section;
	char t[64];

	assert_success(parse_apropos_line("v(0) - d", &section, t, sizeof(t)));
	assert_success(parse_apropos_line("v(1) - d", &section, t, sizeof(t)));
	assert_success(parse_apropos_line("v(8) - d", &section, t, sizeof(t)));

	assert_failure(parse_apropos_line("v - d", &section, t, sizeof(t)));
	assert_failure(parse_apropos_line("v(-1) - d", &section, t, sizeof(t)));
	assert_failure(parse_apropos_line("v() - d", &section, t, sizeof(t)));
	assert_failure(parse_apropos_line("v( - d", &section, t, sizeof(t)));
	assert_failure(parse_apropos_line("v(4", &section, t, sizeof(t)));
}

TEST(section_number_value)
{
	int section;
	char t[64];

	assert_success(parse_apropos_line("v(0) - d", &section, t, sizeof(t)));
	assert_int_equal(0, section);

	assert_success(parse_apropos_line("v(1) - d", &section, t, sizeof(t)));
	assert_int_equal(1, section);

	assert_success(parse_apropos_line("v(8) - d", &section, t, sizeof(t)));
	assert_int_equal(8, section);
}

TEST(too_small_buffer)
{
	int section;
	char topic[64];

	assert_failure(parse_apropos_line("vifm (1) - d", &section, topic, 0));
	assert_failure(parse_apropos_line("vifm (1) - d", &section, topic, 1));
	assert_failure(parse_apropos_line("vifm (1) - d", &section, topic, 2));
	assert_failure(parse_apropos_line("vifm (1) - d", &section, topic, 3));
	assert_failure(parse_apropos_line("vifm (1) - d", &section, topic, 4));
	assert_success(parse_apropos_line("vifm (1) - d", &section, topic, 5));
	assert_string_equal("vifm", topic);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
