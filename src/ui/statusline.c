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

#include <curses.h> /* mvwin() werase() */

#include <assert.h> /* assert() */
#include <ctype.h> /* isdigit() */
#include <stddef.h> /* NULL size_t */
#include <stdlib.h> /* RAND_MAX free() rand() */
#include <string.h> /* strcat() strdup() strlen() */
#include <time.h> /* time() */
#include <unistd.h>

#include "../cfg/config.h"
#include "../compat/pthread.h"
#include "../compat/reallocarray.h"
#include "../engine/mode.h"
#include "../engine/parsing.h"
#include "../engine/var.h"
#include "../lua/vlua.h"
#include "../modes/modes.h"
#include "../utils/fs.h"
#include "../utils/log.h"
#include "../utils/macros.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../utils/test_helpers.h"
#include "../utils/utf8.h"
#include "../utils/utils.h"
#include "../background.h"
#include "../filelist.h"
#include "color_scheme.h"
#include "colored_line.h"
#include "ui.h"

static void split_and_print_status_line(view_t *view, int width);
static void update_stat_window_old(view_t *view, int lazy_redraw);
static void refresh_window(WINDOW *win, int lazily);
TSTATIC cline_t expand_status_line_macros(view_t *view, const char format[]);
static cline_t parse_view_macros(view_t *view, const char **format,
		const char macros[], int opt);
static int expand_num(char buf[], size_t buf_len, int val);
static const char * get_tip(void);
static void check_expanded_str(const char buf[], int skip, int *nexpansions);
static char * fetch_status_line(view_t *view, int width);
TSTATIC char * find_view_macro(const char **format, const char macros[],
		char macro, int opt);
static pthread_spinlock_t * get_job_bar_changed_lock(void);
static void init_job_bar_changed_lock(void);
static int is_job_bar_visible(void);
static const char * format_job_bar(void);
static char ** take_job_descr_snapshot(void);

/* List of macros that are expanded in the status line. */
static const char STATUS_LINE_MACROS[] = "tTfacAugsEdD-xlLoPSz%[]{*";

/* Number of background jobs. */
static size_t nbar_jobs;
/* Array of jobs. */
static bg_op_t **bar_jobs;
/* Whether list of jobs needs to be redrawn. */
static int job_bar_changed;
/* Protects accesses to job_bar_changed variable. */
static pthread_spinlock_t job_bar_changed_lock;

void
ui_stat_update(view_t *view, int lazy_redraw)
{
	if(!cfg.display_statusline || curr_stats.reusing_statusline ||
			view != curr_view)
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

	const int width = getmaxx(stdscr);

	wresize(stat_win, ui_stat_height(), width);
	ui_set_bg(stat_win, &cfg.cs.color[STATUS_LINE_COLOR],
			cfg.cs.pair[STATUS_LINE_COLOR]);
	werase(stat_win);
	checked_wmove(stat_win, 0, 0);

	split_and_print_status_line(view, width);
	refresh_window(stat_win, lazy_redraw);
}

/* Splits status line format by %N macros and prints each on a separate line
 * respecting %=. */
static void
split_and_print_status_line(view_t *view, int width)
{
	char *status_line = fetch_status_line(view, width);
	if(status_line == NULL)
	{
		return;
	}

	const char *part = status_line;
	int line = 0;
	while(*part != '\0')
	{
		const char *current = part;
		char *next = find_view_macro(&part, STATUS_LINE_MACROS, 'N', 0);
		if(next != NULL)
		{
			*next = '\0';
		}

		cline_t result = expand_status_line_macros(view, current);
		assert(strlen(result.attrs) == utf8_strsw(result.line) && "Broken attrs!");
		result.line = break_in_two(result.line, width, "%=");
		result.attrs = break_in_two(result.attrs, width, "=");
		checked_wmove(stat_win, line++, 0);
		cline_print(&result, stat_win, &cfg.cs.color[STATUS_LINE_COLOR]);
		cline_dispose(&result);
	}

	free(status_line);
}

/* Formats status line in the "old way" (before introduction of 'statusline'
 * option). */
static void
update_stat_window_old(view_t *view, int lazy_redraw)
{
	const dir_entry_t *const curr = get_current_entry(view);
	char name_buf[160*2 + 1];
	char perm_buf[26];
	char size_buf[64];
	char id_buf[52];
	int x;
	int cur_x;
	size_t print_width;
	char *filename;

	if(curr == NULL || fentry_is_fake(curr))
	{
		werase(stat_win);
		refresh_window(stat_win, lazy_redraw);
		return;
	}

	x = getmaxx(stdscr);
	wresize(stat_win, 1, x);

	ui_set_bg(stat_win, &cfg.cs.color[STATUS_LINE_COLOR],
			cfg.cs.pair[STATUS_LINE_COLOR]);

	filename = get_current_file_name(view);
	print_width = utf8_strsnlen(filename, 20 + MAX(0, x - 83));
	copy_str(name_buf, MIN(sizeof(name_buf), print_width + 1), filename);
	friendly_size_notation(fentry_get_size(view, curr), sizeof(size_buf),
			size_buf);

	get_uid_string(curr, 0, sizeof(id_buf), id_buf);
	if(id_buf[0] != '\0')
		strcat(id_buf, ":");
	get_gid_string(curr, 0, sizeof(id_buf) - strlen(id_buf),
			id_buf + strlen(id_buf));
#ifndef _WIN32
	get_perm_string(perm_buf, sizeof(perm_buf), curr->mode);
#else
	copy_str(perm_buf, sizeof(perm_buf), attr_str_long(curr->attrs));
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

	refresh_window(stat_win, lazy_redraw);
}

/* Refreshes given window, possibly lazily. */
static void
refresh_window(WINDOW *win, int lazily)
{
	if(lazily)
	{
		wnoutrefresh(stat_win);
	}
	else
	{
		ui_refresh_win(stat_win);
	}
}

/* Expands view macros to be displayed on the status line according to the
 * format string.  Returns colored line. */
TSTATIC cline_t
expand_status_line_macros(view_t *view, const char format[])
{
	const dir_entry_t *const curr = get_current_entry(view);
	if(curr == NULL || fentry_is_fake(curr))
	{
		/* Fake entries don't have valid information. */
		return cline_make();
	}

	return parse_view_macros(view, &format, STATUS_LINE_MACROS, 0);
}

/* Expands possibly limited set of view macros.  Returns newly allocated string,
 * which should be freed by the caller. */
char *
expand_view_macros(view_t *view, const char format[], const char macros[])
{
	cline_t result = parse_view_macros(view, &format, macros, 0);
	free(result.attrs);
	return result.line;
}

/* Expands macros in the *format string advancing the pointer as it goes.  The
 * opt represents conditional expression state, should be zero for non-recursive
 * calls.  Returns colored line. */
static cline_t
parse_view_macros(view_t *view, const char **format, const char macros[],
		int opt)
{
	/* Mind that find_view_macro() needs to be in sync with this function. */

	const dir_entry_t *const curr = get_current_entry(view);
	cline_t result = cline_make();
	char c;
	int nexpansions = 0;
	int has_expander = 0;

	if(curr == NULL)
	{
		return result;
	}

	while((c = **format) != '\0')
	{
		size_t width = 0;
		int left_align = 0;
		char buf[PATH_MAX + 1];
		const char *const next = ++*format;
		int skip, ok;

		if(c != '%' ||
				(!char_is_one_of(macros, *next) && !isdigit(*next) &&
				 (*next != '=' || has_expander)))
		{
			if(strappendch(&result.line, &result.line_len, c) != 0)
			{
				break;
			}
			continue;
		}

		if(*next == '=')
		{
			(void)cline_sync(&result, 0);

			if(strappend(&result.line, &result.line_len, "%=") != 0 ||
					strappendch(&result.attrs, &result.attrs_len, '=') != 0)
			{
				break;
			}
			++*format;
			has_expander = 1;
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

		/* Query drive information at most once. */
		uint64_t free_space;
		uint64_t total_space;
		int have_drive_info = 0;

		skip = 0;
		ok = 1;
		buf[0] = '\0';
		switch(c)
		{
			char path[PATH_MAX + 1];
			char *escaped;

			case 'a':
				have_drive_info = have_drive_info ||
					(get_drive_info(curr_view->curr_dir, &total_space, &free_space) == 0);
				if(have_drive_info)
				{
					friendly_size_notation(free_space, sizeof(buf), buf);
				}
				break;
			case 'c':
				have_drive_info = have_drive_info ||
					(get_drive_info(curr_view->curr_dir, &total_space, &free_space) == 0);
				if(have_drive_info)
				{
					friendly_size_notation(total_space, sizeof(buf), buf);
				}
				break;
			case 't':
			case 'f':
				if(c == 't')
				{
					format_entry_name(curr, NF_FULL, sizeof(path), path);
				}
				else
				{
					get_short_path_of(view, curr, NF_FULL, 0, sizeof(path), path);
				}
				escaped = escape_unreadable(path);
				copy_str(buf, sizeof(buf), escaped);
				free(escaped);
				break;
			case 'T':
				if(curr->type == FT_LINK)
				{
					char full_path[PATH_MAX + 1];
					get_full_path_of(curr, sizeof(full_path), full_path);
					if(get_link_target(full_path, buf, sizeof(buf)) != 0)
					{
						copy_str(buf, sizeof(buf), "Failed to resolve link");
					}
				}
				break;
			case 'A':
#ifndef _WIN32
				get_perm_string(buf, sizeof(buf), curr->mode);
#else
				copy_str(buf, sizeof(buf), attr_str_long(curr->attrs));
#endif
				break;
			case 'o':
#ifndef _WIN32
				snprintf(buf, sizeof(buf), "%03o", curr->mode & 0777);
#endif
				break;
			case 'u':
				get_uid_string(curr, 0, sizeof(buf), buf);
				break;
			case 'g':
				get_gid_string(curr, 0, sizeof(buf), buf);
				break;
			case 's':
				friendly_size_notation(fentry_get_size(view, curr), sizeof(buf), buf);
				break;
			case 'E':
				{
					uint64_t size = 0U;

					typedef int (*iter_f)(view_t *view, dir_entry_t **entry);
					/* No current element for visual mode, since it can contain truly
					 * empty selection when cursor is on ../ directory. */
					iter_f iter = vle_mode_is(VISUAL_MODE) ? &iter_selected_entries
					                                       : &iter_selection_or_current;

					dir_entry_t *entry = NULL;
					while(iter(view, &entry))
					{
						size += fentry_get_size(view, entry);
					}

					friendly_size_notation(size, sizeof(buf), buf);
				}
				break;
			case 'd':
				{
					struct tm *tm_ptr = localtime(&curr->mtime);
					strftime(buf, sizeof(buf), cfg.time_format, tm_ptr);
				}
				break;
			case '-':
			case 'x':
				skip = expand_num(buf, sizeof(buf), view->filtered);
				break;
			case 'l':
				skip = expand_num(buf, sizeof(buf), view->list_pos + 1);
				break;
			case 'L':
				skip = expand_num(buf, sizeof(buf), view->list_rows + view->filtered);
				break;
			case 'P':
				format_position(buf, sizeof(buf), view->top_line, view->list_rows,
						view->window_cells);
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
			case 'D':
				if(curr_stats.number_of_windows == 1)
				{
					view_t *const other = (view == curr_view) ? other_view : curr_view;
					copy_str(buf, sizeof(buf), replace_home_part(other->curr_dir));
				}
				break;
			case '[':
				{
					cline_t opt = parse_view_macros(view, format, macros, 1);
					copy_str(buf, sizeof(buf), opt.line);
					free(opt.line);

					cline_splice_attrs(&result, &opt);
					break;
				}
			case ']':
				if(opt)
				{
					if(nexpansions == 0)
					{
						cline_clear(&result);
					}
					cline_finish(&result);
					return result;
				}

				LOG_INFO_MSG("Unmatched %%]");
				ok = 0;
				break;
			case '{':
				{
					/* Try to find matching closing bracket
					 * TODO: implement the way to escape it, so that the expr may contain
					 * closing brackets */
					const char *e = strchr(*format, '}');
					char *expr = NULL, *resstr = NULL;
					var_t res = var_false();
					ParsingErrors parsing_error;

					/* If there's no matching closing bracket, just add the opening one
					 * literally */
					if(e == NULL)
					{
						ok = 0;
						break;
					}

					/* Create a NULL-terminated copy of the given expr.
					 * TODO: we could temporarily use buf for that, to avoid extra
					 * allocation, but explicitly named variable reads better. */
					expr = calloc(e - (*format) + 1 /* NUL-term */, 1);
					if(expr == NULL)
					{
						ok = 0;
						break;
					}
					memcpy(expr, *format, e - (*format));

					/* Try to parse expr, and convert the res to string if succeed. */
					parsing_error = parse(expr, 0, &res);
					if(parsing_error == PE_NO_ERROR)
					{
						resstr = var_to_str(res);
					}

					if(resstr != NULL)
					{
						copy_str(buf, sizeof(buf), resstr);
					}
					else
					{
						copy_str(buf, sizeof(buf), "<Invalid expr>");
					}

					var_free(res);
					free(resstr);
					free(expr);

					*format = e + 1 /* closing bracket */;
				}
				break;
			case '*':
				if(width > 9)
				{
					snprintf(buf, sizeof(buf), "%%%d*", (int)width);
					width = 0;
					break;
				}
				cline_set_attr(&result, '0' + width);
				width = 0;
				break;

			default:
				LOG_INFO_MSG("Unexpected %%-sequence: %%%c", c);
				ok = 0;
				break;
		}

		if(char_is_one_of("tTAugsEd", c) && fentry_is_fake(curr))
		{
			buf[0] = '\0';
		}

		if(!ok)
		{
			*format = next;
			if(strappendch(&result.line, &result.line_len, '%') != 0)
			{
				break;
			}
			continue;
		}

		check_expanded_str(buf, skip, &nexpansions);
		stralign(buf, width, ' ', left_align);

		if(strappend(&result.line, &result.line_len, buf) != 0)
		{
			break;
		}
	}

	/* Unmatched %[. */
	if(opt)
	{
		(void)strprepend(&result.line, &result.line_len, "%[");
		(void)strprepend(&result.attrs, &result.attrs_len, "  ");
	}

	cline_finish(&result);
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
	  "c key in :file menu puts command into command-line to edit before running",
	  ":copen reopens the last list of files found by a :grep-like command",
	  "Up/down arrows on command-line load history entries with identical prefix",
	  ":copy/:move commands accept absolute paths",
	};

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

	time_t now = time(NULL);
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
ui_stat_reposition(int statusbar_height, int stat_height)
{
	int stat_line_height = stat_height;
	if(stat_line_height == 0 && cfg.display_statusline)
	{
		stat_line_height = getmaxy(stat_win);
	}

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

	if(stat_height > 0 ||
			(cfg.display_statusline && !curr_stats.reusing_statusline))
	{
		mvwin(stat_win, y + job_bar_height, 0);
		return 1;
	}
	return 0;
}

int
ui_stat_height(void)
{
	if(!cfg.display_statusline)
	{
		return 0;
	}

	char *status_line = fetch_status_line(curr_view, getmaxx(stdscr));
	const char *part = status_line;

	int height = 0;
	do
	{
		++height;
	}
	while(find_view_macro(&part, STATUS_LINE_MACROS, 'N', 0) != NULL);

	free(status_line);

	if(curr_stats.reusing_statusline)
	{
		return MIN(getmaxy(stat_win), height);
	}
	return height;
}

/* Retrieves status line format.  Returns newly allocated string. */
static char *
fetch_status_line(view_t *view, int width)
{
	if(vlua_handler_cmd(curr_stats.vlua, cfg.status_line))
	{
		return vlua_make_status_line(curr_stats.vlua, cfg.status_line, view, width);
	}

	return strdup(cfg.status_line);
}

/* strstr() for format line.  Basically parse_view_macros() in dry mode.
 * Returns position of a particular macro or NULL.  *format is updated to keep
 * the state between successive calls. */
TSTATIC char *
find_view_macro(const char **format, const char macros[], char macro, int opt)
{
	/* Mind that parse_view_macros() needs to be in sync with this function. */

	char c;
	while((c = **format) != '\0')
	{
		const char *const next = ++*format;

		if(c != '%' ||
				(!char_is_one_of(macros, *next) && !isdigit(*next) &&
				 *next != '=' && *next != 'N'))
		{
			continue;
		}

		if(*next == macro)
		{
			++*format;
			return (char *)next - 1;
		}

		if(*next == '=' || *next == 'N')
		{
			++*format;
			continue;
		}

		if(*next == '-')
		{
			++*format;
		}

		while(isdigit(**format))
		{
			++*format;
		}
		c = *(*format)++;

		int ok = 1;

		if(c == '[')
		{
			char *ptr = find_view_macro(format, macros, macro, 1);
			if(ptr != NULL)
			{
				return ptr;
			}
		}
		else if(c == ']')
		{
			if(opt)
			{
				return NULL;
			}

			/* Unmatched %]. */
			ok = 0;
		}
		else if(c == '{')
		{
			const char *e = strchr(*format, '}');
			if(e == NULL)
			{
				break;
			}

			*format = e + 1;
		}
		else if(!char_is_one_of(macros, c))
		{
			ok = 0;
		}

		if(!ok)
		{
			*format = next;
		}
	}

	return NULL;
}

void
ui_stat_refresh(void)
{
	ui_stat_job_bar_check_for_updates();

	wnoutrefresh(job_bar);
	wnoutrefresh(stat_win);
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

	ui_stat_job_bar_redraw();

	if(ui_stat_job_bar_height() != prev_height)
	{
		stats_redraw_later();
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
			--nbar_jobs;
			break;
		}
	}

	if(ui_stat_job_bar_height() != 0)
	{
		ui_stat_job_bar_redraw();
	}
	else if(prev_height != 0)
	{
		stats_redraw_later();
	}
}

void
ui_stat_job_bar_changed(bg_op_t *bg_op)
{
	pthread_spinlock_t *const lock = get_job_bar_changed_lock();

	pthread_spin_lock(lock);
	job_bar_changed = 1;
	pthread_spin_unlock(lock);
}

void
ui_stat_job_bar_check_for_updates(void)
{
	static int prev_width;

	pthread_spinlock_t *const lock = get_job_bar_changed_lock();
	int job_bar_changed_value;

	pthread_spin_lock(lock);
	job_bar_changed_value = job_bar_changed;
	job_bar_changed = 0;
	pthread_spin_unlock(lock);

	if(job_bar_changed_value || getmaxx(job_bar) != prev_width)
	{
		ui_stat_job_bar_redraw();
	}

	prev_width = getmaxx(job_bar);
}

/* Gets spinlock for the job_bar_changed variable in thread-safe way.  Returns
 * the lock. */
static pthread_spinlock_t *
get_job_bar_changed_lock(void)
{
	static pthread_once_t once = PTHREAD_ONCE_INIT;
	pthread_once(&once, &init_job_bar_changed_lock);
	return &job_bar_changed_lock;
}

/* Performs spinlock initialization at most once. */
static void
init_job_bar_changed_lock(void)
{
	int ret = pthread_spin_init(&job_bar_changed_lock, PTHREAD_PROCESS_PRIVATE);
	assert(ret == 0 && "Failed to initialize spinlock!");
	(void)ret;
}

/* Checks whether job bar is visible.  Returns non-zero if so, and zero
 * otherwise. */
static int
is_job_bar_visible(void)
{
	/* Pretend that bar isn't visible in tests. */
	return curr_stats.load_stage >= 2
	    && ui_stat_job_bar_height() != 0 && !is_in_menu_like_mode();
}

void
ui_stat_job_bar_redraw(void)
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

	/* The check of stage is for tests. */
	max_width = (curr_stats.load_stage < 2) ? 80 : getmaxx(job_bar);
	width_used = 0U;
	for(i = 0U; i < nbar_jobs; ++i)
	{
		const int progress = bar_jobs[i]->progress;
		const unsigned int reserved = (progress == -1) ? 0U : 5U;
		char item_text[max_width*MAX_UTF_CHAR_LEN + 1U];

		const size_t width = (i == nbar_jobs - 1U)
		                   ? (max_width - width_used)
		                   : (max_width/nbar_jobs);

		char *const ellipsed = left_ellipsis(descrs[i], width - 2U - reserved,
				curr_stats.ellipsis);

		if(progress == -1)
		{
			snprintf(item_text, sizeof(item_text), "[%s]", ellipsed);
		}
		else
		{
			snprintf(item_text, sizeof(item_text), "[%s %3d%%]", ellipsed, progress);
		}

		free(ellipsed);

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
		char *const whole = right_ellipsis(item, win_width, curr_stats.ellipsis);
		wprint(win, whole);
		free(whole);
		return;
	}

	left = right_ellipsis(item, win_width - 3, curr_stats.ellipsis);

	item_width = align_columns ? max_width : utf8_strsw(left);
	width_left = win_width - 2 - MAX(item_width, utf8_strsw(left));

	right = right_ellipsis(descr, width_left, curr_stats.ellipsis);

	line = format_str(fmt, (int)item_width, left, (int)width_left, right);
	free(left);
	free(right);

	wprint(win, line);

	free(line);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
