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

#ifndef VIFM__REGISTERS_H__
#define VIFM__REGISTERS_H__

/* Name of the default register. */
#define DEFAULT_REG_NAME '"'

typedef struct
{
	int name;
	int num_files;
	char **files;
}registers_t;

/* Null terminated list of all valid register names. */
extern const char valid_registers[];

void init_registers(void);
/* Checks whether register with the key name exists (A-Z will be rejected).
 * Returns non-zero if it exists, otherwise zero is returned. */
int register_exists(int key);
registers_t * find_register(int key);
void append_to_register(int reg, const char file[]);
/* Clears all registers. */
void clear_registers(void);
void clear_register(int reg);
void pack_register(int reg);
char ** list_registers_content(const char registers[]);
void rename_in_registers(const char old[], const char new[]);
void clean_regs_with_trash(void);
void update_unnamed_reg(int reg);

#endif /* VIFM__REGISTERS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
