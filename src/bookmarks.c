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

#include <string.h>
#include <ctype.h> /* isalnum() */
#include <sys/types.h>

#include "bookmarks.h"
#include "config.h"
#include "filelist.h"
#include "status.h"
#include "ui.h"
#include "utils.h"

/*
 * transform a mark to an index
 * (0=48->0, 9=57->9, <=60->10, >=60->11, A=65->12,...,Z=90->37, a=97 -> 38,
 * ..., z=122 -> 63)
 */
static int
mark2index(const char mark)
{
	int im;

	im = (int)mark;
	if(im > 96)
		return im - 59;
	else if(im == 60)
		return 10;
	else if(im == 62)
		return 11;
	else if(im < 65)
		return im - 48;
	else
		return im - 53;
}

/*
 * transform an index to a mark
 */
char
index2mark(const int x)
{
	char c;
	if(x > 35)
		c = x + 59;
	else if(x == 11)
		c = '>';
	else if(x == 10)
		c = '<';
	else if(x < 10)
		c = x + 48;
	else
		c = x + 53;
	return c;
}

/*
 * test if a bookmark already exists
 */
int
is_bookmark(const int x)
{
	/* the bookmark is valid if the file and the directory exists.
	 * (i know, checking both is a bit paranoid, one should be enough.) */
	if(bookmarks[x].directory == NULL || bookmarks[x].file == NULL)
		return 0;
	else if(is_dir(bookmarks[x].directory))
		return 1;
	else
		return 0;
}

int
is_spec_bookmark(const int x)
{
	char mark = index2mark(x);
	return mark == '<' || mark == '>';
}

/*
 * low-level function without safety checks
 */
static void
silent_remove_bookmark(const int x)
{
	curr_stats.setting_change = 1;
	free(bookmarks[x].directory);
	free(bookmarks[x].file);
	bookmarks[x].directory = NULL;
	bookmarks[x].file = NULL;
	/* decrease number of active bookmarks */
	cfg.num_bookmarks--;
}

void
remove_bookmark(const int x)
{
	if(is_bookmark(x))
		silent_remove_bookmark(x);
	else
		status_bar_message("Could not find mark");
}

static void
add_mark(const char mark, const char *directory, const char *file)
{
	int x ;

	x = mark2index(mark);

	/* The mark is already being used.	Free pointers first! */
	if(is_bookmark(x))
		silent_remove_bookmark(x);

	bookmarks[x].directory = strdup(directory);
	bookmarks[x].file = strdup(file);
	/* increase number of active bookmarks */
	cfg.num_bookmarks++;
	curr_stats.setting_change = 1;
}

void
add_bookmark(const char mark, const char *directory, const char *file)
{
	if(!isalnum(mark))
	{
		status_bar_message("Invalid mark");
		return;
	}

	add_mark(mark, directory, file);
}

void
set_specmark(const char mark, const char *directory, const char *file)
{
	if(mark != '<' && mark != '>')
		return;

	add_mark(mark, directory, file);
}

int
move_to_bookmark(FileView *view, char mark)
{
	int x  = mark2index(mark);
	int file_pos = -1;

	if(x != -1 && is_bookmark(x))
	{
		change_directory(view, bookmarks[x].directory);

		load_dir_list(view, 1);
		file_pos = find_file_pos_in_list(view, bookmarks[x].file);
		if(file_pos != -1)
			moveto_list_pos(view, file_pos);
		else
			moveto_list_pos(view, 0);
	}
	else
	{
		if(!isalnum(mark))
			status_bar_message("Invalid mark");
		else
			status_bar_message("Mark is not set.");

		moveto_list_pos(view, view->list_pos);
		return 1;
	}
	return 0;
}

int
check_mark_directory(FileView *view, char mark)
{
	int x = mark2index(mark);

	if(bookmarks[x].directory == NULL)
		return -1;

	if(strcmp(view->curr_dir, bookmarks[x].directory) == 0)
		return find_file_pos_in_list(view, bookmarks[x].file);

	return -1;
}

int
get_bookmark(FileView *view, char key)
{
	switch(key)
	{
		case '\'':
			change_directory(view, view->last_dir);
			load_dir_list(view, 0);
			moveto_list_pos(view, view->list_pos);
			return 0;
		case 27: /* ascii Escape */
		case 3: /* ascii ctrl c */
			moveto_list_pos(view, view->list_pos);
			return 0;

		default:
			return move_to_bookmark(view, key);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
