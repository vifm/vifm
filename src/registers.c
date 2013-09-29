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

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <stddef.h> /* NULL size_t */
#include <string.h>

#include "cfg/config.h"
#include "menus/menus.h"
#include "utils/fs.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"

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
register_exists(int key)
{
	int i;
	for(i = 0; i < NUM_REGISTERS; i++)
	{
		if(valid_registers[i] == key)
		{
			return 1;
		}
	}
	return 0;
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
check_for_duplicate_file_names(registers_t *reg, const char file[])
{
	int x;
	for(x = 0; x < reg->num_files; x++)
	{
		if(stroscmp(file, reg->files[x]) == 0)
			return 1;
	}
	return 0;
}

void
append_to_register(int key, const char file[])
{
	registers_t *reg;
	struct stat st;

	if(key == BLACKHOLE_REG_NAME)
		return;

	if((reg = find_register(key)) == NULL)
		return;

	if(lstat(file, &st) != 0)
		return;

	if(check_for_duplicate_file_names(reg, file))
		return;

	reg->num_files = add_to_string_array(&reg->files, reg->num_files, 1, file);
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
clear_register(int key)
{
	registers_t *reg;

	if((reg = find_register(key)) == NULL)
		return;

	free_string_array(reg->files, reg->num_files);
	reg->files = NULL;
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
list_registers_content(const char registers[])
{
	char **list = NULL;
	size_t len = 0;

	while(*registers != '\0')
	{
		registers_t *reg;
		char buf[56];
		int y;
			
		if((reg = find_register(*registers++)) == NULL)
			continue;

		if(reg->num_files <= 0)
			continue;

		snprintf(buf, sizeof(buf), "\"%c", reg->name);
		len = add_to_string_array(&list, len, 1, buf);

		y = reg->num_files;
		while(y-- > 0)
		{
			len = add_to_string_array(&list, len, 1, reg->files[y]);
		}
	}

	(void)put_into_string_array(&list, len, NULL);
	return list;
}

void
rename_in_registers(const char old[], const char new[])
{
	int x;
	for(x = 0; x < NUM_REGISTERS; x++)
	{
		int y, n;
		n = registers[x].num_files;
		for(y = 0; y < n; y++)
		{
			if(stroscmp(registers[x].files[y], old) != 0)
				continue;

			(void)replace_string(&registers[x].files[y], new);
			break; /* registers don't contain duplicates */
		}
	}
}

void
clean_regs_with_trash(void)
{
	int x;
	int trash_dir_len = strlen(cfg.trash_dir);
	for(x = 0; x < NUM_REGISTERS; x++)
	{
		int y, n, needs_pack = 0;
		n = registers[x].num_files;
		for(y = 0; y < n; y++)
		{
			if(strnoscmp(registers[x].files[y], cfg.trash_dir, trash_dir_len) != 0)
				continue;
			if(!path_exists(registers[x].files[y]))
				continue;

			free(registers[x].files[y]);
			registers[x].files[y] = NULL;
			needs_pack = 1;
		}
		if(needs_pack)
			pack_register(registers[x].name);
	}
}

void
update_unnamed_reg(int key)
{
	registers_t *unnamed, *reg;
	int i;

	if(key == UNNAMED_REG_NAME)
		return;

	if((reg = find_register(key)) == NULL)
		return;

	if((unnamed = find_register(UNNAMED_REG_NAME)) == NULL)
		return;

	clear_register(UNNAMED_REG_NAME);

	unnamed->num_files = reg->num_files;
	unnamed->files = (char **)realloc(unnamed->files,
			unnamed->num_files*sizeof(char *));
	for(i = 0; i < unnamed->num_files; i++)
		unnamed->files[i] = strdup(reg->files[i]);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
