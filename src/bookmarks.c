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

#include<string.h>
#include<ctype.h> /* isalnum() */
#include<sys/types.h>

#include "bookmarks.h"
#include "config.h"
#include "filelist.h"
#include "keys.h"
#include "status.h"
#include "ui.h"
#include "utils.h"


/*
 * transform a mark to an index
 * (0=48->0, 9=57->9, A=65->10,...,Z=90->35, a=97 -> 36,..., z=122 -> 61
 */
int
mark2index(const char mark)
{
	if ((int) mark > 96)
		return (int) mark - 61;
	else if ((int) mark < 65)
		return (int) mark - 48;
	else
		return (int) mark - 55;
}


/*
 * transform an index to a mark
 */
char
index2mark(const int x)
{
	if (x > 35)
		return (char) (x + 61);
	else if (x < 10)
		return (char) (x + 48);
	else
		return (char) (x + 55);
}

/*
 * test if a bookmark already exists
 */
int
is_bookmark(const int x)
{

	/* the bookmark is valid if the file and the directory exists.
	 * (i know, checking both is a bit paranoid, one should be enough.) */
	if (bookmarks[x].directory == NULL || bookmarks[x].file == NULL)
		return 0;
	else if (is_dir(bookmarks[x].directory))
		return 1;
	else
		return 0;
}


/*
 * low-level function without safety checks
 */
void
silent_remove_bookmark(const int x)
{
	my_free(bookmarks[x].directory);
	my_free(bookmarks[x].file);
	bookmarks[x].directory = NULL;
	bookmarks[x].file = NULL;
	/* decrease number of active bookmarks */
	cfg.num_bookmarks--;
}


void
remove_bookmark(const int x)
{

	if (is_bookmark(x))
		silent_remove_bookmark(x);
	else
		status_bar_message("Could not find mark");
}


void
add_bookmark(const char mark, const char *directory, const char *file)
{
	int x ;

	if(!isalnum(mark))
	{
		status_bar_message("Invalid mark");
		return;
	}

	x = mark2index(mark);

	/* The mark is already being used.  Free pointers first! */
	if (is_bookmark(x))
		silent_remove_bookmark(x);

	bookmarks[x].directory = strdup(directory);
	bookmarks[x].file = strdup(file);
	/* increase number of active bookmarks */
	cfg.num_bookmarks++;
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
	int file_pos = -1;

	if(strcmp(view->curr_dir, bookmarks[x].directory) == 0)
		file_pos = find_file_pos_in_list(view, bookmarks[x].file);

	return file_pos;

}

int
get_bookmark(FileView *view)
{
	int key;

	wtimeout(curr_view->win, -1);

	key = wgetch(view->win);

	wtimeout(curr_view->win, 1000);

	if (key == ERR)
		return 0;

	switch(key)
	{
		case '\'':
			{
				change_directory(view, view->last_dir);
				load_dir_list(view, 0);
				moveto_list_pos(view, view->list_pos);
				return 0;
			}
			break;
		case 27: /* ascii Escape */
		case 3: /* ascii ctrl c */
			moveto_list_pos(view, view->list_pos);
			return 0;
			break;
		default:
			return move_to_bookmark(view, key);
			break;
	}
	return 0;
}
