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

#include "fops_misc.h"

#include <sys/stat.h> /* stat */
#include <sys/types.h> /* gid_t uid_t */

#include <string.h> /* strdup() strlen() */

#include "cfg/config.h"
#include "compat/os.h"
#include "modes/dialogs/msg_dialog.h"
#include "ui/cancellation.h"
#include "ui/fileview.h"
#include "ui/statusbar.h"
#include "ui/ui.h"
#include "utils/cancellation.h"
#include "utils/fs.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/test_helpers.h"
#include "utils/utils.h"
#include "cmd_completion.h"
#include "filelist.h"
#include "flist_pos.h"
#include "flist_sel.h"
#include "fops_common.h"
#include "registers.h"
#include "trash.h"
#include "undo.h"

/* Arguments pack for dir_size_bg() background function. */
typedef struct
{
	char *path; /* Full path to directory to process, will be freed. */
	int force;  /* Whether cached values should be ignored. */
}
dir_size_args_t;

/* Arguments pack for fops_query_list() verification function. */
typedef struct
{
	const char *dst_path; /* Destination directory. */
	int force;            /* Whether operation should be forced. */
}
verify_args_t;

static int delete_file(dir_entry_t *entry, ops_t *ops, int reg, int use_trash,
		int nested);
static const char * get_top_dir(const view_t *view);
static void delete_files_in_bg(bg_op_t *bg_op, void *arg);
static void delete_file_in_bg(ops_t *ops, const char path[], int use_trash);
static int prepare_register(int reg);
static char ** list_files_to_retarget(view_t *view, int *len);
static int retarget_one(view_t *view);
static void change_link_cb(const char new_target[]);
static int retarget_many(view_t *view, char *files[], int nfiles);
static int verify_retarget_list(char *files[], int nfiles, char *names[],
		int nnames, char **error, void *data);
static void change_link(ops_t *ops, const char path[], const char from[],
		const char to[]);
static int complete_filename(const char str[], void *arg);
static int verify_clone_list(char *files[], int nfiles, char *names[],
		int nnames, char **error, void *data);
TSTATIC const char * gen_clone_name(const char dir[], const char normal_name[]);
static int clone_file(const dir_entry_t *entry, const char path[],
		const char clone[], ops_t *ops);
static void get_group_file_list(char *list[], int count, char buf[]);
static void go_to_first_file(view_t *view, char *names[], int count);
static void update_dir_entry_size(dir_entry_t *entry, int force);
static void start_dir_size_calc(const char path[], int force);
static void dir_size_bg(bg_op_t *bg_op, void *arg);
static void dir_size(bg_op_t *bg_op, char path[], int force);
static int bg_cancellation_hook(void *arg);
static void redraw_after_path_change(view_t *view, const char path[]);
#ifndef _WIN32
static void change_owner_cb(const char new_owner[]);
static int complete_owner(const char str[], void *arg);
static void change_group_cb(const char new_group[]);
static int complete_group(const char str[], void *arg);
#endif

int
fops_delete(view_t *view, int reg, int use_trash)
{
	char undo_msg[COMMAND_GROUP_INFO_LEN];
	int i;
	dir_entry_t *entry;
	int nmarked_files;
	ops_t *ops;
	const char *const top_dir = get_top_dir(view);
	const char *const curr_dir = top_dir == NULL ? flist_get_dir(view) : top_dir;

	if(!fops_view_can_be_changed(view))
	{
		return 0;
	}

	use_trash = use_trash && cfg.use_trash;

	strlist_t marked = {};
	entry = NULL;
	while(iter_marked_entries(view, &entry))
	{
		if(!flist_custom_active(view))
		{
			char name[NAME_MAX + 1];
			format_entry_name(entry, NF_FULL, sizeof(name), name);
			marked.nitems = add_to_string_array(&marked.items, marked.nitems, name);
			continue;
		}

		char short_path[PATH_MAX + 1];
		get_short_path_of(view, entry, NF_FULL, 0, sizeof(short_path), short_path);
		marked.nitems = add_to_string_array(&marked.items, marked.nitems,
				short_path);
	}
	if(!confirm_deletion(marked.items, marked.nitems, use_trash))
	{
		free_string_array(marked.items, marked.nitems);
		return 0;
	}
	free_string_array(marked.items, marked.nitems);

	/* This check for the case when we are for sure in the trash. */
	if(use_trash && top_dir != NULL && trash_has_path(top_dir))
	{
		show_error_msg("Can't perform deletion",
				"Current directory is under trash directory");
		return 0;
	}

	if(use_trash)
	{
		reg = prepare_register(reg);
	}

	snprintf(undo_msg, sizeof(undo_msg), "%celete in %s: ", use_trash ? 'd' : 'D',
			replace_home_part(curr_dir));
	fops_append_marked_files(view, undo_msg, NULL);
	un_group_open(undo_msg);

	ops = fops_get_ops(OP_REMOVE, use_trash ? "deleting" : "Deleting", curr_dir,
			curr_dir);

	nmarked_files = fops_enqueue_marked_files(ops, view, NULL, use_trash);

	entry = NULL;
	i = 0;
	while(iter_marked_entries(view, &entry) && !ui_cancellation_requested())
	{
		int result;

		fops_progress_msg("Deleting files", i++, nmarked_files);
		result = delete_file(entry, ops, reg, use_trash, 0);

		if(result == 0 && entry_to_pos(view, entry) == view->list_pos)
		{
			if(view->list_pos + 1 < view->list_rows)
			{
				++view->list_pos;
			}
		}

		ops_advance(ops, result == 0);
	}

	regs_update_unnamed(reg);

	un_group_close();

	ui_view_reset_selection_and_reload(view);
	ui_view_schedule_reload(view == curr_view ? other_view : curr_view);

	ui_sb_msgf("%d %s %celeted%s", ops->succeeded,
			(ops->succeeded == 1) ? "file" : "files", use_trash ? 'd' : 'D',
			fops_get_cancellation_suffix());

	fops_free_ops(ops);
	regs_sync_to_shared_memory();
	return 1;
}

int
fops_delete_current(view_t *view, int use_trash, int nested)
{
	char undo_msg[COMMAND_GROUP_INFO_LEN];
	dir_entry_t *entry;
	ops_t *ops;
	const char *const top_dir = get_top_dir(view);
	const char *const curr_dir = top_dir == NULL ? flist_get_dir(view) : top_dir;

	use_trash = use_trash && cfg.use_trash;

	/* This check for the case when we are for sure in the trash. */
	if(use_trash && top_dir != NULL && trash_has_path(top_dir))
	{
		show_error_msg("Can't perform deletion",
				"Current directory is under trash directory");
		return 0;
	}

	snprintf(undo_msg, sizeof(undo_msg), "%celete in %s: ", use_trash ? 'd' : 'D',
			replace_home_part(curr_dir));
	if(!nested)
	{
		un_group_open(undo_msg);
	}

	ops = fops_get_ops(OP_REMOVE, use_trash ? "deleting" : "Deleting", curr_dir,
			curr_dir);

	entry = &view->dir_entry[view->list_pos];

	fops_progress_msg("Deleting files", 0, 1);
	(void)delete_file(entry, ops, BLACKHOLE_REG_NAME, use_trash, nested);

	if(!nested)
	{
		un_group_close();
		ui_views_reload_filelists();
	}

	ui_sb_msgf("%d %s %celeted%s", ops->succeeded,
			(ops->succeeded == 1) ? "file" : "files", use_trash ? 'd' : 'D',
			fops_get_cancellation_suffix());

	fops_free_ops(ops);
	return 1;
}

/* Removes single file specified by its entry.  Returns zero on success,
 * otherwise non-zero is returned. */
static int
delete_file(dir_entry_t *entry, ops_t *ops, int reg, int use_trash, int nested)
{
	char full_path[PATH_MAX + 1];
	int result;

	get_full_path_of(entry, sizeof(full_path), full_path);

	if(!use_trash)
	{
		OpsResult r = perform_operation(OP_REMOVE, ops, NULL, full_path, NULL);
		result = (r == OPS_SUCCEEDED ? 0 : -1);

		/* For some reason "rm" sometimes returns 0 on cancellation. */
		if(result == 0 && path_exists(full_path, NODEREF))
		{
			result = -1;
		}

		if(result == 0)
		{
			un_group_add_op(OP_REMOVE, NULL, NULL, full_path, "");
		}
	}
	else if(trash_is_at_path(full_path))
	{
		show_error_msg("Can't perform deletion",
				"You cannot delete trash directory to trash");
		result = -1;
	}
	else if(trash_has_path(full_path))
	{
		show_error_msgf("Skipping file deletion",
				"File is already in trash: %s", full_path);
		result = -1;
	}
	else
	{
		const OPS op = nested ? OP_MOVETMP4 : OP_MOVE;
		char *const dest = trash_gen_path(entry->origin, entry->name);
		if(dest != NULL)
		{
			OpsResult r = perform_operation(op, ops, NULL, full_path, dest);
			result = (r == OPS_SUCCEEDED ? 0 : -1);

			/* For some reason "rm" sometimes returns 0 on cancellation. */
			if(result == 0 && path_exists(full_path, NODEREF))
			{
				result = -1;
			}

			if(result == 0)
			{
				un_group_add_op(op, NULL, NULL, full_path, dest);
				regs_append(reg, dest);
			}
			free(dest);
		}
		else
		{
			show_error_msgf("No trash directory is available",
					"Either correct trash directory paths or prune files.  "
					"Deletion failed on: %s", entry->name);
			result = -1;
		}
	}

	return result;
}

int
fops_delete_bg(view_t *view, int use_trash)
{
	char task_desc[COMMAND_GROUP_INFO_LEN];
	const char *const top_dir = get_top_dir(view);
	const char *const curr_dir = top_dir == NULL ? flist_get_dir(view) : top_dir;

	if(!fops_view_can_be_changed(view))
	{
		return 0;
	}

	use_trash = use_trash && cfg.use_trash;

	if(use_trash && top_dir != NULL && trash_has_path(top_dir))
	{
		show_error_msg("Can't perform deletion",
				"Current directory is under trash directory");
		return 0;
	}

	bg_args_t *args = calloc(1, sizeof(*args));
	if(args == NULL)
	{
		show_error_msg("Can't perform deletion", "Out of memory");
		return 0;
	}

	args->use_trash = use_trash;

	fops_prepare_for_bg_task(view, args);
	if(args->sel_list_len == 0)
	{
		fops_free_bg_args(args);
		ui_sb_msg("Nothing to delete");
		return 1;
	}

	if(use_trash)
	{
		unsigned int i;
		for(i = 0U; i < args->sel_list_len; ++i)
		{
			const char *const full_file_path = args->sel_list[i];
			if(trash_is_at_path(full_file_path))
			{
				show_error_msg("Can't perform deletion",
						"You cannot delete trash directory to trash");
				fops_free_bg_args(args);
				return 0;
			}
			else if(trash_has_path(full_file_path))
			{
				show_error_msgf("Skipping file deletion",
						"File is already in trash: %s", full_file_path);
				fops_free_bg_args(args);
				return 0;
			}
		}
	}

	if(!confirm_deletion(args->sel_list, args->sel_list_len, use_trash))
	{
		fops_free_bg_args(args);
		return 0;
	}

	snprintf(task_desc, sizeof(task_desc), "%celete in %s: ",
			use_trash ? 'd' : 'D', replace_home_part(curr_dir));

	fops_append_marked_files(view, task_desc, NULL);

	args->ops = fops_get_bg_ops(use_trash ? OP_REMOVE : OP_REMOVESL,
			use_trash ? "deleting" : "Deleting", args->path);

	if(bg_execute(task_desc, "...", args->sel_list_len, 1, &delete_files_in_bg,
				args) != 0)
	{
		fops_free_bg_args(args);

		show_error_msg("Can't perform deletion",
				"Failed to initiate background operation");
	}
	return 0;
}

/* Retrieves root directory of file system sub-tree (for regular or tree views).
 * Returns the path or NULL (for custom views). */
static const char *
get_top_dir(const view_t *view)
{
	if(flist_custom_active(view) && !cv_tree(view->custom.type))
	{
		return NULL;
	}
	return flist_get_dir(view);
}

/* Entry point for a background task that deletes files. */
static void
delete_files_in_bg(bg_op_t *bg_op, void *arg)
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
			char *trash_dir = (args->use_trash ? trash_pick_dir(src) : args->path);
			ops_enqueue(ops, src, trash_dir);
			if(trash_dir != args->path)
			{
				free(trash_dir);
			}
		}
	}

	for(i = 0U; i < args->sel_list_len; ++i)
	{
		const char *const src = args->sel_list[i];
		bg_op_set_descr(bg_op, src);
		delete_file_in_bg(ops, src, args->use_trash);
		++bg_op->done;
	}

	fops_free_bg_args(args);
}

/* Actual implementation of background file removal. */
static void
delete_file_in_bg(ops_t *ops, const char path[], int use_trash)
{
	if(!use_trash)
	{
		(void)perform_operation(OP_REMOVE, ops, NULL, path, NULL);
		return;
	}

	if(!trash_is_at_path(path))
	{
		const char *const fname = get_last_path_component(path);
		char *const trash_name = trash_gen_path(path, fname);
		const char *const dest = (trash_name != NULL) ? trash_name : fname;
		(void)perform_operation(OP_MOVE, ops, NULL, path, dest);
		free(trash_name);
	}
}

int
fops_yank(view_t *view, int reg)
{
	int nyanked_files;
	dir_entry_t *entry;

	reg = prepare_register(reg);

	nyanked_files = 0;
	entry = NULL;
	while(iter_marked_entries(view, &entry))
	{
		char full_path[PATH_MAX + 1];
		get_full_path_of(entry, sizeof(full_path), full_path);

		if(regs_append(reg, full_path) == 0)
		{
			++nyanked_files;
		}
	}

	regs_update_unnamed(reg);

	ui_sb_msgf("%d file%s yanked", nyanked_files,
			(nyanked_files == 1) ? "" : "s");

	regs_sync_to_shared_memory();

	return 1;
}

/* Transforms "A-"Z register to "a-"z or clears the reg.  So that for "A-"Z new
 * values will be appended to "a-"z, for other registers old values will be
 * removed.  Returns possibly modified value of the reg parameter. */
static int
prepare_register(int reg)
{
	if(reg >= 'A' && reg <= 'Z')
	{
		reg += 'a' - 'A';
		/* We're going to append to a register and thus need its contents to be up
		 * to date. */
		regs_sync_from_shared_memory();
	}
	else
	{
		regs_clear(reg);
	}
	return reg;
}

int
fops_retarget(view_t *view)
{
	if(!symlinks_available())
	{
		show_error_msg("Symbolic Links Error",
				"Your OS doesn't support symbolic links");
		return 0;
	}
	if(!fops_view_can_be_changed(view))
	{
		return 0;
	}

	int nfiles;
	char **files = list_files_to_retarget(view, &nfiles);
	if(nfiles == -1)
	{
		/* An error has occurred. */
		flist_sel_stash(view);
		return 1;
	}

	int result;
	if(nfiles == 0)
	{
		result = retarget_one(view);
	}
	else
	{
		result = retarget_many(view, files, nfiles);
	}

	flist_sel_stash(view);
	free_string_array(files, nfiles);
	return result;
}

/* Makes list of symbolic links to be retargeted.  Always sets *len.  Returns
 * list of files (NULL if empty) or NULL and sets *len to -1 on error. */
static char **
list_files_to_retarget(view_t *view, int *len)
{
	*len = 0;

	char **files = NULL;
	dir_entry_t *entry = NULL;
	while(iter_marked_entries(view, &entry))
	{
		char path[PATH_MAX + 1];
		get_short_path_of(view, entry, NF_NONE, 0, sizeof(path), path);

		if(entry->type != FT_LINK)
		{
			ui_sb_errf("File is not a symbolic link: %s", path);
			free_string_array(files, *len);
			*len = -1;
			return NULL;
		}

		*len = add_to_string_array(&files, *len, path);
	}

	return files;
}

/* Changes target of a symbolic link under the cursor.  Returns new value for
 * save_msg. */
static int
retarget_one(view_t *view)
{
	char full_path[PATH_MAX + 1];
	char linkto[PATH_MAX + 1];
	const dir_entry_t *const entry = get_current_entry(view);

	if(fentry_is_fake(entry))
	{
		ui_sb_err("Entry doesn't correspond to a file");
		return 1;
	}
	if(entry->type != FT_LINK)
	{
		ui_sb_err("File is not a symbolic link");
		return 1;
	}

	get_full_path_of(entry, sizeof(full_path), full_path);
	if(get_link_target(full_path, linkto, sizeof(linkto)) != 0)
	{
		show_error_msg("Error", "Can't read link");
		return 0;
	}

	fops_line_prompt("Link target: ", linkto, &change_link_cb, &complete_filename,
			0);
	return 0;
}

/* Handles users response for new link target prompt. */
static void
change_link_cb(const char new_target[])
{
	char undo_msg[2*PATH_MAX + 32];
	char full_path[PATH_MAX + 1];
	char linkto[PATH_MAX + 1];
	const char *fname;
	ops_t *ops;
	const char *const curr_dir = flist_get_dir(curr_view);

	if(is_null_or_empty(new_target))
	{
		return;
	}

	curr_stats.confirmed = 1;

	get_current_full_path(curr_view, sizeof(full_path), full_path);
	if(get_link_target(full_path, linkto, sizeof(linkto)) != 0)
	{
		show_error_msg("Error", "Can't read link");
		return;
	}

	ops = fops_get_ops(OP_SYMLINK2, "re-targeting", curr_dir, curr_dir);

	fname = get_last_path_component(full_path);
	snprintf(undo_msg, sizeof(undo_msg), "cl in %s: on %s from \"%s\" to \"%s\"",
			replace_home_part(flist_get_dir(curr_view)), fname, linkto, new_target);
	un_group_open(undo_msg);

	change_link(ops, full_path, linkto, new_target);

	un_group_close();

	fops_free_ops(ops);
}

/* Command-line path completion callback.  Returns completion start offset. */
static int
complete_filename(const char str[], void *arg)
{
	const char *name_begin = after_last(str, '/');
	return name_begin - str + filename_completion(str, CT_ALL_WOE, 0);
}

/* Changes target of marked fifles.  Returns new value for save_msg. */
static int
retarget_many(view_t *view, char *files[], int nfiles)
{
	int nfrom = 0;
	char **from = NULL;

	dir_entry_t *entry = NULL;
	while(iter_marked_entries(view, &entry) && !ui_cancellation_requested())
	{
		char full_path[PATH_MAX + 1];
		get_full_path_of(entry, sizeof(full_path), full_path);

		char linkto[PATH_MAX + 1];
		if(get_link_target(full_path, linkto, sizeof(linkto)) != 0)
		{
			free_string_array(from, nfrom);
			show_error_msgf("Error", "Failed to read target of %s", full_path);
			return 1;
		}

		nfrom = add_to_string_array(&from, nfrom, linkto);
	}

	int nto;
	char **to = fops_query_list(nfrom, from, &nto, /*load_always=*/0,
			&verify_retarget_list, NULL);
	if(nto == 0)
	{
		free(to);
		free_string_array(from, nfrom);
		ui_sb_msg("0 links retargeted");
		return 1;
	}

	const char *curr_dir = flist_get_dir(view);
	ops_t *ops = fops_get_ops(OP_SYMLINK2, "re-targeting", curr_dir, curr_dir);

	char undo_msg[2*PATH_MAX + 32];
	snprintf(undo_msg, sizeof(undo_msg), "cl in %s: ",
			replace_home_part(flist_get_dir(view)));
	fops_append_marked_files(view, undo_msg, to);
	un_group_open(undo_msg);

	entry = NULL;
	while(iter_marked_entries(view, &entry) && !ui_cancellation_requested())
	{
		char full_path[PATH_MAX + 1];
		get_full_path_of(entry, sizeof(full_path), full_path);
		ops_enqueue(ops, full_path, full_path);
	}

	int i = 0;
	entry = NULL;
	while(iter_marked_entries(view, &entry) && !ui_cancellation_requested())
	{
		char full_path[PATH_MAX + 1];
		get_full_path_of(entry, sizeof(full_path), full_path);

		change_link(ops, full_path, from[i], to[i]);
		ops_advance(ops, /*succeeded=*/1);

		++i;
	}

	un_group_close();

	ui_sb_msgf("%d link%s retargeted%s", ops->succeeded,
			(ops->succeeded == 1) ? "" : "s", fops_get_cancellation_suffix());

	fops_free_ops(ops);

	free_string_array(from, nfrom);
	free_string_array(to, nto);
	return 1;
}

/* Checks that retargeting can be performed.  Returns non-zero if so, otherwise
 * zero is returned along with setting *error. */
static int
verify_retarget_list(char *files[], int nfiles, char *names[], int nnames,
		char **error, void *data)
{
	return 1;
}

/* Changes target of a symbolic link. */
static void
change_link(ops_t *ops, const char path[], const char from[], const char to[])
{
	if(perform_operation(OP_REMOVESL, ops, NULL, path, NULL) == OPS_SUCCEEDED)
	{
		un_group_add_op(OP_REMOVESL, NULL, NULL, path, from);
	}

	if(perform_operation(OP_SYMLINK2, ops, NULL, to, path) == OPS_SUCCEEDED)
	{
		un_group_add_op(OP_SYMLINK2, NULL, NULL, to, path);
	}
}

int
fops_clone(view_t *view, char *list[], int nlines, int force, int copies)
{
	char dst_path[PATH_MAX + 1];
	int with_dir = 0;
	const char *const curr_dir = flist_get_dir(view);

	if(!fops_can_read_marked_files(view))
	{
		return 0;
	}

	if(nlines == 1)
	{
		with_dir = fops_check_dir_path(view, list[0], dst_path, sizeof(dst_path));
		if(with_dir)
		{
			nlines = 0;
		}
		else if(!fops_view_can_be_extended(view, -1))
		{
			return 0;
		}
	}
	else
	{
		if(!fops_view_can_be_extended(view, -1))
		{
			return 0;
		}

		copy_str(dst_path, sizeof(dst_path), fops_get_dst_dir(view, -1));
	}
	if(!fops_is_dir_writable(with_dir ? DR_DESTINATION : DR_CURRENT, dst_path))
	{
		return 0;
	}

	size_t nmarked;
	char **marked = fops_grab_marked_files(view, &nmarked);

	verify_args_t verify_args = { .dst_path = dst_path, .force = force };

	const int from_file = (nlines < 0);
	if(from_file)
	{
		list = fops_query_list(nmarked, marked, &nlines, 0, &verify_clone_list,
				&verify_args);
		if(list == NULL)
		{
			free_string_array(marked, nmarked);
			ui_sb_msg("0 files cloned");
			return 1;
		}
	}

	free_string_array(marked, nmarked);

	char *error_str = NULL;
	if(nlines > 0 &&
			!verify_clone_list(NULL, nmarked, list, nlines, &error_str, &verify_args))
	{
		if(error_str != NULL)
		{
			ui_sb_err(error_str);
			free(error_str);
		}

		redraw_view(view);
		if(from_file)
		{
			free_string_array(list, nlines);
		}
		return 1;
	}

	flist_sel_stash(view);

	char undo_msg[COMMAND_GROUP_INFO_LEN + 1];
	if(with_dir)
	{
		snprintf(undo_msg, sizeof(undo_msg), "clone in %s to %s: ", curr_dir,
				list[0]);
	}
	else
	{
		snprintf(undo_msg, sizeof(undo_msg), "clone in %s: ", curr_dir);
	}
	fops_append_marked_files(view, undo_msg, list);

	ops_t *ops = fops_get_ops(OP_COPY, "Cloning", curr_dir,
			with_dir ? list[0] : curr_dir);

	int nmarked_files = fops_enqueue_marked_files(ops, view, dst_path, 0);

	const int custom_fnames = (nlines > 0);

	un_group_open(undo_msg);
	dir_entry_t *entry = NULL;
	int i = 0;
	while(iter_marked_entries(view, &entry) && !ui_cancellation_requested())
	{
		const char *const name = entry->name;
		const char *const clone_dst = with_dir ? dst_path : entry->origin;
		const char *clone_name;
		if(custom_fnames)
		{
			clone_name = list[i];
		}
		else
		{
			clone_name = path_exists_at(clone_dst, name, NODEREF)
			           ? gen_clone_name(clone_dst, name)
			           : name;
		}

		fops_progress_msg("Cloning files", i, nmarked_files);

		int j;
		int err = 0;
		for(j = 0; j < copies; ++j)
		{
			if(path_exists_at(clone_dst, clone_name, NODEREF))
			{
				clone_name = gen_clone_name(clone_dst, custom_fnames ? list[i] : name);
			}
			err += clone_file(entry, clone_dst, clone_name, ops);
		}

		/* Don't update cursor position if more than one file is cloned. */
		if(nmarked == 1U)
		{
			fentry_rename(view, entry, clone_name);
		}
		ops_advance(ops, err == 0);

		++i;
	}
	un_group_close();

	ui_views_reload_filelists();
	if(from_file)
	{
		free_string_array(list, nlines);
	}

	ui_sb_msgf("%d file%s cloned%s", ops->succeeded,
			(ops->succeeded == 1) ? "" : "s", fops_get_cancellation_suffix());

	fops_free_ops(ops);
	return 1;
}

/* Checks that cloning can be performed.  Returns non-zero if so, otherwise zero
 * is returned along with setting *error. */
static int
verify_clone_list(char *files[], int nfiles, char *names[], int nnames,
		char **error, void *data)
{
	verify_args_t *args = data;
	return fops_is_name_list_ok(nfiles, nnames, names, NULL, error)
	    && fops_is_copy_list_ok(args->dst_path, nnames, names, args->force,
	                            error);
}

/* Generates name of clone for a file.  Returns pointer to statically allocated
 * buffer. */
TSTATIC const char *
gen_clone_name(const char dir[], const char normal_name[])
{
	static char result[NAME_MAX + 1];

	char extension[NAME_MAX + 1];
	int i;
	size_t len;
	char *p;

	copy_str(result, sizeof(result), normal_name);
	chosp(result);

	copy_str(extension, sizeof(extension),
			cut_extension(result[0] == '.' ? &result[1] : result));

	len = strlen(result);
	i = 1;
	if(result[len - 1] == ')' && (p = strrchr(result, '(')) != NULL)
	{
		char *t;
		long l;
		if((l = strtol(p + 1, &t, 10)) > 0 && t[1] == '\0')
		{
			len = p - result;
			i = l + 1;
		}
	}

	do
	{
		snprintf(result + len, sizeof(result) - len, "(%d)%s%s", i++,
				(extension[0] == '\0') ? "" : ".", extension);
	}
	while(path_exists_at(dir, result, NODEREF));

	return result;
}

/* Clones single file/directory to directory specified by the path under name in
 * the clone.  Returns zero on success, otherwise non-zero is returned. */
static int
clone_file(const dir_entry_t *entry, const char path[], const char clone[],
		ops_t *ops)
{
	char full_path[PATH_MAX + 1];
	char clone_name[PATH_MAX + 1];

	to_canonic_path(clone, path, clone_name, sizeof(clone_name));
	if(path_exists(clone_name, DEREF))
	{
		if(perform_operation(OP_REMOVESL, NULL, NULL, clone_name, NULL) !=
				OPS_SUCCEEDED)
		{
			return 1;
		}
	}

	get_full_path_of(entry, sizeof(full_path), full_path);

	if(perform_operation(OP_COPY, ops, NULL, full_path, clone_name) !=
			OPS_SUCCEEDED)
	{
		return 1;
	}

	un_group_add_op(OP_COPY, NULL, NULL, full_path, clone_name);
	return 0;
}

int
fops_mkdirs(view_t *view, int at, char **names, int count, int create_parent)
{
	char buf[COMMAND_GROUP_INFO_LEN + 1];
	int i;
	int n;
	void *cp;
	const char *const dst_dir = fops_get_dst_dir(view, at);

	if(!fops_view_can_be_extended(view, at))
	{
		return 1;
	}

	cp = (void *)(size_t)create_parent;

	for(i = 0; i < count; ++i)
	{
		char full[PATH_MAX + 1];

		if(is_in_string_array(names, i, names[i]))
		{
			ui_sb_errf("Name \"%s\" duplicates", names[i]);
			return 1;
		}
		if(names[i][0] == '\0')
		{
			ui_sb_errf("Name #%d is empty", i + 1);
			return 1;
		}

		to_canonic_path(names[i], dst_dir, full, sizeof(full));
		if(path_exists(full, NODEREF))
		{
			ui_sb_errf("File \"%s\" already exists", names[i]);
			return 1;
		}
	}

	ops_t *ops = fops_get_ops(OP_MKDIR, "making dirs", dst_dir, dst_dir);

	snprintf(buf, sizeof(buf), "mkdir in %s: ", replace_home_part(dst_dir));

	get_group_file_list(names, count, buf);
	un_group_open(buf);
	n = 0;
	for(i = 0; i < count && !ui_cancellation_requested(); ++i)
	{
		char full[PATH_MAX + 1];
		to_canonic_path(names[i], dst_dir, full, sizeof(full));

		if(perform_operation(OP_MKDIR, ops, cp, full, NULL) == OPS_SUCCEEDED)
		{
			un_group_add_op(OP_MKDIR, cp, NULL, full, "");
			++n;
		}
		else if(i == 0)
		{
			--i;
			++names;
			--count;
		}
	}
	un_group_close();

	if(count > 0)
	{
		if(create_parent)
		{
			for(i = 0; i < count; ++i)
			{
				break_at(names[i], '/');
			}
		}
		go_to_first_file(view, names, count);
	}

	ui_sb_msgf("%d director%s created%s", n, (n == 1) ? "y" : "ies",
			fops_get_cancellation_suffix());

	fops_free_ops(ops);
	return 1;
}

int
fops_mkfiles(view_t *view, int at, char *names[], int count)
{
	int i;
	int n;
	char buf[COMMAND_GROUP_INFO_LEN + 1];
	ops_t *ops;
	const char *const dst_dir = fops_get_dst_dir(view, at);

	if(!fops_view_can_be_extended(view, at))
	{
		return 0;
	}

	for(i = 0; i < count; ++i)
	{
		char full[PATH_MAX + 1];

		if(is_in_string_array(names, i, names[i]))
		{
			ui_sb_errf("Name \"%s\" duplicates", names[i]);
			return 1;
		}
		if(names[i][0] == '\0')
		{
			ui_sb_errf("Name #%d is empty", i + 1);
			return 1;
		}

		to_canonic_path(names[i], dst_dir, full, sizeof(full));
		if(path_exists(full, NODEREF))
		{
			ui_sb_errf("File \"%s\" already exists", names[i]);
			return 1;
		}
	}

	ops = fops_get_ops(OP_MKFILE, "touching", dst_dir, dst_dir);

	snprintf(buf, sizeof(buf), "touch in %s: ", replace_home_part(dst_dir));

	get_group_file_list(names, count, buf);
	un_group_open(buf);
	n = 0;
	for(i = 0; i < count && !ui_cancellation_requested(); ++i)
	{
		char full[PATH_MAX + 1];
		to_canonic_path(names[i], dst_dir, full, sizeof(full));

		if(perform_operation(OP_MKFILE, ops, NULL, full, NULL) == OPS_SUCCEEDED)
		{
			un_group_add_op(OP_MKFILE, NULL, NULL, full, "");
			++n;
		}
	}
	un_group_close();

	if(n > 0)
	{
		go_to_first_file(view, names, count);
	}

	ui_sb_msgf("%d file%s created%s", n, (n == 1) ? "" : "s",
			fops_get_cancellation_suffix());

	fops_free_ops(ops);
	return 1;
}

/* Fills undo message buffer with list of files.  buf should be at least
 * COMMAND_GROUP_INFO_LEN characters length. */
static void
get_group_file_list(char *list[], int count, char buf[])
{
	size_t len;
	int i;

	len = strlen(buf);
	for(i = 0; i < count && len < COMMAND_GROUP_INFO_LEN; ++i)
	{
		fops_append_fname(buf, len, list[i]);
		len = strlen(buf);
	}
}

/* Sets view cursor to point at first file found found in supplied list of file
 * names. */
static void
go_to_first_file(view_t *view, char *names[], int count)
{
	load_dir_list(view, 1);

	int i;
	for(i = 0; i < view->list_rows; ++i)
	{
		if(is_in_string_array(names, count, view->dir_entry[i].name))
		{
			view->list_pos = i;
			break;
		}
	}

	redraw_view(view);
}

int
fops_restore(view_t *view)
{
	int m, n;
	dir_entry_t *entry;

	/* This is general check for regular views only. */
	if(!flist_custom_active(view) && !trash_is_at_path(view->curr_dir))
	{
		show_error_msg("Restore error", "Not a top-level trash directory.");
		return 0;
	}

	un_group_open("restore: ");
	un_group_close();

	m = 0;
	n = 0;
	entry = NULL;
	while(iter_marked_entries(view, &entry) && !ui_cancellation_requested())
	{
		char full_path[PATH_MAX + 1];
		get_full_path_of(entry, sizeof(full_path), full_path);

		if(trash_is_at_path(entry->origin) && trash_restore(full_path) == 0)
		{
			++m;
		}
		++n;
	}

	ui_view_schedule_reload(view);

	ui_sb_msgf("Restored %d of %d%s", m, n, fops_get_cancellation_suffix());
	return 1;
}

void
fops_size_bg(view_t *view, int force)
{
	/* flist_set_marking(view, 1) isn't helpful here as it won't mark "..". */
	int user_selection = !view->pending_marking;
	flist_set_marking(view, 0);

	dir_entry_t *curr = get_current_entry(view);
	if(!curr->marked && user_selection)
	{
		update_dir_entry_size(curr, force);
		return;
	}

	dir_entry_t *entry = NULL;
	while(iter_marked_entries(view, &entry))
	{
		update_dir_entry_size(entry, force);
	}
}

/* Initiates background size calculation for view entry. */
static void
update_dir_entry_size(dir_entry_t *entry, int force)
{
	if(fentry_is_fake(entry) || !fentry_is_dir(entry))
	{
		return;
	}

	char full_path[PATH_MAX + 1];
	if(is_parent_dir(entry->name))
	{
		copy_str(full_path, sizeof(full_path), entry->origin);
	}
	else
	{
		get_full_path_of(entry, sizeof(full_path), full_path);
	}
	start_dir_size_calc(full_path, force);
}

/* Initiates background directory size calculation. */
static void
start_dir_size_calc(const char path[], int force)
{
	char task_desc[PATH_MAX + 32];
	dir_size_args_t *args;

	args = malloc(sizeof(*args));
	if(args == NULL)
	{
		show_error_msg("Can't calculate size", "Out of memory");
		return;
	}

	args->path = strdup(path);
	args->force = force;

	snprintf(task_desc, sizeof(task_desc), "Calculating size: %s", path);

	if(bg_execute(task_desc, path, BG_UNDEFINED_TOTAL, 0, &dir_size_bg,
				args) != 0)
	{
		free(args->path);
		free(args);

		show_error_msg("Can't calculate size",
				"Failed to initiate background operation");
	}
}

/* Entry point for a background task that calculates size of a directory. */
static void
dir_size_bg(bg_op_t *bg_op, void *arg)
{
	dir_size_args_t *const args = arg;

	dir_size(bg_op, args->path, args->force);

	free(args->path);
	free(args);
}

/* Calculates directory size and triggers view updates if necessary.  Changes
 * path. */
static void
dir_size(bg_op_t *bg_op, char path[], int force)
{
	const cancellation_t bg_cancellation_info = {
		.arg = bg_op,
		.hook = &bg_cancellation_hook,
	};

	(void)fops_dir_size(path, force, &bg_cancellation_info);

	remove_last_path_component(path);

	redraw_after_path_change(&lwin, path);
	redraw_after_path_change(&rwin, path);
}

/* Implementation of cancellation hook for background tasks. */
static int
bg_cancellation_hook(void *arg)
{
	return bg_op_cancelled(arg);
}

/* Schedules view redraw in case path change might have affected it. */
static void
redraw_after_path_change(view_t *view, const char path[])
{
	if(path_starts_with(view->curr_dir, path) || flist_custom_active(view))
	{
		ui_view_schedule_redraw(view);
	}
}

uint64_t
fops_dir_size(const char path[], int force_update,
		const cancellation_t *cancellation)
{
	struct dirent *dentry;
	const char *slash;
	uint64_t size;

	time_t mtime = 0;
	uint64_t inode = DCACHE_UNKNOWN;
	struct stat s;
	if(os_stat(path, &s) == 0)
	{
		mtime = s.st_mtime;
		inode = s.st_ino;
	}

	/* The check is at the top and not in the loop to do only one stat() for each
	 * path. */
	if(!force_update)
	{
		uint64_t dir_size;
		dcache_get_at(path, mtime, inode, &dir_size, NULL);
		if(dir_size != DCACHE_UNKNOWN)
		{
			return dir_size;
		}
	}

	DIR *dir = os_opendir(path);
	if(dir == NULL)
	{
		return 0U;
	}

	slash = (ends_with_slash(path) ? "" : "/");
	size = 0U;
	while((dentry = os_readdir(dir)) != NULL)
	{
		char full_path[PATH_MAX + 1];

		if(is_builtin_dir(dentry->d_name))
		{
			continue;
		}

		snprintf(full_path, sizeof(full_path), "%s%s%s", path, slash,
				dentry->d_name);
		if(fops_is_dir_entry(full_path, dentry))
		{
			size += fops_dir_size(full_path, force_update, cancellation);
		}
		else
		{
			size += get_file_size(full_path);
		}

		if(cancellation_requested(cancellation))
		{
			os_closedir(dir);
			return 0U;
		}
	}

	os_closedir(dir);

	/* Could calculate nitems here, but they aren't recursive and might only take
	 * up memory, because interest in size sort of excludes interest in nitems. */
	(void)dcache_set_at(path, inode, size, DCACHE_UNKNOWN);
	return size;
}

#ifndef _WIN32

int
fops_chown(int u, int g, uid_t uid, gid_t gid)
{
/* Integer to pointer conversion. */
#define V(e) ((void *)(uintptr_t)(e))

	view_t *const view = curr_view;
	char undo_msg[COMMAND_GROUP_INFO_LEN + 1];
	ops_t *ops;
	dir_entry_t *entry;
	const char *const curr_dir = flist_get_dir(view);

	snprintf(undo_msg, sizeof(undo_msg), "ch%s in %s: ", u ? "own" : "grp",
			replace_home_part(curr_dir));

	ops = fops_get_ops(OP_CHOWN, "re-owning", curr_dir, curr_dir);
	(void)fops_enqueue_marked_files(ops, view, NULL, 0);

	fops_append_marked_files(view, undo_msg, NULL);
	un_group_open(undo_msg);

	entry = NULL;
	while(iter_marked_entries(view, &entry) && !ui_cancellation_requested())
	{
		int full_success = (u != 0) + (g != 0);
		char full_path[PATH_MAX + 1];
		get_full_path_of(entry, sizeof(full_path), full_path);

		if(u && perform_operation(OP_CHOWN, ops, V(uid), full_path, NULL) ==
				OPS_SUCCEEDED)
		{
			un_group_add_op(OP_CHOWN, V(uid), V(entry->uid), full_path, "");
		}
		else
		{
			full_success -= (u != 0);
		}

		if(g && perform_operation(OP_CHGRP, ops, V(gid), full_path, NULL) ==
				OPS_SUCCEEDED)
		{
			un_group_add_op(OP_CHGRP, V(gid), V(entry->gid), full_path, "");
		}
		else
		{
			full_success -= (g != 0);
		}

		ops_advance(ops, full_success == (u != 0) + (g != 0));
	}
	un_group_close();

	ui_sb_msgf("%d file%s fully processed%s", ops->succeeded,
			(ops->succeeded == 1) ? "" : "s", fops_get_cancellation_suffix());
	fops_free_ops(ops);

	ui_view_reset_selection_and_reload(view);
	return 1;
#undef V
}

void
fops_chuser(void)
{
	if(mark_selection_or_current(curr_view) == 0)
	{
		show_error_msg("Change owner", "No files to process");
		return;
	}
	fops_line_prompt("New owner: ", "", &change_owner_cb, &complete_owner, 0);
}

/* Handles users response for new file owner name prompt. */
static void
change_owner_cb(const char new_owner[])
{
	uid_t uid;

	if(is_null_or_empty(new_owner))
	{
		return;
	}

	if(get_uid(new_owner, &uid) != 0)
	{
		ui_sb_errf("Invalid user name: \"%s\"", new_owner);
		curr_stats.save_msg = 1;
		return;
	}

	curr_stats.save_msg = fops_chown(1, 0, uid, 0);
}

/* Command-line user name completion callback.  Returns completion start
 * offset. */
static int
complete_owner(const char str[], void *arg)
{
	complete_user_name(str);
	return 0;
}

void
fops_chgroup(void)
{
	if(mark_selection_or_current(curr_view) == 0)
	{
		show_error_msg("Change group", "No files to process");
		return;
	}
	fops_line_prompt("New group: ", "", &change_group_cb, &complete_group, 0);
}

/* Handles users response for new file group name prompt. */
static void
change_group_cb(const char new_group[])
{
	gid_t gid;

	if(is_null_or_empty(new_group))
	{
		return;
	}

	if(get_gid(new_group, &gid) != 0)
	{
		ui_sb_errf("Invalid group name: \"%s\"", new_group);
		curr_stats.save_msg = 1;
		return;
	}

	curr_stats.save_msg = fops_chown(0, 1, 0, gid);
}

/* Command-line group name completion callback.  Returns completion start
 * offset. */
static int
complete_group(const char str[], void *arg)
{
	complete_group_name(str);
	return 0;
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
