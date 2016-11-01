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
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/utils.h"
#include "trash.h"

/* Name of the "unnamed" (the default) register. */
#define UNNAMED_REG_NAME '"'
/* Name of the "black hole" register. */
#define BLACKHOLE_REG_NAME '_'
/* Number of registers named as alphabet letters. */
#define NUM_LETTER_REGISTERS 26
/* Number of all available registers (excludes 26 uppercase letters). */
#define NUM_REGISTERS (2 + NUM_LETTER_REGISTERS)

/* Data of all registers. */
static reg_t registers[NUM_REGISTERS];

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
regs_init(void)
{
	int i;
	for(i = 0; i < NUM_REGISTERS; i++)
	{
		registers[i].name = valid_registers[i];
		registers[i].nfiles = 0;
		registers[i].files = NULL;
	}
}

int
regs_exists(int reg_name)
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

reg_t *
regs_find(int reg_name)
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
check_for_duplicate_file_names(reg_t *reg, const char file[])
{
	int i;
	for(i = 0; i < reg->nfiles; ++i)
	{
		if(stroscmp(file, reg->files[i]) == 0)
		{
			return 1;
		}
	}
	return 0;
}

int
regs_append(int reg_name, const char file[])
{
	reg_t *reg;

	if(reg_name == BLACKHOLE_REG_NAME)
	{
		return 0;
	}
	if((reg = regs_find(reg_name)) == NULL)
	{
		return 1;
	}
	if(check_for_duplicate_file_names(reg, file))
	{
		return 1;
	}

	reg->nfiles = add_to_string_array(&reg->files, reg->nfiles, 1, file);
	return 0;
}

void
regs_reset(void)
{
	const char *p = valid_registers;
	while(*p != '\0')
	{
		regs_clear(*p++);
	}
}

void
regs_clear(int reg_name)
{
	reg_t *const reg = regs_find(reg_name);
	if(reg == NULL)
	{
		return;
	}

	free_string_array(reg->files, reg->nfiles);
	reg->files = NULL;
	reg->nfiles = 0;
}

void
regs_pack(int reg_name)
{
	int j, i;
	reg_t *const reg = regs_find(reg_name);
	if(reg == NULL)
	{
		return;
	}

	j = 0;
	for(i = 0; i < reg->nfiles; ++i)
	{
		if(reg->files[i] != NULL)
		{
			reg->files[j++] = reg->files[i];
		}
	}
	reg->nfiles = j;
}

char **
regs_list(const char registers[])
{
	char **list = NULL;
	size_t len = 0;

	while(*registers != '\0')
	{
		reg_t *reg = regs_find(*registers++);
		char reg_str[16];
		int i;

		if(reg == NULL || reg->nfiles <= 0)
		{
			continue;
		}

		snprintf(reg_str, sizeof(reg_str), "\"%c", reg->name);
		len = add_to_string_array(&list, len, 1, reg_str);

		i = reg->nfiles;
		while(i-- > 0)
		{
			len = add_to_string_array(&list, len, 1, reg->files[i]);
		}
	}

	(void)put_into_string_array(&list, len, NULL);
	return list;
}

void
regs_rename_contents(const char old[], const char new[])
{
	int i;
	for(i = 0; i < NUM_REGISTERS; ++i)
	{
		int j;
		const int n = registers[i].nfiles;
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
regs_remove_trashed_files(const char trash_dir[])
{
	int i;
	for(i = 0; i < NUM_REGISTERS; ++i)
	{
		int j, needs_packing = 0;
		const int n = registers[i].nfiles;
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
			regs_pack(registers[i].name);
		}
	}
}

void
regs_update_unnamed(int reg_name)
{
	reg_t *unnamed, *reg;
	int i;

	if(reg_name == UNNAMED_REG_NAME)
		return;

	if((reg = regs_find(reg_name)) == NULL)
		return;

	if((unnamed = regs_find(UNNAMED_REG_NAME)) == NULL)
		return;

	regs_clear(UNNAMED_REG_NAME);

	unnamed->nfiles = reg->nfiles;
	unnamed->files = reallocarray(unnamed->files, unnamed->nfiles,
			sizeof(char *));
	for(i = 0; i < unnamed->nfiles; ++i)
	{
		unnamed->files[i] = strdup(reg->files[i]);
	}
}

void
regs_suggest(regs_suggest_cb cb, int max_entries_per_reg)
{
	wchar_t reg_name[] = L"reg: X";
	const char *registers = valid_registers;

	while(*registers != '\0')
	{
		reg_t *const reg = regs_find(*registers++);
		int i, max = max_entries_per_reg;

		if(reg == NULL || reg->nfiles <= 0)
		{
			continue;
		}

		i = reg->nfiles - 1;

		reg_name[5] = reg->name;
		cb(reg_name, L"", replace_home_part(reg->files[i]));

		while(--max > 0 && i-- > 0)
		{
			cb(L"", L"", replace_home_part(reg->files[i]));
		}
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
