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

#include "fops_put.h"

#include <assert.h> /* assert() */
#include <ctype.h> /* tolower() */
#include <string.h> /* memmove() memset() strdup() */

#include "cfg/config.h"
#include "compat/os.h"
#include "compat/reallocarray.h"
#include "engine/text_buffer.h"
#include "modes/dialogs/msg_dialog.h"
#include "modes/modes.h"
#include "modes/wk.h"
#include "ui/cancellation.h"
#include "ui/statusbar.h"
#include "utils/fs.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/test_helpers.h"
#include "utils/utils.h"
#include "background.h"
#include "filelist.h"
#include "flist_pos.h"
#include "fops_common.h"
#include "fops_cpmv.h"
#include "ops.h"
#include "registers.h"
#include "trash.h"
#include "undo.h"

static void put_files_in_bg(bg_op_t *bg_op, void *arg);
static int initiate_put_files(view_t *view, int at, CopyMoveLikeOp op,
		const char descr[], int reg_name);
static void reset_put_confirm(CopyMoveLikeOp main_op, const char descr[],
		const char dst_dir[]);
static OPS cmlo_to_op(CopyMoveLikeOp op);
static int path_depth_sort(const void *one, const void *two);
static int is_dir_clash(const char src_path[], const char dst_dir[]);
static int put_files_i(view_t *view, int start);
static void update_cursor_position(view_t *view);
static int put_next(int force);
static int unprocessed_dirs_present(void);
TSTATIC int merge_dirs(const char src[], const char dst[], ops_t *ops);
static int handle_clashing(int move, const char src[], const char dst[]);
static void prompt_what_to_do(const char fname[], const char caused_by[]);
static void handle_prompt_response(const char fname[], const char caused_by[],
		char response);
static void prompt_dst_name(const char src_name[]);
static void prompt_dst_name_cb(const char dst_name[]);
static void put_continue(int force);
static void show_difference(const char fname[], const char caused_by[]);
static char * compare_files(const char dst_path[], const char src_path[],
		struct stat *dst, struct stat *src);

/* Global state for file putting and name conflicts resolution that happen in
 * the process. */
static struct
{
	reg_t *reg;          /* Register used for the operation. */
	int *file_order;     /* Defines custom ordering of files in register. */
	view_t *view;        /* View in which operation takes place. */
	CopyMoveLikeOp op;   /* Type of current operation. */
	int index;           /* Index of the next file of the register to process. */
	int processed;       /* Number of successfully processed files. */
	int skip_all;        /* Skip all conflicting files/directories. */
	int overwrite_all;   /* Overwrite all future conflicting files/directories. */
	int append;          /* Whether we're appending ending of a file or not. */
	int allow_merge;     /* Allow merging of files in directories. */
	int allow_merge_all; /* Allow merging of files in directories by default. */
	int merge;           /* Merge conflicting directory once. */
	int merge_all;       /* Merge all conflicting directories. */
	ops_t *ops;          /* Currently running operation. */
	char *dst_name;      /* Name of destination file. */
	char *dst_dir;       /* Destination path. */
	strlist_t put;       /* List of files that were successfully put. */
	char *last_conflict; /* Path to the file of the last conflict. */
}
put_confirm;

int
fops_put(view_t *view, int at, int reg_name, int move)
{
	const CopyMoveLikeOp op = move ? CMLO_MOVE : CMLO_COPY;
	const char *const descr = move ? "Putting" : "putting";
	return initiate_put_files(view, at, op, descr, reg_name);
}

int
fops_put_bg(view_t *view, int at, int reg_name, int move)
{
	char task_desc[COMMAND_GROUP_INFO_LEN];
	size_t task_desc_len;
	int i;
	bg_args_t *args;
	reg_t *reg;
	const char *const dst_dir = fops_get_dst_dir(view, at);

	/* Check that operation generally makes sense given our input. */

	if(!fops_view_can_be_extended(view, at))
	{
		return 0;
	}

	regs_sync_from_shared_memory();
	reg = regs_find(tolower(reg_name));
	if(reg == NULL || reg->nfiles < 1)
	{
		ui_sb_err(reg == NULL ? "No such register" : "Register is empty");
		return 1;
	}

	/* Prepare necessary data for background procedure and perform checks to
	 * ensure there will be no conflicts. */

	args = calloc(1, sizeof(*args));
	args->move = move;
	copy_str(args->path, sizeof(args->path), dst_dir);

	snprintf(task_desc, sizeof(task_desc), "%cut in %s: ", move ? 'P' : 'p',
			replace_home_part(dst_dir));
	task_desc_len = strlen(task_desc);
	for(i = 0; i < reg->nfiles; ++i)
	{
		char *const src = reg->files[i];
		const char *dst_name;
		char *dst;
		int j;

		chosp(src);

		if(!path_exists(src, NODEREF))
		{
			/* Skip nonexistent files. */
			continue;
		}

		fops_append_fname(task_desc, task_desc_len, src);
		task_desc_len = strlen(task_desc);

		args->sel_list_len = add_to_string_array(&args->sel_list,
				args->sel_list_len, src);

		dst_name = fops_get_dst_name(src, trash_has_path(src));

		/* Check that no destination files have the same name. */
		for(j = 0; j < args->nlines; ++j)
		{
			if(stroscmp(get_last_path_component(args->list[j]), dst_name) == 0)
			{
				ui_sb_errf("Two destination files have name \"%s\"", dst_name);
				fops_free_bg_args(args);
				return 1;
			}
		}

		dst = join_paths(args->path, dst_name);
		args->nlines = put_into_string_array(&args->list, args->nlines, dst);

		if(!paths_are_equal(src, dst) && path_exists(dst, NODEREF))
		{
			char *escaped_dst = escape_unreadable(dst);
			ui_sb_errf("File \"%s\" already exists", escaped_dst);
			free(escaped_dst);

			fops_free_bg_args(args);
			return 1;
		}
	}

	/* Initiate the operation. */

	args->ops = fops_get_bg_ops((args->move ? OP_MOVE : OP_COPY),
			move ? "Putting" : "putting", args->path);

	if(bg_execute(task_desc, "...", args->sel_list_len, 1, &put_files_in_bg,
				args) != 0)
	{
		fops_free_bg_args(args);

		show_error_msg("Can't put files",
				"Failed to initiate background operation");
	}

	return 0;
}

/* Entry point for background task that puts files. */
static void
put_files_in_bg(bg_op_t *bg_op, void *arg)
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

	for(i = 0U; i < args->sel_list_len; ++i, ++bg_op->done)
	{
		struct stat src_st;
		const char *const src = args->sel_list[i];
		const char *const dst = args->list[i];

		if(paths_are_equal(src, dst))
		{
			/* Just ignore this file. */
			continue;
		}

		if(os_lstat(src, &src_st) != 0)
		{
			/* File isn't there, assume that it's fine and don't error in this
			 * case. */
			continue;
		}

		if(path_exists(dst, NODEREF))
		{
			/* This file wasn't here before (when checking in fops_put_bg()), won't
			 * overwrite. */
			continue;
		}

		bg_op_set_descr(bg_op, src);
		(void)perform_operation(ops->main_op, ops, NULL, src, dst);
	}

	fops_free_bg_args(args);
}

int
fops_put_links(view_t *view, int reg_name, int relative)
{
	const CopyMoveLikeOp op = relative ? CMLO_LINK_REL : CMLO_LINK_ABS;
	return initiate_put_files(view, -1, op, "Symlinking", reg_name);
}

/* Performs preparations necessary for putting files/links.  Returns new value
 * for save_msg flag. */
static int
initiate_put_files(view_t *view, int at, CopyMoveLikeOp op, const char descr[],
		int reg_name)
{
	reg_t *reg;
	int i;
	const char *const dst_dir = fops_get_dst_dir(view, at);

	if(!fops_view_can_be_extended(view, -1))
	{
		return 0;
	}

	regs_sync_from_shared_memory();
	reg = regs_find(tolower(reg_name));
	if(reg == NULL || reg->nfiles < 1)
	{
		ui_sb_err("Register is empty");
		return 1;
	}

	reset_put_confirm(op, descr, dst_dir);

	put_confirm.op = op;
	put_confirm.reg = reg;
	put_confirm.view = view;

	/* Map each element onto itself initially. */
	put_confirm.file_order = reallocarray(NULL, reg->nfiles,
			sizeof(*put_confirm.file_order));
	for(i = 0; i < reg->nfiles; ++i)
	{
		put_confirm.file_order[i] = i;
	}

	/* If clashes are harmful, order files by descending depth in the file system
	 * and then move all that will clash to the tail of the array. */
	if(op == CMLO_MOVE || op == CMLO_COPY)
	{
		safe_qsort(put_confirm.file_order, reg->nfiles,
				sizeof(put_confirm.file_order[0]), &path_depth_sort);

		/* We basically do partition by "clash" predicate moving all files for which
		 * it's true to the tail.  Mind that moved files end up in reverse order,
		 * which is beneficial for us as we can move larger sub-tree and discard
		 * some of other files. */
		int nclashes = 0;
		int i;
		for(i = 0; i < reg->nfiles - nclashes; ++i)
		{
			const int id = put_confirm.file_order[i];
			if(is_dir_clash(reg->files[id], dst_dir))
			{
				/* Rotate unvisited elements of the array in place. */
				memmove(&put_confirm.file_order[i], &put_confirm.file_order[i + 1],
						sizeof(id)*(reg->nfiles - nclashes - (i + 1)));
				put_confirm.file_order[reg->nfiles - 1 - nclashes++] = id;
				--i;
			}
		}
	}

	ui_cancellation_push_on();

	for(i = 0; i < reg->nfiles && !ui_cancellation_requested(); ++i)
	{
		ops_enqueue(put_confirm.ops, reg->files[i], dst_dir);
	}

	ui_cancellation_pop();

	return put_files_i(view, 1);
}

/* Resets state of global put_confirm variable in this module. */
static void
reset_put_confirm(CopyMoveLikeOp main_op, const char descr[],
		const char dst_dir[])
{
	fops_free_ops(put_confirm.ops);
	free(put_confirm.dst_name);
	free(put_confirm.dst_dir);
	free(put_confirm.file_order);
	free_string_array(put_confirm.put.items, put_confirm.put.nitems);
	free(put_confirm.last_conflict);

	memset(&put_confirm, 0, sizeof(put_confirm));

	put_confirm.dst_dir = strdup(dst_dir);
	put_confirm.ops = fops_get_ops(cmlo_to_op(main_op), descr, dst_dir, dst_dir);
}

/* Gets operation kind that corresponds to copy/move-like operation.  Returns
 * the kind. */
static OPS
cmlo_to_op(CopyMoveLikeOp op)
{
	switch(op)
	{
		case CMLO_COPY:
			return OP_COPY;
		case CMLO_MOVE:
			return OP_MOVE;
		case CMLO_LINK_REL:
		case CMLO_LINK_ABS:
			return OP_SYMLINK;

		default:
			assert(0 && "Unexpected operation type.");
			return OP_COPY;
	}
}

/* Compares two entries by the depth of their real paths.  Returns positive
 * value if one is greater than two, zero if they are equal, otherwise negative
 * value is returned. */
static int
path_depth_sort(const void *one, const void *two)
{
	const char *s = put_confirm.reg->files[*(const int *)one];
	const char *t = put_confirm.reg->files[*(const int *)two];

	char s_real[PATH_MAX + 1], t_real[PATH_MAX + 1];

	if(os_realpath(s, s_real) != s_real)
	{
		copy_str(s_real, sizeof(s_real), s);
	}
	if(os_realpath(t, t_real) != t_real)
	{
		copy_str(t_real, sizeof(t_real), t);
	}

	return chars_in_str(t_real, '/') - chars_in_str(s_real, '/');
}

/* Checks whether moving the file into specified directory might potentially
 * result in loss of some other files scheduled for processing (because we
 * overwrite one of its parents).  Returns non-zero if so, otherwise zero is
 * returned. */
static int
is_dir_clash(const char src_path[], const char dst_dir[])
{
	char dst_path[PATH_MAX + 1];

	snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir,
			fops_get_dst_name(src_path, trash_has_path(src_path)));
	chosp(dst_path);

	return is_dir(dst_path);
}

/* Returns new value for save_msg flag. */
static int
put_files_i(view_t *view, int start)
{
	if(start)
	{
		char undo_msg[COMMAND_GROUP_INFO_LEN + 1];
		const char *descr;
		const int from_trash =
			trash_has_path(put_confirm.reg->files[put_confirm.file_order[0]]);

		if(put_confirm.op == CMLO_LINK_ABS)
		{
			descr = "put absolute links";
		}
		else if(put_confirm.op == CMLO_LINK_REL)
		{
			descr = "put relative links";
		}
		else
		{
			descr = (put_confirm.op == CMLO_MOVE || from_trash) ? "Put" : "put";
		}

		snprintf(undo_msg, sizeof(undo_msg), "%s in %s: ", descr,
				replace_home_part(flist_get_dir(view)));
		un_group_open(undo_msg);
		un_group_close();
	}

	if(vifm_chdir(put_confirm.dst_dir) != 0)
	{
		show_error_msg("Directory Return", "Can't chdir() to current directory");
		return 1;
	}

	while(put_confirm.index < put_confirm.reg->nfiles)
	{
		int put_result;

		update_string(&put_confirm.dst_name, NULL);

		put_result = put_next(0);
		if(put_result > 0)
		{
			/* In this case put_next() takes care of interacting with a user. */
			return 0;
		}
		else if(put_result < 0)
		{
			break;
		}
		++put_confirm.index;
	}

	/* Free ops here when we're done, because as a side-effect it also prints
	 * error messages. */
	fops_free_ops(put_confirm.ops);
	put_confirm.ops = NULL;

	regs_pack(put_confirm.reg->name);
	update_cursor_position(view);
	ui_sb_msgf("%d file%s inserted%s", put_confirm.processed,
			(put_confirm.processed == 1) ? "" : "s", fops_get_cancellation_suffix());

	return 1;
}

/* Moves cursor to one of processed or conflicting files, if any. */
static void
update_cursor_position(view_t *view)
{
	if(put_confirm.last_conflict == NULL && put_confirm.put.nitems == 0)
	{
		/* Apparently, nothing has changed. */
		return;
	}

	populate_dir_list(view, 1);
	ui_view_schedule_redraw(view);

	if(put_confirm.last_conflict != NULL)
	{
		dir_entry_t *const entry = entry_from_path(view, view->dir_entry,
				view->list_rows, put_confirm.last_conflict);
		if(entry != NULL)
		{
			fpos_set_pos(view, entry_to_pos(view, entry));
		}
		return;
	}

	int i;
	int new_pos = -1;
	for(i = 0; i < put_confirm.put.nitems; ++i)
	{
		dir_entry_t *const entry = entry_from_path(view, view->dir_entry,
				view->list_rows, put_confirm.put.items[i]);
		if(entry != NULL)
		{
			const int pos = entry_to_pos(view, entry);
			if(new_pos == -1 || pos < new_pos)
			{
				new_pos = pos;
			}
		}
	}
	if(new_pos != -1)
	{
		fpos_set_pos(view, new_pos);
	}
}

/* The force argument enables overwriting/replacing/merging.  Returns 0 on
 * success, otherwise non-zero is returned where value greater then zero means
 * error already reported to the user, while negative one means unreported
 * error. */
static int
put_next(int force)
{
	char *filename;
	const char *dst_name;
	const char *const dst_dir = put_confirm.dst_dir;
	struct stat src_st;
	char src_buf[PATH_MAX + 1], dst_buf[PATH_MAX + 1];
	int from_trash;
	int op;
	int move;
	int success;
	int merge;
	int safe_operation = 0;

	/* TODO: refactor this function (put_next()) */

	if(ui_cancellation_requested())
	{
		return -1;
	}

	force = force || put_confirm.overwrite_all || put_confirm.merge_all;
	merge = put_confirm.merge || put_confirm.merge_all;

	filename = put_confirm.reg->files[put_confirm.file_order[put_confirm.index]];
	if(filename == NULL)
	{
		/* This file has been excluded from processing. */
		return 0;
	}

	chosp(filename);
	if(os_lstat(filename, &src_st) != 0)
	{
		/* File isn't there, assume that it's fine and don't error in this case. */
		return 0;
	}

	from_trash = trash_has_path(filename);
	move = from_trash || put_confirm.op == CMLO_MOVE;

	copy_str(src_buf, sizeof(src_buf), filename);

	dst_name = put_confirm.dst_name;
	if(dst_name == NULL)
	{
		dst_name = fops_get_dst_name(src_buf, from_trash);
	}

	snprintf(dst_buf, sizeof(dst_buf), "%s/%s", dst_dir, dst_name);
	chosp(dst_buf);

	if(!put_confirm.append && path_exists(dst_buf, NODEREF))
	{
		if(force)
		{
			struct stat dst_st;

			if(paths_are_equal(src_buf, dst_buf))
			{
				/* Skip if destination matches source. */
				return 0;
			}

			if(os_lstat(dst_buf, &dst_st) == 0 && (!merge ||
					S_ISDIR(dst_st.st_mode) != S_ISDIR(src_st.st_mode)))
			{
				if(S_ISDIR(dst_st.st_mode))
				{
					if(handle_clashing(move, src_buf, dst_buf))
					{
						return 1;
					}

					if(is_in_subtree(src_buf, dst_buf, 0))
					{
						/* Don't delete /a/b before moving /a/b/c to /a/b. */
						safe_operation = 1;
					}
				}

				if(!safe_operation && perform_operation(OP_REMOVESL, put_confirm.ops,
							NULL, dst_buf, NULL) != OPS_SUCCEEDED)
				{
					show_error_msgf("Can't replace a file",
							"Failed to remove a file:\n%s", dst_buf);
					return 0;
				}
				/* Schedule view update to reflect changes in UI. */
				ui_view_schedule_reload(put_confirm.view);
			}
			else if(!cfg.use_system_calls && get_env_type() == ET_UNIX)
			{
				remove_last_path_component(dst_buf);
			}
		}
		else if(put_confirm.skip_all)
		{
			return 0;
		}
		else if(put_confirm.op == CMLO_COPY || put_confirm.op == CMLO_MOVE)
		{
			struct stat dst_st;
			put_confirm.allow_merge = S_ISDIR(src_st.st_mode)
			                       && (os_lstat(dst_buf, &dst_st) == 0)
			                       && S_ISDIR(dst_st.st_mode);

			/* Enable "merge all" in conflict resolution options if at least one of
			 * source paths left to process (including current) is a directory. */
			put_confirm.allow_merge_all = unprocessed_dirs_present();

			prompt_what_to_do(dst_name, src_buf);
			return 1;
		}
		else
		{
			/* Merging makes sense only for copy and move. */
			put_confirm.allow_merge = 0;
			put_confirm.allow_merge_all = 0;
			prompt_what_to_do(dst_name, src_buf);
			return 1;
		}
	}

	if(put_confirm.op == CMLO_LINK_REL || put_confirm.op == CMLO_LINK_ABS)
	{
		op = OP_SYMLINK;
		if(put_confirm.op == CMLO_LINK_REL)
		{
			copy_str(src_buf, sizeof(src_buf), make_rel_path(filename, dst_dir));
		}
	}
	else if(put_confirm.append)
	{
		op = move ? OP_MOVEA : OP_COPYA;
		put_confirm.append = 0;
	}
	else if(move)
	{
		op = merge ? OP_MOVEF : OP_MOVE;
	}
	else
	{
		op = merge ? OP_COPYF : OP_COPY;
	}

	fops_progress_msg("Putting files", put_confirm.index,
			put_confirm.reg->nfiles);

	/* Merging directory on move requires special handling as it can't be done by
	 * move operation itself. */
	if(move && merge)
	{
		char dst_path[PATH_MAX + 1];

		success = 1;

		un_group_reopen_last();

		snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, dst_name);

		if(merge_dirs(src_buf, dst_path, put_confirm.ops) != 0)
		{
			success = 0;
		}

		un_group_close();
	}
	else if(safe_operation)
	{
		const char *const unique_dst = make_name_unique(dst_buf);

		/* An optimization: if we're going to remove destination anyway, don't
		 * bother copying it, just move. */
		if(op == OP_COPY)
		{
			op = OP_MOVE;
		}

		success = (perform_operation(op, put_confirm.ops, NULL, src_buf,
					unique_dst) == OPS_SUCCEEDED);
		if(success)
		{
			success = (perform_operation(OP_REMOVESL, put_confirm.ops, NULL, dst_buf,
						NULL) == OPS_SUCCEEDED);
		}
		if(success)
		{
			success = (perform_operation(OP_MOVE, put_confirm.ops, NULL, unique_dst,
						dst_buf) == OPS_SUCCEEDED);
		}
	}
	else
	{
		success = (perform_operation(op, put_confirm.ops, NULL, src_buf,
					dst_buf) == OPS_SUCCEEDED);
	}

	ops_advance(put_confirm.ops, success);

	if(success)
	{
		char *msg, *p;
		size_t len;

		/* For some reason "mv" sometimes returns 0 on cancellation. */
		if(!path_exists(dst_buf, NODEREF))
		{
			return -1;
		}

		un_group_reopen_last();

		msg = un_replace_group_msg(NULL);
		len = strlen(msg);
		p = realloc(msg, COMMAND_GROUP_INFO_LEN);
		if(p == NULL)
			len = COMMAND_GROUP_INFO_LEN;
		else
			msg = p;

		snprintf(msg + len, COMMAND_GROUP_INFO_LEN - len, "%s%s",
				(msg[len - 2] != ':') ? ", " : "", dst_name);
		un_replace_group_msg(msg);
		free(msg);

		if(!(move && merge))
		{
			un_group_add_op(op, NULL, NULL, src_buf, dst_buf);
		}

		un_group_close();
		++put_confirm.processed;
		if(move)
		{
			update_string(
					&put_confirm.reg->files[put_confirm.file_order[put_confirm.index]],
					NULL);
		}

		char dst_path[PATH_MAX + 1];
		snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, dst_name);
		put_confirm.put.nitems = add_to_string_array(&put_confirm.put.items,
				put_confirm.put.nitems, dst_path);
	}

	return 0;
}

/* Checks whether current or any other unprocessed paths point to directories.
 * Returns non-zero if so, otherwise zero is returned. */
static int
unprocessed_dirs_present(void)
{
	int i;
	for(i = put_confirm.index; i < put_confirm.reg->nfiles; ++i)
	{
		const char *path = put_confirm.reg->files[put_confirm.file_order[i]];
		if(path == NULL)
		{
			/* This file has been excluded from processing. */
			continue;
		}

		struct stat src_st;
		if(os_lstat(path, &src_st) == 0 && S_ISDIR(src_st.st_mode))
		{
			return 1;
		}
	}
	return 0;
}

/* Merges src into dst.  Returns zero on success, otherwise non-zero is
 * returned. */
TSTATIC int
merge_dirs(const char src[], const char dst[], ops_t *ops)
{
	struct stat st;
	DIR *dir;
	struct dirent *d;

	if(os_stat(src, &st) != 0)
	{
		return -1;
	}

	dir = os_opendir(src);
	if(dir == NULL)
	{
		return -1;
	}

	/* Make sure target directory exists.  Ignore error as we don't care whether
	 * it existed before we try to create it and following operations will fail
	 * if we can't create this directory for some reason. */
	(void)perform_operation(OP_MKDIR, NULL, (void *)(size_t)1, dst, NULL);

	while((d = os_readdir(dir)) != NULL)
	{
		char src_path[PATH_MAX + 1];
		char dst_path[PATH_MAX + 1];

		if(is_builtin_dir(d->d_name))
		{
			continue;
		}

		snprintf(src_path, sizeof(src_path), "%s/%s", src, d->d_name);
		snprintf(dst_path, sizeof(dst_path), "%s/%s", dst, d->d_name);

		if(fops_is_dir_entry(dst_path, d))
		{
			if(merge_dirs(src_path, dst_path, ops) != 0)
			{
				break;
			}
		}
		else
		{
			if(perform_operation(OP_MOVEF, put_confirm.ops, NULL, src_path,
						dst_path) != OPS_SUCCEEDED)
			{
				break;
			}
			un_group_add_op(OP_MOVEF, put_confirm.ops, NULL, src_path, dst_path);
		}
	}
	os_closedir(dir);

	if(d != NULL)
	{
		return 1;
	}

	OpsResult result = perform_operation(OP_RMDIR, put_confirm.ops, NULL, src,
			NULL);
	if(result == OPS_SUCCEEDED)
	{
		un_group_add_op(OP_RMDIR, NULL, NULL, src, "");
	}

	/* Clone file properties as the last step, because modifying directory affects
	 * timestamps and permissions can affect operations. */
	clone_attribs(dst, src, &st);
	(void)chmod(dst, st.st_mode);

	return (result == OPS_SUCCEEDED ? 0 : 1);
}

/* Goes through the rest of files in the register to see whether they are inside
 * path that we're going to overwrite or move and asks/warns the user if
 * necessary.  Returns non-zero on abort, otherwise zero is returned. */
static int
handle_clashing(int move, const char src[], const char dst[])
{
	int i;
	vle_textbuf *const lost = vle_tb_create();
	vle_textbuf *const to_exclude = vle_tb_create();

	for(i = put_confirm.index + 1; i < put_confirm.reg->nfiles; ++i)
	{
		const char *const another_src =
			put_confirm.reg->files[put_confirm.file_order[i]];
		const int sub_path = is_in_subtree(another_src, src, 0);
		if(is_in_subtree(another_src, dst, 0) && !sub_path)
		{
			vle_tb_append_line(lost, replace_home_part(another_src));
		}
		if(sub_path)
		{
			vle_tb_append_line(to_exclude, replace_home_part(another_src));
		}
	}

	if(*vle_tb_get_data(lost) != '\0')
	{
		int i;
		char msg[PATH_MAX + 1];
		response_variant responses[] = {
			{ .key = 'y', .descr = "[y]es " },
			{ .key = 'n', .descr = " [n]o\n" },
			{ .key = NC_C_c, .descr = "\nEsc or Ctrl-C to abort" },
			{}
		};

		/* Screen needs to be restored after displaying progress dialog. */
		modes_update();

		snprintf(msg, sizeof(msg), "Overwriting\n%s\nwith\n%s\nwill result "
				"in loss of the following files.  Are you sure?\n%s",
				replace_home_part(dst), replace_home_part(src), vle_tb_get_data(lost));
		switch(fops_options_prompt("Possible data loss", msg, responses))
		{
			case 'y':
				/* Do nothing. */
				break;
			case 'n':
				prompt_what_to_do(get_last_path_component(dst), src);
				/* Fall through. */
			case NC_C_c:
				vle_tb_free(to_exclude);
				vle_tb_free(lost);
				return 1;
		}

		for(i = put_confirm.index + 1; i < put_confirm.reg->nfiles; ++i)
		{
			char **const another_src =
				&put_confirm.reg->files[put_confirm.file_order[i]];
			const int sub_path = is_in_subtree(*another_src, src, 0);
			if(is_in_subtree(*another_src, dst, 0) && !sub_path)
			{
				update_string(another_src, NULL);
			}
		}
	}

	if(*vle_tb_get_data(to_exclude) != '\0')
	{
		int i;

		show_error_msgf("Operation Warning",
				"The following files got excluded from further processing:\n%s",
				vle_tb_get_data(to_exclude));

		for(i = put_confirm.index + 1; i < put_confirm.reg->nfiles; ++i)
		{
			char **const another_src =
				&put_confirm.reg->files[put_confirm.file_order[i]];
			if(is_in_subtree(*another_src, src, 0))
			{
				update_string(another_src, NULL);
			}
		}
	}

	vle_tb_free(to_exclude);
	vle_tb_free(lost);

	return 0;
}

/* Prompt user for conflict resolution strategy about given filename. */
static void
prompt_what_to_do(const char fname[], const char caused_by[])
{
	/* Strange spacing is for left alignment.  Doesn't look nice here, but it is
	 * problematic to get such alignment otherwise. */
	static const response_variant
		compare        = { .key = 'c', .descr = "[c]ompare files              \n" },
		rename         = { .key = 'r', .descr = "[r]ename (also Enter)        \n" },
		enter          = { .key = '\r', .descr = "" },
		skip           = { .key = 's', .descr = "[s]kip " },
		skip_all       = { .key = 'S', .descr = " [S]kip all          \n" },
		append         = { .key = 'a', .descr = "[a]ppend the tail            \n" },
		overwrite      = { .key = 'o', .descr = "[o]verwrite " },
		overwrite_all  = { .key = 'O', .descr = " [O]verwrite all\n" },
		merge          = { .key = 'm', .descr = "[m]erge " },
		merge_all      = { .key = 'M', .descr = " [M]erge all        \n" },
		merge_all_only = { .key = 'M', .descr = "[M]erge all                  \n" },
		escape         = { .key = NC_C_c, .descr = "\nEsc or Ctrl-C to cancel" };

	char response;
	/* Last element is a terminator. */
	response_variant responses[12] = {};
	size_t i = 0;

	char dst_buf[PATH_MAX + 1];
	snprintf(dst_buf, sizeof(dst_buf), "%s/%s", put_confirm.dst_dir, fname);
	const int same_file = paths_are_equal(dst_buf, caused_by);

	if(!same_file)
	{
		responses[i++] = compare;
	}

	responses[i++] = rename;
	responses[i++] = enter;

	responses[i++] = skip;
	responses[i++] = skip_all;

	if(!same_file)
	{
		if(cfg.use_system_calls && is_regular_file_noderef(fname) &&
				is_regular_file_noderef(caused_by))
		{
			responses[i++] = append;
		}
		responses[i++] = overwrite;
		responses[i++] = overwrite_all;
		if(put_confirm.allow_merge)
		{
			responses[i++] = merge;
		}
		if(put_confirm.allow_merge_all)
		{
			responses[i++] = (put_confirm.allow_merge ? merge_all : merge_all_only);
		}
	}

	responses[i++] = escape;
	assert(i + 1 <= ARRAY_LEN(responses) && "Array is too small.");

	/* Screen needs to be restored after displaying progress dialog. */
	modes_update();

	char *escaped_cause = escape_unreadable(replace_home_part(caused_by));

	char msg[PATH_MAX*3];
	if(same_file)
	{
		snprintf(msg, sizeof(msg),
				"Same file is both source and destination:\n%s\nWhat to do?",
				escaped_cause);
	}
	else
	{
		char *escaped_fname = escape_unreadable(fname);
		snprintf(msg, sizeof(msg),
				"Name conflict for %s.  Caused by:\n%s\nWhat to do?", escaped_fname,
				escaped_cause);
		free(escaped_fname);
	}

	free(escaped_cause);

	response = fops_options_prompt("File Conflict", msg, responses);
	handle_prompt_response(fname, caused_by, response);
}

/* Handles response to the prompt asked by prompt_what_to_do(). */
static void
handle_prompt_response(const char fname[], const char caused_by[],
		char response)
{
	char dst_path[PATH_MAX + 1];
	snprintf(dst_path, sizeof(dst_path), "%s/%s", put_confirm.dst_dir, fname);

	/* Record last conflict to position cursor at it later. */
	update_string(&put_confirm.last_conflict, dst_path);

	if(response == '\r' || response == 'r')
	{
		prompt_dst_name(fname);
	}
	else if(response == 'c')
	{
		show_difference(fname, caused_by);
		prompt_what_to_do(fname, caused_by);
	}
	else if(response == 's' || response == 'S')
	{
		if(response == 'S')
		{
			put_confirm.skip_all = 1;
		}

		++put_confirm.index;
		curr_stats.save_msg = put_files_i(put_confirm.view, 0);
	}
	else if(response == 'o')
	{
		put_continue(1);
	}
	else if(response == 'a' && cfg.use_system_calls && !is_dir(fname))
	{
		put_confirm.append = 1;
		put_continue(0);
	}
	else if(response == 'O')
	{
		put_confirm.overwrite_all = 1;
		put_continue(1);
	}
	else if(put_confirm.allow_merge && response == 'm')
	{
		put_confirm.merge = 1;
		put_continue(1);
	}
	else if(put_confirm.allow_merge_all && response == 'M')
	{
		put_confirm.merge_all = 1;
		put_continue(1);
	}
	else if(response == NC_C_c)
	{
		/* After user cancels at conflict resolution, put the cursor at the last
		 * file that caused the conflict. */
		dir_entry_t *const entry = entry_from_path(put_confirm.view,
				put_confirm.view->dir_entry, put_confirm.view->list_rows, dst_path);
		if(entry != NULL)
		{
			fpos_set_pos(put_confirm.view, entry_to_pos(put_confirm.view, entry));
		}
	}
	else
	{
		prompt_what_to_do(fname, caused_by);
	}
}

/* Prompts user for new name for the file being inserted.  Callback based. */
static void
prompt_dst_name(const char src_name[])
{
	char prompt[128 + PATH_MAX];

	snprintf(prompt, ARRAY_LEN(prompt), "New name for %s: ", src_name);
	fops_line_prompt(prompt, src_name, &prompt_dst_name_cb, NULL, 0);
}

/* Callback for line prompt result. */
static void
prompt_dst_name_cb(const char dst_name[])
{
	if(is_null_or_empty(dst_name))
	{
		return;
	}

	/* Record new destination path. */
	char dst_path[PATH_MAX + 1];
	snprintf(dst_path, sizeof(dst_path), "%s/%s", put_confirm.dst_dir, dst_name);
	update_string(&put_confirm.last_conflict, dst_path);

	if(replace_string(&put_confirm.dst_name, dst_name) != 0)
	{
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return;
	}

	if(put_next(0) == 0)
	{
		++put_confirm.index;
		curr_stats.save_msg = put_files_i(put_confirm.view, 0);
	}
}

/* Continues putting files. */
static void
put_continue(int force)
{
	if(put_next(force) == 0)
	{
		++put_confirm.index;
		curr_stats.save_msg = put_files_i(put_confirm.view, 0);
	}
}

/* Displays differences in metadata among two conflicting files in a dialog. */
static void
show_difference(const char fname[], const char caused_by[])
{
	char dst_path[PATH_MAX + 1];
	snprintf(dst_path, sizeof(dst_path), "%s/%s", put_confirm.dst_dir, fname);

	struct stat dst;
	if(os_stat(dst_path, &dst) != 0)
	{
		show_error_msgf("Comparison error", "Unable to query metadata of %s",
				dst_path);
		return;
	}

	struct stat src;
	if(os_stat(caused_by, &src) != 0)
	{
		show_error_msgf("Comparison error", "Unable to query metadata of %s",
				caused_by);
		return;
	}

	char *diff = compare_files(dst_path, caused_by, &dst, &src);

	static const response_variant responses[] = {
		{ .key = '\r', .descr = "Press Enter to continue", },
		{ },
	};
	(void)prompt_msg_custom("File difference", diff, responses);

	free(diff);
}

/* Produces textual description of metadata difference between two files.
 * Returns newly allocated string. */
static char *
compare_files(const char dst_path[], const char src_path[], struct stat *dst,
		struct stat *src)
{
	vle_textbuf *text = vle_tb_create();

	vle_tb_append_linef(text, "Target file: %s", replace_home_part(dst_path));
	vle_tb_append_linef(text, "Source file: %s", replace_home_part(src_path));

	char buf[64];

	vle_tb_append_line(text, " ");
	format_iso_time(dst->st_mtime, buf, sizeof(buf));
	if(dst->st_mtime == src->st_mtime)
	{
		vle_tb_append_linef(text, "Same modification date: %s", buf);
	}
	else
	{
		vle_tb_append_line(text, "Modification dates:");
		vle_tb_append_linef(text, "%s", buf);

		format_iso_time(src->st_mtime, buf, sizeof(buf));
		vle_tb_append_linef(text, "%s", buf);
	}

	vle_tb_append_line(text, " ");
	(void)friendly_size_notation(dst->st_size, sizeof(buf), buf);
	if(dst->st_size == src->st_size)
	{
		vle_tb_append_linef(text, "Same size: %s (%llu)", buf,
				(unsigned long long)dst->st_size);
	}
	else
	{
		vle_tb_append_line(text, "Sizes:");
		vle_tb_append_linef(text, "%s (%llu)", buf,
				(unsigned long long)dst->st_size);

		(void)friendly_size_notation(src->st_size, sizeof(buf), buf);
		vle_tb_append_linef(text, "%s (%llu)", buf,
				(unsigned long long)src->st_size);
	}

	return vle_tb_release(text);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
