/* vifm
 * Copyright (C) 2016 xaizek.
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

#ifndef VIFM__MODES__WK_H__
#define VIFM__MODES__WK_H__

/* This file provides convenience definitions of keys.
 *
 * Abbreviations:
 *  - WK  -- Wide Key
 *  - WC  -- Wide Character
 *  - NC  -- Narrow Character
 *  - _C_ -- Control key
 * */

/* Ctrl+<x>. */
#define WK_C_a      L"\x01"
#define WK_C_b      L"\x02"
#define WK_C_c      L"\x03"
#define WK_C_d      L"\x04"
#define WK_C_e      L"\x05"
#define WK_C_f      L"\x06"
#define WK_C_g      L"\x07"
#define WK_C_h      L"\x08"
#define WK_C_i      L"\x09"
#define WK_C_j      L"\x0a"
#define WK_C_k      L"\x0b"
#define WK_C_l      L"\x0c"
#define WK_C_m      L"\x0d"
#define WK_C_n      L"\x0e"
#define WK_C_o      L"\x0f"
#define WK_C_p      L"\x10"
#define WK_C_q      L"\x11"
#define WK_C_r      L"\x12"
#define WK_C_s      L"\x13"
#define WK_C_t      L"\x14"
#define WK_C_u      L"\x15"
#define WK_C_v      L"\x16"
#define WK_C_w      L"\x17"
#define WK_C_x      L"\x18"
#define WK_C_y      L"\x19"
#define WK_C_z      L"\x1a"
#define WK_C_RB     L"\x1d"
#define WK_C_USCORE L"\x1f"

/* Non-letter characters and convenience defines. */
#define WK_ALT     WK_ESC
#define WK_CR      WK_C_m
#define WK_BAR     L"|"
#define WK_CARET   L"^"
#define WK_COLON   L":"
#define WK_COMMA   L","
#define WK_DELETE  L"\x7f"
#define WK_DOLLAR  L"$"
#define WK_DOT     L"."
#define WK_EM      L"!"
#define WK_EQUALS  L"="
#define WK_ESC     L"\x1b"
#define WK_GT      L">"
#define WK_LB      L"["
#define WK_RB      L"]"
#define WK_LCB     L"{"
#define WK_LP      L"("
#define WK_LT      L"<"
#define WK_MINUS   L"-"
#define WK_PERCENT L"%"
#define WK_PLUS    L"+"
#define WK_QM      L"?"
#define WK_QUOTE   L"'"
#define WK_RCB     L"}"
#define WK_RP      L")"
#define WK_SCOLON  L";"
#define WK_SLASH   L"/"
#define WK_SPACE   L" "
#define WK_USCORE  L"_"
#define WK_ZERO    L"0"

/* Upper-case letters. */
#define WK_A L"A"
#define WK_B L"B"
#define WK_C L"C"
#define WK_D L"D"
#define WK_E L"E"
#define WK_F L"F"
#define WK_G L"G"
#define WK_H L"H"
#define WK_I L"I"
#define WK_J L"J"
#define WK_K L"K"
#define WK_L L"L"
#define WK_M L"M"
#define WK_N L"N"
#define WK_O L"O"
#define WK_P L"P"
#define WK_Q L"Q"
#define WK_R L"R"
#define WK_S L"S"
#define WK_T L"T"
#define WK_U L"U"
#define WK_V L"V"
#define WK_W L"W"
#define WK_X L"X"
#define WK_Y L"Y"
#define WK_Z L"Z"

/* Lower-case letters. */
#define WK_a L"a"
#define WK_b L"b"
#define WK_c L"c"
#define WK_d L"d"
#define WK_e L"e"
#define WK_f L"f"
#define WK_g L"g"
#define WK_h L"h"
#define WK_i L"i"
#define WK_j L"j"
#define WK_k L"k"
#define WK_l L"l"
#define WK_m L"m"
#define WK_n L"n"
#define WK_o L"o"
#define WK_p L"p"
#define WK_q L"q"
#define WK_r L"r"
#define WK_s L"s"
#define WK_t L"t"
#define WK_u L"u"
#define WK_v L"v"
#define WK_w L"w"
#define WK_x L"x"
#define WK_y L"y"
#define WK_z L"z"

/* Character form for use in aggregate initializers of wide strings or direct
 * comparisons. */
#define WC_C_m L'\x0d'
#define WC_C_w L'\x17'
#define WC_C_z L'\x1a'
#define WC_CR  WC_C_m
#define WC_z   L'z'
#define NC_C_c '\x03'
#define NC_C_w '\x17'
#define NC_ESC '\x1b'

/* Custom keys. */
#define WC_C_SPACE ((wchar_t)((wint_t)0xe000 + 0))

#endif /* VIFM__MODES__WK_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
