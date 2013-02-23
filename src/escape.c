/* vifm
 * Copyright (C) 2013 xaizek.
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

#define _GNU_SOURCE /* I don't know how portable this is but it is
                     * needed in Linux for wide char function wcwidth().
                     */

#include <curses.h>

#include <ctype.h> /* isdigit() */
#include <stddef.h> /* size_t */
#include <stdlib.h> /* strtol() */
#include <string.h> /* memset() strchr() strcpy() strlen() */
#include <wchar.h> /* wcwidth() */

#include "cfg/config.h"
#include "utils/test_helpers.h"
#include "utils/str.h"
#include "utils/utf8.h"
#ifdef _WIN32
/* For wcwidth() stub. */
#include "utils/utils.h"
#endif
#include "color_manager.h"
#include "ui.h"

#include "escape.h"

static size_t get_char_width_esc(const char str[]);
static void print_char_esc(WINDOW *win, const char str[], esc_state *state);
static void esc_state_update(esc_state *state, const char str[]);
static void esc_state_set_attr(esc_state *state, int n);
TSTATIC const char * strchar2str(const char str[], int pos,
		size_t *screen_width);

int
print_line_esc(const char line[], WINDOW *win, int col, int row, int max_width,
		esc_state *state, int *printed)
{
	const char *curr = line;
	size_t pos = 0;
	wmove(win, row, col);
	while(pos <= max_width && *curr != '\0')
	{
		size_t screen_width;
		const char *const char_str = strchar2str(curr, pos, &screen_width);
		if((pos += screen_width) <= max_width)
		{
			print_char_esc(win, char_str, state);
			curr += get_char_width_esc(curr);
		}
	}
	*printed = pos != 0;
	return curr - line;
}

size_t
get_esc_overhead(const char str[])
{
	size_t overhead = 0U;
	while(*str != '\0')
	{
		const size_t char_width_esc = get_char_width_esc(str);
		if(*str == '\033')
		{
			overhead += char_width_esc;
		}
		str += char_width_esc;
	}
	return overhead;
}

/* Returns number of characters at the beginning of the str which form one
 * logical symbol.  Takes UTF-8 encoding and terminal escape sequences into
 * account. */
static size_t
get_char_width_esc(const char str[])
{
	if(*str != '\033')
	{
		return get_char_width(str);
	}
	else
	{
		const char *pos = strchr(str, 'm');
		pos = (pos == NULL) ? (str + strlen(str)) : (pos + 1);
		return pos - str;
	}
}

/* Prints the leading character of the str to the win window parsing terminal
 * escape sequences. */
static void
print_char_esc(WINDOW *win, const char str[], esc_state *state)
{
	if(str[0] == '\033')
	{
		int next_pair;

		esc_state_update(state, str);

		next_pair = colmgr_alloc_pair(state->fg, state->bg);
		wattrset(win, COLOR_PAIR(next_pair) | state->attrs);
	}
	else
	{
		wprint(win, str);
	}
}

/* Handles escape sequence.  Applies whole escape sequence specified by the str
 * to the state. */
static void
esc_state_update(esc_state *state, const char str[])
{
	str++;
	do
	{
		int n = 0;
		if(isdigit(str[1]))
		{
			char *end;
			n = strtol(str + 1, &end, 10);
			str = end;
		}
		else
		{
			str++;
		}

		esc_state_set_attr(state, n);
	}
	while(str[0] == ';');
}

/* Applies one escape sequence attribute (the n parameter) to the state at a
 * time. */
static void
esc_state_set_attr(esc_state *state, int n)
{
	switch(n)
	{
		case 0:
			esc_state_init(state);
			break;
		case 1:
			state->attrs |= A_BOLD;
			break;
		case 2:
			state->attrs |= A_DIM;
			break;
		case 3: case 7:
			state->attrs |= A_REVERSE;
			break;
		case 4:
			state->attrs |= A_UNDERLINE;
			break;
		case 5: case 6:
			state->attrs |= A_BLINK;
			break;
		case 22:
			state->attrs &= ~(A_BOLD | A_UNDERLINE | A_BLINK | A_REVERSE | A_DIM);
			break;
		case 24:
			state->attrs &= ~A_UNDERLINE;
			break;
		case 25:
			state->attrs &= ~A_BLINK;
			break;
		case 27:
			state->attrs &= ~A_REVERSE;
			break;
		case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37:
			state->fg = n - 30;
			break;
		case 39:
			state->fg = -1;
			break;
		case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47:
			state->bg = n - 40;
			break;
		case 49:
			state->bg = -1;
			break;
	}
}

void
esc_state_init(esc_state *state)
{
	state->attrs = 0;
	state->fg = -1;
	state->bg = -1;
}

/* Converts the leading character of the str string to a printable string.  Puts
 * number of screen character positions taken by the resulting string
 * representation of a character into *screen_width.  Returns pointer to a
 * statically allocated buffer. */
TSTATIC const char *
strchar2str(const char str[], int pos, size_t *screen_width)
{
	static char buf[32];

	const size_t char_width = get_char_width(str);
	if(char_width != 1 || str[0] >= ' ')
	{
		memcpy(buf, str, char_width);
		buf[char_width] = '\0';
		*screen_width = wcwidth(get_first_wchar(str));
	}
	else if(str[0] == '\n')
	{
		buf[0] = '\0';
		*screen_width = 0;
	}
	else if(str[0] == '\r')
	{
		strcpy(buf, "<cr>");
		*screen_width = 4;
	}
	else if(str[0] == '\t')
	{
		const size_t space_count = cfg.tab_stop - pos%cfg.tab_stop;
		memset(buf, ' ', space_count);
		buf[space_count] = '\0';
		*screen_width = space_count;
	}
	else if(str[0] == '\033')
	{
		char *dst = buf;
		while(*str != 'm' && *str != '\0' && dst - buf < sizeof(buf) - 2)
		{
			*dst++ = *str++;
		}
		if(*str != 'm')
		{
			buf[0] = '\0';
		}
		else
		{
			*dst++ = 'm';
			*dst = '\0';
		}
		*screen_width = 0;
	}
	else if((unsigned char)str[0] < (unsigned char)' ')
	{
		buf[0] = '^';
		buf[1] = ('A' - 1) + str[0];
		buf[2] = '\0';
		*screen_width = 2;
	}
	else
	{
		buf[0] = str[0];
		buf[1] = '\0';
		*screen_width = 1;
	}
	return buf;
}


/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
