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

#if(defined(BSD) && (BSD>=199103)) 
	#include<sys/types.h> /* required for regex.h on FreeBSD 4.2 */
#endif
#include<sys/types.h>

#include<ncurses.h>
#include<regex.h>

#include"filelist.h"
#include"keys.h"
#include"ui.h"

enum
{
	PREVIOUS,
	NEXT
};

static int
find_next_pattern_match(FileView *view, int start, int direction)
{
	int found = 0;
	int x;

	if(direction == 	PREVIOUS)
	{
		for(x = start -1; x > 0; x--)
		{
			if(view->dir_entry[x].selected)
			{
				found = 1;
				view->list_pos = x;
				break;
			}
		}
	}
	else if(direction == NEXT)
	{
		for(x = start +1; x < view->list_rows; x++)
		{
			if(view->dir_entry[x].selected)
			{
				found = 1;
				view->list_pos = x;
				break;
			}
		}
	}
	return found;
}

void
find_previous_pattern(FileView *view)
{
	if(find_next_pattern_match(view, view->list_pos, PREVIOUS))
		moveto_list_pos(view, view->list_pos);
	else if(find_next_pattern_match(view, view->list_rows, PREVIOUS))
		moveto_list_pos(view, view->list_pos);

}

void
find_next_pattern(FileView *view)
{
	if(find_next_pattern_match(view, view->list_pos, NEXT))
		moveto_list_pos(view, view->list_pos);
	else if(find_next_pattern_match(view, 0, NEXT))
		moveto_list_pos(view, view->list_pos);
}

int
find_pattern(FileView *view, char *pattern)
{
	int found = 0;
	regex_t re;
	int x;
	int first_match = 0;
	int first_match_pos = 0;

	view->selected_files = 0;

	for(x = 0; x < view->list_rows; x++)
	{
		if(regcomp(&re, pattern, REG_EXTENDED) == 0)
		{
			if(regexec(&re, view->dir_entry[x].name, 0, NULL, 0) == 0)
			{
				if(!first_match)
				{
					first_match++;
					first_match_pos = x;
				}
				view->dir_entry[x].selected = 1;
				view->selected_files++;
				found++;
			}
			else
				view->dir_entry[x].selected = 0;
		}
		regfree(&re);
	}


	/* Need to redraw the list so that the matching files are highlighted */
	draw_dir_list(view, view->top_line, view->curr_line);

	if(found)
	{
		draw_dir_list(view, view->top_line, view->curr_line);
		moveto_list_pos(view, first_match_pos);
		return 0;
	}
	else
	{
		char buf[48];
		moveto_list_pos(view, view->list_pos);
		snprintf(buf, sizeof(buf), "No matching files for %s", view->regexp);
		status_bar_message(buf);
		return 1;
	}
}


