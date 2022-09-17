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

#define VIFM_PSEUDO_CMD "vifm"

struct matchers_t;

/* Type of file association by its source. */
typedef enum
{
	ART_BUILTIN, /* Builtin type of association, set automatically by vifm. */
	ART_CUSTOM,  /* Custom type of association, which is set by user. */
}
AssocRecordType;

/* Type of viewers. */
typedef enum
{
	VK_TEXTUAL,      /* Regular text preview. */
	VK_GRAPHICAL,    /* Graphics by external means. */
	VK_PASS_THROUGH, /* Graphics by directly influencing terminal state. */
}
ViewerKind;

typedef struct
{
	char *command;
	char *description;
	AssocRecordType type;
}
assoc_record_t;

/* It's guarantied that for existent association there is at least one element
 * in the records list */
typedef struct
{
	assoc_record_t *list;
	int count;
}
assoc_records_t;

/* Single file association entry. */
typedef struct
{
	struct matchers_t *matchers; /* Matching mechanism. */
	assoc_records_t records;     /* Associated programs. */
}
assoc_t;

typedef struct
{
	assoc_t *list;
	int count;
}
assoc_list_t;

/* Prototype for external function that checks existence of a command by its
 * name.  Should return non-zero if it exists and zero otherwise. */
typedef int (*external_command_exists_t)(const char name[]);

/* Predefined fake builtin command. */
extern const assoc_record_t NONE_PSEUDO_PROG;

/* All registered non-X filetypes. */
extern assoc_list_t filetypes;
/* All registered X filetypes. */
extern assoc_list_t xfiletypes;
/* All registered viewers. */
extern assoc_list_t fileviewers;

/* Unit setup. */

/* Configures external functions for filetype unit.  If ece_func is NULL or this
 * function is not called, the module acts like all commands exist. */
void ft_init(external_command_exists_t ece_func);

/* Checks whether command exists from the point of view of this unit.  Returns
 * non-zero if so, otherwise zero is returned. */
int ft_exists(const char cmd[]);

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
 * current execution environment.  Takes over ownership of the matchers. */
void ft_set_programs(struct matchers_t *matchers, const char programs[],
		int for_x, int in_x);

/* Viewers. */

/* Gets viewer for file.  Returns NULL if no suitable viewer available,
 * otherwise returns pointer to string stored internally. */
const char * ft_get_viewer(const char file[]);

struct strlist_t;

/* Gets all existing viewers for file.  Returns the list, which can be empty. */
struct strlist_t ft_get_viewers(const char file[]);

/* Gets list of programs associated with specified file name.  Returns the list.
 * Caller should free the result by calling ft_assoc_records_free() on it. */
assoc_records_t ft_get_all_viewers(const char file[]);

/* Associates list of comma separated patterns with each item in the list of
 * comma separated viewers. */
void ft_set_viewers(struct matchers_t *matchers, const char viewers[]);

/* Guesses kind of viewer from the invocation command.  The parameter can be
 * empty or NULL in which case textual kind is implied.  Returns the kind. */
ViewerKind ft_viewer_kind(const char viewer[]);

/* Records managing. */

/* Checks that given pair of pattern and command exists in specified list of
 * associations.  Returns non-zero if so, otherwise zero is returned. */
int ft_assoc_exists(const assoc_list_t *assocs, const char pattern[],
		const char cmd[]);

void ft_assoc_record_add(assoc_records_t *assocs, const char *command,
		const char *description);

void ft_assoc_record_add_all(assoc_records_t *assocs,
		const assoc_records_t *src);

/* After this call the structure contains NULL values. */
void ft_assoc_records_free(assoc_records_t *records);

#endif /* VIFM__FILETYPE_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
