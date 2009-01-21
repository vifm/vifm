/* vifm
 * Copyright (C) 2001 Ken Steen.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifndef __BOOKMARKS_H__
#define __BOOKMARKS_H__

#include"ui.h"

#define NUM_BOOKMARKS 62

typedef struct
{
	/*
	 * 'mark' is unnecessary, we already reserve all possible bookmarks,
	 * therfore we can use the mark as an index:
	 *	0:  0		( 0=48, ascii)
	 *	9:  9		( 9=57 )
	 *	A: 10		( A=65 )
	 *	...
	 *	Z: 35
	 *	a: 36		( a=97 )
	 *	...
	 *	z: 61
	char mark;
	*/
	/* use pointers instead of fixed length char arrays -> save space!
	char file[NAME_MAX];
	char directory[PATH_MAX];
	*/
	char *file;
	char *directory;
} bookmarks_t;

bookmarks_t bookmarks[NUM_BOOKMARKS];

/* array of active bookmarks, populated in menu.c: init_active_bookmarks() */
int active_bookmarks[NUM_BOOKMARKS];

char index2mark(const int x);
int is_bookmark(const int x);
void add_bookmark(const char mark, const char *directory, const char *file);
int get_bookmark(FileView *view);
int move_to_bookmark(FileView *view, const char mark);
void remove_bookmark(const int x);
int check_mark_directory(FileView *view, char mark);

#endif
