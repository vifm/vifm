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

#include "commands.h"
#include "config.h"
#include "filelist.h"
#include "fileops.h"
#include "visual.h"
#include "status.h"
#include<string.h>

/* tbrown  
	move up one position in the window, adding 
  to the selection list */

void
select_up_one(FileView *view, int start_pos) {

    view->list_pos--;
    if(view->list_pos < 0)
		{
			if (start_pos == 0)
				view->selected_files = 0;
			view->list_pos = 0;
		}
		else if(view->list_pos == 0)
		{
			if(start_pos == 0)
			{
        view->dir_entry[view->list_pos +1].selected = 0;
        view->selected_files = 0;
			}
		}
    else if(view->list_pos < start_pos)
    {
        view->dir_entry[view->list_pos].selected = 1;
        view->selected_files++;
    }
    else if(view->list_pos == start_pos)
    {
        view->dir_entry[view->list_pos].selected = 1;
        view->dir_entry[view->list_pos +1].selected = 0;
        view->selected_files = 1;
    }
    else
    {
        view->dir_entry[view->list_pos +1].selected = 0;
        view->selected_files--;
    }
}

/*tbrown  
  move down one position in the window, adding 
  to the selection list */

void 
select_down_one(FileView *view, int start_pos) {

    view->list_pos++;

    if(view->list_pos >= view->list_rows)
            ;
		else if(view->list_pos == 0)
		{
			if (start_pos == 0)
				view->selected_files = 0;
		}
		else if (view->list_pos == 1 && start_pos != 0)
		{
			return;
		}
    else if(view->list_pos > start_pos)
    {
        view->dir_entry[view->list_pos].selected = 1;
        view->selected_files++;
    }
    else if(view->list_pos == start_pos)
    {
        view->dir_entry[view->list_pos].selected = 1;
        view->dir_entry[view->list_pos -1].selected = 0;
        view->selected_files = 1;
    }
    else
    {
        view->dir_entry[view->list_pos -1].selected = 0;
        view->selected_files--;
    }
}


int
visual_key_cb(FileView *view)
{
	int done = 0;
	int abort = 0;
	int count = 0;
	int start_pos = view->list_pos;
	char buf[36];
	int save_msg = 0;

	while(!done)
	{
		int key;
		curr_stats.getting_input = 1;
		key = wgetch(view->win);
		curr_stats.getting_input = 0;

		switch(key)
		{
			case 13: /* ascii Return */
				done = 1;
				break;
			case 27: /* ascii Escape */
			case 3: /* ascii Ctrl C */
				abort = 1;
				break;
			case ':':
				{
					get_command(view, GET_VISUAL_COMMAND, NULL);
					/*abort = 1;*/
					return 0;
				}
				break;
			case 'd':
				delete_file(view);
				done = 1;
				break;
			case KEY_DOWN:
			case 'j':
			{
				select_down_one(view, start_pos);
			}
				break;
			case KEY_UP:
			case 'k':
			{
				select_up_one(view, start_pos);
			}
				break;
			case 'y':
				{
					get_all_selected_files(view);
					yank_selected_files(view);
					count = view->selected_files;
					free_selected_file_array(view);
					save_msg = 1;
				}
				abort = 1;
				break;
			case 'H':
					while (view->list_pos>view->top_line) {
							select_up_one(view,start_pos);
					}
					break;
			/*tbrown move to middle of window, selecting from start position to there
			 */
			case 'M':
					/*window smaller than file list */
					if (view->list_rows < view->window_rows)
					{
						/*in upper portion */
						if (view->list_pos < view->list_rows/2)
						{
							while (view->list_pos < view->list_rows/2)
							{
									select_down_one(view,start_pos);
							}
						}
						 /* lower portion */
						else if (view->list_pos > view->list_rows/2)
						{
								while (view->list_pos > view->list_rows/2)
								{
										select_up_one(view,start_pos);
								}
						}
					}
					 /* window larger than file list */
					else
					{
						/* top half */
						if (view->list_pos < (view->top_line+view->window_rows/2))
						{ 
							while (view->list_pos < (view->top_line+view->window_rows/2))
							{
								select_down_one(view,start_pos);
							}
						}
						/* bottom half */
						else if (view->list_pos > (view->top_line+view->window_rows/2))
						{ 
							while (view->list_pos > (view->top_line+view->window_rows/2))
							{
								select_up_one(view,start_pos);
							}
						}
					}
					break;
			/* tbrown move to last line of window, selecting as we go */
			case 'L':
					while (view->list_pos<(view->top_line + view->window_rows))
					{
						select_down_one(view,start_pos);
					}
					break;
			/* tbrown select all files */
			case 1: /* ascii Ctrl A */ 
					moveto_list_pos(view,1);
					while (view->list_pos < view->list_rows)
					{
						select_down_one(view,view->list_pos);
					}
					break;
			/* tbrown */
			case 2: /* ascii Ctrl B */
			case KEY_PPAGE:
				while (view->list_pos > (view->top_line - view->window_rows -1))
				{
           select_up_one(view,start_pos);
        }
				break;
       /* tbrown */
      case 'G':
				while (view->list_pos < view->list_rows)
				{
           select_down_one(view,start_pos);
        }
        break;
       /* tbrown */
			case 6: /* ascii Ctrl F */
			case KEY_NPAGE:
        while (view->list_pos < (view->top_line+(2*view->window_rows)+1))
				{
          select_down_one(view,start_pos);
        }
				break;
			default:
			break;
		}
		if(abort)
		{
			int x;
			if(view->selected_files)
			{
				for(x = 0; x < view->list_rows; x++)
					view->dir_entry[x].selected = 0;

				view->selected_files = 0;
			}
			done = 1;
		}

		draw_dir_list(view, view->top_line, view->list_pos);
		moveto_list_pos(view, view->list_pos);
		update_pos_window(view);

		if(abort)
			return 0;

		if(count)
		{
			status_bar_message(buf);
			save_msg = 1;
		}
		else
		{
			char status_buf[64] = "";
			snprintf(status_buf, sizeof(status_buf), " %d %s Selected",
					view->selected_files, view->selected_files == 1 ? "File" :
					"Files");
			status_bar_message(status_buf);
		}

		write_stat_win("  --VISUAL--");
	}
	return save_msg;
}

int
start_visual_mode(FileView *view)
{
	int x;

	for(x = 0; x < view->list_rows; x++)
		view->dir_entry[x].selected = 0;

	/* Don't allow the ../ dir to be selected */
	if(strcmp(view->dir_entry[view->list_pos].name, "../"))
	{
		view->selected_files = 1;
		view->dir_entry[view->list_pos].selected = 1;
	}

	draw_dir_list(view, view->top_line, view->list_pos);
	moveto_list_pos(view, view->list_pos);

	if(view->list_pos != 0)
		status_bar_message(" 1 File Selected");
	else
		status_bar_message("");

	write_stat_win("  --VISUAL--");
	return visual_key_cb(view);
}
