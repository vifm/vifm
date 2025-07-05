#include <stic.h>

#include <stdlib.h> /* free() */
#include <string.h> /* memset() strcmp() */
#include <wchar.h> /* wchar_t */

#include "../../src/engine/parsing.h"
#include "../../src/engine/var.h"
#include "../../src/utils/macros.h"

#include "asserts.h"

static const wchar_t * expand_notation(const char str[]);

TEST(empty_ok)
{
	ASSERT_OK("\"\"", "");
}

TEST(simple_ok)
{
	ASSERT_OK("\"test\"", "test");
}

TEST(not_closed_error)
{
	ASSERT_FAIL("\"test", PE_MISSING_QUOTE);
}

TEST(concatenation)
{
	ASSERT_OK("\"NV\".\"AR\"", "NVAR");
	ASSERT_OK("\"NV\" .\"AR\"", "NVAR");
	ASSERT_OK("\"NV\". \"AR\"", "NVAR");
	ASSERT_OK("\"NV\" . \"AR\"", "NVAR");
}

TEST(double_quote_escaping_ok)
{
	ASSERT_OK("\"\\\"\"", "\"");
}

TEST(special_chars_ok)
{
	ASSERT_OK("\"\\t\"", "\t");
}

TEST(spaces_ok)
{
	ASSERT_OK("\" s y \"", " s y ");
}

TEST(dot_ok)
{
	ASSERT_OK("\"a . c\"", "a . c");
}

TEST(escaping_char_with_highest_bit)
{
	ASSERT_OK("\"\\\x80 \\\xff\"", "\x80 \xff");
}

TEST(very_long_string)
{
	char string[8192];
	string[0] = '\"';
	memset(string + 1, '0', sizeof(string) - 2U);
	string[sizeof(string) - 1U] = '\0';

	ASSERT_FAIL(string, PE_INTERNAL);
}

TEST(bracket_notation)
{
	/* No notation by default. */
	vle_parser_set_notation(NULL);
	ASSERT_OK("\"\\<space>\"", "<space>");

	vle_parser_set_notation(&expand_notation);

	/* Unescaped notation. */
	ASSERT_OK("\"<space>\"", "<space>");

	/* Escaped notation. */
	ASSERT_OK("\"\\<space>\"", " ");

	/* Unrecognized notation. */
	ASSERT_OK("\"\\<notspace>\"", "<notspace>");

	/* Unrecognized notation which is too long for the parser. */
	ASSERT_OK("\"\\<nosuchexcessivelylongnotation>\"",
			"<nosuchexcessivelylongnotation>");

	/* Recognized notation which is too long for the parser. */
	ASSERT_OK("\"\\<thisnotationiswaytoolong>\"", "<thisnotationiswaytoolong>");

	/* Unfinished notation. */
	ASSERT_OK("\"\\<space\"", "<space");

	/* Nested notation. */
	ASSERT_OK("\"\\<sp\\ace>\"", "<space>");
	ASSERT_OK("\"\\<sp\\<a>ce>\"", "<sp<a>ce>");
	ASSERT_OK("\"\\<\\<tab>>\"", "<\t>");

	/* Unrealistically long expansion. */
	ASSERT_FAIL("\"\\<very_long>\"", PE_INTERNAL);

	/* More realistic case. */
	ASSERT_OK("\"ma\\<space>yy\\<tab>\\<tab>p'a\"", "ma yy\t\tp'a");
}

static const wchar_t *
expand_notation(const char str[])
{
	static wchar_t very_long[5*1024];

	if(very_long[0] == '\0')
	{
		int i;
		for(i = 0; i < (int)ARRAY_LEN(very_long) - 1; ++i)
		{
			very_long[i] = L'1';
		}
	}

	if(strcmp(str, "<thisnotationiswaytoolong>") == 0)
	{
		return L"!!!";
	}
	if(strcmp(str, "<very_long>") == 0)
	{
		return very_long;
	}
	if(strcmp(str, "<space>") == 0)
	{
		return L" ";
	}
	if(strcmp(str, "<tab>") == 0)
	{
		return L"\t";
	}
	return NULL;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
