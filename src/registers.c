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
#include "registers.h"
#include "utils.h"

char valid_registers[] = {
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
		reg[i].name = valid_registers[i];
		reg[i].num_files = 0;
		reg[i].files = NULL;
		reg[i].deleted = 0;
	}
}

registers_t *
find_register(int key)
{
	int i;
	for(i = 0; i < NUM_REGISTERS; i++)
	{
		if(reg[i].name == key)
			return &reg[i];
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
