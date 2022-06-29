#include <stic.h>

#include <curses.h> /* KEY_BACKSPACE */

#include <stddef.h> /* wchar_t */
#include <stdlib.h> /* free() */

#include <test-utils.h>

#include "../../src/compat/curses.h"
#include "../../src/modes/wk.h"
#include "../../src/utils/macros.h"
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

TEST(known_sequences)
{
	struct match {
		const wchar_t from[5];
		const char *to;
	} matches[] = {
		{ { L'\r' },            "<cr>" },
		{ { L'\n' },            "<c-j>" },
		{ { L'\177' },          "<del>" },
		{ { K(KEY_HOME) },      "<home>" },
		{ { K(KEY_END) },       "<end>" },
		{ { K(KEY_UP) },        "<up>" },
		{ { K(KEY_DOWN) },      "<down>" },
		{ { K(KEY_LEFT) },      "<left>" },
		{ { K(KEY_RIGHT) },     "<right>" },
		{ { K(KEY_DC) },        "<delete>" },
		{ { K(KEY_BTAB) },      "<s-tab>" },
		{ { K(KEY_PPAGE) },     "<pageup>" },
		{ { K(KEY_NPAGE) },     "<pagedown>" },
		{ { WC_C_SPACE },       "<c-@>" },
		{ { K(KEY_SHOME) },     "<s-home>" },
		{ { K(KEY_SEND) },      "<s-end>" },
		{ { K(KEY_SR) },        "<s-up>" },
		{ { K(KEY_SF) },        "<s-down>" },
		{ { K(KEY_SLEFT) },     "<s-left>" },
		{ { K(KEY_SRIGHT) },    "<s-right>" },
		{ { K(KEY_SDC) },       "<s-delete>" },
		{ { K(KEY_SIC) },       "<s-insert>" },
		{ { K(KEY_SPREVIOUS) }, "<s-pageup>" },
		{ { K(KEY_SNEXT) },     "<s-pagedown>" },
		{ { L"\x1b" },          "<esc>" },
		{ { L"\x1b" L"a" },     "<a-a>" },
		{ { L"\x1b" L"z" },     "<a-z>" },
		{ { L"\x1b" L"0" },     "<a-0>" },
		{ { L"\x1b" L"9" },     "<a-9>" },
		{ { L"\x1b" L"A" },     "<a-s-a>" },
		{ { L"\x1b" L"Z" },     "<a-s-z>" },
		{ { L"\x1b[Z" },        "<s-tab>" },
		{ { K(KEY_F(0)) },      "<f0>" },
		{ { K(KEY_F(12)) },     "<f12>" },
		{ { K(KEY_F(13)) },     "<s-f1>" },
		{ { K(KEY_F(24)) },     "<s-f12>" },
		{ { K(KEY_F(25)) },     "<c-f1>" },
		{ { K(KEY_F(36)) },     "<c-f12>" },
		{ { K(KEY_F(37)) },     "<a-f1>" },
		{ { K(KEY_F(48)) },     "<a-f12>" },
	};

	unsigned int i;
	for(i = 0; i < ARRAY_LEN(matches); ++i)
	{
		char *spec = wstr_to_spec(matches[i].from);
		assert_string_equal(matches[i].to, spec);
		free(spec);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
