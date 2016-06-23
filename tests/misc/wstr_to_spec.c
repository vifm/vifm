#include <stic.h>

#include <curses.h> /* KEY_BACKSPACE */

#include <locale.h> /* setlocale() */
#include <stddef.h> /* wchar_t */
#include <stdlib.h> /* free() */

#include "../../src/modes/wk.h"
#include "../../src/utils/utils.h"
#include "../../src/bracket_notation.h"

static int locale_works(void);

SETUP_ONCE()
{
	(void)setlocale(LC_ALL, "");
	if(!locale_works())
	{
		(void)setlocale(LC_ALL, "en_US.utf8");
	}
}

TEST(c_h_is_bs_at_start_only, IF(locale_works))
{
	char *spec;

	spec = wstr_to_spec(WK_C_h);
	assert_string_equal("<bs>", spec);
	free(spec);

	spec = wstr_to_spec(WK_C_w WK_C_h);
	assert_string_equal("<c-w><c-h>", spec);
	free(spec);
}

TEST(backspace_is_bs_always, IF(locale_works))
{
	{
		const wchar_t key_seq[] = { KEY_BACKSPACE, L'\0' };
		char *const spec = wstr_to_spec(key_seq);
		assert_string_equal("<bs>", spec);
		free(spec);
	}

	{
		const wchar_t key_seq[] = { WC_C_w, KEY_BACKSPACE, L'\0' };
		char *const spec = wstr_to_spec(key_seq);
		assert_string_equal("<c-w><bs>", spec);
		free(spec);
	}
}

TEST(non_ascii_chars_are_handled_correctly, IF(locale_works))
{
	char *const spec = wstr_to_spec(L"П");
	assert_string_equal("П", spec);
	free(spec);
}

TEST(more_non_ascii_chars_are_handled_correctly, IF(locale_works))
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

static int
locale_works(void)
{
	return (vifm_wcwidth(L'丝') == 2);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
