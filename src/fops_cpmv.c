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

#include "fops_cpmv.h"

#include <assert.h> /* assert() */
#include <string.h> /* strcmp() strdup() */

#include "compat/reallocarray.h"
#include "modes/dialogs/msg_dialog.h"
#include "ui/cancellation.h"
#include "ui/fileview.h"
#include "ui/statusbar.h"
#include "utils/fs.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "filelist.h"
#include "flist_pos.h"
#include "fops_common.h"
#include "fops_misc.h"
#include "ops.h"
#include "trash.h"
#include "undo.h"

/* Arguments pack for fops_query_list() verification function. */
typedef struct
{
	view_t *view;         /* Source view. */
	const char *dst_path; /* Destination directory. */
	int force;            /* Whether name conflicts should be ignored. */
	CopyMoveLikeOp op;    /* Operation. */
}
verify_args_t;

static int cp_file(const char src_dir[], const char dst_dir[], const char src[],
		const char dst[], CopyMoveLikeOp op, int cancellable, ops_t *ops,
		int force);
static int is_erroneous(view_t *view, const char dst_dir[], int force);
static int cpmv_prepare(view_t *view, char ***list, int *nlines,
		CopyMoveLikeOp op, int ignore_conflicts, char undo_msg[],
		size_t undo_msg_len, char dst_path[], size_t dst_path_len, int *from_file);
static int verify_list(char *files[], int nfiles, char *names[], int nnames,
		char **error, void *data);
static int check_for_clashes(verify_args_t *args, char *list[], char *marked[],
		int nlines, char **error);
static const char * cmlo_to_str(CopyMoveLikeOp op);
static void cpmv_files_in_bg(bg_op_t *bg_op, void *arg);
static void cpmv_file_in_bg(ops_t *ops, const char src[], const char dst[],
		int move, int force, int skip, int from_trash, const char dst_dir[]);
static int cp_file_f(const char src[], const char dst[], CopyMoveLikeOp op,
		int bg, int cancellable, ops_t *ops, int force);

int
fops_cpmv(view_t *view, char *list[], int nlines, CopyMoveLikeOp op, int flags)
{
	const int force = (flags & CMLF_FORCE);
	const int skip = (flags & CMLF_SKIP);

	int err;
	int nmarked_files;
	int custom_fnames;
	int i;
	char undo_msg[COMMAND_GROUP_INFO_LEN + 1];
	dir_entry_t *entry;
	char dst_dir[PATH_MAX + 1];
	int from_file;
	ops_t *ops;

	if((op == CMLO_LINK_REL || op == CMLO_LINK_ABS) && !symlinks_available())
	{
		show_error_msg("Symbolic Links Error",
				"Your OS doesn't support symbolic links");
		return 0;
	}

	const int ignore_conflicts = (force || skip);
	err = cpmv_prepare(view, &list, &nlines, op, ignore_conflicts, undo_msg,
			sizeof(undo_msg), dst_dir, sizeof(dst_dir), &from_file);
	if(err != 0)
	{
		return err > 0;
	}

	if(is_erroneous(view, dst_dir, force))
	{
		return 0;
	}

	switch(op)
	{
		case CMLO_COPY:
			ops = fops_get_ops(OP_COPY, "Copying", flist_get_dir(view), dst_dir);
			break;
		case CMLO_MOVE:
			ops = fops_get_ops(OP_MOVE, "Moving", flist_get_dir(view), dst_dir);
			break;
		case CMLO_LINK_REL:
		case CMLO_LINK_ABS:
			ops = fops_get_ops(OP_SYMLINK, "Linking", flist_get_dir(view), dst_dir);
			break;

		default:
			assert(0 && "Unexpected operation type.");
			return 0;
	}

	nmarked_files = fops_enqueue_marked_files(ops, view, dst_dir, 0);

	un_group_open(undo_msg);
	i = 0;
	entry = NULL;
	custom_fnames = (nlines > 0);
	while(iter_marked_entries(view, &entry) && !ui_cancellation_requested())
	{
		/* Must be at this level as dst might point into this buffer. */
		char src_full[PATH_MAX + 1];

		char dst_full[PATH_MAX + 8];
		const char *dst = (custom_fnames ? list[i] : entry->name);
		int err, from_trash;
		int skipped = 0;

		get_full_path_of(entry, sizeof(src_full), src_full);
		from_trash = trash_has_path(src_full);

		if(from_trash && !custom_fnames)
		{
			snprintf(src_full, sizeof(src_full), "%s/%s", entry->origin, dst);
			chosp(src_full);
			dst = trash_get_real_name_of(src_full);
		}

		snprintf(dst_full, sizeof(dst_full), "%s/%s", dst_dir, dst);
		if(path_exists(dst_full, NODEREF))
		{
			if(skip)
			{
				skipped = 1;
			}
			else if(force && !from_trash)
			{
				(void)perform_operation(OP_REMOVESL, NULL, NULL, dst_full, NULL);
			}
		}

		if(op == CMLO_COPY)
		{
			fops_progress_msg("Copying files", i, nmarked_files);
		}
		else if(op == CMLO_MOVE)
		{
			fops_progress_msg("Moving files", i, nmarked_files);
		}

		if(skipped)
		{
			/* Nothing to do for this path. */
			err = 0;
		}
		else if(op == CMLO_MOVE)
		{
			err = fops_mv_file(entry->name, entry->origin, dst, dst_dir, OP_MOVE, 1,
					ops);
			if(err != 0)
			{
				view->list_pos = fpos_find_by_name(view, entry->name);
			}
		}
		else
		{
			err = cp_file(entry->origin, dst_dir, entry->name, dst, op, 1, ops,
					force);
		}

		ops_advance(ops, err == 0);

		++i;
	}
	un_group_close();

	if(flist_custom_active(view) || pane_in_dir(view, dst_dir))
	{
		ui_view_schedule_reload(view);
	}
	ui_view_schedule_reload(view == curr_view ? other_view : curr_view);

	if(from_file)
	{
		free_string_array(list, nlines);
	}

	ui_sb_msgf("%d file%s successfully processed%s", ops->succeeded,
			(ops->succeeded == 1) ? "" : "s", fops_get_cancellation_suffix());

	fops_free_ops(ops);

	return 1;
}

void
fops_replace(view_t *view, const char dst[], int force)
{
	char undo_msg[2*PATH_MAX + 32];
	dir_entry_t *entry;
	char dst_dir[PATH_MAX + 1];
	char src_full[PATH_MAX + 1];
	const char *const fname = get_last_path_component(dst);
	ops_t *ops;
	void *cp = (void *)(size_t)1;
	view_t *const other = (view == curr_view) ? other_view : curr_view;

	copy_str(dst_dir, sizeof(dst_dir), dst);
	remove_last_path_component(dst_dir);

	entry = &view->dir_entry[view->list_pos];
	get_full_path_of(entry, sizeof(src_full), src_full);

	if(paths_are_same(src_full, dst))
	{
		/* Nothing to do if destination and source are the same file. */
		return;
	}

	ops = fops_get_ops(OP_COPY, "Copying", flist_get_dir(view), dst_dir);

	snprintf(undo_msg, sizeof(undo_msg), "Copying %s to %s",
			replace_home_part(src_full), dst_dir);

	un_group_open(undo_msg);

	if(path_exists(dst, NODEREF) && force)
	{
		(void)fops_delete_current(other, 1, 1);
	}

	fops_progress_msg("Copying files", 0, 1);

	if(!ui_cancellation_requested() && !is_valid_dir(dst_dir) &&
			perform_operation(OP_MKDIR, NULL, cp, dst_dir, NULL) == OPS_SUCCEEDED)
	{
		un_group_add_op(OP_MKDIR, cp, NULL, dst_dir, "");
	}

	if(!ui_cancellation_requested())
	{
		(void)cp_file(entry->origin, dst_dir, entry->name, fname, CMLO_COPY, 1, ops,
				1);
	}

	un_group_close();

	ui_view_schedule_reload(other);

	fops_free_ops(ops);
}

/* Adapter for cp_file_f() that accepts paths broken into directory/file
 * parts. */
static int
cp_file(const char src_dir[], const char dst_dir[], const char src[],
		const char dst[], CopyMoveLikeOp op, int cancellable, ops_t *ops, int force)
{
	char full_src[PATH_MAX + 1], full_dst[PATH_MAX + 1];

	to_canonic_path(src, src_dir, full_src, sizeof(full_src));
	to_canonic_path(dst, dst_dir, full_dst, sizeof(full_dst));

	return cp_file_f(full_src, full_dst, op, 0, cancellable, ops, force);
}

int
fops_cpmv_bg(view_t *view, char *list[], int nlines, int move, int flags)
{
	const int force = (flags & CMLF_FORCE);
	const int skip = (flags & CMLF_SKIP);

	int err;
	size_t i;
	char task_desc[COMMAND_GROUP_INFO_LEN];

	bg_args_t *args = calloc(1, sizeof(*args));
	if(args == NULL)
	{
		show_error_msg("Can't perform operation", "Out of memory");
		return 0;
	}

	args->nlines = nlines;
	args->move = move;
	args->force = force;
	args->skip = skip;

	const int ignore_conflicts = (force || skip);
	err = cpmv_prepare(view, &list, &args->nlines, move ? CMLO_MOVE : CMLO_COPY,
			ignore_conflicts, task_desc, sizeof(task_desc), args->path,
			sizeof(args->path), &args->from_file);
	if(err != 0)
	{
		fops_free_bg_args(args);
		return err > 0;
	}

	if(is_erroneous(view, args->path, force))
	{
		fops_free_bg_args(args);
		return 0;
	}

	args->list = args->from_file ? list :
	             args->nlines > 0 ? copy_string_array(list, nlines) : NULL;

	fops_prepare_for_bg_task(view, args);

	args->is_in_trash = malloc(args->sel_list_len);
	for(i = 0U; i < args->sel_list_len; ++i)
	{
		args->is_in_trash[i] = trash_has_path(args->sel_list[i]);
	}

	if(args->list == NULL)
	{
		int i;
		args->nlines = args->sel_list_len;
		args->list = reallocarray(NULL, args->nlines, sizeof(*args->list));
		for(i = 0; i < args->nlines; ++i)
		{
			args->list[i] =
				strdup(fops_get_dst_name(args->sel_list[i], args->is_in_trash[i]));
		}
	}

	args->ops = fops_get_bg_ops(move ? OP_MOVE : OP_COPY,
			move ? "moving" : "copying", args->path);

	if(bg_execute(task_desc, "...", args->sel_list_len, 1, &cpmv_files_in_bg,
				args) != 0)
	{
		fops_free_bg_args(args);

		show_error_msg("Can't process files",
				"Failed to initiate background operation");
	}

	return 0;
}

/* Checks operation for adequacy.  Displays error message in case of issues.
 * Returns non-zero if operation must be aborted, otherwise zero is returned. */
static int
is_erroneous(view_t *view, const char dst_dir[], int force)
{
	const int same_dir = pane_in_dir(view, dst_dir);
	if(same_dir && force)
	{
		show_error_msg("Operation Aborted",
				"Forcing overwrite when destination and source is the same directory "
				"will lead to losing data.");
		return 1;
	}
	return 0;
}

/* Performs general preparations for file copy/move-like operations: resolving
 * destination path, validating names, checking for conflicts, formatting undo
 * message.  Returns zero on success, otherwise positive number for status bar
 * message and negative number for other errors. */
static int
cpmv_prepare(view_t *view, char ***list, int *nlines, CopyMoveLikeOp op,
		int ignore_conflicts, char undo_msg[], size_t undo_msg_len, char dst_path[],
		size_t dst_path_len, int *from_file)
{
	view_t *const other = (view == curr_view) ? other_view : curr_view;

	if(op == CMLO_MOVE)
	{
		if(!fops_view_can_be_changed(view))
		{
			return -1;
		}
	}
	else if(op == CMLO_COPY && !fops_can_read_marked_files(view))
	{
		return -1;
	}

	if(*nlines == 1)
	{
		if(fops_check_dir_path(other, (*list)[0], dst_path, dst_path_len))
		{
			*nlines = 0;
		}
	}
	else
	{
		copy_str(dst_path, dst_path_len, fops_get_dst_dir(other, -1));
	}

	if(!fops_view_can_be_extended(other, -1) ||
			!fops_is_dir_writable(DR_DESTINATION, dst_path))
	{
		return -1;
	}

	size_t nmarked;
	char **marked = fops_grab_marked_files(view, &nmarked);

	/* Custom views can contain several files with the same name. */
	if(*nlines == 0 && flist_custom_active(view))
	{
		size_t i;
		for(i = 0U; i < nmarked; ++i)
		{
			if(is_in_string_array(marked, i, marked[i]))
			{
				ui_sb_errf("Source name \"%s\" duplicates", marked[i]);
				free_string_array(marked, nmarked);
				return 1;
			}
		}
	}

	verify_args_t verify_args = {
		.view = view,
		.dst_path = dst_path,
		.force = ignore_conflicts,
		.op = op,
	};

	*from_file = (*nlines < 0);
	if(*from_file)
	{
		*list = fops_query_list(nmarked, marked, nlines, 1, &verify_list,
				&verify_args);
		if(*list == NULL)
		{
			free_string_array(marked, nmarked);
			return -1;
		}
	}

	char *error_str = NULL;
	int error = !verify_list(marked, nmarked, *list, *nlines, &error_str,
			&verify_args);

	free_string_array(marked, nmarked);

	if(error)
	{
		if(error_str != NULL)
		{
			ui_sb_err(error_str);
			free(error_str);
		}

		redraw_view(view);
		if(*from_file)
		{
			free_string_array(*list, *nlines);
		}
		return 1;
	}

	snprintf(undo_msg, undo_msg_len, "%s from %s to ", cmlo_to_str(op),
			replace_home_part(flist_get_dir(view)));
	snprintf(undo_msg + strlen(undo_msg), undo_msg_len - strlen(undo_msg),
			"%s: ", replace_home_part(dst_path));
	fops_append_marked_files(view, undo_msg, (*nlines > 0) ? *list : NULL);

	return 0;
}

/* Checks that operation can be performed.  Returns non-zero if so, otherwise
 * zero is returned along with setting *error. */
static int
verify_list(char *files[], int nfiles, char *names[], int nnames, char **error,
		void *data)
{
	verify_args_t *args = data;

	int ok = 1;

	if(nnames > 0 &&
			(!fops_is_name_list_ok(nfiles, nnames, names, NULL, error) ||
			!fops_is_copy_list_ok(args->dst_path, nnames, names, args->force, error)))
	{
		ok = 0;
	}
	else if(nnames == 0 &&
			!fops_is_copy_list_ok(args->dst_path, nfiles, files, args->force, error))
	{
		ok = 0;
	}

	if(ok && check_for_clashes(args, names, files, nnames, error) != 0)
	{
		ok = 0;
	}

	return ok;
}

/* Checks whether operation is OK from the point of view of losing files due to
 * tree clashes (child move over parent or vice versa).  Reallocates *error to
 * provide error message.  Returns zero if everything is fine, otherwise
 * non-zero is returned. */
static int
check_for_clashes(verify_args_t *args, char *list[], char *marked[], int nlines,
		char **error)
{
	const int is_cpmv = ONE_OF(args->op, CMLO_MOVE, CMLO_COPY);

	dir_entry_t *entry = NULL;
	int i = 0;
	while(iter_marked_entries(args->view, &entry))
	{
		char src_full[PATH_MAX + 1], dst_full[PATH_MAX + 1];
		const char *const dst_name = (nlines > 0) ? list[i] : marked[i];
		++i;

		get_full_path_of(entry, sizeof(src_full), src_full);

		snprintf(dst_full, sizeof(dst_full), "%s/%s", args->dst_path, dst_name);
		chosp(dst_full);

		if(is_cpmv && is_in_subtree(dst_full, src_full, 0))
		{
			put_string(error, format_str("Can't move/copy parent inside itself: %s",
						replace_home_part(src_full)));
			return 1;
		}

		if(is_in_subtree(src_full, dst_full, 0))
		{
			put_string(error,
					format_str("Operation would result in losing contents of %s",
						replace_home_part(src_full)));
			return 1;
		}
	}
	return 0;
}

/* Gets string representation of a copy/move-like operation.  Returns the
 * string. */
static const char *
cmlo_to_str(CopyMoveLikeOp op)
{
	switch(op)
	{
		case CMLO_COPY:
			return "copy";
		case CMLO_MOVE:
			return "move";
		case CMLO_LINK_REL:
			return "rlink";
		case CMLO_LINK_ABS:
			return "alink";

		default:
			assert(0 && "Unexpected operation type.");
			return "";
	}
}

/* Entry point for a background task that copies/moves files. */
static void
cpmv_files_in_bg(bg_op_t *bg_op, void *arg)
{
	size_t i;
	bg_args_t *const args = arg;
	ops_t *ops = args->ops;
	fops_bg_ops_init(ops, bg_op);

	if(ops->use_system_calls)
	{
		size_t i;
		bg_op_set_descr(bg_op, "estimating...");
		for(i = 0U; i < args->sel_list_len; ++i)
		{
			const char *const src = args->sel_list[i];
			const char *const dst = args->list[i];
			ops_enqueue(ops, src, dst);
		}
	}

	for(i = 0U; i < args->sel_list_len; ++i)
	{
		const char *const src = args->sel_list[i];
		const char *const dst = args->list[i];
		bg_op_set_descr(bg_op, src);
		cpmv_file_in_bg(ops, src, dst, args->move, args->force, args->skip,
				args->is_in_trash[i], args->path);
		++bg_op->done;
	}

	fops_free_bg_args(args);
}

/* Actual implementation of background file copying/moving. */
static void
cpmv_file_in_bg(ops_t *ops, const char src[], const char dst[], int move,
		int force, int skip, int from_trash, const char dst_dir[])
{
	char dst_full[PATH_MAX + 1];
	snprintf(dst_full, sizeof(dst_full), "%s/%s", dst_dir, dst);
	if(path_exists(dst_full, NODEREF))
	{
		if(skip)
		{
			return;
		}

		if(force && !from_trash)
		{
			(void)perform_operation(OP_REMOVESL, NULL, (void *)1, dst_full, NULL);
		}
	}

	if(move)
	{
		(void)fops_mv_file_f(src, dst_full, OP_MOVE, 1, 1, ops);
	}
	else
	{
		(void)cp_file_f(src, dst_full, CMLO_COPY, 1, 1, ops, 0);
	}
}

/* Copies file from one location to another.  Returns zero on success, otherwise
 * non-zero is returned. */
static int
cp_file_f(const char src[], const char dst[], CopyMoveLikeOp op, int bg,
		int cancellable, ops_t *ops, int force)
{
	if(strcmp(src, dst) == 0)
	{
		return 0;
	}

	OPS file_op;
	char rel_path[PATH_MAX + 1];
	if(op == CMLO_COPY)
	{
		file_op = force ? OP_COPYF : OP_COPY;
	}
	else
	{
		file_op = OP_SYMLINK;

		if(op == CMLO_LINK_REL)
		{
			char dst_dir[PATH_MAX + 1];

			copy_str(dst_dir, sizeof(dst_dir), dst);
			remove_last_path_component(dst_dir);

			copy_str(rel_path, sizeof(rel_path), make_rel_path(src, dst_dir));
			src = rel_path;
		}
	}

	OpsResult result = perform_operation(file_op, ops,
			cancellable ? NULL : (void *)1, src, dst);
	if(result != OPS_SUCCEEDED)
	{
		return 1;
	}

	if(!bg)
	{
		un_group_add_op(file_op, NULL, NULL, src, dst);
	}
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
