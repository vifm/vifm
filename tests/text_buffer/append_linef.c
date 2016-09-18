#include <stic.h>

#include "../../src/engine/text_buffer.h"

TEST(new_line_is_inserted_between_lines)
{
	vle_textbuf *str = vle_tb_create();

	vle_tb_append_linef(str, "%s", "a");
	vle_tb_append_linef(str, "%s", "b");

	assert_string_equal("a\nb", vle_tb_get_data(str));

	vle_tb_free(str);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
