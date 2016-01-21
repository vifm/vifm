/* vifm
 * Copyright (C) 2001 Ken Steen.
 * Copyright (C) 2011 xaizek.
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

#include "registers.h"

#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <string.h>

#include "compat/reallocarray.h"
#include "utils/fs.h"
#include "utils/macros.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/utils.h"
#include "trash.h"

/* Name of the "unnamed" (the default) register. */
#define UNNAMED_REG_NAME '"'
/* Name of the "blackhole" register. */
#define BLACKHOLE_REG_NAME '_'
/* Number of registers named as alphabet letters. */
#define NUM_LETTER_REGISTERS 26
/* Number of all available registers (excludes 26 uppercase letters). */
#define NUM_REGISTERS (2 + NUM_LETTER_REGISTERS)

/* Data of all registers. */
static registers_t registers[NUM_REGISTERS];

/* Names of registers + names of 26 uppercase register names + termination null
 * character. */
const char valid_registers[] = {
	BLACKHOLE_REG_NAME, UNNAMED_REG_NAME,
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
	'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
	'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'\0'
};
/* Length should be equal to number of names of existing registers + names of 26
 * uppercase registers (virtual ones) + termination null character. */
ARRAY_GUARD(valid_registers, NUM_REGISTERS + NUM_LETTER_REGISTERS + 1);

void
init_registers(void)
{
	int i;
	for(i = 0; i < NUM_REGISTERS; i++)
	{
		registers[i].name = valid_registers[i];
		registers[i].num_files = 0;
		registers[i].files = NULL;
	}
}

int
register_exists(int reg_name)
{
	int i;
	for(i = 0; i < NUM_REGISTERS; ++i)
	{
		if(valid_registers[i] == reg_name)
		{
			return 1;
		}
	}
	return 0;
}

registers_t *
find_register(int reg_name)
{
	int i;
	for(i = 0; i < NUM_REGISTERS; ++i)
	{
		if(registers[i].name == reg_name)
		{
			return &registers[i];
		}
	}
	return NULL;
}

static int
check_for_duplicate_file_names(registers_t *reg, const char file[])
{
	int i;
	for(i = 0; i < reg->num_files; ++i)
	{
		if(stroscmp(file, reg->files[i]) == 0)
		{
			return 1;
		}
	}
	return 0;
}

int
append_to_register(int reg_name, const char file[])
{
	registers_t *reg;

	if(reg_name == BLACKHOLE_REG_NAME)
	{
		return 0;
	}
	if((reg = find_register(reg_name)) == NULL)
	{
		return 1;
	}
	if(!path_exists(file, NODEREF))
	{
		return 1;
	}
	if(check_for_duplicate_file_names(reg, file))
	{
		return 1;
	}

	reg->num_files = add_to_string_array(&reg->files, reg->num_files, 1, file);
	return 0;
}

void
clear_registers(void)
{
	const char *p = valid_registers;
	while(*p != '\0')
	{
		clear_register(*p++);
	}
}

void
clear_register(int reg_name)
{
	registers_t *const reg = find_register(reg_name);
	if(reg == NULL)
	{
		return;
	}

	free_string_array(reg->files, reg->num_files);
	reg->files = NULL;
	reg->num_files = 0;
}

void
pack_register(int reg_name)
{
	int j, i;
	registers_t *const reg = find_register(reg_name);
	if(reg == NULL)
	{
		return;
	}

	j = 0;
	for(i = 0; i < reg->num_files; ++i)
	{
		if(reg->files[i] != NULL)
		{
			reg->files[j++] = reg->files[i];
		}
	}
	reg->num_files = j;
}

char **
list_registers_content(const char registers[])
{
	char **list = NULL;
	size_t len = 0;

	while(*registers != '\0')
	{
		registers_t *reg = find_register(*registers++);
		char reg_str[16];
		int i;

		if(reg == NULL || reg->num_files <= 0)
		{
			continue;
		}

		snprintf(reg_str, sizeof(reg_str), "\"%c", reg->name);
		len = add_to_string_array(&list, len, 1, reg_str);

		i = reg->num_files;
		while(i-- > 0)
		{
			len = add_to_string_array(&list, len, 1, reg->files[i]);
		}
	}

	(void)put_into_string_array(&list, len, NULL);
	return list;
}

void
rename_in_registers(const char old[], const char new[])
{
	int i;
	for(i = 0; i < NUM_REGISTERS; ++i)
	{
		int j;
		const int n = registers[i].num_files;
		for(j = 0; j < n; ++j)
		{
			if(stroscmp(registers[i].files[j], old) != 0)
				continue;

			(void)replace_string(&registers[i].files[j], new);
			/* Registers don't contain duplicates, so exit this loop. */
			break;
		}
	}
}

void
clean_regs_with_trash(const char trash_dir[])
{
	int i;
	for(i = 0; i < NUM_REGISTERS; ++i)
	{
		int j, needs_packing = 0;
		const int n = registers[i].num_files;
		for(j = 0; j < n; ++j)
		{
			if(!trash_contains(trash_dir, registers[i].files[j]))
				continue;
			if(!path_exists(registers[i].files[j], DEREF))
				continue;

			update_string(&registers[i].files[j], NULL);
			needs_packing = 1;
		}
		if(needs_packing)
		{
			pack_register(registers[i].name);
		}
	}
}

void
update_unnamed_reg(int reg_name)
{
	registers_t *unnamed, *reg;
	int i;

	if(reg_name == UNNAMED_REG_NAME)
		return;

	if((reg = find_register(reg_name)) == NULL)
		return;

	if((unnamed = find_register(UNNAMED_REG_NAME)) == NULL)
		return;

	clear_register(UNNAMED_REG_NAME);

	unnamed->num_files = reg->num_files;
	unnamed->files = reallocarray(unnamed->files, unnamed->num_files,
			sizeof(char *));
	for(i = 0; i < unnamed->num_files; ++i)
	{
		unnamed->files[i] = strdup(reg->files[i]);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
