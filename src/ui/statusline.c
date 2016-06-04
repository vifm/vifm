/* vifm
 * Copyright (C) 2014 xaizek.
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

#include "statusline.h"
#include "private/statusline.h"

#include <curses.h> /* mvwin() wbkgdset() werase() */

#include <ctype.h> /* isdigit() */
#include <stddef.h> /* NULL size_t */
#include <stdlib.h> /* RAND_MAX rand() */
#include <string.h> /* strcat() strdup() strlen() */
#include <time.h> /* time() */
#include <unistd.h>

#include "../cfg/config.h"
#include "../compat/reallocarray.h"
#include "../engine/mode.h"
#include "../modes/modes.h"
#include "../utils/log.h"
#include "../utils/macros.h"
#include "../utils/string_array.h"
#include "../utils/test_helpers.h"
#include "../utils/utf8.h"
#include "../utils/utils.h"
#include "../background.h"
#include "../filelist.h"
#include "ui.h"

#include "../utils/str.h"

static void update_stat_window_old(FileView *view, int lazy_redraw);
TSTATIC char * expand_status_line_macros(FileView *view, const char format[]);
static char * parse_view_macros(FileView *view, const char **format,
		const char macros[], int opt);
static int expand_num(char buf[], size_t buf_len, int val);
static const char * get_tip(void);
static void check_expanded_str(const char buf[], int skip, int *nexpansions);
static int is_job_bar_visible(void);
static void update_job_bar(void);
static const char * format_job_bar(void);
static char ** take_job_descr_snapshot(void);

/* Number of background jobs. */
static size_t nbar_jobs;
/* Array of jobs. */
static bg_op_t **bar_jobs;
/* Whether list of jobs needs to be redrawn. */
static int job_bar_changed;

void
update_stat_window(FileView *view, int lazy_redraw)
{
	int x;
	char *buf;

	if(!cfg.display_statusline)
	{
		return;
	}

	/* Don't redraw anything until :restart command is finished. */
	if(curr_stats.restart_in_progress)
	{
		return;
	}

	ui_stat_job_bar_check_for_updates();

	if(cfg.status_line[0] == '\0')
	{
		update_stat_window_old(view, lazy_redraw);
		return;
	}

	x = getmaxx(stdscr);
	wresize(stat_win, 1, x);
	wbkgdset(stat_win, COLOR_PAIR(cfg.cs.pair[STATUS_LINE_COLOR]) |
			cfg.cs.color[STATUS_LINE_COLOR].attr);

	buf = expand_status_line_macros(view, cfg.status_line);
	buf = break_in_two(buf, getmaxx(stdscr));

	werase(stat_win);
	checked_wmove(stat_win, 0, 0);
	wprint(stat_win, buf);

	if(lazy_redraw)
	{
		wnoutrefresh(stat_win);
	}
	else
	{
		wrefresh(stat_win);
	}

	free(buf);
}

/* Formats status line in the "old way" (before introduction of 'statusline'
 * option). */
static void
update_stat_window_old(FileView *view, int lazy_redraw)
{
	const dir_entry_t *const entry = &view->dir_entry[view->list_pos];
	char name_buf[160*2 + 1];
	char perm_buf[26];
	char size_buf[56];
	char id_buf[52];
	int x;
	int cur_x;
	size_t print_width;
	char *filename;

	x = getmaxx(stdscr);
	wresize(stat_win, 1, x);
	wbkgdset(stat_win, COLOR_PAIR(cfg.cs.pair[STATUS_LINE_COLOR]) |
			cfg.cs.color[STATUS_LINE_COLOR].attr);

	filename = get_current_file_name(view);
	print_width = utf8_strsnlen(filename, 20 + MAX(0, x - 83));
	snprintf(name_buf, MIN(sizeof(name_buf), print_width + 1), "%s", filename);
	friendly_size_notation(entry->size, sizeof(size_buf), size_buf);

	get_uid_string(entry, 0, sizeof(id_buf), id_buf);
	if(id_buf[0] != '\0')
		strcat(id_buf, ":");
	get_gid_string(entry, 0, sizeof(id_buf) - strlen(id_buf),
			id_buf + strlen(id_buf));
#ifndef _WIN32
	get_perm_string(perm_buf, sizeof(perm_buf), entry->mode);
#else
	snprintf(perm_buf, sizeof(perm_buf), "%s", attr_str_long(entry->attrs));
#endif

	werase(stat_win);
	cur_x = 2;
	checked_wmove(stat_win, 0, cur_x);
	wprint(stat_win, name_buf);
	cur_x += 22;
	if(x > 83)
		cur_x += x - 83;
	mvwaddstr(stat_win, 0, cur_x, size_buf);
	cur_x += 12;
	mvwaddstr(stat_win, 0, cur_x, perm_buf);
	cur_x += 11;

	snprintf(name_buf, sizeof(name_buf), "%d %s filtered", view->filtered,
			(view->filtered == 1) ? "file" : "files");
	if(view->filtered > 0)
		mvwaddstr(stat_win, 0, x - (strlen(name_buf) + 2), name_buf);

	if(cur_x + strlen(id_buf) + 1 > x - (strlen(name_buf) + 2))
		break_at(id_buf, ':');
	if(cur_x + strlen(id_buf) + 1 > x - (strlen(name_buf) + 2))
		id_buf[0] = '\0';
	mvwaddstr(stat_win, 0, cur_x, id_buf);

	if(lazy_redraw)
	{
		wnoutrefresh(stat_win);
	}
	else
	{
		wrefresh(stat_win);
	}
}

/* Expands view macros to be displayed on the status line according to the
 * format string.  Returns newly allocated string, which should be freed by the
 * caller, or NULL if there is not enough memory. */
TSTATIC char *
expand_status_line_macros(FileView *view, const char format[])
{
	return expand_view_macros(view, format, "tAugsEd-lLSz%[]");
}

/* Expands possibly limited set of view macros.  Returns newly allocated string,
 * which should be freed by the caller. */
char *
expand_view_macros(FileView *view, const char format[], const char macros[])
{
	return parse_view_macros(view, &format, macros, 0);
}

/* Expands macros in the *format string advancing the pointer as it goes.  The
 * opt represents conditional expression state, should be zero for non-recursive
 * calls.  Returns newly allocated string, which should be freed by the
 * caller. */
static char *
parse_view_macros(FileView *view, const char **format, const char macros[],
		int opt)
{
	const dir_entry_t *const entry = &view->dir_entry[view->list_pos];
	char *result = strdup("");
	size_t len = 0;
	char c;
	int nexpansions = 0;

	while((c = **format) != '\0')
	{
		size_t width = 0;
		int left_align = 0;
		char buf[PATH_MAX];
		const char *const next = ++*format;
		int skip, ok;

		if(c != '%' || (!char_is_one_of(macros, *next) && !isdigit(*next)))
		{
			if(strappendch(&result, &len, c) != 0)
			{
				break;
			}
			continue;
		}

		if(*next == '-')
		{
			left_align = 1;
			++*format;
		}

		while(isdigit(**format))
		{
			width = width*10 + *(*format)++ - '0';
		}
		c = *(*format)++;

		skip = 0;
		ok = 1;
		switch(c)
		{
			case 't':
				format_entry_name(entry, sizeof(buf), buf);
				break;
			case 'A':
#ifndef _WIN32
				get_perm_string(buf, sizeof(buf), entry->mode);
#else
				copy_str(buf, sizeof(buf), attr_str_long(entry->attrs));
#endif
				break;
			case 'u':
				get_uid_string(entry, 0, sizeof(buf), buf);
				break;
			case 'g':
				get_gid_string(entry, 0, sizeof(buf), buf);
				break;
			case 's':
				friendly_size_notation(entry->size, sizeof(buf), buf);
				break;
			case 'E':
				{
					uint64_t size = 0;
					if(view->selected_files > 0)
					{
						int i;
						for(i = 0; i < view->list_rows; i++)
						{
							if(view->dir_entry[i].selected)
							{
								size += get_file_size_by_entry(view, i);
							}
						}
					}
					/* Make exception for VISUAL_MODE, since it can contain empty
					 * selection when cursor is on ../ directory. */
					else if(!vle_mode_is(VISUAL_MODE))
					{
						size = get_file_size_by_entry(view, view->list_pos);
					}
					friendly_size_notation(size, sizeof(buf), buf);
				}
				break;
			case 'd':
				{
					struct tm *tm_ptr = localtime(&entry->mtime);
					strftime(buf, sizeof(buf), cfg.time_format, tm_ptr);
				}
				break;
			case '-':
				skip = expand_num(buf, sizeof(buf), view->filtered);
				break;
			case 'l':
				skip = expand_num(buf, sizeof(buf), view->list_pos + 1);
				break;
			case 'L':
				skip = expand_num(buf, sizeof(buf), view->list_rows + view->filtered);
				break;
			case 'S':
				skip = expand_num(buf, sizeof(buf), view->list_rows);
				break;
			case '%':
				copy_str(buf, sizeof(buf), "%");
				break;
			case 'z':
				copy_str(buf, sizeof(buf), get_tip());
				break;
			case '[':
				{
					char *const opt_str = parse_view_macros(view, format, macros, 1);
					copy_str(buf, sizeof(buf), opt_str);
					free(opt_str);
					break;
				}
			case ']':
				if(opt)
				{
					if(nexpansions == 0)
					{
						replace_string(&result, "");
					}
					return result;
				}
				else
				{
					LOG_INFO_MSG("Unmatched %%]");
					ok = 0;
				}
				break;

			default:
				LOG_INFO_MSG("Unexpected %%-sequence: %%%c", c);
				ok = 0;
				break;
		}

		if(!ok)
		{
			*format = next;
			if(strappendch(&result, &len, '%') != 0)
			{
				break;
			}
			continue;
		}

		check_expanded_str(buf, skip, &nexpansions);
		stralign(buf, width, ' ', left_align);

		if(strappend(&result, &len, buf) != 0)
		{
			break;
		}
	}

	/* Unmatched %[. */
	if(opt)
	{
		(void)strprepend(&result, &len, "%[");
	}

	return result;
}

/* Prints number into the buffer.  Returns non-zero if numeric value is
 * "empty" (zero). */
static int
expand_num(char buf[], size_t buf_len, int val)
{
	snprintf(buf, buf_len, "%d", val);
	return (val == 0);
}

/* Picks tip to be displayed.  Returns pointer to the tip. */
static const char *
get_tip(void)
{
	enum { SECS_PER_MIN = 60 };

	static time_t last_time;
	static int need_to_shuffle = 1;
	static unsigned int last_item = 0;
	static const char *tips[] = {
	  "Space after :command name is sometimes optional: :chmod+x",
	  "[count]h moves several directories up, e.g. for `cd ../..`: 2h",
	  ":set options without completions are completed with current value",
	  ":help command completes matches by substring match",
	  "'classify' can be used to decorate names with chars/icons",
	  "You can rename files recursively via `:rename!`",
	  "t key toggles selection of current file",
	  ":sync command navigates to the same location on the other panel",
	  "Enter key leaves visual mode preserving selection",
	  "Enable 'runexec' option to run executable files via l/Enter",
	  "Various FUSE file-systems can be used to extend functionality",
	  "Trailing slash in :file[x]type, :filter and = matches directories only",
	  ":edit forwards arguments to editor, e.g.: :edit +set\\ filetype=sh script",
	  "Use :nmap/:vmap/etc. for short information on available mappings",
	};

	time_t now;

	if(need_to_shuffle)
	{
		/* Fisher-Yates ("Knuth") shuffle algorithm implementation that mildly
		 * considers potential biases (not important here, but why not). */
		unsigned int i;
		for(i = 0U; i < ARRAY_LEN(tips) - 1U; ++i)
		{
			const unsigned int j =
				i + (rand()/(RAND_MAX + 1.0))*(ARRAY_LEN(tips) - i);
			const char *const t = tips[i];
			tips[i] = tips[j];
			tips[j] = t;
		}
		need_to_shuffle = 0;
	}

	now = time(NULL);
	if(now > last_time + SECS_PER_MIN)
	{
		last_time = now;
		last_item = (last_item + 1U)%ARRAY_LEN(tips);
	}
	return tips[last_item];
}

/* Examines expansion buffer to check whether expansion took place.  Updates
 * *nexpansions accordingly. */
static void
check_expanded_str(const char buf[], int skip, int *nexpansions)
{
	if(buf[0] != '\0' && !skip)
	{
		++*nexpansions;
	}
}

int
ui_stat_reposition(int statusbar_height, int force_stat_win)
{
	const int stat_line_height = (force_stat_win || cfg.display_statusline)
	                           ? getmaxy(stat_win)
	                           : 0;
	const int job_bar_height = ui_stat_job_bar_height();
	const int y = getmaxy(stdscr)
	            - statusbar_height
	            - stat_line_height
	            - job_bar_height;

	mvwin(job_bar, y, 0);
	if(job_bar_height != 0)
	{
		wresize(job_bar, job_bar_height, getmaxx(job_bar));
	}

	if(force_stat_win || cfg.display_statusline)
	{
		mvwin(stat_win, y + job_bar_height, 0);
		return 1;
	}
	return 0;
}

void
ui_stat_refresh(void)
{
	ui_stat_job_bar_check_for_updates();

	wrefresh(job_bar);
	wrefresh(stat_win);
}

int
ui_stat_job_bar_height(void)
{
	return (nbar_jobs != 0U) ? 1 : 0;
}

void
ui_stat_job_bar_add(bg_op_t *bg_op)
{
	const int prev_height = ui_stat_job_bar_height();

	bg_op_t **p = reallocarray(bar_jobs, nbar_jobs + 1, sizeof(*bar_jobs));
	if(p == NULL)
	{
		return;
	}
	bar_jobs = p;

	bar_jobs[nbar_jobs] = bg_op;
	++nbar_jobs;

	if(!is_job_bar_visible())
	{
		return;
	}

	update_job_bar();

	if(ui_stat_job_bar_height() != prev_height)
	{
		schedule_redraw();
	}
}

void
ui_stat_job_bar_remove(bg_op_t *bg_op)
{
	size_t i;
	const int prev_height = ui_stat_job_bar_height();

	for(i = 0U; i < nbar_jobs; ++i)
	{
		if(bar_jobs[i] == bg_op)
		{
			memmove(&bar_jobs[i], &bar_jobs[i + 1],
					sizeof(*bar_jobs)*(nbar_jobs - 1 - i));
			break;
		}
	}

	--nbar_jobs;

	if(ui_stat_job_bar_height() != 0)
	{
		update_job_bar();
	}
	else if(prev_height != 0)
	{
		schedule_redraw();
	}
}

void
ui_stat_job_bar_changed(bg_op_t *bg_op)
{
	job_bar_changed = 1;
}

void
ui_stat_job_bar_redraw(void)
{
	update_job_bar();
}

void
ui_stat_job_bar_check_for_updates(void)
{
	static int prev_width;

	if(job_bar_changed || getmaxx(job_bar) != prev_width)
	{
		job_bar_changed = 0;
		update_job_bar();
	}

	prev_width = getmaxx(job_bar);
}

/* Checks whether job bar is visible.  Returns non-zero if so, and zero
 * otherwise. */
static int
is_job_bar_visible(void)
{
	return ui_stat_job_bar_height() != 0 && !is_in_menu_like_mode();
}

/* Fills job bar with up-to-date content. */
static void
update_job_bar(void)
{
	if(!is_job_bar_visible())
	{
		return;
	}

	werase(job_bar);
	checked_wmove(job_bar, 0, 0);
	wprint(job_bar, format_job_bar());

	wnoutrefresh(job_bar);
	/* Update status_bar after job_bar just to ensure that it owns the cursor.
	 * Don't know a cleaner way of doing this. */
	wnoutrefresh(status_bar);
	doupdate();
}

/* Formats contents of the job bar.  Returns pointer to statically allocated
 * storage. */
static const char *
format_job_bar(void)
{
	enum { MAX_UTF_CHAR_LEN = 4 };

	static char bar_text[512*MAX_UTF_CHAR_LEN + 1];

	size_t i;
	size_t text_width;
	size_t width_used;
	size_t max_width;
	char **descrs;

	descrs = take_job_descr_snapshot();

	bar_text[0] = '\0';
	text_width = 0U;

	max_width = getmaxx(job_bar);
	width_used = 0U;
	for(i = 0U; i < nbar_jobs; ++i)
	{
		const int progress = bar_jobs[i]->progress;
		const unsigned int reserved = (progress == -1) ? 0U : 5U;
		char item_text[max_width*MAX_UTF_CHAR_LEN + 1U];

		const size_t width = (i == nbar_jobs - 1U)
		                   ? (max_width - width_used)
		                   : (max_width/nbar_jobs);

		const char *ellipsis = left_ellipsis(descrs[i], width - 2U - reserved);

		if(progress == -1)
		{
			snprintf(item_text, sizeof(item_text), "[%s]", ellipsis);
		}
		else
		{
			snprintf(item_text, sizeof(item_text), "[%s %3d%%]", ellipsis, progress);
		}

		(void)sstrappend(bar_text, &text_width, sizeof(bar_text), item_text);

		width_used += width;
	}

	free_string_array(descrs, nbar_jobs);

	return bar_text;
}

/* Makes snapshot of current job descriptions.  Returns array of length
 * nbar_jobs which should be freed via free_string_array(). */
static char **
take_job_descr_snapshot(void)
{
	size_t i;
	char **descrs;

	descrs = reallocarray(NULL, nbar_jobs, sizeof(*descrs));
	for(i = 0U; i < nbar_jobs; ++i)
	{
		const char *descr;

		bg_op_lock(bar_jobs[i]);
		descr = bar_jobs[i]->descr;
		descrs[i] = strdup((descr == NULL) ? "UNKNOWN" : descr);
		bg_op_unlock(bar_jobs[i]);
	}

	return descrs;
}

void
ui_stat_draw_popup_line(WINDOW *win, const char item[], const char descr[],
		size_t max_width)
{
	char *left, *right, *line;
	const size_t text_width = utf8_strsw(item);
	const size_t win_width = getmaxx(win);
	const int align_columns = (max_width <= win_width/4);
	const char *const fmt = align_columns ? "%-*s  %-*s" : "%-*s  %*s";
	size_t width_left;
	size_t item_width;

	if(text_width >= win_width)
	{
		char *const line = strdup(item);
		right_ellipsis(line, win_width);
		wprint(win, line);
		free(line);
		return;
	}

	left = strdup(item);
	right_ellipsis(left, win_width - 3);

	item_width = align_columns ? max_width : utf8_strsw(left);
	width_left = win_width - 2 - MAX(item_width, utf8_strsw(left));

	right = strdup(descr);
	right_ellipsis(right, width_left);

	line = format_str(fmt, (int)item_width, left, (int)width_left, right);
	free(left);
	free(right);

	wprint(win, line);

	free(line);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
