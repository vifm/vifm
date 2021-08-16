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

#include "escape.h"

#include <regex.h>

#include <curses.h>

#include <assert.h> /* assert() */
#include <ctype.h> /* iscntrl() isdigit() */
#include <stddef.h> /* NULL size_t */
#include <stdlib.h> /* free() malloc() realloc() strtol() */
#include <string.h> /* memcpy() memset() strchr() strcpy() strdup() strlen()
                       strncpy() */

#include "../cfg/config.h"
#include "../compat/reallocarray.h"
#include "../utils/test_helpers.h"
#include "../utils/str.h"
#include "../utils/utf8.h"
#include "../utils/utils.h"
#include "ui.h"

static char * add_pattern_highlights(const char line[], size_t len,
		const char no_esc[], const int offsets[], const regex_t *re);
static size_t correct_offset(const char line[], const int offsets[],
		size_t offset);
static size_t count_substr_chars(const char line[], regmatch_t *match);
static char * add_highlighted_substr(const char sub[], size_t sub_len,
		char out[]);
static char * add_highlighted_sym(const char sym[], size_t sym_width,
		char out[]);
TSTATIC size_t get_char_width_esc(const char str[]);
static void print_char_esc(WINDOW *win, const char str[], esc_state *state);
static void apply_state(WINDOW *win, esc_state *state);
static int fix_direct_color(int color);
TSTATIC void esc_state_update(esc_state *state, const char str[]);
static void esc_state_process_attr(esc_state *state, int n);
static void esc_state_set_attr(esc_state *state, int n);
TSTATIC const char * strchar2str(const char str[], int pos,
		size_t *screen_width);

/* Escape sequence which starts block of highlighted symbols. */
static const char INV_START[] = "\033[7,1m";
/* Escape sequence which ends block of highlighted symbols. */
static const char INV_END[] = "\033[27,22m";
/* Number of extra characters added to highlight one string object. */
static const size_t INV_OVERHEAD = sizeof(INV_START) - 1 + sizeof(INV_END) - 1;

char *
esc_remove(const char str[])
{
	char *no_esc = strdup(str);
	if(no_esc != NULL)
	{
		char *p = no_esc;
		while(*str != '\0')
		{
			const size_t char_width_esc = get_char_width_esc(str);
			if(*str != '\033')
			{
				memcpy(p, str, char_width_esc);
				p += char_width_esc;
			}
			str += char_width_esc;
		}
		*p = '\0';
	}
	return no_esc;
}

size_t
esc_str_overhead(const char str[])
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

char *
esc_highlight_pattern(const char line[], const regex_t *re)
{
	char *processed;
	const size_t len = strlen(line);

	int *const offsets = reallocarray(NULL, len + 1, sizeof(int));

	char *const no_esc = malloc(len + 1);
	char *no_esc_sym = no_esc;
	int no_esc_sym_pos = 0;

	/* Fill no_esc and offsets. */
	const char *src_sym = line;
	while(*src_sym != '\0')
	{
		const size_t char_width_esc = get_char_width_esc(src_sym);
		if(*src_sym != '\033')
		{
			size_t i;
			const int offset = src_sym - line;
			/* Each offset value is filled over whole character width. */
			for(i = 0U; i < char_width_esc; ++i)
			{
				offsets[no_esc_sym_pos + i] = offset;
			}
			no_esc_sym_pos += char_width_esc;

			memcpy(no_esc_sym, src_sym, char_width_esc);
			no_esc_sym += char_width_esc;
		}
		src_sym += char_width_esc;
	}
	offsets[no_esc_sym_pos] = src_sym - line;
	*no_esc_sym = '\0';
	assert(no_esc_sym_pos == no_esc_sym - no_esc);

	processed = add_pattern_highlights(line, len, no_esc, offsets, re);

	free(offsets);
	free(no_esc);

	return (processed == NULL) ? strdup(line) : processed;
}

/* Forms new line with highlights of matcher of the re regular expression using
 * escape sequences that invert colors.  Returns NULL when no match found or
 * memory allocation error occurred. */
static char *
add_pattern_highlights(const char line[], size_t len, const char no_esc[],
		const int offsets[], const regex_t *re)
{
	/* XXX: this might benefit from a rewrite, logic of when escape sequences are
	 *      copied is unclear (sometimes along with first matched character,
	 *      sometimes before the match). */

	regmatch_t match;
	char *next;
	char *processed;
	int no_esc_pos = 0;
	int overhead = 0;
	int offset;

	if(regexec(re, no_esc, 1, &match, 0) != 0)
	{
		return NULL;
	}
	if((processed = malloc(len + 1)) == NULL)
	{
		return NULL;
	}

	/* Before the first match. */
	if(match.rm_so != 0 && no_esc[match.rm_so] == '\0')
	{
		/* This is needed to handle possibility of immediate break from the loop
		 * below. */
		offset = correct_offset(line, offsets, match.rm_so);
	}
	else
	{
		offset = offsets[match.rm_so];
	}
	strncpy(processed, line, offset);
	next = processed + offset;

	/* All matches. */
	do
	{
		int so_offset;
		void *ptr;
		const int empty_match = (match.rm_so == match.rm_eo);

		match.rm_so += no_esc_pos;
		match.rm_eo += no_esc_pos;

		so_offset = offsets[match.rm_so];

		if(empty_match)
		{
			if(no_esc[match.rm_eo] == '\0')
			{
				no_esc_pos = match.rm_eo;
				break;
			}
		}

		/* Between matches. */
		if(no_esc_pos != 0)
		{
			const int corrected = correct_offset(line, offsets, no_esc_pos);
			strncpy(next, line + corrected, so_offset - corrected);
		}

		if(empty_match)
		{
			/* Copy single character after the match to advance forward. */

			/* Position inside the line string. */
			const int esc_pos = (no_esc_pos == 0)
			                  ? (size_t)(next - processed)
			                  : correct_offset(line, offsets, no_esc_pos);
			/* Number of characters to copy from the line string. */
			const int len = (match.rm_so == 0)
			              ? utf8_chrw(no_esc)
			              : correct_offset(line, offsets, match.rm_so + 1) - esc_pos;
			strncpy(next, line + esc_pos, len);
			next += len;
			no_esc_pos += utf8_chrw(&no_esc[no_esc_pos]);
		}
		else
		{
			size_t new_overhead;
			size_t match_len;

			new_overhead = INV_OVERHEAD*count_substr_chars(no_esc, &match);
			len += new_overhead;
			if((ptr = realloc(processed, len + 1)) == NULL)
			{
				free(processed);
				return NULL;
			}
			processed = ptr;

			match_len = correct_offset(line, offsets, match.rm_eo) - so_offset;
			next = processed + so_offset + overhead;
			next = add_highlighted_substr(line + so_offset, match_len, next);

			no_esc_pos = match.rm_eo;
			overhead += new_overhead;
		}
	}
	while(regexec(re, no_esc + no_esc_pos, 1, &match, 0) == 0);

	/* Abort if there were no non-empty matches. */
	if(overhead == 0)
	{
		free(processed);
		return 0;
	}

	/* After the last match. */
	strcpy(next, line +
			(no_esc_pos == 0 ? (size_t)(next - processed) :
			 correct_offset(line, offsets, no_esc_pos)));

	return processed;
}

/* Corrects offset inside the line so that it points to the char after previous
 * character instead of the beginning of the current one. */
static size_t
correct_offset(const char line[], const int offsets[], size_t offset)
{
	assert(offset != 0U && "Offset has to be greater than zero.");
	const int prev_offset = offsets[offset - 1];
	const size_t char_width = utf8_chrw(line + prev_offset);
	return prev_offset + char_width;
}

/* Counts number of multi-byte characters inside the match of a regular
 * expression. */
static size_t
count_substr_chars(const char line[], regmatch_t *match)
{
	const size_t sub_len = match->rm_eo - match->rm_so;
	const char *const sub = line + match->rm_so;
	size_t count = 0;
	size_t i = 0;
	while(i < sub_len)
	{
		const size_t char_width_esc = get_char_width_esc(sub + i);
		i += char_width_esc;
		count++;
	}
	return count;
}

/* Adds all symbols of substring pointed to by the sub parameter of the length
 * sub_len to the out buffer with highlight effect applied.  Returns next
 * position in the out buffer. */
static char *
add_highlighted_substr(const char sub[], size_t sub_len, char out[])
{
	size_t i = 0;
	while(i < sub_len)
	{
		const size_t char_width_esc = get_char_width_esc(sub + i);
		out = add_highlighted_sym(sub + i, char_width_esc, out);
		i += char_width_esc;
	}
	return out;
}

/* Adds one symbol pointed to by the sym parameter of the length sym_width to
 * the out buffer with highlight effect applied.  Returns next position in the
 * out buffer. */
static char *
add_highlighted_sym(const char sym[], size_t sym_width, char out[])
{
	if(sym[0] != '\033')
	{
		memcpy(out, INV_START, sizeof(INV_START) - 1);
		out += sizeof(INV_START) - 1;
	}

	strncpy(out, sym, sym_width);
	out += sym_width;

	if(sym[0] != '\033')
	{
		memcpy(out, INV_END, sizeof(INV_END) - 1);
		out += sizeof(INV_END) - 1;
	}

	return out;
}

int
esc_print_line(const char line[], WINDOW *win, int column, int row,
		int max_width, int dry_run, int truncated, esc_state *state, int *printed)
{
	const char *curr = line;
	size_t pos = 0U;
	checked_wmove(win, row, column);

	/* Attributes are set at the beginning of each line and after state change. */
	apply_state(win, state);

	while(pos <= (size_t)max_width && *curr != '\0')
	{
		size_t screen_width;
		const char *const char_str = strchar2str(curr, pos, &screen_width);
		pos += screen_width;
		if(pos <= (size_t)max_width)
		{
			if(!dry_run || screen_width == 0)
			{
				/* Compute real screen width by how much cursor was moved.  Sometimes
				 * character width differs from what it should be. */
				int old_x = getcurx(win);

				print_char_esc(win, char_str, state);

				int new_x = getcurx(win);
				if(new_x < old_x)
				{
					new_x += getmaxx(win);
				}
				pos += (new_x - old_x) - screen_width;
			}

			if(*curr == '\b')
			{
				if(!dry_run)
				{
					int y, x;
					getyx(win, y, x);
					if(x > 0)
					{
						checked_wmove(win, y, x - 1);
					}
				}

				if(pos > 0)
				{
					pos--;
				}
			}

			curr += get_char_width_esc(curr);
		}
	}
	*printed = pos;

	if(truncated)
	{
		/* Process remaining escape sequences of the line in order to preserve all
		 * elements of highlighting even when lines are not fully drawn. */
		const char *tail = curr - 1;
		while((tail = strchr(tail + 1, '\033')) != NULL)
		{
			size_t screen_width;
			const char *const char_str = strchar2str(tail, 0, &screen_width);
			print_char_esc(win, char_str, state);
		}
	}

	return curr - line;
}

/* Returns number of characters at the beginning of the str which form one
 * logical symbol.  Takes UTF-8 encoding and terminal escape sequences into
 * account. */
TSTATIC size_t
get_char_width_esc(const char str[])
{
	if(*str != '\033')
	{
		return utf8_chrw(str);
	}

	/* Skip prefix. */
	const char *p = str + 1;
	if(*p == '[')
	{
		++p;
	}

	/* And sequences of numbers separated by ";" or ",". */
	while(isdigit(*p))
	{
		do
		{
			++p;
		}
		while(isdigit(*p));

		if(*p == ';' || *p == ',')
		{
			++p;
		}
	}
	return (*p == '\0' ? p - str : p - str + 1);
}

/* Prints the leading character of the str to the win window parsing terminal
 * escape sequences. */
static void
print_char_esc(WINDOW *win, const char str[], esc_state *state)
{
	if(str[0] == '\033')
	{
		esc_state_update(state, str);
		apply_state(win, state);
	}
	else
	{
		wprint(win, str);
	}
}

/* Applies current state to the window setting its attribute. */
static void
apply_state(WINDOW *win, esc_state *state)
{
	/* Mix direct part on top of cterm.  If direct colors aren't used, cterm color
	 * prevails.  Attrs are zero by default and combined. */
	const col_attr_t direct_col = {
		.fg = -1,
		.bg = -1,
		.attr = -1,
		.gui_fg = fix_direct_color(state->is_fg_direct ? state->fg : -1),
		.gui_bg = fix_direct_color(state->is_bg_direct ? state->bg : -1),
		.gui_attr = state->attrs,
		.combine_gui_attrs = 1,
		.gui_set = 1,
	};
	col_attr_t state_col = {
		.fg = (state->is_fg_direct ? -1 : state->fg),
		.bg = (state->is_bg_direct ? -1 : state->bg),
		.attr = state->attrs,
		.combine_attrs = 1,
	};
	cs_mix_colors(&state_col, &direct_col);

	/* Try to approximate 24-bit colors to a palette. */
	if(!state_col.gui_set && state->is_fg_direct)
	{
		state_col.fg = cs_downscale_color(direct_col.gui_fg);
	}
	if(!state_col.gui_set && state->is_bg_direct)
	{
		state_col.bg = cs_downscale_color(direct_col.gui_bg);
	}

	/* Mix state color (mix of cterm and direct) to defaults to produce final
	 * color. */
	col_attr_t final_col = state->defaults;
	cs_mix_colors(&final_col, &state_col);

	ui_set_attr(win, &final_col, -1);
}

/* Corrects color to work around 0x000001 - 0x000007 not being dark blue colors.
 * Returns corrected color. */
static int
fix_direct_color(int color)
{
	if(color >= 1 && color <= 3)
	{
		return 0;
	}
	if(color >= 4 && color <= 7)
	{
		return 8;
	}
	return color;
}

/* Handles escape sequence.  Applies whole escape sequence specified by the str
 * to the state. */
TSTATIC void
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

		esc_state_process_attr(state, n);
	}
	while(str[0] == ';');
}

/* Processes one escape sequence attribute at a time.  Handles both standard and
 * extended (xterm256) escape sequences. */
static void
esc_state_process_attr(esc_state *state, int n)
{
	switch(state->mode)
	{
		case ESM_SHORT:
			switch(n)
			{
				case 38:
					state->mode = ESM_GOT_FG_PREFIX;
					break;
				case 48:
					state->mode = ESM_GOT_BG_PREFIX;
					break;
				default:
					esc_state_set_attr(state, n);
					break;
			}
			break;

		case ESM_GOT_FG_PREFIX:
			if(n == 5)
			{
				state->mode = ESM_WAIT_FG_COLOR;
			}
			else
			{
				state->mode = (n == 2 ? ESM_WAIT_FG_R_COMP : ESM_SHORT);
			}
			break;

		case ESM_WAIT_FG_COLOR:
			if(n < state->max_colors)
			{
				state->is_fg_direct = 0;
				state->fg = n;
			}
			state->mode = ESM_SHORT;
			break;

		case ESM_WAIT_FG_R_COMP:
			state->fg = n;
			state->mode = ESM_WAIT_FG_G_COMP;
			break;
		case ESM_WAIT_FG_G_COMP:
			state->fg = state->fg*256 + n;
			state->mode = ESM_WAIT_FG_B_COMP;
			break;
		case ESM_WAIT_FG_B_COMP:
			state->is_fg_direct = 1;
			state->fg = state->fg*256 + n;
			state->mode = ESM_SHORT;
			break;

		case ESM_GOT_BG_PREFIX:
			if(n == 5)
			{
				state->mode = ESM_WAIT_BG_COLOR;
			}
			else
			{
				state->mode = (n == 2 ? ESM_WAIT_BG_R_COMP : ESM_SHORT);
			}
			break;

		case ESM_WAIT_BG_COLOR:
			if(n < state->max_colors)
			{
				state->is_bg_direct = 0;
				state->bg = n;
			}
			state->mode = ESM_SHORT;
			break;

		case ESM_WAIT_BG_R_COMP:
			state->bg = n;
			state->mode = ESM_WAIT_BG_G_COMP;
			break;
		case ESM_WAIT_BG_G_COMP:
			state->bg = state->bg*256 + n;
			state->mode = ESM_WAIT_BG_B_COMP;
			break;
		case ESM_WAIT_BG_B_COMP:
			state->is_bg_direct = 1;
			state->bg = state->bg*256 + n;
			state->mode = ESM_SHORT;
			break;
	}
}

/* Applies one escape sequence attribute (the n parameter) to the state at a
 * time. */
static void
esc_state_set_attr(esc_state *state, int n)
{
#ifdef HAVE_A_ITALIC_DECL
	const int italic_attr = A_ITALIC;
#else
	/* If A_ITALIC is missing (it's an extension), use A_REVERSE instead. */
	const int italic_attr = A_REVERSE;
#endif

	switch(n)
	{
		case 0:
			state->attrs = 0;
			state->fg = -1;
			state->bg = -1;
			state->is_fg_direct = 0;
			state->is_bg_direct = 0;
			break;
		case 1:
			state->attrs |= A_BOLD;
			break;
		case 2:
			state->attrs |= A_DIM;
			break;
		case 3:
			state->attrs |= italic_attr;
			break;
		case 4:
			state->attrs |= A_UNDERLINE;
			break;
		case 5: case 6:
			state->attrs |= A_BLINK;
			break;
		case 7:
			state->attrs |= A_REVERSE;
			break;
		case 22:
			state->attrs &= ~(A_BOLD | A_UNDERLINE | A_BLINK | A_REVERSE | A_DIM);
			break;
		case 23:
			state->attrs &= ~italic_attr;
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
			state->is_fg_direct = 0;
			state->fg = n - 30;
			break;
		case 39:
			state->is_fg_direct = 0;
			state->fg = -1;
			break;
		case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47:
			state->is_bg_direct = 0;
			state->bg = n - 40;
			break;
		case 49:
			state->is_bg_direct = 0;
			state->bg = -1;
			break;
	}
}

void
esc_state_init(esc_state *state, const col_attr_t *defaults, int max_colors)
{
	state->mode = ESM_SHORT;
	state->attrs = 0;
	state->fg = -1;
	state->bg = -1;
	state->is_fg_direct = 0;
	state->is_bg_direct = 0;
	state->defaults = *defaults;
	state->max_colors = max_colors;
}

/* Converts the leading character of the str string to a printable string.  Puts
 * number of screen character positions taken by the resulting string
 * representation of a character into *screen_width.  Returns pointer to a
 * statically allocated buffer. */
TSTATIC const char *
strchar2str(const char str[], int pos, size_t *screen_width)
{
	static char buf[128];

	const size_t char_width = utf8_chrw(str);
	if(char_width != 1 || (unsigned char)str[0] >= (unsigned char)' ')
	{
		if(char_width == 1 && (size_t)str[0] >= 0x80 && (size_t)str[0] < 0x100)
		{
			const char *name = keyname((unsigned char)str[0]);
			if(name != NULL)
			{
				strcpy(buf, name);
				*screen_width = strlen(name);
				return buf;
			}
		}

		memcpy(buf, str, char_width);
		buf[char_width] = '\0';
		*screen_width = vifm_wcwidth(get_first_wchar(str));
	}
	else if(str[0] == '\n')
	{
		buf[0] = '\0';
		*screen_width = 0;
	}
	else if(str[0] == '\b')
	{
		strcpy(buf, "");
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
		size_t len = get_char_width_esc(str);
		if(len >= sizeof(buf) || (len > 0 && str[len - 1] != 'm'))
		{
			buf[0] = '\0';
		}
		else
		{
			copy_str(buf, len + 1, str);
		}
		*screen_width = 0;
	}
	else if(iscntrl((unsigned char)str[0]))
	{
		buf[0] = '^';
		buf[1] = str[0] ^ 64;
		buf[2] = '\0';
		*screen_width = 2;
	}
	else
	{
		/* XXX: is this code completely unreachable? */
		buf[0] = str[0];
		buf[1] = '\0';
		*screen_width = 1;
	}
	return buf;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
