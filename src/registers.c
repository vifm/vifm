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

#include <unistd.h>

#include <string.h>

#include "menus.h"
#include "utils.h"

#include "registers.h"

#define NUM_REGISTERS 27

static registers_t registers[NUM_REGISTERS];

static char valid_registers[] = {
	'"',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
	'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
	'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z'
};

void
init_registers(void)
{
	int i;
	for(i = 0; i < NUM_REGISTERS; i++)
	{
		registers[i].name = valid_registers[i];
		registers[i].num_files = 0;
		registers[i].files = NULL;
		registers[i].deleted = 0;
	}
}

registers_t *
find_register(int key)
{
	int i;
	for(i = 0; i < NUM_REGISTERS; i++)
	{
		if(registers[i].name == key)
			return &registers[i];
	}
	return NULL;
}

static int
check_for_duplicate_file_names(registers_t *reg, const char *file)
{
	int x;
	for(x = 0; x < reg->num_files; x++)
	{
		if(strcmp(file, reg->files[x]) == 0)
			return 1;
	}
	return 0;
}

void
append_to_register(int key, char *file)
{
	registers_t *reg;

	if((reg = find_register(key)) == NULL)
		return;

	if(access(file, F_OK))
		return;

	if(check_for_duplicate_file_names(reg, file))
		return;

	reg->num_files++;
	reg->files = (char **)realloc(reg->files, reg->num_files * sizeof(char *));
	reg->files[reg->num_files - 1] = strdup(file);
}

void
clear_register(int key)
{
	int y;
	registers_t *reg;

	if((reg = find_register(key)) == NULL)
		return;

	y = reg->num_files;
	while(y--)
	{
		free(reg->files[y]);
	}
	reg->num_files = 0;
}

void
pack_register(int key)
{
	int x, y;
	registers_t *reg;

	if((reg = find_register(key)) == NULL)
		return;

	x = 0;
	for(y = 0; y < reg->num_files; y++)
		if(reg->files[y] != NULL)
			reg->files[x++] = reg->files[y];
	reg->num_files = x;
}

char **
list_registers_content(void)
{
	int x;
	char **list = NULL;
	size_t len = 0;

	for(x = 0; x < NUM_REGISTERS; x++)
	{
		char buf[56];
		int y;
			
		if(registers[x].num_files <= 0)
			continue;

		snprintf(buf, sizeof(buf), "\"%c", registers[x].name);
		list = (char **)realloc(list, sizeof(char *)*(len + 1));
		list[len] = strdup(buf);
		len++;

		y = registers[x].num_files;
		while(y-- > 0)
		{
			list = (char **)realloc(list, sizeof(char *)*(len + 1));
			list[len] = strdup(registers[x].files[y]);

			len++;
		}
	}

	list = (char **)realloc(list, sizeof(char *)*(len + 1));
	list[len] = NULL;
	return list;
}

void
rename_in_registers(const char *old, const char *new)
{
	int x;
	for(x = 0; x < NUM_REGISTERS; x++)
	{
		int y, n;
		n = registers[x].num_files;
		for(y = 0; y < n; y++)
		{
			if(strcmp(registers[x].files[y], old) != 0)
				continue;

			free(registers[x].files[y]);
			registers[x].files[y] = strdup(new);
			break; /* registers don't contain duplicates */
		}
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
