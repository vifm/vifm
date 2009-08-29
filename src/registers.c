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

#include<unistd.h>
#include<string.h>

#include"menus.h"
#include"registers.h"
#include"utils.h"

char valid_registers[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i',
'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
'y', 'z', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 
'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'};

int
is_valid_register(int key)
{
	int x = 0;

	for (x = 0; x < strlen(valid_registers); x++)
	{
		if (key == (int)valid_registers[x])
			return 1;
	}

	return 0;
}

static int
check_for_duplicate_file_names(int pos, char *file)
{
	int z;
	int x = 0;
	for (z = 0; z < reg[pos].num_files; z++)
	{
		if (!strcmp(file, reg[pos].files[z]))
			return ++x;
	}
	return x;
}

void
append_to_register(int key, char *file)
{
	int i = 0;

	if (access(file, F_OK))
		return;

	for (i = 0; i < NUM_REGISTERS; i++)
	{
		if (reg[i].name == key)
		{
			if (check_for_duplicate_file_names(i, file))
				break;
			reg[i].num_files++;
			reg[i].files = (char **)realloc(reg[i].files,
					reg[i].num_files  * sizeof(char *));
			reg[i].files[reg[i].num_files - 1] = strdup(file);
			break;
		}

	}
}

void
clear_register(int key)
{
	int i = 0;

	for (i = 0; i < NUM_REGISTERS; i++)
	{
		if (reg[i].name == key)
		{
			int y = reg[i].num_files;
			while (y)
			{
				y--;
				my_free(reg[i].files[y]);
			}
			reg[i].num_files = 0;
			break;
		}
	}
}
