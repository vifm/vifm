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

#ifndef VIFM__CFG__INFO_CHARS_H__
#define VIFM__CFG__INFO_CHARS_H__

/* Comments. */
#define LINE_TYPE_COMMENT '#'

/* :set command option. */
#define LINE_TYPE_OPTION '='

/* Specification of a filetype. */
#define LINE_TYPE_FILETYPE '.'

/* Specification of an xfiletype. */
#define LINE_TYPE_XFILETYPE 'x'

/* Specification of a fileviewer. */
#define LINE_TYPE_FILEVIEWER ','

/* User defined command-line mode command. */
#define LINE_TYPE_COMMAND '!'

/* Mark in file system. */
#define LINE_TYPE_MARK '\''

/* Bookmark in file system. */
#define LINE_TYPE_BOOKMARK 'b'

/* Sign of an active pane. */
#define LINE_TYPE_ACTIVE_VIEW 'a'

/* State of quick view mode. */
#define LINE_TYPE_QUICK_VIEW_STATE 'q'

/* Number of windows (single or dual view mode). */
#define LINE_TYPE_WIN_COUNT 'v'

/* Split orientation (vertical or horizontal). */
#define LINE_TYPE_SPLIT_ORIENTATION 'o'

/* Splitter position (relative to width/height of terminal). */
#define LINE_TYPE_SPLIT_POSITION 'm'

/* Left pane sort. */
#define LINE_TYPE_LWIN_SORT 'l'

/* Right pane sort. */
#define LINE_TYPE_RWIN_SORT 'r'

/* Left pane history. */
#define LINE_TYPE_LWIN_HIST 'd'

/* Right pane history. */
#define LINE_TYPE_RWIN_HIST 'D'

/* Command line history. */
#define LINE_TYPE_CMDLINE_HIST ':'

/* Search history. */
#define LINE_TYPE_SEARCH_HIST '/'

/* Prompt history. */
#define LINE_TYPE_PROMPT_HIST 'p'

/* Local filter history. */
#define LINE_TYPE_FILTER_HIST '|'

/* Directory stack. */
#define LINE_TYPE_DIR_STACK 'S'

/* Trash. */
#define LINE_TYPE_TRASH 't'

/* Registers. */
#define LINE_TYPE_REG '"'

/* Left pane filter. */
#define LINE_TYPE_LWIN_FILT 'f'

/* Right pane filter. */
#define LINE_TYPE_RWIN_FILT 'F'

/* Left pane filter inversion state. */
#define LINE_TYPE_LWIN_FILT_INV 'i'

/* Right pane filter inversion state. */
#define LINE_TYPE_RWIN_FILT_INV 'I'

/* Use screen program. */
#define LINE_TYPE_USE_SCREEN 's'

/* Default color scheme. */
#define LINE_TYPE_COLORSCHEME 'c'

/* Left pane property. */
#define LINE_TYPE_LWIN_SPECIFIC '['

/* Right pane property. */
#define LINE_TYPE_RWIN_SPECIFIC ']'

/* Dot files filter. */
#define PROP_TYPE_DOTFILES '.'

/* Automatically populated filename filter. */
#define PROP_TYPE_AUTO_FILTER 'F'

#endif /* VIFM__CFG__INFO_CHARS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
