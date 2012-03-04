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

#ifndef __FILETYPE_H__
#define __FILETYPE_H__

#define VIFM_PREUDO_CMD "vifm"

typedef struct
{
	char *com;
	char *description;
}
assoc_prog_t;

/* It's guarantied that for existent association there is at least on element in
 * the programs list */
typedef struct
{
	assoc_prog_t *list;
	int count;
}
assoc_progs_t;

typedef struct
{
	char *pattern;
	assoc_progs_t programs;
}
assoc_t;

assoc_t *filetypes;
assoc_t *xfiletypes;
assoc_t *fileviewers;

/* Returns non-zero on success. */
int get_default_program_for_file(const char *file, assoc_prog_t *result);
char * get_viewer_for_file(char *file);
void set_programs(const char *patterns, const char *programs,
		const char *description, int x);
void set_fileviewer(const char *patterns, const char *viewer);
/* Caller should free only the array, but not its elements. */
assoc_progs_t get_all_programs_for_file(const char *file);
/* Resets associations set by :filetype, :filextype and :fileviewer commands. */
void reset_all_file_associations(void);
void replace_double_comma(char *cmd, int put_null);
/* After this call structure contains NULL values */
void free_assoc_prog(assoc_prog_t *assoc_prog);
void add_assoc_prog(assoc_progs_t *assocs, const char *program,
		const char *description);
/* Returns non-zero for an empty assoc_prog_t structure. */
int assoc_prog_is_empty(const assoc_prog_t *assoc_prog);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
