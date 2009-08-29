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


#include<stdio.h>
#include<sys/time.h>
#include <unistd.h>
#include<stdlib.h>
#include<signal.h>
#include<string.h>
#include<sys/stat.h>

#include "ui.h"
#include "status.h"
/*_SZ_BEGIN_*/
#include "utils.h"

struct Fuse_List *fuse_mounts = NULL;
/*_SZ_END_*/

/* Checks for a NULL pointer before calling free() */
void
my_free(void *stuff)
{
	if(stuff != NULL)
		free(stuff);
}

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

