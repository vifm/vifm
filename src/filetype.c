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

#include<ncurses.h>
#include<string.h>
#include<stdlib.h>

#include "config.h"
#include "filetype.h"

int 
get_filetype_number(char *file)
{
	char *strptr;
	char *ptr;
	char ext[24];
	int x;

	/* Stefan Walter fixed this to catch tar.gz extensions */
	strptr = file;
	while((ptr = strchr(strptr, '.')) != NULL)
	{
		ptr++;
		snprintf(ext, sizeof(ext), "%s", ptr);

		for(x = 0; x < cfg.filetypes_num; x++)
		{
			char *exptr = NULL;

			/* Only one extension */
			if((exptr = strchr(filetypes[x].ext, ',')) == NULL)
			{
				if(!strcasecmp(filetypes[x].ext, ext))
					return x;
			}
			else
			{
				char *ex_copy = strdup(filetypes[x].ext);
				char *free_this = ex_copy;
				char *exptr2 = NULL;
				while((exptr = exptr2= strchr(ex_copy, ',')) != NULL)
				{
					*exptr = '\0';
					exptr2++;

					if(!strcasecmp(ext, ex_copy))
					{
						free(free_this);
						return x;
					}

					ex_copy = exptr2;	
				}
				if(!strcasecmp(ext, ex_copy))
				{
					free(free_this);
					return x;
				}
				free(free_this);
			}
		}
		strptr = ptr;
	}
	return -1;
}

char *
get_default_program_for_file(char *file)
{
	int x = get_filetype_number(file);
	char *strptr = NULL;
	char *ptr = NULL;
	char *program_name = NULL;

	if(x < 0)
		return NULL;

	strptr = strdup(filetypes[x].programs);

	/* Only one program */
	if((ptr = strchr(strptr, ',')) == NULL)
	{
			program_name = strdup(strptr);
	}
	else
	{
		*ptr = '\0';
		program_name = strdup(strptr);
	}
	free(strptr);
	return program_name;
}


char *
get_all_programs_for_file(char *file)
{
	int x = get_filetype_number(file);

	if(x > -1)
		return filetypes[x].programs;

	return NULL;
}

void
clear_filetypes(void)
{
	int x;

	for(x = 0; x < cfg.filetypes_num; x++)
	{
		if(filetypes[x].type)
			free(filetypes[x].type);
		if(filetypes[x].ext)
			free(filetypes[x].ext);
		if(filetypes[x].programs)
			free(filetypes[x].programs);
	}
	cfg.filetypes_num = 0;
}

void
add_filetype(char *description, char *extension, char *program)
{

	filetypes = (filetype_t *)realloc(filetypes, 
			(cfg.filetypes_num +1) * sizeof(filetype_t));

	filetypes[cfg.filetypes_num].type = strdup(description);

	filetypes[cfg.filetypes_num].ext = strdup(extension);
	filetypes[cfg.filetypes_num].programs = strdup(program);
	cfg.filetypes_num++;
}

