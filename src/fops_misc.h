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

#ifndef VIFM__FOPS_MISC_H__
#define VIFM__FOPS_MISC_H__

#include <sys/types.h> /* gid_t uid_t */

#include <stdint.h> /* uint64_t */

#include "utils/test_helpers.h"

struct view_t;

/* Removes marked files (optionally into trash directory) of the view to
 * specified register.  Returns zero on failure and non-zero otherwise, in which
 * case status bar message is also printed. */
int fops_delete(struct view_t *view, int reg, int use_trash);

/* Removes current entry of the view.  Non-zero nested flag means that this is
 * not a standalone operation and is surrounded by other file operations.
 * Returns new value for save_msg flag. */
int fops_delete_current(struct view_t *view, int use_trash, int nested);

/* Initiates removal of marked files (optionally into trash directory) of the
 * view to specified register.  Returns new value for save_msg flag. */
int fops_delete_bg(struct view_t *view, int use_trash);

/* Yanks marked files of the view into register specified by its name via reg
 * parameter.  Returns new value for save_msg. */
int fops_yank(struct view_t *view, int reg);

/* Changes target of symbolic links interactively.  When marking is present,
 * link targets are edited in external editor, otherwise new value for current
 * file is prompted.  Returns new value for save_msg. */
int fops_retarget(struct view_t *view);

/* Clones marked files in the view.  Returns new value for save_msg flag. */
int fops_clone(struct view_t *view, char *list[], int nlines, int force,
		int copies);

/* Creates directories, possibly including intermediate ones.  Can modify
 * strings in the names array.  at specifies index of entry to be used to obtain
 * destination path, -1 means current position.  Returns new value for save_msg
 * flag. */
int fops_mkdirs(struct view_t *view, int at, char *names[], int count,
		int create_parent);

/* Creates files.  at specifies index of entry to be used to obtain destination
 * path, -1 means current position.  Returns new value for save_msg flag. */
int fops_mkfiles(struct view_t *view, int at, char *names[], int count);

/* Returns new value for save_msg flag. */
int fops_restore(struct view_t *view);

/* Initiates background calculation of directory sizes.  Forcing disables using
 * previously cached values. */
void fops_size_bg(struct view_t *view, int force);

struct cancellation_t;

/* Calculates size of a directory specified by path possibly using cache of
 * known sizes.  Forcing disables using previously cached values.  Returns size
 * of a directory or zero on error. */
uint64_t fops_dir_size(const char path[], int force,
		const struct cancellation_t *cancellation);

#ifndef _WIN32

/* Sets uid and or gid for marked files.  Non-zero u enables setting of uid,
 * non-zero g of gid.  Returns new value for save_msg flag. */
int fops_chown(int u, int g, uid_t uid, gid_t gid);

/* Changes owner of selected or current file interactively. */
void fops_chuser(void);

/* Changes group of selected or current file interactively. */
void fops_chgroup(void);

#endif

TSTATIC_DEFS(
	const char * gen_clone_name(const char dir[], const char normal_name[]);
)

#endif /* VIFM__FOPS_MISC_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
