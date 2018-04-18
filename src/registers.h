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

#include <stddef.h> /* wchar_t */

#include "utils/test_helpers.h"

/* Name of the default register. */
#define DEFAULT_REG_NAME '"'

/* Name of the "black hole" register. */
#define BLACKHOLE_REG_NAME '_'

/* Holds register data. */
typedef struct
{
	int name;     /* Name of the register. */
	int nfiles;   /* Number of files in the register. */
	char **files; /* List of full paths of files. */
}
reg_t;

/* Callback for regs_suggest() function invoked per active register. */
typedef void (*regs_suggest_cb)(const wchar_t text[], const wchar_t value[],
		const char descr[]);

/* Null terminated list of all valid register names. */
extern const char valid_registers[];

/* Initializes registers unit. */
void regs_init(void);

/* Checks whether register with specified name exists (A-Z is rejected).
 * Returns non-zero if it exists, otherwise zero is returned. */
int regs_exists(int reg_name);

/* Retrieves register structure by register name.  Returns the structure or NULL
 * if register name is incorrect. */
reg_t * regs_find(int reg_name);

/* Appends path to the file to register specified by name.  Might fail for
 * duplicate, non-existing path or wrong register name.  Returns zero when file
 * is added, otherwise non-zero is returned. */
int regs_append(int reg_name, const char file[]);

/* Clears all registers.  Pair of regs_init(). */
void regs_reset(void);

/* Clears register with specified name or does nothing if name is incorrect. */
void regs_clear(int reg_name);

/* Packs registers file list by removing NULL entries in its list of files. */
void regs_pack(int reg_name);

/* Formats array of strings describing contents of registers specified in the
 * registers string (each character is processed as register name).  Returns
 * NULL terminated list of strings or NULL if there is not enough memory. */
char ** regs_list(const char registers[]);

/* Replaces records of the old path with the new path in all registers. */
void regs_rename_contents(const char old[], const char new[]);

/* Ensures that registers don't refer to files in specified trash directory or
 * to any of trash directories if trash_dir is NULL. */
void regs_remove_trashed_files(const char trash_dir[]);

/* Populates default (unnamed) register with contents of the specified
 * register. */
void regs_update_unnamed(int reg_name);

/* Lists active registers. */
void regs_suggest(regs_suggest_cb cb, int max_entries_per_reg);

/* Management of shared state. */

/* Enables or re-enables sharing of registers' state using given identifier. */
void regs_sync_enable(const char shared_memory_name[]);

/* Disables sharing of registers' state. */
void regs_sync_disable(void);

/* Puts contents of registers into shared memory. */
void regs_sync_to_shared_memory(void);

/* Retrieves contents of registers from shared memory. */
void regs_sync_from_shared_memory(void);

TSTATIC_DEFS(
	/* Checks whether register sharing is currently enabled.  Returns non-zero if
	 * so, otherwise zero is returned. */
	int regs_sync_enabled(void);
	/* Dumps debug information about shared memory to stdout. */
	void regs_sync_debug_print_memory(void);
	/* Enables test mode of shared memory. */
	void regs_sync_enable_test_mode(void);
)

#endif /* VIFM__REGISTERS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
