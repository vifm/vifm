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
#include <stddef.h> /* NULL size_t wchar_t */
#include <stdlib.h> /* free() malloc() qsort() */
#include <string.h> /* strlen() */
#include <wchar.h> /* wcscpy() wcslen() */

#include "utils/macros.h"
#include "utils/str.h"

#ifdef __PDCURSES__
#define BACKSPACE_KEY L'\b'
#else
#define BACKSPACE_KEY KEY_BACKSPACE
#endif

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

/* All notation fields must be written in lower case. */
static key_pair_t key_pairs[] = {
	{ L"<esc>",      L"\x1b"              },
	{ L"<c-a>",      L"\x01"              },
	{ L"<c-b>",      L"\x02"              },
	{ L"<c-c>",      L"\x03"              },
	{ L"<c-d>",      L"\x04"              },
	{ L"<c-e>",      L"\x05"              },
	{ L"<c-f>",      L"\x06"              },
	{ L"<c-g>",      L"\x07"              },
	{ L"<c-h>",      { BACKSPACE_KEY, 0 } },
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
	{ L"<tab>",      L"\t"                },
#ifdef ENABLE_EXTENDED_KEYS
	{ L"<s-tab>",    { KEY_BTAB, 0 }      },
#else
	{ L"<s-tab>",    L"\033"L"[Z"         },
#endif
#ifndef __PDCURSES__
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
#else
	{ L"<a-a>",      { ALT_A, 0 }         },
	{ L"<a-b>",      { ALT_B, 0 }         },
	{ L"<a-c>",      { ALT_C, 0 }         },
	{ L"<a-d>",      { ALT_D, 0 }         },
	{ L"<a-e>",      { ALT_E, 0 }         },
	{ L"<a-f>",      { ALT_F, 0 }         },
	{ L"<a-g>",      { ALT_G, 0 }         },
	{ L"<a-h>",      { ALT_H, 0 }         },
	{ L"<a-i>",      { ALT_I, 0 }         },
	{ L"<a-j>",      { ALT_J, 0 }         },
	{ L"<a-k>",      { ALT_K, 0 }         },
	{ L"<a-l>",      { ALT_L, 0 }         },
	{ L"<a-m>",      { ALT_M, 0 }         },
	{ L"<a-n>",      { ALT_N, 0 }         },
	{ L"<a-o>",      { ALT_O, 0 }         },
	{ L"<a-p>",      { ALT_P, 0 }         },
	{ L"<a-q>",      { ALT_Q, 0 }         },
	{ L"<a-r>",      { ALT_R, 0 }         },
	{ L"<a-s>",      { ALT_S, 0 }         },
	{ L"<a-t>",      { ALT_T, 0 }         },
	{ L"<a-u>",      { ALT_U, 0 }         },
	{ L"<a-v>",      { ALT_V, 0 }         },
	{ L"<a-w>",      { ALT_W, 0 }         },
	{ L"<a-x>",      { ALT_X, 0 }         },
	{ L"<a-y>",      { ALT_Y, 0 }         },
	{ L"<a-z>",      { ALT_Z, 0 }         },
	{ L"<m-a>",      { ALT_A, 0 }         },
	{ L"<m-b>",      { ALT_B, 0 }         },
	{ L"<m-c>",      { ALT_C, 0 }         },
	{ L"<m-d>",      { ALT_D, 0 }         },
	{ L"<m-e>",      { ALT_E, 0 }         },
	{ L"<m-f>",      { ALT_F, 0 }         },
	{ L"<m-g>",      { ALT_G, 0 }         },
	{ L"<m-h>",      { ALT_H, 0 }         },
	{ L"<m-i>",      { ALT_I, 0 }         },
	{ L"<m-j>",      { ALT_J, 0 }         },
	{ L"<m-k>",      { ALT_K, 0 }         },
	{ L"<m-l>",      { ALT_L, 0 }         },
	{ L"<m-m>",      { ALT_M, 0 }         },
	{ L"<m-n>",      { ALT_N, 0 }         },
	{ L"<m-o>",      { ALT_O, 0 }         },
	{ L"<m-p>",      { ALT_P, 0 }         },
	{ L"<m-q>",      { ALT_Q, 0 }         },
	{ L"<m-r>",      { ALT_R, 0 }         },
	{ L"<m-s>",      { ALT_S, 0 }         },
	{ L"<m-t>",      { ALT_T, 0 }         },
	{ L"<m-u>",      { ALT_U, 0 }         },
	{ L"<m-v>",      { ALT_V, 0 }         },
	{ L"<m-w>",      { ALT_W, 0 }         },
	{ L"<m-x>",      { ALT_X, 0 }         },
	{ L"<m-y>",      { ALT_Y, 0 }         },
	{ L"<m-z>",      { ALT_Z, 0 }         },
#endif
	{ L"<del>",      L"\177"              },
#ifdef ENABLE_EXTENDED_KEYS
	{ L"<home>",     { KEY_HOME,      0 } },
	{ L"<end>",      { KEY_END,       0 } },
	{ L"<left>",     { KEY_LEFT,      0 } },
	{ L"<right>",    { KEY_RIGHT,     0 } },
	{ L"<up>",       { KEY_UP,        0 } },
	{ L"<down>",     { KEY_DOWN,      0 } },
	{ L"<bs>",       { BACKSPACE_KEY, 0 } },
	{ L"<delete>",   { KEY_DC,        0 } },
	{ L"<pageup>",   { KEY_PPAGE,     0 } },
	{ L"<pagedown>", { KEY_NPAGE,     0 } },
	{ L"<f0>",       { KEY_F(0),      0 } },
	{ L"<f1>",       { KEY_F(1),      0 } },
	{ L"<f2>",       { KEY_F(2),      0 } },
	{ L"<f3>",       { KEY_F(3),      0 } },
	{ L"<f4>",       { KEY_F(4),      0 } },
	{ L"<f5>",       { KEY_F(5),      0 } },
	{ L"<f6>",       { KEY_F(6),      0 } },
	{ L"<f7>",       { KEY_F(7),      0 } },
	{ L"<f8>",       { KEY_F(8),      0 } },
	{ L"<f9>",       { KEY_F(9),      0 } },
	{ L"<f10>",      { KEY_F(10),     0 } },
	{ L"<f11>",      { KEY_F(11),     0 } },
	{ L"<f12>",      { KEY_F(12),     0 } },
	{ L"<f13>",      { KEY_F(13),     0 } },
	{ L"<f14>",      { KEY_F(14),     0 } },
	{ L"<f15>",      { KEY_F(15),     0 } },
	{ L"<f16>",      { KEY_F(16),     0 } },
	{ L"<f17>",      { KEY_F(17),     0 } },
	{ L"<f18>",      { KEY_F(18),     0 } },
	{ L"<f19>",      { KEY_F(19),     0 } },
	{ L"<f20>",      { KEY_F(20),     0 } },
	{ L"<f21>",      { KEY_F(21),     0 } },
	{ L"<f22>",      { KEY_F(22),     0 } },
	{ L"<f23>",      { KEY_F(23),     0 } },
	{ L"<f24>",      { KEY_F(24),     0 } },
	{ L"<f25>",      { KEY_F(25),     0 } },
	{ L"<f26>",      { KEY_F(26),     0 } },
	{ L"<f27>",      { KEY_F(27),     0 } },
	{ L"<f28>",      { KEY_F(28),     0 } },
	{ L"<f29>",      { KEY_F(29),     0 } },
	{ L"<f30>",      { KEY_F(30),     0 } },
	{ L"<f31>",      { KEY_F(31),     0 } },
	{ L"<f32>",      { KEY_F(32),     0 } },
	{ L"<f33>",      { KEY_F(33),     0 } },
	{ L"<f34>",      { KEY_F(34),     0 } },
	{ L"<f35>",      { KEY_F(35),     0 } },
	{ L"<f36>",      { KEY_F(36),     0 } },
	{ L"<f37>",      { KEY_F(37),     0 } },
	{ L"<f38>",      { KEY_F(38),     0 } },
	{ L"<f39>",      { KEY_F(39),     0 } },
	{ L"<f40>",      { KEY_F(40),     0 } },
	{ L"<f41>",      { KEY_F(41),     0 } },
	{ L"<f42>",      { KEY_F(42),     0 } },
	{ L"<f43>",      { KEY_F(43),     0 } },
	{ L"<f44>",      { KEY_F(44),     0 } },
	{ L"<f45>",      { KEY_F(45),     0 } },
	{ L"<f46>",      { KEY_F(46),     0 } },
	{ L"<f47>",      { KEY_F(47),     0 } },
	{ L"<f48>",      { KEY_F(48),     0 } },
	{ L"<f49>",      { KEY_F(49),     0 } },
	{ L"<f50>",      { KEY_F(50),     0 } },
	{ L"<f51>",      { KEY_F(51),     0 } },
	{ L"<f52>",      { KEY_F(52),     0 } },
	{ L"<f53>",      { KEY_F(53),     0 } },
	{ L"<f54>",      { KEY_F(54),     0 } },
	{ L"<f55>",      { KEY_F(55),     0 } },
	{ L"<f56>",      { KEY_F(56),     0 } },
	{ L"<f57>",      { KEY_F(57),     0 } },
	{ L"<f58>",      { KEY_F(58),     0 } },
	{ L"<f59>",      { KEY_F(59),     0 } },
	{ L"<f60>",      { KEY_F(60),     0 } },
	{ L"<f61>",      { KEY_F(61),     0 } },
	{ L"<f62>",      { KEY_F(62),     0 } },
	{ L"<f63>",      { KEY_F(63),     0 } },
	{ L"<s-f1>",     { KEY_F(13),     0 } },
	{ L"<s-f2>",     { KEY_F(14),     0 } },
	{ L"<s-f3>",     { KEY_F(15),     0 } },
	{ L"<s-f4>",     { KEY_F(16),     0 } },
	{ L"<s-f5>",     { KEY_F(17),     0 } },
	{ L"<s-f6>",     { KEY_F(18),     0 } },
	{ L"<s-f7>",     { KEY_F(19),     0 } },
	{ L"<s-f8>",     { KEY_F(20),     0 } },
	{ L"<s-f9>",     { KEY_F(21),     0 } },
	{ L"<s-f10>",    { KEY_F(22),     0 } },
	{ L"<s-f11>",    { KEY_F(23),     0 } },
	{ L"<s-f12>",    { KEY_F(24),     0 } },
	{ L"<c-f1>",     { KEY_F(25),     0 } },
	{ L"<c-f2>",     { KEY_F(26),     0 } },
	{ L"<c-f3>",     { KEY_F(27),     0 } },
	{ L"<c-f4>",     { KEY_F(28),     0 } },
	{ L"<c-f5>",     { KEY_F(29),     0 } },
	{ L"<c-f6>",     { KEY_F(30),     0 } },
	{ L"<c-f7>",     { KEY_F(31),     0 } },
	{ L"<c-f8>",     { KEY_F(32),     0 } },
	{ L"<c-f9>",     { KEY_F(33),     0 } },
	{ L"<c-f10>",    { KEY_F(34),     0 } },
	{ L"<c-f11>",    { KEY_F(35),     0 } },
	{ L"<c-f12>",    { KEY_F(36),     0 } },
	{ L"<a-f1>",     { KEY_F(37),     0 } },
	{ L"<a-f2>",     { KEY_F(38),     0 } },
	{ L"<a-f3>",     { KEY_F(39),     0 } },
	{ L"<a-f4>",     { KEY_F(40),     0 } },
	{ L"<a-f5>",     { KEY_F(41),     0 } },
	{ L"<a-f6>",     { KEY_F(42),     0 } },
	{ L"<a-f7>",     { KEY_F(43),     0 } },
	{ L"<a-f8>",     { KEY_F(44),     0 } },
	{ L"<a-f9>",     { KEY_F(45),     0 } },
	{ L"<a-f10>",    { KEY_F(46),     0 } },
	{ L"<a-f11>",    { KEY_F(47),     0 } },
	{ L"<a-f12>",    { KEY_F(48),     0 } },
	{ L"<m-f1>",     { KEY_F(37),     0 } },
	{ L"<m-f2>",     { KEY_F(38),     0 } },
	{ L"<m-f3>",     { KEY_F(39),     0 } },
	{ L"<m-f4>",     { KEY_F(40),     0 } },
	{ L"<m-f5>",     { KEY_F(41),     0 } },
	{ L"<m-f6>",     { KEY_F(42),     0 } },
	{ L"<m-f7>",     { KEY_F(43),     0 } },
	{ L"<m-f8>",     { KEY_F(44),     0 } },
	{ L"<m-f9>",     { KEY_F(45),     0 } },
	{ L"<m-f10>",    { KEY_F(46),     0 } },
	{ L"<m-f11>",    { KEY_F(47),     0 } },
	{ L"<m-f12>",    { KEY_F(48),     0 } },
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

	qsort(key_pairs, ARRAY_LEN(key_pairs), sizeof(key_pairs[0]), notation_sorter);
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
	wchar_t *buf, *p;
	const size_t len = strlen(cmd) + 1;
	wchar_t *const cmdw = to_wide(cmd);
	wchar_t *s = cmdw;

	buf = malloc(len*sizeof(wchar_t));
	if(cmdw == NULL || buf == NULL)
	{
		free(cmdw);
		free(buf);
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
	assert(p + 1 - buf <= len && "Destination buffer was too small.");

	free(cmdw);
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
		const int i = (l + u)/2;
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
