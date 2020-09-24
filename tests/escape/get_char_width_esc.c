#include <stic.h>

#include "../../src/ui/escape.h"

TEST(line_erasure_sequence_is_eaten_semicolons)
{
	size_t w = get_char_width_esc("\033[K\033[22;24;25;27;28;38;5;247;48;5;235m");
	assert_int_equal(3, w);
}

TEST(line_erasure_sequence_is_eaten_commas)
{
	size_t w = get_char_width_esc("\033[K\033[22,24,25,27,28,38,5,247,48,5,235m");
	assert_int_equal(3, w);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
