#include <stic.h>

#include <stdlib.h> /* free() */

#include "../../src/bracket_notation.h"

TEST(non_ascii_chars_are_handled_correctly)
{
	char *const spec = wstr_to_spec(L"П");
	assert_string_equal("П", spec);
	free(spec);
}

TEST(more_non_ascii_chars_are_handled_correctly)
{
	char *spec;

	spec = wstr_to_spec(L"Д");
	assert_string_equal("Д", spec);
	free(spec);

	spec = wstr_to_spec(L"Д");
	assert_string_equal("Д", spec);
	free(spec);

	spec = wstr_to_spec(L"Ж");
	assert_string_equal("Ж", spec);
	free(spec);

	spec = wstr_to_spec(L"М");
	assert_string_equal("М", spec);
	free(spec);

	spec = wstr_to_spec(L"О");
	assert_string_equal("О", spec);
	free(spec);

	spec = wstr_to_spec(L"Ф");
	assert_string_equal("Ф", spec);
	free(spec);

	spec = wstr_to_spec(L"Ц");
	assert_string_equal("Ц", spec);
	free(spec);

	spec = wstr_to_spec(L"Ь");
	assert_string_equal("Ь", spec);
	free(spec);

	spec = wstr_to_spec(L"Ю");
	assert_string_equal("Ю", spec);
	free(spec);

	spec = wstr_to_spec(L"д");
	assert_string_equal("д", spec);
	free(spec);

	spec = wstr_to_spec(L"ж");
	assert_string_equal("ж", spec);
	free(spec);

	spec = wstr_to_spec(L"м");
	assert_string_equal("м", spec);
	free(spec);

	spec = wstr_to_spec(L"о");
	assert_string_equal("о", spec);
	free(spec);

	spec = wstr_to_spec(L"ф");
	assert_string_equal("ф", spec);
	free(spec);

	spec = wstr_to_spec(L"ц");
	assert_string_equal("ц", spec);
	free(spec);

	spec = wstr_to_spec(L"ь");
	assert_string_equal("ь", spec);
	free(spec);

	spec = wstr_to_spec(L"ю");
	assert_string_equal("ю", spec);
	free(spec);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
