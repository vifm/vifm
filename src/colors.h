/* vifm
 * Copyright (C) 2001 Ken Steen.
 * Copyright (C) 2011 xaizek.
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

#ifndef VIFM__COLORS_H__
#define VIFM__COLORS_H__

/* Color information structure. */
typedef struct
{
	int fg;   /* Foreground color. */
	int bg;   /* Background color. */
	int attr; /* Attributes (bold, inverse, etc.). */
}
col_attr_t;

/* Elements of a color scheme.  Each one is an offset of a color pair inside of
 * color scheme. */
enum
{
	WIN_COLOR,          /* Pane background and default file highlight. */
	DIRECTORY_COLOR,    /* Directory. */
	LINK_COLOR,         /* Symbolic link. */
	BROKEN_LINK_COLOR,  /* Dangling symbolic link. */
	SOCKET_COLOR,       /* Socket. */
	DEVICE_COLOR,       /* Device file. */
	FIFO_COLOR,         /* Named pipe. */
	EXECUTABLE_COLOR,   /* Executable. */
	SELECTED_COLOR,     /* Selected item. */
	CURR_LINE_COLOR,    /* Line under the cursor in the selected pane. */
	TOP_LINE_COLOR,     /* Top line of the other pane. */
	TOP_LINE_SEL_COLOR, /* Top line of the selected pane. */
	STATUS_LINE_COLOR,  /* Status line. */
	WILD_MENU_COLOR,    /* Wild menu. */
	CMD_LINE_COLOR,     /* Command line. */
	ERROR_MSG_COLOR,    /* Error of the command line. */
	BORDER_COLOR,       /* Vertical border lines. */
	OTHER_LINE_COLOR,   /* Line under the cursor in the other pane. */
	JOB_LINE_COLOR,     /* Line that displays status of background jobs. */
	MAXNUM_COLOR        /* Number of elements of a color scheme. */
};

#endif /* VIFM__COLORS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
