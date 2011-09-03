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

#include <sys/types.h>

#include <ctype.h> /* isalnum() */
#include <string.h>

#include "config.h"
#include "filelist.h"
#include "status.h"
#include "ui.h"
#include "utils.h"

#include "bookmarks.h"

const char valid_bookmarks[] = {
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '<', '>',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
	'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
	'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'\0'
};

/*
 * transform a mark to an index
 * (0=48->0, 9=57->9, <=60->10, >=60->11, A=65->12,...,Z=90->37, a=97 -> 38,
 * ..., z=122 -> 63)
 */
int
mark2index(const char mark)
{
	int im;

	im = (int)mark;
	if(im >= '0' && im <= '9')
		return im - 12;
	else if(im == '<')
		return 10;
	else if(im == '>')
		return 11;
	else if(im >= 'A' && im <= 'Z')
		return im - 53;
	else if(im >= 'a' && im <= 'z')
		return im - 59;
	else
		return im - 48;
}

/*
 * transform an index to a mark
 */
char
index2mark(const int x)
{
	char c;

	if(x < 10)
		c = '0' + x;
	else if(x == 10)
		c = '<';
	else if(x == 11)
		c = '>';
	else if(x < 38)
		c = 'A' + (x - 12);
	else
		c = 'a' + (x - 38);
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
 *
 * Returns 1 if bookmark existed
 */
static int
silent_remove_bookmark(const int x)
{
	if(bookmarks[x].directory == NULL && bookmarks[x].file == NULL)
		return 0;

	free(bookmarks[x].directory);
	free(bookmarks[x].file);
	bookmarks[x].directory = NULL;
	bookmarks[x].file = NULL;
	/* decrease number of active bookmarks */
	cfg.num_bookmarks--;
	return 1;
}

int
remove_bookmark(const int x)
{
	if(silent_remove_bookmark(x) == 0)
	{
		status_bar_message("Could not find mark");
		return 1;
	}
	return 0;
}

static void
add_mark(const char mark, const char *directory, const char *file)
{
	int x;

	x = mark2index(mark);

	/* In case the mark is already being used.  Free pointers first! */
	(void)silent_remove_bookmark(x);

	bookmarks[x].directory = strdup(directory);
	bookmarks[x].file = strdup(file);
	/* increase number of active bookmarks */
	cfg.num_bookmarks++;
}

int
add_bookmark(const char mark, const char *directory, const char *file)
{
	if(!isalnum(mark))
	{
		status_bar_message("Invalid mark");
		return 1;
	}

	add_mark(mark, directory, file);
	return 0;
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
		if(change_directory(view, bookmarks[x].directory) >= 0)
		{
			load_dir_list(view, 1);
			file_pos = find_file_pos_in_list(view, bookmarks[x].file);
			if(file_pos != -1)
				moveto_list_pos(view, file_pos);
			else
				moveto_list_pos(view, 0);
		}
	}
	else
	{
		if(!isalnum(mark))
			status_bar_message("Invalid mark");
		else
			status_bar_message("Mark is not set");

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
			if(change_directory(view, view->last_dir) >= 0)
			{
				load_dir_list(view, 0);
				moveto_list_pos(view, view->list_pos);
			}
			return 0;
		case 27: /* ascii Escape */
		case 3: /* ascii ctrl c */
			moveto_list_pos(view, view->list_pos);
			return 0;

		default:
			return move_to_bookmark(view, key);
	}
}

/* Returns number of active bookmarks */
int
init_active_bookmarks(const char *marks)
{
	int i, x;

	i = 0;
	for(x = 0; x < NUM_BOOKMARKS; ++x)
	{
		if(!is_bookmark(x))
			continue;
		if(strchr(marks, index2mark(x)) == NULL)
			continue;
		active_bookmarks[i++] = x;
	}
	return i;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
