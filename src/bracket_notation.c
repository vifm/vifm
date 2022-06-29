/* vifm
 * Copyright (C) 2012 xaizek.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "bracket_notation.h"

#include <curses.h>

#include <assert.h> /* assert() */
#include <ctype.h> /* tolower() */
#include <stddef.h> /* NULL size_t wchar_t */
#include <stdlib.h> /* free() */
#include <string.h> /* strcpy() strlen() */
#include <wchar.h> /* wcscpy() wcslen() */
#include <wctype.h> /* iswcntrl() */

#include "compat/curses.h"
#include "compat/reallocarray.h"
#include "engine/text_buffer.h"
#include "modes/wk.h"
#include "utils/macros.h"
#include "utils/str.h"
#include "utils/utils.h"

/* Bracket notation entry description structure. */
typedef struct
{
	const wchar_t *notation; /* The notation itself. */
	const wchar_t key[8]; /* The replacement for the notation. */
	size_t len; /* Length of the notation. */
}
key_pair_t;

static int notation_sorter(const void *first, const void *second);
static const key_pair_t * find_notation(const wchar_t str[]);
static const char * wchar_to_spec(const wchar_t c[], size_t *len, int bs);

/* All notation fields must be written in lower case. */
static key_pair_t key_pairs[] = {
	{ L"<esc>",      L"\x1b"              },
	{ L"<c-@>",      { WC_C_SPACE }       },
	{ L"<c-a>",      L"\x01"              },
	{ L"<c-b>",      L"\x02"              },
	{ L"<c-c>",      L"\x03"              },
	{ L"<c-d>",      L"\x04"              },
	{ L"<c-e>",      L"\x05"              },
	{ L"<c-f>",      L"\x06"              },
	{ L"<c-g>",      L"\x07"              },
	{ L"<c-h>",      L"\x08"              },
	{ L"<c-i>",      L"\x09"              },
	{ L"<c-j>",      L"\x0a"              },
	{ L"<c-k>",      L"\x0b"              },
	{ L"<c-l>",      L"\x0c"              },
	{ L"<c-m>",      L"\x0d"              },
	{ L"<c-n>",      L"\x0e"              },
	{ L"<c-o>",      L"\x0f"              },
	{ L"<c-p>",      L"\x10"              },
	{ L"<c-q>",      L"\x11"              },
	{ L"<c-r>",      L"\x12"              },
	{ L"<c-s>",      L"\x13"              },
	{ L"<c-t>",      L"\x14"              },
	{ L"<c-u>",      L"\x15"              },
	{ L"<c-v>",      L"\x16"              },
	{ L"<c-w>",      L"\x17"              },
	{ L"<c-x>",      L"\x18"              },
	{ L"<c-y>",      L"\x19"              },
	{ L"<c-z>",      L"\x1a"              },
	{ L"<c-[>",      L"\x1b"              },
	{ L"<c-\\>",     L"\x1c"              },
	{ L"<c-]>",      L"\x1d"              },
	{ L"<c-^>",      L"\x1e"              },
	{ L"<c-_>",      L"\x1f"              },
	{ L"<s-c-a>",    L"\x01"              },
	{ L"<s-c-b>",    L"\x02"              },
	{ L"<s-c-c>",    L"\x03"              },
	{ L"<s-c-d>",    L"\x04"              },
	{ L"<s-c-e>",    L"\x05"              },
	{ L"<s-c-f>",    L"\x06"              },
	{ L"<s-c-g>",    L"\x07"              },
	{ L"<s-c-h>",    L"\x08"              },
	{ L"<s-c-i>",    L"\x09"              },
	{ L"<s-c-j>",    L"\x0a"              },
	{ L"<s-c-k>",    L"\x0b"              },
	{ L"<s-c-l>",    L"\x0c"              },
	{ L"<s-c-m>",    L"\x0d"              },
	{ L"<s-c-n>",    L"\x0e"              },
	{ L"<s-c-o>",    L"\x0f"              },
	{ L"<s-c-p>",    L"\x10"              },
	{ L"<s-c-q>",    L"\x11"              },
	{ L"<s-c-r>",    L"\x12"              },
	{ L"<s-c-s>",    L"\x13"              },
	{ L"<s-c-t>",    L"\x14"              },
	{ L"<s-c-u>",    L"\x15"              },
	{ L"<s-c-v>",    L"\x16"              },
	{ L"<s-c-w>",    L"\x17"              },
	{ L"<s-c-x>",    L"\x18"              },
	{ L"<s-c-y>",    L"\x19"              },
	{ L"<s-c-z>",    L"\x1a"              },
	{ L"<s-c-[>",    L"\x1b"              },
	{ L"<s-c-\\>",   L"\x1c"              },
	{ L"<s-c-]>",    L"\x1d"              },
	{ L"<s-c-^>",    L"\x1e"              },
	{ L"<s-c-_>",    L"\x1f"              },
	{ L"<c-s-a>",    L"\x01"              },
	{ L"<c-s-b>",    L"\x02"              },
	{ L"<c-s-c>",    L"\x03"              },
	{ L"<c-s-d>",    L"\x04"              },
	{ L"<c-s-e>",    L"\x05"              },
	{ L"<c-s-f>",    L"\x06"              },
	{ L"<c-s-g>",    L"\x07"              },
	{ L"<c-s-h>",    L"\x08"              },
	{ L"<c-s-i>",    L"\x09"              },
	{ L"<c-s-j>",    L"\x0a"              },
	{ L"<c-s-k>",    L"\x0b"              },
	{ L"<c-s-l>",    L"\x0c"              },
	{ L"<c-s-m>",    L"\x0d"              },
	{ L"<c-s-n>",    L"\x0e"              },
	{ L"<c-s-o>",    L"\x0f"              },
	{ L"<c-s-p>",    L"\x10"              },
	{ L"<c-s-q>",    L"\x11"              },
	{ L"<c-s-r>",    L"\x12"              },
	{ L"<c-s-s>",    L"\x13"              },
	{ L"<c-s-t>",    L"\x14"              },
	{ L"<c-s-u>",    L"\x15"              },
	{ L"<c-s-v>",    L"\x16"              },
	{ L"<c-s-w>",    L"\x17"              },
	{ L"<c-s-x>",    L"\x18"              },
	{ L"<c-s-y>",    L"\x19"              },
	{ L"<c-s-z>",    L"\x1a"              },
	{ L"<c-s-[>",    L"\x1b"              },
	{ L"<c-s-\\>",   L"\x1c"              },
	{ L"<c-s-]>",    L"\x1d"              },
	{ L"<c-s-^>",    L"\x1e"              },
	{ L"<c-s-_>",    L"\x1f"              },
	{ L"<cr>",       L"\r"                },
	{ L"<space>",    L" "                 },
	{ L"<nop>",      L""                  },
	{ L"<tab>",      L"\t"                },
	{ L"<lt>",       L"<"                 },
#ifdef ENABLE_EXTENDED_KEYS
	{ L"<s-tab>",    { K(KEY_BTAB) }      },
#else
	{ L"<s-tab>",    L"\033"L"[Z"         },
#endif
#ifndef __PDCURSES__
	{ L"<a-0>",      L"\033"L"0"          },
	{ L"<a-1>",      L"\033"L"1"          },
	{ L"<a-2>",      L"\033"L"2"          },
	{ L"<a-3>",      L"\033"L"3"          },
	{ L"<a-4>",      L"\033"L"4"          },
	{ L"<a-5>",      L"\033"L"5"          },
	{ L"<a-6>",      L"\033"L"6"          },
	{ L"<a-7>",      L"\033"L"7"          },
	{ L"<a-8>",      L"\033"L"8"          },
	{ L"<a-9>",      L"\033"L"9"          },
	{ L"<a-a>",      L"\033"L"a"          },
	{ L"<a-b>",      L"\033"L"b"          },
	{ L"<a-c>",      L"\033"L"c"          },
	{ L"<a-d>",      L"\033"L"d"          },
	{ L"<a-e>",      L"\033"L"e"          },
	{ L"<a-f>",      L"\033"L"f"          },
	{ L"<a-g>",      L"\033"L"g"          },
	{ L"<a-h>",      L"\033"L"h"          },
	{ L"<a-i>",      L"\033"L"i"          },
	{ L"<a-j>",      L"\033"L"j"          },
	{ L"<a-k>",      L"\033"L"k"          },
	{ L"<a-l>",      L"\033"L"l"          },
	{ L"<a-m>",      L"\033"L"m"          },
	{ L"<a-n>",      L"\033"L"n"          },
	{ L"<a-o>",      L"\033"L"o"          },
	{ L"<a-p>",      L"\033"L"p"          },
	{ L"<a-q>",      L"\033"L"q"          },
	{ L"<a-r>",      L"\033"L"r"          },
	{ L"<a-s>",      L"\033"L"s"          },
	{ L"<a-t>",      L"\033"L"t"          },
	{ L"<a-u>",      L"\033"L"u"          },
	{ L"<a-v>",      L"\033"L"v"          },
	{ L"<a-w>",      L"\033"L"w"          },
	{ L"<a-x>",      L"\033"L"x"          },
	{ L"<a-y>",      L"\033"L"y"          },
	{ L"<a-z>",      L"\033"L"z"          },
	{ L"<m-0>",      L"\033"L"0"          },
	{ L"<m-1>",      L"\033"L"1"          },
	{ L"<m-2>",      L"\033"L"2"          },
	{ L"<m-3>",      L"\033"L"3"          },
	{ L"<m-4>",      L"\033"L"4"          },
	{ L"<m-5>",      L"\033"L"5"          },
	{ L"<m-6>",      L"\033"L"6"          },
	{ L"<m-7>",      L"\033"L"7"          },
	{ L"<m-8>",      L"\033"L"8"          },
	{ L"<m-9>",      L"\033"L"9"          },
	{ L"<m-a>",      L"\033"L"a"          },
	{ L"<m-b>",      L"\033"L"b"          },
	{ L"<m-c>",      L"\033"L"c"          },
	{ L"<m-d>",      L"\033"L"d"          },
	{ L"<m-e>",      L"\033"L"e"          },
	{ L"<m-f>",      L"\033"L"f"          },
	{ L"<m-g>",      L"\033"L"g"          },
	{ L"<m-h>",      L"\033"L"h"          },
	{ L"<m-i>",      L"\033"L"i"          },
	{ L"<m-j>",      L"\033"L"j"          },
	{ L"<m-k>",      L"\033"L"k"          },
	{ L"<m-l>",      L"\033"L"l"          },
	{ L"<m-m>",      L"\033"L"m"          },
	{ L"<m-n>",      L"\033"L"n"          },
	{ L"<m-o>",      L"\033"L"o"          },
	{ L"<m-p>",      L"\033"L"p"          },
	{ L"<m-q>",      L"\033"L"q"          },
	{ L"<m-r>",      L"\033"L"r"          },
	{ L"<m-s>",      L"\033"L"s"          },
	{ L"<m-t>",      L"\033"L"t"          },
	{ L"<m-u>",      L"\033"L"u"          },
	{ L"<m-v>",      L"\033"L"v"          },
	{ L"<m-w>",      L"\033"L"w"          },
	{ L"<m-x>",      L"\033"L"x"          },
	{ L"<m-y>",      L"\033"L"y"          },
	{ L"<m-z>",      L"\033"L"z"          },
	{ L"<a-c-a>",    L"\033"L"\x01"       },
	{ L"<a-c-b>",    L"\033"L"\x02"       },
	{ L"<a-c-c>",    L"\033"L"\x03"       },
	{ L"<a-c-d>",    L"\033"L"\x04"       },
	{ L"<a-c-e>",    L"\033"L"\x05"       },
	{ L"<a-c-f>",    L"\033"L"\x06"       },
	{ L"<a-c-g>",    L"\033"L"\x07"       },
	{ L"<a-c-h>",    L"\033"L"\x08"       },
	{ L"<a-c-i>",    L"\033"L"\x09"       },
	{ L"<a-c-j>",    L"\033"L"\x0a"       },
	{ L"<a-c-k>",    L"\033"L"\x0b"       },
	{ L"<a-c-l>",    L"\033"L"\x0c"       },
	{ L"<a-c-m>",    L"\033"L"\x0d"       },
	{ L"<a-c-n>",    L"\033"L"\x0e"       },
	{ L"<a-c-o>",    L"\033"L"\x0f"       },
	{ L"<a-c-p>",    L"\033"L"\x10"       },
	{ L"<a-c-q>",    L"\033"L"\x11"       },
	{ L"<a-c-r>",    L"\033"L"\x12"       },
	{ L"<a-c-s>",    L"\033"L"\x13"       },
	{ L"<a-c-t>",    L"\033"L"\x14"       },
	{ L"<a-c-u>",    L"\033"L"\x15"       },
	{ L"<a-c-v>",    L"\033"L"\x16"       },
	{ L"<a-c-w>",    L"\033"L"\x17"       },
	{ L"<a-c-x>",    L"\033"L"\x18"       },
	{ L"<a-c-y>",    L"\033"L"\x19"       },
	{ L"<a-c-z>",    L"\033"L"\x1a"       },
	{ L"<m-c-a>",    L"\033"L"\x01"       },
	{ L"<m-c-b>",    L"\033"L"\x02"       },
	{ L"<m-c-c>",    L"\033"L"\x03"       },
	{ L"<m-c-d>",    L"\033"L"\x04"       },
	{ L"<m-c-e>",    L"\033"L"\x05"       },
	{ L"<m-c-f>",    L"\033"L"\x06"       },
	{ L"<m-c-g>",    L"\033"L"\x07"       },
	{ L"<m-c-h>",    L"\033"L"\x08"       },
	{ L"<m-c-i>",    L"\033"L"\x09"       },
	{ L"<m-c-j>",    L"\033"L"\x0a"       },
	{ L"<m-c-k>",    L"\033"L"\x0b"       },
	{ L"<m-c-l>",    L"\033"L"\x0c"       },
	{ L"<m-c-m>",    L"\033"L"\x0d"       },
	{ L"<m-c-n>",    L"\033"L"\x0e"       },
	{ L"<m-c-o>",    L"\033"L"\x0f"       },
	{ L"<m-c-p>",    L"\033"L"\x10"       },
	{ L"<m-c-q>",    L"\033"L"\x11"       },
	{ L"<m-c-r>",    L"\033"L"\x12"       },
	{ L"<m-c-s>",    L"\033"L"\x13"       },
	{ L"<m-c-t>",    L"\033"L"\x14"       },
	{ L"<m-c-u>",    L"\033"L"\x15"       },
	{ L"<m-c-v>",    L"\033"L"\x16"       },
	{ L"<m-c-w>",    L"\033"L"\x17"       },
	{ L"<m-c-x>",    L"\033"L"\x18"       },
	{ L"<m-c-y>",    L"\033"L"\x19"       },
	{ L"<m-c-z>",    L"\033"L"\x1a"       },
	{ L"<c-a-a>",    L"\033"L"\x01"       },
	{ L"<c-a-b>",    L"\033"L"\x02"       },
	{ L"<c-a-c>",    L"\033"L"\x03"       },
	{ L"<c-a-d>",    L"\033"L"\x04"       },
	{ L"<c-a-e>",    L"\033"L"\x05"       },
	{ L"<c-a-f>",    L"\033"L"\x06"       },
	{ L"<c-a-g>",    L"\033"L"\x07"       },
	{ L"<c-a-h>",    L"\033"L"\x08"       },
	{ L"<c-a-i>",    L"\033"L"\x09"       },
	{ L"<c-a-j>",    L"\033"L"\x0a"       },
	{ L"<c-a-k>",    L"\033"L"\x0b"       },
	{ L"<c-a-l>",    L"\033"L"\x0c"       },
	{ L"<c-a-m>",    L"\033"L"\x0d"       },
	{ L"<c-a-n>",    L"\033"L"\x0e"       },
	{ L"<c-a-o>",    L"\033"L"\x0f"       },
	{ L"<c-a-p>",    L"\033"L"\x10"       },
	{ L"<c-a-q>",    L"\033"L"\x11"       },
	{ L"<c-a-r>",    L"\033"L"\x12"       },
	{ L"<c-a-s>",    L"\033"L"\x13"       },
	{ L"<c-a-t>",    L"\033"L"\x14"       },
	{ L"<c-a-u>",    L"\033"L"\x15"       },
	{ L"<c-a-v>",    L"\033"L"\x16"       },
	{ L"<c-a-w>",    L"\033"L"\x17"       },
	{ L"<c-a-x>",    L"\033"L"\x18"       },
	{ L"<c-a-y>",    L"\033"L"\x19"       },
	{ L"<c-a-z>",    L"\033"L"\x1a"       },
	{ L"<c-m-a>",    L"\033"L"\x01"       },
	{ L"<c-m-b>",    L"\033"L"\x02"       },
	{ L"<c-m-c>",    L"\033"L"\x03"       },
	{ L"<c-m-d>",    L"\033"L"\x04"       },
	{ L"<c-m-e>",    L"\033"L"\x05"       },
	{ L"<c-m-f>",    L"\033"L"\x06"       },
	{ L"<c-m-g>",    L"\033"L"\x07"       },
	{ L"<c-m-h>",    L"\033"L"\x08"       },
	{ L"<c-m-i>",    L"\033"L"\x09"       },
	{ L"<c-m-j>",    L"\033"L"\x0a"       },
	{ L"<c-m-k>",    L"\033"L"\x0b"       },
	{ L"<c-m-l>",    L"\033"L"\x0c"       },
	{ L"<c-m-m>",    L"\033"L"\x0d"       },
	{ L"<c-m-n>",    L"\033"L"\x0e"       },
	{ L"<c-m-o>",    L"\033"L"\x0f"       },
	{ L"<c-m-p>",    L"\033"L"\x10"       },
	{ L"<c-m-q>",    L"\033"L"\x11"       },
	{ L"<c-m-r>",    L"\033"L"\x12"       },
	{ L"<c-m-s>",    L"\033"L"\x13"       },
	{ L"<c-m-t>",    L"\033"L"\x14"       },
	{ L"<c-m-u>",    L"\033"L"\x15"       },
	{ L"<c-m-v>",    L"\033"L"\x16"       },
	{ L"<c-m-w>",    L"\033"L"\x17"       },
	{ L"<c-m-x>",    L"\033"L"\x18"       },
	{ L"<c-m-y>",    L"\033"L"\x19"       },
	{ L"<c-m-z>",    L"\033"L"\x1a"       },
	{ L"<a-s-a>",    L"\033"L"A"          },
	{ L"<a-s-b>",    L"\033"L"B"          },
	{ L"<a-s-c>",    L"\033"L"C"          },
	{ L"<a-s-d>",    L"\033"L"D"          },
	{ L"<a-s-e>",    L"\033"L"E"          },
	{ L"<a-s-f>",    L"\033"L"F"          },
	{ L"<a-s-g>",    L"\033"L"G"          },
	{ L"<a-s-h>",    L"\033"L"H"          },
	{ L"<a-s-i>",    L"\033"L"I"          },
	{ L"<a-s-j>",    L"\033"L"J"          },
	{ L"<a-s-k>",    L"\033"L"K"          },
	{ L"<a-s-l>",    L"\033"L"L"          },
	{ L"<a-s-m>",    L"\033"L"M"          },
	{ L"<a-s-n>",    L"\033"L"N"          },
	{ L"<a-s-o>",    L"\033"L"O"          },
	{ L"<a-s-p>",    L"\033"L"P"          },
	{ L"<a-s-q>",    L"\033"L"Q"          },
	{ L"<a-s-r>",    L"\033"L"R"          },
	{ L"<a-s-s>",    L"\033"L"S"          },
	{ L"<a-s-t>",    L"\033"L"T"          },
	{ L"<a-s-u>",    L"\033"L"U"          },
	{ L"<a-s-v>",    L"\033"L"V"          },
	{ L"<a-s-w>",    L"\033"L"W"          },
	{ L"<a-s-x>",    L"\033"L"X"          },
	{ L"<a-s-y>",    L"\033"L"Y"          },
	{ L"<a-s-z>",    L"\033"L"Z"          },
	{ L"<s-a-a>",    L"\033"L"A"          },
	{ L"<s-a-b>",    L"\033"L"B"          },
	{ L"<s-a-c>",    L"\033"L"C"          },
	{ L"<s-a-d>",    L"\033"L"D"          },
	{ L"<s-a-e>",    L"\033"L"E"          },
	{ L"<s-a-f>",    L"\033"L"F"          },
	{ L"<s-a-g>",    L"\033"L"G"          },
	{ L"<s-a-h>",    L"\033"L"H"          },
	{ L"<s-a-i>",    L"\033"L"I"          },
	{ L"<s-a-j>",    L"\033"L"J"          },
	{ L"<s-a-k>",    L"\033"L"K"          },
	{ L"<s-a-l>",    L"\033"L"L"          },
	{ L"<s-a-m>",    L"\033"L"M"          },
	{ L"<s-a-n>",    L"\033"L"N"          },
	{ L"<s-a-o>",    L"\033"L"O"          },
	{ L"<s-a-p>",    L"\033"L"P"          },
	{ L"<s-a-q>",    L"\033"L"Q"          },
	{ L"<s-a-r>",    L"\033"L"R"          },
	{ L"<s-a-s>",    L"\033"L"S"          },
	{ L"<s-a-t>",    L"\033"L"T"          },
	{ L"<s-a-u>",    L"\033"L"U"          },
	{ L"<s-a-v>",    L"\033"L"V"          },
	{ L"<s-a-w>",    L"\033"L"W"          },
	{ L"<s-a-x>",    L"\033"L"X"          },
	{ L"<s-a-y>",    L"\033"L"Y"          },
	{ L"<s-a-z>",    L"\033"L"Z"          },
	{ L"<m-s-a>",    L"\033"L"A"          },
	{ L"<m-s-b>",    L"\033"L"B"          },
	{ L"<m-s-c>",    L"\033"L"C"          },
	{ L"<m-s-d>",    L"\033"L"D"          },
	{ L"<m-s-e>",    L"\033"L"E"          },
	{ L"<m-s-f>",    L"\033"L"F"          },
	{ L"<m-s-g>",    L"\033"L"G"          },
	{ L"<m-s-h>",    L"\033"L"H"          },
	{ L"<m-s-i>",    L"\033"L"I"          },
	{ L"<m-s-j>",    L"\033"L"J"          },
	{ L"<m-s-k>",    L"\033"L"K"          },
	{ L"<m-s-l>",    L"\033"L"L"          },
	{ L"<m-s-m>",    L"\033"L"M"          },
	{ L"<m-s-n>",    L"\033"L"N"          },
	{ L"<m-s-o>",    L"\033"L"O"          },
	{ L"<m-s-p>",    L"\033"L"P"          },
	{ L"<m-s-q>",    L"\033"L"Q"          },
	{ L"<m-s-r>",    L"\033"L"R"          },
	{ L"<m-s-s>",    L"\033"L"S"          },
	{ L"<m-s-t>",    L"\033"L"T"          },
	{ L"<m-s-u>",    L"\033"L"U"          },
	{ L"<m-s-v>",    L"\033"L"V"          },
	{ L"<m-s-w>",    L"\033"L"W"          },
	{ L"<m-s-x>",    L"\033"L"X"          },
	{ L"<m-s-y>",    L"\033"L"Y"          },
	{ L"<m-s-z>",    L"\033"L"Z"          },
	{ L"<s-m-a>",    L"\033"L"A"          },
	{ L"<s-m-b>",    L"\033"L"B"          },
	{ L"<s-m-c>",    L"\033"L"C"          },
	{ L"<s-m-d>",    L"\033"L"D"          },
	{ L"<s-m-e>",    L"\033"L"E"          },
	{ L"<s-m-f>",    L"\033"L"F"          },
	{ L"<s-m-g>",    L"\033"L"G"          },
	{ L"<s-m-h>",    L"\033"L"H"          },
	{ L"<s-m-i>",    L"\033"L"I"          },
	{ L"<s-m-j>",    L"\033"L"J"          },
	{ L"<s-m-k>",    L"\033"L"K"          },
	{ L"<s-m-l>",    L"\033"L"L"          },
	{ L"<s-m-m>",    L"\033"L"M"          },
	{ L"<s-m-n>",    L"\033"L"N"          },
	{ L"<s-m-o>",    L"\033"L"O"          },
	{ L"<s-m-p>",    L"\033"L"P"          },
	{ L"<s-m-q>",    L"\033"L"Q"          },
	{ L"<s-m-r>",    L"\033"L"R"          },
	{ L"<s-m-s>",    L"\033"L"S"          },
	{ L"<s-m-t>",    L"\033"L"T"          },
	{ L"<s-m-u>",    L"\033"L"U"          },
	{ L"<s-m-v>",    L"\033"L"V"          },
	{ L"<s-m-w>",    L"\033"L"W"          },
	{ L"<s-m-x>",    L"\033"L"X"          },
	{ L"<s-m-y>",    L"\033"L"Y"          },
	{ L"<s-m-z>",    L"\033"L"Z"          },
#else
	{ L"<a-a>",      { K(ALT_A) }         },
	{ L"<a-b>",      { K(ALT_B) }         },
	{ L"<a-c>",      { K(ALT_C) }         },
	{ L"<a-d>",      { K(ALT_D) }         },
	{ L"<a-e>",      { K(ALT_E) }         },
	{ L"<a-f>",      { K(ALT_F) }         },
	{ L"<a-g>",      { K(ALT_G) }         },
	{ L"<a-h>",      { K(ALT_H) }         },
	{ L"<a-i>",      { K(ALT_I) }         },
	{ L"<a-j>",      { K(ALT_J) }         },
	{ L"<a-k>",      { K(ALT_K) }         },
	{ L"<a-l>",      { K(ALT_L) }         },
	{ L"<a-m>",      { K(ALT_M) }         },
	{ L"<a-n>",      { K(ALT_N) }         },
	{ L"<a-o>",      { K(ALT_O) }         },
	{ L"<a-p>",      { K(ALT_P) }         },
	{ L"<a-q>",      { K(ALT_Q) }         },
	{ L"<a-r>",      { K(ALT_R) }         },
	{ L"<a-s>",      { K(ALT_S) }         },
	{ L"<a-t>",      { K(ALT_T) }         },
	{ L"<a-u>",      { K(ALT_U) }         },
	{ L"<a-v>",      { K(ALT_V) }         },
	{ L"<a-w>",      { K(ALT_W) }         },
	{ L"<a-x>",      { K(ALT_X) }         },
	{ L"<a-y>",      { K(ALT_Y) }         },
	{ L"<a-z>",      { K(ALT_Z) }         },
	{ L"<m-a>",      { K(ALT_A) }         },
	{ L"<m-b>",      { K(ALT_B) }         },
	{ L"<m-c>",      { K(ALT_C) }         },
	{ L"<m-d>",      { K(ALT_D) }         },
	{ L"<m-e>",      { K(ALT_E) }         },
	{ L"<m-f>",      { K(ALT_F) }         },
	{ L"<m-g>",      { K(ALT_G) }         },
	{ L"<m-h>",      { K(ALT_H) }         },
	{ L"<m-i>",      { K(ALT_I) }         },
	{ L"<m-j>",      { K(ALT_J) }         },
	{ L"<m-k>",      { K(ALT_K) }         },
	{ L"<m-l>",      { K(ALT_L) }         },
	{ L"<m-m>",      { K(ALT_M) }         },
	{ L"<m-n>",      { K(ALT_N) }         },
	{ L"<m-o>",      { K(ALT_O) }         },
	{ L"<m-p>",      { K(ALT_P) }         },
	{ L"<m-q>",      { K(ALT_Q) }         },
	{ L"<m-r>",      { K(ALT_R) }         },
	{ L"<m-s>",      { K(ALT_S) }         },
	{ L"<m-t>",      { K(ALT_T) }         },
	{ L"<m-u>",      { K(ALT_U) }         },
	{ L"<m-v>",      { K(ALT_V) }         },
	{ L"<m-w>",      { K(ALT_W) }         },
	{ L"<m-x>",      { K(ALT_X) }         },
	{ L"<m-y>",      { K(ALT_Y) }         },
	{ L"<m-z>",      { K(ALT_Z) }         },
#endif
	{ L"<del>",      L"\177"              },
#ifdef ENABLE_EXTENDED_KEYS
	{ L"<home>",     { K(KEY_HOME) }      },
	{ L"<end>",      { K(KEY_END) }       },
	{ L"<left>",     { K(KEY_LEFT) }      },
	{ L"<right>",    { K(KEY_RIGHT) }     },
	{ L"<up>",       { K(KEY_UP) }        },
	{ L"<down>",     { K(KEY_DOWN) }      },
	{ L"<bs>",       { K(KEY_BACKSPACE) } },
	{ L"<delete>",   { K(KEY_DC) }        },
	{ L"<insert>",   { K(KEY_IC) }        },
	{ L"<pageup>",   { K(KEY_PPAGE) }     },
	{ L"<pagedown>", { K(KEY_NPAGE) }     },
	{ L"<s-home>",   { K(KEY_SHOME) }     },
	{ L"<s-end>",    { K(KEY_SEND) }      },
	{ L"<s-left>",   { K(KEY_SLEFT) }     },
	{ L"<s-right>",  { K(KEY_SRIGHT) }    },
	{ L"<s-up>",     { K(KEY_SR) }        },
	{ L"<s-down>",   { K(KEY_SF) }        },
	{ L"<s-delete>", { K(KEY_SDC) }       },
	{ L"<s-insert>", { K(KEY_SIC) }       },
	{ L"<s-pageup>", { K(KEY_SPREVIOUS) } },
	{ L"<s-pagedown>", { K(KEY_SNEXT) }   },
	{ L"<f0>",       { K(KEY_F(0)) }      },
	{ L"<f1>",       { K(KEY_F(1)) }      },
	{ L"<f2>",       { K(KEY_F(2)) }      },
	{ L"<f3>",       { K(KEY_F(3)) }      },
	{ L"<f4>",       { K(KEY_F(4)) }      },
	{ L"<f5>",       { K(KEY_F(5)) }      },
	{ L"<f6>",       { K(KEY_F(6)) }      },
	{ L"<f7>",       { K(KEY_F(7)) }      },
	{ L"<f8>",       { K(KEY_F(8)) }      },
	{ L"<f9>",       { K(KEY_F(9)) }      },
	{ L"<f10>",      { K(KEY_F(10)) }     },
	{ L"<f11>",      { K(KEY_F(11)) }     },
	{ L"<f12>",      { K(KEY_F(12)) }     },
	{ L"<f13>",      { K(KEY_F(13)) }     },
	{ L"<f14>",      { K(KEY_F(14)) }     },
	{ L"<f15>",      { K(KEY_F(15)) }     },
	{ L"<f16>",      { K(KEY_F(16)) }     },
	{ L"<f17>",      { K(KEY_F(17)) }     },
	{ L"<f18>",      { K(KEY_F(18)) }     },
	{ L"<f19>",      { K(KEY_F(19)) }     },
	{ L"<f20>",      { K(KEY_F(20)) }     },
	{ L"<f21>",      { K(KEY_F(21)) }     },
	{ L"<f22>",      { K(KEY_F(22)) }     },
	{ L"<f23>",      { K(KEY_F(23)) }     },
	{ L"<f24>",      { K(KEY_F(24)) }     },
	{ L"<f25>",      { K(KEY_F(25)) }     },
	{ L"<f26>",      { K(KEY_F(26)) }     },
	{ L"<f27>",      { K(KEY_F(27)) }     },
	{ L"<f28>",      { K(KEY_F(28)) }     },
	{ L"<f29>",      { K(KEY_F(29)) }     },
	{ L"<f30>",      { K(KEY_F(30)) }     },
	{ L"<f31>",      { K(KEY_F(31)) }     },
	{ L"<f32>",      { K(KEY_F(32)) }     },
	{ L"<f33>",      { K(KEY_F(33)) }     },
	{ L"<f34>",      { K(KEY_F(34)) }     },
	{ L"<f35>",      { K(KEY_F(35)) }     },
	{ L"<f36>",      { K(KEY_F(36)) }     },
	{ L"<f37>",      { K(KEY_F(37)) }     },
	{ L"<f38>",      { K(KEY_F(38)) }     },
	{ L"<f39>",      { K(KEY_F(39)) }     },
	{ L"<f40>",      { K(KEY_F(40)) }     },
	{ L"<f41>",      { K(KEY_F(41)) }     },
	{ L"<f42>",      { K(KEY_F(42)) }     },
	{ L"<f43>",      { K(KEY_F(43)) }     },
	{ L"<f44>",      { K(KEY_F(44)) }     },
	{ L"<f45>",      { K(KEY_F(45)) }     },
	{ L"<f46>",      { K(KEY_F(46)) }     },
	{ L"<f47>",      { K(KEY_F(47)) }     },
	{ L"<f48>",      { K(KEY_F(48)) }     },
	{ L"<f49>",      { K(KEY_F(49)) }     },
	{ L"<f50>",      { K(KEY_F(50)) }     },
	{ L"<f51>",      { K(KEY_F(51)) }     },
	{ L"<f52>",      { K(KEY_F(52)) }     },
	{ L"<f53>",      { K(KEY_F(53)) }     },
	{ L"<f54>",      { K(KEY_F(54)) }     },
	{ L"<f55>",      { K(KEY_F(55)) }     },
	{ L"<f56>",      { K(KEY_F(56)) }     },
	{ L"<f57>",      { K(KEY_F(57)) }     },
	{ L"<f58>",      { K(KEY_F(58)) }     },
	{ L"<f59>",      { K(KEY_F(59)) }     },
	{ L"<f60>",      { K(KEY_F(60)) }     },
	{ L"<f61>",      { K(KEY_F(61)) }     },
	{ L"<f62>",      { K(KEY_F(62)) }     },
	{ L"<f63>",      { K(KEY_F(63)) }     },
	{ L"<s-f1>",     { K(KEY_F(13)) }     },
	{ L"<s-f2>",     { K(KEY_F(14)) }     },
	{ L"<s-f3>",     { K(KEY_F(15)) }     },
	{ L"<s-f4>",     { K(KEY_F(16)) }     },
	{ L"<s-f5>",     { K(KEY_F(17)) }     },
	{ L"<s-f6>",     { K(KEY_F(18)) }     },
	{ L"<s-f7>",     { K(KEY_F(19)) }     },
	{ L"<s-f8>",     { K(KEY_F(20)) }     },
	{ L"<s-f9>",     { K(KEY_F(21)) }     },
	{ L"<s-f10>",    { K(KEY_F(22)) }     },
	{ L"<s-f11>",    { K(KEY_F(23)) }     },
	{ L"<s-f12>",    { K(KEY_F(24)) }     },
	{ L"<c-f1>",     { K(KEY_F(25)) }     },
	{ L"<c-f2>",     { K(KEY_F(26)) }     },
	{ L"<c-f3>",     { K(KEY_F(27)) }     },
	{ L"<c-f4>",     { K(KEY_F(28)) }     },
	{ L"<c-f5>",     { K(KEY_F(29)) }     },
	{ L"<c-f6>",     { K(KEY_F(30)) }     },
	{ L"<c-f7>",     { K(KEY_F(31)) }     },
	{ L"<c-f8>",     { K(KEY_F(32)) }     },
	{ L"<c-f9>",     { K(KEY_F(33)) }     },
	{ L"<c-f10>",    { K(KEY_F(34)) }     },
	{ L"<c-f11>",    { K(KEY_F(35)) }     },
	{ L"<c-f12>",    { K(KEY_F(36)) }     },
	{ L"<a-f1>",     { K(KEY_F(37)) }     },
	{ L"<a-f2>",     { K(KEY_F(38)) }     },
	{ L"<a-f3>",     { K(KEY_F(39)) }     },
	{ L"<a-f4>",     { K(KEY_F(40)) }     },
	{ L"<a-f5>",     { K(KEY_F(41)) }     },
	{ L"<a-f6>",     { K(KEY_F(42)) }     },
	{ L"<a-f7>",     { K(KEY_F(43)) }     },
	{ L"<a-f8>",     { K(KEY_F(44)) }     },
	{ L"<a-f9>",     { K(KEY_F(45)) }     },
	{ L"<a-f10>",    { K(KEY_F(46)) }     },
	{ L"<a-f11>",    { K(KEY_F(47)) }     },
	{ L"<a-f12>",    { K(KEY_F(48)) }     },
	{ L"<m-f1>",     { K(KEY_F(37)) }     },
	{ L"<m-f2>",     { K(KEY_F(38)) }     },
	{ L"<m-f3>",     { K(KEY_F(39)) }     },
	{ L"<m-f4>",     { K(KEY_F(40)) }     },
	{ L"<m-f5>",     { K(KEY_F(41)) }     },
	{ L"<m-f6>",     { K(KEY_F(42)) }     },
	{ L"<m-f7>",     { K(KEY_F(43)) }     },
	{ L"<m-f8>",     { K(KEY_F(44)) }     },
	{ L"<m-f9>",     { K(KEY_F(45)) }     },
	{ L"<m-f10>",    { K(KEY_F(46)) }     },
	{ L"<m-f11>",    { K(KEY_F(47)) }     },
	{ L"<m-f12>",    { K(KEY_F(48)) }     },
#endif
};

void
init_bracket_notation(void)
{
	size_t i;
	for(i = 0; i < ARRAY_LEN(key_pairs); i++)
	{
		key_pairs[i].len = wcslen(key_pairs[i].notation);
	}

	safe_qsort(key_pairs, ARRAY_LEN(key_pairs), sizeof(key_pairs[0]),
			notation_sorter);
}

/* Sorter function to be called by qsort. */
static int
notation_sorter(const void *first, const void *second)
{
	const key_pair_t *paira = (const key_pair_t *)first;
	const key_pair_t *pairb = (const key_pair_t *)second;
	const wchar_t *stra = paira->notation;
	const wchar_t *strb = pairb->notation;
	return wcscmp(stra, strb);
}

wchar_t *
substitute_specs(const char cmd[])
{
	wchar_t *result;

	wchar_t *const cmdw = to_wide(cmd);
	if(cmdw == NULL)
	{
		return NULL;
	}

	result = substitute_specsw(cmdw);
	free(cmdw);

	return result;
}

wchar_t *
substitute_specsw(const wchar_t cmd[])
{
	wchar_t *buf, *p;
	const size_t len = wcslen(cmd) + 1;
	const wchar_t *s = cmd;

	buf = reallocarray(NULL, len, sizeof(wchar_t));
	if(buf == NULL)
	{
		return NULL;
	}

	p = buf;
	while(*s != L'\0')
	{
		const key_pair_t *const pair = find_notation(s);
		if(pair == NULL)
		{
			*p++ = *s++;
		}
		else
		{
			wcscpy(p, pair->key);
			p += wcslen(p);
			s += pair->len;
		}
	}
	*p = L'\0';
	assert((size_t)(p + 1 - buf) <= len && "Destination buffer was too small.");

	return buf;
}

/* Performs binary search in the list of bracket notations.  Returns NULL if
 * str wasn't found in the list. */
static const key_pair_t *
find_notation(const wchar_t str[])
{
	wchar_t str_lowered[wcslen(str) + 1];
	int l = 0, u = ARRAY_LEN(key_pairs) - 1;

	wcscpy(str_lowered, str);
	wcstolower(str_lowered);

	while(l <= u)
	{
		const int i = l + (u - l)/2;
		const key_pair_t *const key_pair = &key_pairs[i];
		const int comp = wcsncmp(str_lowered, key_pair->notation, key_pair->len);
		if(comp == 0)
		{
			return key_pair;
		}
		else if(comp < 0)
		{
			u = i - 1;
		}
		else
		{
			l = i + 1;
		}
	}
	return NULL;
}

char *
wstr_to_spec(const wchar_t str[])
{
	vle_textbuf *const descr = vle_tb_create();

	const size_t str_len = wcslen(str);
	size_t seq_len;
	size_t i;

	for(i = 0U; i < str_len; i += seq_len)
	{
		if(str[i] == L' ' && i != 0U && i != str_len - 1)
		{
			vle_tb_append(descr, " ");
			seq_len = 1U;
		}
		else
		{
			vle_tb_append(descr, wchar_to_spec(&str[i], &seq_len, (i == 0U)));
		}
	}

	return vle_tb_release(descr);
}

/* Converts unicode character(s) starting at c into string form representing
 * corresponding key.  Upon exit *len is set to number of used characters from
 * the string.  Returns pointer to internal buffer. */
static const char *
wchar_to_spec(const wchar_t c[], size_t *len, int bs)
{
	/* TODO: refactor this function wchar_to_spec() */

	static char buf[256];

	*len = 1;
	switch(*c)
	{
		case L' ':             strcpy(buf, "<space>");      break;
		case L'\r':            strcpy(buf, "<cr>");         break;
		case L'\n':            strcpy(buf, "<c-j>");        break;
		case L'\177':          strcpy(buf, "<del>");        break;
		case K(KEY_HOME):      strcpy(buf, "<home>");       break;
		case K(KEY_END):       strcpy(buf, "<end>");        break;
		case K(KEY_UP):        strcpy(buf, "<up>");         break;
		case K(KEY_DOWN):      strcpy(buf, "<down>");       break;
		case K(KEY_LEFT):      strcpy(buf, "<left>");       break;
		case K(KEY_RIGHT):     strcpy(buf, "<right>");      break;
		case K(KEY_DC):        strcpy(buf, "<delete>");     break;
		case K(KEY_BTAB):      strcpy(buf, "<s-tab>");      break;
		case K(KEY_PPAGE):     strcpy(buf, "<pageup>");     break;
		case K(KEY_NPAGE):     strcpy(buf, "<pagedown>");   break;
		case K(KEY_SHOME):     strcpy(buf, "<s-home>");     break;
		case K(KEY_SEND):      strcpy(buf, "<s-end>");      break;
		case K(KEY_SR):        strcpy(buf, "<s-up>");       break;
		case K(KEY_SF):        strcpy(buf, "<s-down>");     break;
		case K(KEY_SLEFT):     strcpy(buf, "<s-left>");     break;
		case K(KEY_SRIGHT):    strcpy(buf, "<s-right>");    break;
		case K(KEY_SDC):       strcpy(buf, "<s-delete>");   break;
		case K(KEY_SIC):       strcpy(buf, "<s-insert>");   break;
		case K(KEY_SPREVIOUS): strcpy(buf, "<s-pageup>");   break;
		case K(KEY_SNEXT):     strcpy(buf, "<s-pagedown>"); break;
		case WC_C_SPACE:       strcpy(buf, "<c-@>");        break;

		case L'\b':
			if(!bs)
			{
				goto def;
			}
			/* Fall through. */
		case K(KEY_BACKSPACE):
			strcpy(buf, "<bs>");
			break;

		case L'\033':
			if(c[1] == L'[' && c[2] == 'Z')
			{
				strcpy(buf, "<s-tab>");
				*len += 2;
				break;
			}
			if(c[1] != L'\0' && c[1] != L'\033')
			{
				if(isupper(c[1]))
				{
					strcpy(buf, "<a-s-a>");
					buf[5] += c[1] - L'A';
				}
				else
				{
					strcpy(buf, "<a-a>");
					buf[3] += c[1] - L'a';
				}
				++*len;
				break;
			}
			strcpy(buf, "<esc>");
			break;

		default:
		def:
			if(*c == '\n' || (*c > L' ' && *c < 256))
			{
				buf[0] = *c;
				buf[1] = '\0';
			}
			else if(*c >= K(KEY_F0) && *c < K(KEY_F0) + 10)
			{
				strcpy(buf, "<f0>");
				buf[2] += *c - K(KEY_F0);
			}
			else if(*c >= K(KEY_F0) + 13 && *c <= K(KEY_F0) + 21)
			{
				strcpy(buf, "<s-f1>");
				buf[4] += *c - (K(KEY_F0) + 13);
			}
			else if(*c >= K(KEY_F0) + 22 && *c <= K(KEY_F0) + 24)
			{
				strcpy(buf, "<s-f10>");
				buf[5] += *c - (K(KEY_F0) + 22);
			}
			else if(*c >= K(KEY_F0) + 25 && *c <= K(KEY_F0) + 33)
			{
				strcpy(buf, "<c-f1>");
				buf[4] += *c - (K(KEY_F0) + 25);
			}
			else if(*c >= K(KEY_F0) + 34 && *c <= K(KEY_F0) + 36)
			{
				strcpy(buf, "<c-f10>");
				buf[5] += *c - (K(KEY_F0) + 34);
			}
			else if(*c >= K(KEY_F0) + 37 && *c <= K(KEY_F0) + 45)
			{
				strcpy(buf, "<a-f1>");
				buf[4] += *c - (K(KEY_F0) + 37);
			}
			else if(*c >= K(KEY_F0) + 46 && *c <= K(KEY_F0) + 48)
			{
				strcpy(buf, "<a-f10>");
				buf[5] += *c - (K(KEY_F0) + 46);
			}
			else if(*c >= K(KEY_F0) + 10 && *c < K(KEY_F0) + 63)
			{
				strcpy(buf, "<f00>");
				buf[2] += (*c - K(KEY_F0))/10;
				buf[3] += (*c - K(KEY_F0))%10;
			}
			else if(iswcntrl(*c))
			{
				strcpy(buf, "<c-A>");
				buf[3] = tolower(buf[3] + *c - 1);
			}
			else
			{
				const wchar_t wchar[] = { *c, L'\0' };
				char *const mb = to_multibyte(wchar);
				copy_str(buf, sizeof(buf), mb);
				free(mb);
			}
			break;
	}
	return buf;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
