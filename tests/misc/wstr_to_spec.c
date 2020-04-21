#include <stic.h>

#include <curses.h> /* KEY_BACKSPACE */

#include <stddef.h> /* wchar_t */
#include <stdlib.h> /* free() */

#include <test-utils.h>

#include "../../src/compat/curses.h"
#include "../../src/modes/wk.h"
#include "../../src/utils/utils.h"
#include "../../src/bracket_notation.h"

SETUP_ONCE()
{
	try_enable_utf8_locale();
}

TEST(c_h_is_bs_at_start_only, IF(utf8_locale))
{
	char *spec;

	spec = wstr_to_spec(WK_C_h);
	assert_string_equal("<bs>", spec);
	free(spec);

	spec = wstr_to_spec(WK_C_w WK_C_h);
	assert_string_equal("<c-w><c-h>", spec);
	free(spec);
}

TEST(backspace_is_bs_always, IF(utf8_locale))
{
	{
		const wchar_t key_seq[] = { K(KEY_BACKSPACE), L'\0' };
		char *const spec = wstr_to_spec(key_seq);
		assert_string_equal("<bs>", spec);
		free(spec);
	}

	{
		const wchar_t key_seq[] = { WC_C_w, K(KEY_BACKSPACE), L'\0' };
		char *const spec = wstr_to_spec(key_seq);
		assert_string_equal("<c-w><bs>", spec);
		free(spec);
	}
}

TEST(functional_keys_do_not_clash_with_characters, IF(utf8_locale))
{
	{
		const wchar_t key_seq[] = { L'š', L'\0' };
		char *const spec = wstr_to_spec(key_seq);
		assert_string_equal("š", spec);
		free(spec);
	}

	{
		const wchar_t key_seq[] = { WC_C_w, L'ć', L'\0' };
		char *const spec = wstr_to_spec(key_seq);
		assert_string_equal("<c-w>ć", spec);
		free(spec);
	}
}

TEST(non_ascii_chars_are_handled_correctly, IF(utf8_locale))
{
	char *const spec = wstr_to_spec(L"П");
	assert_string_equal("П", spec);
	free(spec);
}

TEST(more_non_ascii_chars_are_handled_correctly, IF(utf8_locale))
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
