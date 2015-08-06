#include <stic.h>

#include "../../src/menus/apropos_menu.h"

TEST(groff_format)
{
	char section[64], topic[64];
	assert_success(parse_apropos_line("vifm    (1)  - vi file manager", section,
			sizeof(section), topic, sizeof(topic)));

	assert_string_equal("1", section);
	assert_string_equal("vifm", topic);
}

TEST(mandoc_format)
{
	char section[64], topic[64];
	assert_success(parse_apropos_line("vifm(1)  - vi file manager", section,
			sizeof(section), topic, sizeof(topic)));

	assert_string_equal("1", section);
	assert_string_equal("vifm", topic);
}

/* Topics shouldn't contain parenthesis or we won't be able to parse output
 * reliably. */
TEST(bad_format)
{
	char section[64], topic[64];
	assert_success(parse_apropos_line("v(1)fm (1) - vi file manager", section,
			sizeof(section), topic, sizeof(topic)));

	assert_string_equal("1", section);
	assert_string_equal("v", topic);
}

TEST(section_validation)
{
	char s[64], t[64];

	assert_success(parse_apropos_line("v(0) - d", s, sizeof(s), t, sizeof(t)));
	assert_success(parse_apropos_line("v(1) - d", s, sizeof(s), t, sizeof(t)));
	assert_success(parse_apropos_line("v(8) - d", s, sizeof(s), t, sizeof(t)));

	assert_failure(parse_apropos_line("v - d", s, sizeof(s), t, sizeof(t)));
	assert_failure(parse_apropos_line("v() - d", s, sizeof(s), t, sizeof(t)));
	assert_failure(parse_apropos_line("v( - d", s, sizeof(s), t, sizeof(t)));
	assert_failure(parse_apropos_line("v(4", s, sizeof(s), t, sizeof(t)));
	assert_failure(parse_apropos_line("v(()", s, sizeof(s), t, sizeof(t)));
	assert_failure(parse_apropos_line("v(())", s, sizeof(s), t, sizeof(t)));
	assert_failure(parse_apropos_line("v())", s, sizeof(s), t, sizeof(t)));
	assert_failure(parse_apropos_line("v( )", s, sizeof(s), t, sizeof(t)));
	assert_failure(parse_apropos_line("v(a\tb)", s, sizeof(s), t, sizeof(t)));
}

TEST(section_value)
{
	char s[64], t[64];

	assert_success(parse_apropos_line("v(0) - d", s, sizeof(s), t, sizeof(t)));
	assert_string_equal("0", s);

	assert_success(parse_apropos_line("v(1) - d", s, sizeof(s), t, sizeof(t)));
	assert_string_equal("1", s);

	assert_success(parse_apropos_line("v(666) - d", s, sizeof(s), t, sizeof(t)));
	assert_string_equal("666", s);

	assert_success(parse_apropos_line("v(n) - d", s, sizeof(s), t, sizeof(t)));
	assert_string_equal("n", s);

	assert_success(parse_apropos_line("v(p1) - d", s, sizeof(s), t, sizeof(t)));
	assert_string_equal("p1", s);

	assert_success(parse_apropos_line("v(#3-*) - d", s, sizeof(s), t, sizeof(t)));
	assert_string_equal("#3-*", s);
}

TEST(too_small_buffer)
{
	char section[64], topic[64];

	assert_failure(parse_apropos_line("vifm (1) - d", section, 10, topic, 0));
	assert_failure(parse_apropos_line("vifm (1) - d", section, 10, topic, 1));
	assert_failure(parse_apropos_line("vifm (1) - d", section, 10, topic, 2));
	assert_failure(parse_apropos_line("vifm (1) - d", section, 10, topic, 3));
	assert_failure(parse_apropos_line("vifm (1) - d", section, 10, topic, 4));
	assert_success(parse_apropos_line("vifm (1) - d", section, 10, topic, 5));
	assert_string_equal("vifm", topic);

	assert_failure(parse_apropos_line("vifm (123) - d", section, 0, topic, 10));
	assert_failure(parse_apropos_line("vifm (123) - d", section, 1, topic, 10));
	assert_failure(parse_apropos_line("vifm (123) - d", section, 2, topic, 10));
	assert_failure(parse_apropos_line("vifm (123) - d", section, 3, topic, 10));
	assert_success(parse_apropos_line("vifm (123) - d", section, 4, topic, 10));
	assert_string_equal("123", section);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
