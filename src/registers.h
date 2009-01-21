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

#ifndef __REGISTERS_H_
#define __REGISTERS_H_

#define NUM_REGISTERS 26

typedef struct _Registers
{
	int name;
	int num_files;
	char ** files;
	int deleted;

}registers_t;

registers_t reg[NUM_REGISTERS];
extern char valid_registers[];


int is_valid_register(int key);
void load_register(int reg, char *file);
void append_to_register(int reg, char *file);
void clear_register(int reg);

#endif
