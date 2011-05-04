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

#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "ui.h"
#include "status.h"
#include "utils.h"

struct Fuse_List *fuse_mounts = NULL;

int
is_dir(char *file)
{
	struct stat statbuf;
	stat(file, &statbuf);

	if(S_ISDIR(statbuf.st_mode))
		return 1;
	else
		return 0;
}

void *
duplicate (void *stuff, int size)
{
  void *new_stuff = (void *) malloc (size);
  memcpy (new_stuff, stuff, size);
  return new_stuff;
}

/*
 * Escape the filename for the purpose of inserting it into the shell.
 * Returns new string, caller should free it.
 */
char *
escape_filename(const char *string, size_t len, int quote_percent)
{
	char *ret, *dup;

	dup = ret = (char *)malloc (len * 2 + 2 + 1);

	if (*string == '-')
	{
		*dup++ = '.';
		*dup++ = '/';
	}

	int i;
	for (i = 0; i < len; i++, string++, dup++)
	{
		switch (*string)
		{
			case '%':
				if (quote_percent)
					*dup++ = '%';
				break;
			case '\'':
			case '\\':
			case '\r':
			case '\n':
			case '\t':
			case '"':
			case ';':
			case ' ':
			case '?':
			case '|':
			case '[':
			case ']':
			case '{':
			case '}':
			case '<':
			case '>':
			case '`':
			case '!':
			case '$':
			case '&':
			case '*':
			case '(':
			case ')':
				*dup++ = '\\';
				break;
			case '~':
			case '#':
				if (dup == ret)
					*dup++ = '\\';
				break;
		}
		*dup = *string;
  }
  *dup = '\0';
  return ret;
}

void
chomp(char *text)
{
	if(text[strlen(text) -1] == '\n')
		text[strlen(text) -1] = '\0';
}

/* Only used for debugging */
int
write_string_to_file(char *filename, char *string)
{
	FILE *fp;

	if((fp = fopen(filename, "w")) == NULL)
		return 0;

	fprintf(fp, "%s", string);

	fclose(fp);
	return 1;
}

size_t
guess_char_width(char c)
{
	if ((c & 0xe0) == 0xc0)
		return 2;
	else if ((c & 0xf0) == 0xe0)
		return 3;
	else if ((c & 0xf8) == 0xf0)
		return 4;
	else
		return 1;
}

size_t
get_char_width(const char* string)
{
	if((string[0] & 0xe0) == 0xc0 && (string[1] & 0xc0) == 0x80)
		return 2;
	else if((string[0] & 0xf0) == 0xe0 && (string[1] & 0xc0) == 0x80 &&
			 (string[2] & 0xc0) == 0x80)
		return 3;
	else if ((string[0] & 0xf8) == 0xf0 && (string[1] & 0xc0) == 0x80 &&
			 (string[2] & 0xc0) == 0x80 && (string[3] & 0xc0) == 0x80)
		return 4;
	else if(string[0] == '\0')
		return 0;
	else
		return 1;
}

size_t
get_real_string_width(char *string, size_t max_len)
{
	size_t width = 0;
	while(*string != '\0' && max_len-- != 0)
	{
		size_t char_width = get_char_width(string);
		width += char_width;
		string += char_width;
	}
	return width;
}

size_t
get_utf8_string_length(const char *string)
{
	size_t length = 0;
	while(*string != '\0')
	{
		size_t char_width = get_char_width(string);
		string += char_width;
		length++;
	}
	return length;
}

size_t
get_utf8_overhead(const char *string)
{
	size_t overhead = 0;
	while(*string != '\0')
	{
		size_t char_width = get_char_width(string);
		string += char_width;
		overhead += char_width - 1;
	}
	return overhead;
}

size_t
get_utf8_prev_width(char *string, size_t cur_width)
{
	size_t width = 0;
	while (*string != '\0') {
		size_t char_width = get_char_width(string);
		if (width + char_width >= cur_width)
			break;
		width += char_width;
		string += char_width;
	}
	return width;
}

wchar_t *
to_wide(const char *s)
{
	wchar_t *result;
	int len;

	len = mbstowcs(NULL, s, 0);
	result = malloc((len + 1)*sizeof(wchar_t));
	if(result != NULL)
		mbstowcs(result, s, len + 1);
	return result;
}

void
run_from_fork(int pipe[2], int err, char *cmd)
{
	char *args[4];
	int nullfd;

	close(err ? 2 : 1); /* Close stderr or stdout */
	dup(pipe[1]);       /* Redirect stderr or stdout to write end of pipe. */
	close(pipe[0]);     /* Close read end of pipe. */
	close(0);           /* Close stdin */
	close(err ? 1 : 2); /* Close stdout or stderr */

	/* Send stdout, stdin to /dev/null */
	if((nullfd = open("/dev/null", O_RDONLY)) != -1)
	{
		dup2(nullfd, 0);
		dup2(nullfd, err ? 1 : 2);
	}

	args[0] = "sh";
	args[1] = "-c";
	args[2] = cmd;
	args[3] = NULL;

	execvp(args[0], args);
	exit(-1);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
