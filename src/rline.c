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
#define _GNU_SOURCE /* I don't know how portable this is but it is
					   needed in Linux for the ncurses wide char
					   functions
					   */
#include<ctype.h>
#include<sys/types.h> /* Needed for Fink with Mac OS X */
#include<dirent.h> /* DIR */
#include<wchar.h>
#include<signal.h>
#include<string.h>
#include<wctype.h>
#include<stdio.h>

#include<wchar.h>

#include<ncurses.h>

#include"commands.h"
#include"config.h"
#include"keys.h"
#include"menus.h"
#include"status.h"
#include"utils.h"

struct line_stats
{
	int type;              /* type */
	wchar_t *line;         /* the line reading */
	int index;             /* index of the current character */
	int curs_pos;          /* position of the cursor */
	int len;               /* length of the string */
	int cmd_pos;           /* position in the history */
	wchar_t prompt[8];     /* prompt */
	int prompt_wid;        /* width of prompt */
	int complete_continue; /* If non-zero, continue the previous completion */
};

static char *
check_for_executable(char *string)
{
	char *temp = (char *)NULL;

	if (!string)
		return NULL;

	if (string[0] == '!')
	{
		if (strlen(string) > 2)
		{
			if (string[1] == '!')
				temp = strdup(string + 2);
			else
				temp = strdup(string + 1);
		}
		else if (strlen(string) > 1)
			temp = strdup(string + 1);
	}
	return temp;
}

static char *
get_last_word(char * string)
{

	char * temp = (char *)NULL;

	if (!string)
		return NULL;

	/*:command filename */
	temp = strrchr(string, ' ');

	if(temp)
	{
		temp++;
		return temp;
	}
 /* :!filename or :!!filename */
	temp = check_for_executable(string);
	if (temp)
		return temp;

	return NULL;
}

/* On the first call to this function,
 * the string to be parsed should be specified in str.
 * In each subsequent call that should parse the same string, str should be NULL
 */
static char *
filename_completion(char *str)
{
	static char *string;
	static int offset;

	DIR *dir;
	struct dirent *d;
	char * dirname = NULL;
	char * filename = NULL;
	char * temp = NULL;
	int i = 0;
	int found = 0;
	int filename_len = 0;

	if (str != NULL)
	{
		string = str;
		offset = 0;
	}
	else
	{
		offset++;
	}

	if (!strncmp(string, "~/", 2))
	{
		char * homedir = getenv("HOME");

		dirname = (char *)malloc((strlen(homedir) + strlen(string) + 1));

		if (dirname == NULL)
			return NULL;

		snprintf(dirname, strlen(homedir) + strlen(string) + 1, "%s/%s",
				homedir, string + 2);

		filename = strdup(dirname);
	}
	else
	{
		dirname = strdup(string);
		filename = strdup(string);
	}

	temp = strrchr(dirname, '/');

	if (temp)
	{
		strcpy(filename, ++temp);
		*temp = '\0';
	}
	else
	{
		dirname[0] = '.';
		dirname[1] = '\0';
	}


	dir = opendir(dirname);
	filename_len = strlen(filename);

	if(dir == NULL)
	{
		free(filename);
		free(dirname);
		return NULL;
	}

	while ((d=readdir(dir)))
	{
		if ( ! strncmp(d->d_name, filename, filename_len)
			 && strcmp(d->d_name, ".") && strcmp(d->d_name, ".."))
		{
			i = 0;
			while (i < offset)
			{
				if (!(d=readdir(dir)))
				{
					offset = -1;

					closedir(dir);
					free(dirname);

					return strdup(filename);
				}
				if (	!strncmp(d->d_name, filename, filename_len)
					 && strcmp(d->d_name, ".") && strcmp(d->d_name, ".."))
				{
					i++;
				}
			}
			found = 1;
			break;
		}
	}

	closedir(dir);

	if (!found)
	{
		return NULL;
	}

	int isdir = 0;
	if (is_dir(d->d_name))
	{
		isdir = 1;
	}
	else if (strcmp(dirname, "."))
	{
		char * tempfile = (char *)NULL;
		int len = strlen(dirname) + strlen(d->d_name) + 1;
		tempfile = (char *)malloc((len) * sizeof(char));
		if (!tempfile)
			return NULL;
		snprintf(tempfile, len, "%s%s", dirname, d->d_name);
		if (is_dir(tempfile))
			isdir = 1;
		else
			temp = strdup(d->d_name);

		free(tempfile);
	}
	else
		temp = strdup(d->d_name);

	if (isdir)
	{
		char * tempfile = (char *)NULL;
		tempfile = (char *) malloc((strlen(d->d_name) + 2) * sizeof(char));
		if (!tempfile)
			return NULL;
		snprintf(tempfile, strlen(d->d_name) + 2, "%s/", d->d_name);
		temp = strdup(tempfile);

		free(tempfile);
	}

	free(filename);
	free(dirname);
	return temp;
}

/* Insert a string into another string
 * For example, wcsins(L"foor", L"ba", 4) returns L"foobar"
 * If pos is larger then wcslen(src), the character will
 * be added at the end of the src */
static wchar_t *
wcsins(wchar_t *src, wchar_t *ins, int pos)
{
	int i;
	wchar_t *p;

	pos--;

	for (p = src + pos; *p; p++)
		;

	for (i = 0; ins[i]; i++)
		;

	for (; p >= src + pos; p--)
		*(p + i) = *p;
	p++;

	for (; *ins; ins++, p++)
		*p = *ins;

	return src;
}


/* Delete a character in a string
 * For example, wcsdelch(L"fooXXbar", 4, 2) returns L"foobar" */
static wchar_t *
wcsdel(wchar_t *src, int pos, int len)
{
	int p;

	pos--;

	for (p = pos; pos - p < len; pos++)
		if (! src[pos])
		{
			src[p] = L'\0';
			return src;
		}

	pos--;

	while(src[++pos])
		src[pos-len] = src[pos];
	src[pos-len] = src[pos];

	return src;
}


static int
line_completion(struct line_stats *stat)
{
	static char *line_mb = (char *)NULL;
	char *last_word = (char *)NULL;
	char *raw_name = (char *)NULL;
	char *filename = (char *)NULL;
	void *p;
	wchar_t t;
	int i;

	if (stat->line[stat->index] != L' ' && stat->index != stat->len)
	{
		stat->complete_continue = 0;
		return -1;
	}

	if (stat->complete_continue == 0)
	{
		/* only complete the part before the curser
		 * so just copy that part to line_mb */
		t = stat->line[stat->index];
		stat->line[stat->index] = L'\0';

		i = wcstombs(NULL, stat->line, 0) + 1;

		if (! (p = realloc(line_mb, i * sizeof(char))))
		{
			free(line_mb);
			line_mb = NULL;
			return -1;
		}

		line_mb = (char *) p;
		wcstombs(line_mb, stat->line, i);

		stat->line[stat->index] = t;
	}

	last_word = get_last_word(line_mb);

	if (last_word)
	{
		raw_name = filename_completion(
				stat->complete_continue ? NULL : last_word);

		stat->complete_continue = 1;

		if (raw_name)
			filename = escape_filename(raw_name, strlen(raw_name), 1);
	}
	 /* :partial_command */
	else
	{
		char *complete_command = command_completion(
				stat->complete_continue ? NULL : line_mb);
		wchar_t *q;

		if (complete_command)
		{
			if (!stat->line)
			{
				free(raw_name);
				free(filename);
				free(complete_command);
				return 0;
			}

			if ((q = wcschr(stat->line, L' ')) != NULL)
			{	/* If the cursor is not at the end of the string... */
				wchar_t *buf;

				i = mbstowcs(NULL, complete_command, 0) + 1;
				if (! (buf = (wchar_t *) malloc((i + 1) * sizeof(wchar_t))))
				{
					free(raw_name);
					free(filename);
					free(complete_command);
					return -1;
				}
				mbstowcs(buf, complete_command, i);

				i += wcslen(q);

				wcsdel(stat->line, 1, q - stat->line);

				if (! (p = realloc(stat->line, i * sizeof(wchar_t))))
				{
					free(raw_name);
					free(filename);
					free(complete_command);
					return -1;
				}
				stat->line = (wchar_t *) p;

				wcsins(stat->line, buf, 1);

				stat->index = wcschr(stat->line, L' ') - stat->line;
				stat->curs_pos = wcswidth(stat->line, stat->index)
								  + stat->prompt_wid;
				stat->len = wcslen(stat->line);

				free(buf);
			}
			else
			{
				i = mbstowcs(NULL, complete_command, 0) + 1;
				if (! (p = realloc(stat->line, i * sizeof(wchar_t))))
				{
					free(raw_name);
					free(filename);
					free(complete_command);
					return -1;
				}
				stat->line = (wchar_t *) p;

				mbstowcs(stat->line, complete_command, i);
				stat->index = stat->len = i - 1;
				stat->curs_pos = wcswidth(stat->line, stat->len)
								  + stat->prompt_wid;
			}

			werase(status_bar);
			mvwaddwstr(status_bar, 0, 0, stat->prompt);
			mvwaddwstr(status_bar, 0, stat->prompt_wid, stat->line);
			wmove(status_bar, 0, stat->curs_pos);
		}

		free(raw_name);
		free(filename);
		free(complete_command);

		stat->complete_continue = 1;
		return 0;
	}

	if (filename)
	{
		char *cur_file_pos = strrchr(line_mb, ' ');
		char *temp = (char *) NULL;

		if (cur_file_pos)
		{
			/* :command /some/directory/partial_filename anything_else... */
			if ((temp = strrchr(cur_file_pos, '/')))
			{
				char x;
				wchar_t *temp2;

				temp++;
				x = *temp;
				*temp = '\0';

				/* I'm really worry about the portability... */
//				temp2 = (wchar_t *) wcsdup(stat->line + stat->index)
				temp2 = (wchar_t *) malloc((wcslen(stat->line
						 + stat->index) + 1) * sizeof(wchar_t));
				if (!temp2)
				{
					free(raw_name);
					free(filename);
					free(temp2);
					return -1;
				}
				wcscpy(temp2, stat->line + stat->index);

				i = mbstowcs(NULL, line_mb, 0) + mbstowcs(NULL, filename, 0)
					 + (stat->len - stat->index) + 1;

				if (! (p = realloc(stat->line, i * sizeof(wchar_t))))
				{
					*temp = x;
					free(raw_name);
					free(filename);
					free(temp2);
					return -1;
				}
				stat->line = (wchar_t *) p;

				swprintf(stat->line, i, L"%s%s%ls", line_mb,filename,temp2);

				stat->index = i - (stat->len - stat->index) - 1;
				stat->curs_pos = stat->prompt_wid
								  + wcswidth(stat->line, stat->index);
				stat->len = i - 1;

				*temp = x;
				free(raw_name);
				free(filename);
				free(temp2);
			}
			/* :command partial_filename anything_else... */
			else
			{
				char x;
				wchar_t *temp2;

				temp = cur_file_pos + 1;
				x = *temp;
				*temp = '\0';

				temp2 = (wchar_t *) malloc((wcslen(stat->line
						 + stat->index) + 1) * sizeof(wchar_t));
				if (!temp2)
				{
					free(raw_name);
					free(filename);
					free(temp2);
					return -1;
				}
				wcscpy(temp2, stat->line + stat->index);

				i = mbstowcs(NULL, line_mb, 0) + mbstowcs(NULL, filename, 0)
					 + (stat->len - stat->index) + 1;

				if (! (p = realloc(stat->line, i * sizeof(wchar_t))))
				{
					*temp = x;
					free(raw_name);
					free(filename);
					free(temp2);
					return -1;
				}
				stat->line = (wchar_t *) p;

				swprintf(stat->line, i, L"%s%s%ls", line_mb,filename,temp2);

				stat->index = i - (stat->len - stat->index) - 1;
				stat->curs_pos = stat->prompt_wid
								  + wcswidth(stat->line, stat->index);
				stat->len = i - 1;

				*temp = x;
				free(raw_name);
				free(filename);
				free(temp2);
			}
		}
		/* :!partial_filename anthing_else...		 or
		 * :!!partial_filename anthing_else... */
		else if ((temp = strrchr(line_mb, '!')))
		{
			char x;
			wchar_t *temp2;

			temp++;
			x = *temp;
			*temp = '\0';

			temp2 = (wchar_t *) malloc((wcslen(stat->line
					 + stat->index) + 1) * sizeof(wchar_t));
			if (!temp2)
			{
				free(raw_name);
				free(filename);
				return -1;
			}
			wcscpy(temp2, stat->line + stat->index);

			i = mbstowcs(NULL, line_mb, 0) + mbstowcs(NULL, filename, 0)
				 + (stat->len - stat->index) + 1;

			if (! (p = realloc(stat->line, i * sizeof(wchar_t))))
			{
				*temp = x;
				free(raw_name);
				free(filename);
				free(temp2);
				return -1;
			}
			stat->line = (wchar_t *) p;

			swprintf(stat->line, i , L"%s%s%ls", line_mb, filename, temp2);

			stat->index = i - (stat->len - stat->index) - 1;
			stat->curs_pos = stat->prompt_wid
							  + wcswidth(stat->line, stat->index);
			stat->len = i - 1;

			*temp = x;
			free(raw_name);
			free(filename);
			free(temp2);
		}
		/* error */
		else
		{
			show_error_msg(" Debug Error ",
					"Harmless error in rline.c line 564");
			free(raw_name);
			free(filename);
			return -1;
		}


		werase(status_bar);
		mvwaddwstr(status_bar, 0, 0, stat->prompt);
		mvwaddwstr(status_bar, 0, stat->prompt_wid, stat->line);
		wmove(status_bar, 0, stat->curs_pos);
	}

	stat->complete_continue = 1;
	return 0;
}

char *
my_rl_gets(int type)
{
	static char *line_read_mb = (char *) NULL;
	struct line_stats stat;
	wchar_t key[2] = { 0 };
	int done = 0;
	int abort = 0;

	void *p;
	int i;

	/* If the buffer has already been allocated, free the memory. */
	free(line_read_mb);
	line_read_mb = (char *)NULL;

	stat.line = (wchar_t *) NULL;
	stat.index = 0;
	stat.curs_pos = 0;
	stat.len = 0;
	stat.cmd_pos = -1;
	stat.complete_continue = 0;
	stat.type = type;

	if (stat.type == GET_COMMAND || stat.type == MENU_COMMAND)
		wcsncpy(stat.prompt, L":", sizeof(stat.prompt) / sizeof(wchar_t));
	else if (stat.type == GET_SEARCH_PATTERN || stat.type == MENU_SEARCH)
		wcsncpy(stat.prompt, L"/", sizeof(stat.prompt) / sizeof(wchar_t));
	/*
	else if (type == MAPPED_COMMAND || type == MAPPED_SEARCH)
	{
		snprintf(buf, sizeof(buf), "%s", string);
		mvwaddstr(status_bar, 0, 0, string);
		pos = strlen(string);
		index = pos -1;
		len = index;
	}
	*/
	else if (stat.type == GET_VISUAL_COMMAND)
		wcsncpy(stat.prompt, L":'<,'>", sizeof(stat.prompt)
				 / sizeof(wchar_t));

	/*
	stat.prompt_wid = wcslen(prompt);
	stat.curs_pos = wcswidth(prompt);
	 */
	stat.prompt_wid = stat.curs_pos = wcslen(stat.prompt);

	curs_set(1);
	wresize(status_bar, 1, getmaxx(stdscr));
	werase(status_bar);

	mvwaddwstr(status_bar, 0, 0, stat.prompt);

	while (!done)
	{
		int line_width;
		line_width = getmaxx(stdscr);
		/* FIXME looks bad after terminal is resized
		 * maybe store somewhere parameters and restore them every time */
		wresize(status_bar, getmaxy(status_bar), getmaxx(stdscr));
		if(curr_stats.freeze)
		  continue;

		curs_set(1);
		flushinp();
		curr_stats.getting_input = 1;
		wget_wch(status_bar, (wint_t *) key);

		switch (*key)
		{
			case 27: /* ascii Escape */
			case 3: /* ascii Ctrl C */
				done = 1;
				abort = 1;
				break;
			case 13: /* ascii Return */
				done = 1;
				break;
			case 9: /* ascii Tab */
				if (   stat.type != MENU_SEARCH && stat.type != MENU_COMMAND
					&& stat.line )
				{
					int len;
					line_completion(&stat);
					len = (1 + stat.len + line_width - 1 + 1)/line_width;
					if (len > getmaxy(status_bar))
					{
						int delta = len - getmaxy(status_bar);
						mvwin(status_bar, getbegy(status_bar) - delta, 0);
						wresize(status_bar, getmaxy(status_bar) + delta, line_width);
						werase(status_bar);
						mvwaddwstr(status_bar, 0, 0, stat.prompt);
						mvwaddwstr(status_bar, 0, stat.prompt_wid, stat.line);
					}
				}
				break;
			case 16: /* ascii Ctrl P */
			case KEY_UP:

				stat.complete_continue = 0;

				if (stat.type == GET_COMMAND)
				{
					if (0 > cfg.cmd_history_num)
						break;

					if (++stat.cmd_pos > cfg.cmd_history_num)
						stat.cmd_pos = 0;

					stat.len = mbstowcs(NULL, cfg.cmd_history[stat.cmd_pos]
								, 0);
					p = realloc(stat.line, (stat.len+1) * sizeof(wchar_t));

					if (!p)
						break;

					stat.line = (wchar_t *) p;
					mbstowcs(stat.line, cfg.cmd_history[stat.cmd_pos]
							  , stat.len + 1);

					stat.curs_pos = stat.prompt_wid
									 + wcswidth(stat.line, stat.len);
					stat.index = stat.len;

					if (stat.len >= line_width - 1)
					{
						int new_height = (1 + stat.len + 1 + line_width - 1)/line_width;
						mvwin(status_bar, getbegy(status_bar) - (new_height - 1), 0);
						wresize(status_bar, new_height, line_width);
					}

					werase(status_bar);
					mvwaddwstr(status_bar, 0, 0, stat.prompt);
					mvwaddwstr(status_bar, 0, stat.prompt_wid, stat.line);

					if (stat.cmd_pos >= cfg.cmd_history_len - 1)
						stat.cmd_pos = cfg.cmd_history_len - 1;
				}
				else if (stat.type == GET_SEARCH_PATTERN)
				{
					if (0 > cfg.search_history_num)
						break;

					if(++stat.cmd_pos > cfg.search_history_num)
						stat.cmd_pos = 0;

					stat.len = mbstowcs(NULL
							, cfg.search_history[stat.cmd_pos], 0);

					p = realloc(stat.line, (stat.len+1) * sizeof(wchar_t));

					if (!p)
						break;

					stat.line = (wchar_t *) p;
					mbstowcs(stat.line, cfg.cmd_history[stat.cmd_pos]
							, stat.len + 1);

					stat.curs_pos = wcswidth(stat.line, stat.len)
									 + stat.prompt_wid;
					stat.index = stat.len;

					werase(status_bar);
					mvwaddwstr(status_bar, 0, 0, stat.prompt);
					mvwaddwstr(status_bar, 0, stat.prompt_wid, stat.line);

					if (stat.cmd_pos >= cfg.search_history_len - 1)
						stat.cmd_pos = cfg.search_history_len - 1;
				}
				break;
			case 14: /* ascii Ctrl N */
			case KEY_DOWN:

				stat.complete_continue = 0;

				if (stat.type == GET_COMMAND)
				{
					if (0 > cfg.cmd_history_num)
						break;

					if(--stat.cmd_pos < 0)
						stat.cmd_pos = cfg.cmd_history_num;

					stat.len = mbstowcs(NULL, cfg.cmd_history[stat.cmd_pos]
								, 0);

					p = realloc(stat.line, (stat.len+1) * sizeof(wchar_t));

					if (!p)
						break;

					stat.line = (wchar_t *) p;
					mbstowcs(stat.line, cfg.cmd_history[stat.cmd_pos]
							, stat.len + 1);

					stat.curs_pos = wcswidth(stat.line, stat.len)
									 + stat.prompt_wid;
					stat.index = stat.len;

					werase(status_bar);
					mvwaddwstr(status_bar, 0, 0, stat.prompt);
					mvwaddwstr(status_bar, 0, stat.prompt_wid, stat.line);

					if (stat.cmd_pos >= cfg.cmd_history_len - 1)
						stat.cmd_pos = cfg.cmd_history_len - 1;
				}
				else if (type == GET_SEARCH_PATTERN)
				{
					if (0 > cfg.search_history_num)
						break;

					if(--stat.cmd_pos < 0)
						stat.cmd_pos = cfg.search_history_num;

					stat.len = mbstowcs(NULL
								, cfg.search_history[stat.cmd_pos], 0);

					p = realloc(stat.line, (stat.len+1) * sizeof(wchar_t));

					if (!p)
						break;

					stat.line = (wchar_t *) p;
					mbstowcs(stat.line, cfg.cmd_history[stat.cmd_pos]
							  , stat.len + 1);

					stat.curs_pos = stat.prompt_wid
									+ wcswidth(stat.line, stat.len);
					stat.index = stat.len;

					werase(status_bar);
					mvwaddwstr(status_bar, 0, 0, stat.prompt);
					mvwaddwstr(status_bar, 0, stat.prompt_wid, stat.line);

					if (stat.cmd_pos >= cfg.search_history_len - 1)
						stat.cmd_pos = cfg.search_history_len - 1;
				}
				break;
			case KEY_LEFT:

				stat.complete_continue = 0;

				if (stat.index > 0)
				{
					stat.index--;
					stat.curs_pos -= wcwidth(stat.line[stat.index]);
					wmove(status_bar, stat.curs_pos/line_width, stat.curs_pos%line_width);
				}
				break;
			case KEY_RIGHT:

				stat.complete_continue = 0;

				if (stat.index < stat.len)
				{
					stat.curs_pos += wcwidth(stat.line[stat.index]);
					stat.index++;
					wmove(status_bar, stat.curs_pos/line_width, stat.curs_pos%line_width);
				}
				break;
			case KEY_HOME:
				stat.index = 0;
				stat.curs_pos = 1;
				wmove(status_bar, 0, stat.curs_pos);
				break;
			case KEY_END:
				stat.index = stat.len;
				stat.curs_pos = 1 + stat.len;
				wmove(status_bar, 0, stat.curs_pos);
				break;
			case 127: /* ascii Delete */
			case 8: /* ascii Backspace ascii Ctrl H */
			case KEY_BACKSPACE: /* ncurses BACKSPACE KEY */

				stat.complete_continue = 0;

				if (stat.index == 0)
					break;

				if (stat.index == stat.len)
				{ /* If the cursor is at the end of the line, maybe filling
				   * spaces by ourselves would goes faster then repaint
				   * the whole window entirely :-) */

					int w;

					stat.index--;
					stat.len--;

					i = stat.curs_pos;
					w = wcwidth(stat.line[stat.index]);
					while (i - stat.curs_pos < w)
					{
						mvwaddch(status_bar, stat.curs_pos/line_width,
								stat.curs_pos%line_width, ' ');
						stat.curs_pos--;
					}
					mvwaddch(status_bar, stat.curs_pos/line_width,
							stat.curs_pos%line_width, ' ');

					stat.line[stat.index] = L'\0';
				}
				else
				{
					stat.index--;
					stat.len--;

					stat.curs_pos -= wcwidth(stat.line[stat.index]);
					wcsdel(stat.line, stat.index+1, 1);

					werase(status_bar);
					mvwaddwstr(status_bar, 0, 0, stat.prompt);
					mvwaddwstr(status_bar, 0, stat.prompt_wid, stat.line);
				}

				wmove(status_bar, stat.curs_pos/line_width, stat.curs_pos%line_width);
				break;
			case 21: /* ascii Ctrl U */
				stat.complete_continue = 0;

				if (stat.index == 0)
					break;

				stat.len -= stat.index;

				stat.curs_pos = 1;
				wcsdel(stat.line, 1, stat.index);

				stat.index = 0;

				werase(status_bar);
				mvwaddwstr(status_bar, 0, 0, stat.prompt);
				mvwaddwstr(status_bar, 0, stat.prompt_wid, stat.line);

				wmove(status_bar, 0, stat.curs_pos);
				break;
			case KEY_DC: /* ncurses Delete */

				stat.complete_continue = 0;

				if (stat.index == stat.len)
					break;

				wcsdel(stat.line, stat.index+1, 1);
				stat.len--;

				werase(status_bar);
				mvwaddwstr(status_bar, 0, 0, stat.prompt);
				mvwaddwstr(status_bar, 0, stat.prompt_wid, stat.line);

				wmove(status_bar, stat.curs_pos/line_width, stat.curs_pos%line_width);
				break;
			default:
				if (	stat.complete_continue == 1
					 && stat.line[stat.index - 1] == L'/'
					 && *key == L'/' )
				{
					stat.complete_continue = 0;
					break;
				}

				stat.complete_continue = 0;

				if (!iswprint(*key))
					break;

				if (stat.index + 1 > CMD_MAX_LEN)
				{
					abort = 1;
					done = 1;
				}

				p = realloc(stat.line, (stat.len+2) * sizeof(wchar_t));

				if (!p)
					return NULL;

				stat.line = (wchar_t *) p;

				if (stat.len == 0)
					stat.line[0] = L'\0';

				stat.index++;
				wcsins(stat.line, key, stat.index);
				stat.len++;

				if ((stat.len + 1) % getmaxx(status_bar) == 0)
				{
					mvwin(status_bar, getbegy(status_bar) - 1, 0);
					wresize(status_bar, getmaxy(status_bar) + 1, getmaxx(status_bar));
					werase(status_bar);
				}

				mvwaddwstr(status_bar, 0, 0, stat.prompt);
				mvwaddwstr(status_bar, 0, stat.prompt_wid, stat.line);

				stat.curs_pos += wcwidth(*key);
				wmove(status_bar, 0, stat.curs_pos);
				break;
		}
		curr_stats.getting_input = 0;
	}

	curs_set(0);
	if (getmaxy(status_bar) > 1)
	{
		redraw_window();
		mvwin(status_bar, getmaxy(stdscr) - 1, 0);
		wresize(status_bar, 1, getmaxx(stdscr) -19);
	}
	werase(status_bar);
	wnoutrefresh(status_bar);

	if (!stat.line || !stat.line[0] || abort)
	{
		if (abort && stat.len > CMD_MAX_LEN)
			show_error_msg(" Error ", "Too long command line");
		free(stat.line);
		return NULL;
	}

	i = wcstombs(NULL, stat.line, 0) + 1;
	if (! (p = (char *) malloc(i * sizeof(char))))
	{
		free(stat.line);
		return NULL;
	}

	line_read_mb = p;
	wcstombs(line_read_mb, stat.line, i);

	free(stat.line);

	return line_read_mb;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
