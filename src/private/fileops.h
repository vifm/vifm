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

#ifndef VIFM__PRIVATE__FILEOPS_H__
#define VIFM__PRIVATE__FILEOPS_H__

#include "../ui/ui.h"
#include "../fileops.h"
#include "../ops.h"

struct dirent;

const char * get_dst_dir(const FileView *view, int at);
int can_add_files_to_view(const FileView *view, int at);
void free_ops(ops_t *ops);
ops_t * get_ops(OPS main_op, const char descr[], const char base_dir[],
		const char target_dir[]);
const char * get_dst_name(const char src_path[], int from_trash);
const char * get_cancellation_suffix(void);
void progress_msg(const char text[], int ready, int total);
int is_dir_entry(const char full_path[], const struct dirent* dentry);

extern line_prompt_func line_prompt;
extern options_prompt_func options_prompt;

#endif /* VIFM__PRIVATE__FILEOPS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
