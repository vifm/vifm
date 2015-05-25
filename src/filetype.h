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

#ifndef VIFM__FILETYPE_H__
#define VIFM__FILETYPE_H__

#include "utils/test_helpers.h"

#define VIFM_PSEUDO_CMD "vifm"

/* Type of file association by it's source. */
typedef enum
{
	ART_BUILTIN, /* builtin type of file association, set automatically by vifm */
	ART_CUSTOM, /* custom type of file association, which is set by user */
}
assoc_record_type_t;

typedef struct
{
	char *command;
	char *description;
	assoc_record_type_t type;
}
assoc_record_t;

/* It's guarantied that for existent association there is at least on element in
 * the records list */
typedef struct
{
	assoc_record_t *list;
	int count;
}
assoc_records_t;

typedef struct
{
	char *pattern;
	int globs;               /* Whether pattern field contains globs, not res. */
	assoc_records_t records;
}
assoc_t;

typedef struct
{
	assoc_t *list;
	int count;
}
assoc_list_t;

/* Prototype for external command existence check function. */
typedef int (*external_command_exists_t)(const char *name);

const assoc_record_t NONE_PSEUDO_PROG;

assoc_list_t filetypes;
assoc_list_t xfiletypes;
assoc_list_t fileviewers;

/* Unit setup. */

/* Configures external functions for filetype unit.  If ece_func is NULL or this
 * function is not called, the module acts like all commands exist. */
void ft_init(external_command_exists_t ece_func);

/* Resets associations set by :filetype, :filextype and :fileviewer commands.
 * Also registers default file associations. */
void ft_reset(int in_x);

/* Programs. */

/* Gets default program that can be used to handle the file.  Returns command
 * on success, otherwise NULL is returned. */
const char * ft_get_program(const char file[]);

/* Gets list of programs associated with specified file name.  Returns the list.
 * Caller should free the result by calling ft_assoc_records_free() on it. */
assoc_records_t ft_get_all_programs(const char file[]);

/* Associates list of comma separated patterns with each item in the list of
 * comma separated programs either for X or non-X associations and depending on
 * current execution environment. */
void ft_set_programs(const char patterns[], int globs, const char programs[],
		int for_x, int in_x);

/* Viewers. */

/* Gets viewer for file.  Returns NULL if no suitable viewer available,
 * otherwise returns pointer to string stored internally. */
const char * ft_get_viewer(const char file[]);

/* Gets list of programs associated with specified file name.  Returns the list.
 * Caller should free the result by calling ft_assoc_records_free() on it. */
assoc_records_t ft_get_all_viewers(const char file[]);

/* Associates list of comma separated patterns with each item in the list of
 * comma separated viewers. */
void ft_set_viewers(const char patterns[], int globs, const char viewers[]);

/* Records managing. */

void ft_assoc_record_add(assoc_records_t *assocs, const char *command,
		const char *description);

void ft_assoc_record_add_all(assoc_records_t *assocs,
		const assoc_records_t *src);

/* After this call the structure contains NULL values. */
void ft_assoc_records_free(assoc_records_t *records);

TSTATIC_DEFS(
	void replace_double_comma(char cmd[], int put_null);
)

#endif /* VIFM__FILETYPE_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
