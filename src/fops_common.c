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

#include "fops_common.h"

#include <regex.h>

#include <fcntl.h>
#include <sys/stat.h> /* stat umask() */
#include <sys/types.h> /* mode_t */
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif
#include <unistd.h> /* unlink() */

#include <assert.h> /* assert() */
#include <errno.h> /* errno */
#include <stddef.h> /* NULL size_t */
#include <stdint.h> /* uint64_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* calloc() free() malloc() realloc() strtol() */
#include <string.h> /* memcmp() strcmp() strdup() strlen() */

#include "cfg/config.h"
#include "compat/dtype.h"
#include "compat/fs_limits.h"
#include "compat/os.h"
#include "int/vim.h"
#include "io/ioeta.h"
#include "io/ionotif.h"
#include "modes/dialogs/msg_dialog.h"
#include "modes/modes.h"
#include "ui/cancellation.h"
#include "ui/fileview.h"
#include "ui/statusbar.h"
#include "ui/ui.h"
#include "utils/cancellation.h"
#ifdef _WIN32
#include "utils/env.h"
#endif
#include "utils/fs.h"
#include "utils/fsdata.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/utils.h"
#include "background.h"
#include "filelist.h"
#include "flist_pos.h"
#include "flist_sel.h"
#include "ops.h"
#include "running.h"
#include "status.h"
#include "trash.h"
#include "types.h"
#include "undo.h"

/* 10 to the power of number of digits after decimal point to take into account
 * on progress percentage counting. */
#define IO_PRECISION 10

/* Key used to switch to progress dialog. */
#define IO_DETAILS_KEY 'i'

/* Object for auxiliary information related to progress of operations in
 * io_progress_changed() handler. */
typedef struct
{
	int bg; /* Whether this is background operation. */
	union
	{
		ops_t *ops;     /* Information for foreground operation. */
		bg_op_t *bg_op; /* Information for background operation. */
	};

	int last_progress; /* Progress of the operation during previous call. */
	IoPs last_stage;   /* Stage of the operation during previous call. */

	/* Whether progress is displayed in a dialog, rather than on status bar. */
	int dialog;

	int width; /* Maximum reached width of the dialog. */
}
progress_data_t;

static void io_progress_changed(const io_progress_t *state);
static int calc_io_progress(const io_progress_t *state, int *skip);
static void io_progress_fg(const io_progress_t *state, int progress);
static void io_progress_fg_sb(const io_progress_t *state, int progress);
static void io_progress_bg(const io_progress_t *state, int progress);
static char * format_file_progress(const ioeta_estim_t *estim, int precision);
static void format_pretty_path(const char base_dir[], const char path[],
		char pretty[], size_t pretty_size);
static int is_file_name_changed(const char old[], const char new[]);
static int ui_cancellation_hook(void *arg);
static progress_data_t * alloc_progress_data(int bg, void *info);

line_prompt_func fops_line_prompt;
options_prompt_func fops_options_prompt;

void
fops_init(line_prompt_func line_func, options_prompt_func options_func)
{
	fops_line_prompt = line_func;
	fops_options_prompt = options_func;
	ionotif_register(&io_progress_changed);
}

/* I/O operation update callback. */
static void
io_progress_changed(const io_progress_t *state)
{
	const ioeta_estim_t *const estim = state->estim;
	progress_data_t *const pdata = estim->param;

	int redraw = 0;
	int progress, skip;

	progress = calc_io_progress(state, &skip);
	if(skip)
	{
		return;
	}

	/* Don't query for scheduled redraw or input for background operations. */
	if(!pdata->bg)
	{
		redraw = (stats_update_fetch() != UT_NONE);

		if(!pdata->dialog)
		{
			if(ui_char_pressed(IO_DETAILS_KEY))
			{
				pdata->dialog = 1;
				ui_sb_clear();
			}
		}
	}

	/* Do nothing if progress change is small, but force update on stage
	 * change or redraw request. */
	if(progress == pdata->last_progress &&
			state->stage == pdata->last_stage && !redraw)
	{
		return;
	}

	pdata->last_stage = state->stage;

	if(progress >= 0)
	{
		pdata->last_progress = progress;
	}

	if(redraw)
	{
		modes_redraw();
	}

	if(pdata->bg)
	{
		io_progress_bg(state, progress);
	}
	else
	{
		io_progress_fg(state, progress);
	}
}

/* Calculates current IO operation progress.  *skip will be set to non-zero
 * value to indicate that progress change is irrelevant.  Returns progress in
 * the range [-1; 100], where -1 means "unknown". */
static int
calc_io_progress(const io_progress_t *state, int *skip)
{
	const ioeta_estim_t *const estim = state->estim;
	progress_data_t *const pdata = estim->param;

	*skip = 0;
	if(state->stage == IO_PS_ESTIMATING)
	{
		return estim->total_items/IO_PRECISION;
	}
	else if(estim->total_bytes == 0)
	{
		if(estim->total_items == 0)
		{
			return 0;
		}
		/* When files are empty, use their number for progress counting. */
		return (estim->current_item*100*IO_PRECISION)/estim->total_items;
	}
	else if(pdata->last_progress >= 100*IO_PRECISION &&
			estim->current_byte == estim->total_bytes)
	{
		/* Special handling for unknown total size. */
		++pdata->last_progress;
		if(pdata->last_progress%IO_PRECISION != 0)
		{
			*skip = 1;
		}
		return -1;
	}
	else
	{
		return (estim->current_byte*100*IO_PRECISION)/estim->total_bytes;
	}
}

/* Takes care of progress for foreground operations. */
static void
io_progress_fg(const io_progress_t *state, int progress)
{
	char current_size_str[64];
	char total_size_str[64];
	char src_path[PATH_MAX + 1];
	const char *title, *ctrl_msg;
	const char *target_name;
	char *as_part;
	const char *item_name;
	int item_num;

	const ioeta_estim_t *const estim = state->estim;
	progress_data_t *const pdata = estim->param;
	ops_t *const ops = pdata->ops;

	if(!pdata->dialog)
	{
		io_progress_fg_sb(state, progress);
		return;
	}

	(void)friendly_size_notation(estim->total_bytes, sizeof(total_size_str),
			total_size_str);

	copy_str(src_path, sizeof(src_path), replace_home_part(estim->item));
	remove_last_path_component(src_path);

	title = ops_describe(ops);
	ctrl_msg = "Press Ctrl-C to cancel";
	if(state->stage == IO_PS_ESTIMATING)
	{
		char pretty_path[PATH_MAX + 1];
		format_pretty_path(ops->base_dir, estim->item, pretty_path,
				sizeof(pretty_path));
		draw_msgf(title, ctrl_msg, pdata->width,
				"In %s\nestimating...\nItems: %" PRINTF_ULL "\n"
				"Overall: %s\nCurrent: %s",
				ops->target_dir, (unsigned long long)estim->total_items, total_size_str,
				pretty_path);
		pdata->width = getmaxx(error_win);
		return;
	}

	(void)friendly_size_notation(estim->current_byte,
			sizeof(current_size_str), current_size_str);

	item_name = get_last_path_component(estim->item);

	target_name = get_last_path_component(estim->target);
	if(stroscmp(target_name, item_name) == 0)
	{
		as_part = strdup("");
	}
	else
	{
		as_part = format_str("\nas   %s", target_name);
	}

	item_num = MIN(estim->current_item + 1, estim->total_items);

	if(progress < 0)
	{
		/* Simplified message for unknown total size. */
		draw_msgf(title, ctrl_msg, pdata->width,
				"Location: %s\nItem:     %d of %" PRINTF_ULL "\n"
				"Overall:  %s\n"
				" \n" /* Space is on purpose to preserve empty line. */
				"file %s\nfrom %s%s",
				replace_home_part(ops->target_dir), item_num,
				(unsigned long long)estim->total_items, total_size_str, item_name,
				src_path, as_part);
	}
	else
	{
		char *const file_progress = format_file_progress(estim, IO_PRECISION);

		draw_msgf(title, ctrl_msg, pdata->width,
				"Location: %s\nItem:     %d of %" PRINTF_ULL "\n"
				"Overall:  %s/%s (%2d%%)\n"
				" \n" /* Space is on purpose to preserve empty line. */
				"file %s\nfrom %s%s%s",
				replace_home_part(ops->target_dir), item_num,
				(unsigned long long)estim->total_items, current_size_str,
				total_size_str, progress/IO_PRECISION, item_name, src_path, as_part,
				file_progress);

		free(file_progress);
	}
	pdata->width = getmaxx(error_win);

	free(as_part);
}

/* Takes care of progress for foreground operations displayed on status line. */
static void
io_progress_fg_sb(const io_progress_t *state, int progress)
{
	const ioeta_estim_t *const estim = state->estim;
	progress_data_t *const pdata = estim->param;
	ops_t *const ops = pdata->ops;

	char current_size_str[64];
	char total_size_str[64];
	char pretty_path[PATH_MAX + 1];
	char *suffix;

	(void)friendly_size_notation(estim->total_bytes, sizeof(total_size_str),
			total_size_str);

	format_pretty_path(ops->base_dir, estim->item, pretty_path,
			sizeof(pretty_path));

	switch(state->stage)
	{
		case IO_PS_ESTIMATING:
			suffix = format_str("estimating... %" PRINTF_ULL "; %s %s",
					(unsigned long long)estim->total_items, total_size_str, pretty_path);
			break;
		case IO_PS_IN_PROGRESS:
			(void)friendly_size_notation(estim->current_byte,
					sizeof(current_size_str), current_size_str);

			if(progress < 0)
			{
				/* Simplified message for unknown total size. */
				suffix = format_str("%" PRINTF_ULL " of %" PRINTF_ULL "; %s %s",
						(unsigned long long)estim->current_item + 1U,
						(unsigned long long)estim->total_items, total_size_str,
						pretty_path);
			}
			else
			{
				suffix = format_str("%" PRINTF_ULL " of %" PRINTF_ULL "; "
						"%s/%s (%2d%%) %s",
						(unsigned long long)estim->current_item + 1,
						(unsigned long long)estim->total_items, current_size_str,
						total_size_str, progress/IO_PRECISION, pretty_path);
			}
			break;

		default:
			assert(0 && "Unhandled progress stage");
			suffix = strdup("");
			break;
	}

	ui_sb_quick_msgf("(hit %c for details) %s: %s", IO_DETAILS_KEY,
			ops_describe(ops), suffix);
	free(suffix);
}

/* Takes care of progress for background operations. */
static void
io_progress_bg(const io_progress_t *state, int progress)
{
	const ioeta_estim_t *const estim = state->estim;
	progress_data_t *const pdata = estim->param;
	bg_op_t *const bg_op = pdata->bg_op;

	bg_op->progress = progress/IO_PRECISION;
	bg_op_changed(bg_op);
}

/* Formats file progress part of the progress message.  Returns pointer to newly
 * allocated memory. */
static char *
format_file_progress(const ioeta_estim_t *estim, int precision)
{
	char current_size[64];
	char total_size[64];

	const int file_progress = (estim->total_file_bytes == 0U) ? 0 :
		(estim->current_file_byte*100*precision)/estim->total_file_bytes;

	if(estim->total_items == 1)
	{
		return strdup("");
	}

	(void)friendly_size_notation(estim->current_file_byte, sizeof(current_size),
			current_size);
	(void)friendly_size_notation(estim->total_file_bytes, sizeof(total_size),
			total_size);

	return format_str("\nprogress %s/%s (%2d%%)", current_size, total_size,
			file_progress/precision);
}

/* Pretty prints path shortening it by skipping base directory path if
 * possible, otherwise fallbacks to the full path. */
static void
format_pretty_path(const char base_dir[], const char path[], char pretty[],
		size_t pretty_size)
{
	if(!path_starts_with(path, base_dir))
	{
		copy_str(pretty, pretty_size, path);
		return;
	}

	copy_str(pretty, pretty_size, skip_char(path + strlen(base_dir), '/'));
}

int
fops_is_name_list_ok(int count, int nlines, char *list[], char *files[])
{
	int i;

	if(nlines < count)
	{
		ui_sb_errf("Not enough file names (%d/%d)", nlines, count);
		curr_stats.save_msg = 1;
		return 0;
	}

	if(nlines > count)
	{
		ui_sb_errf("Too many file names (%d/%d)", nlines, count);
		curr_stats.save_msg = 1;
		return 0;
	}

	for(i = 0; i < count; ++i)
	{
		chomp(list[i]);

		if(files != NULL)
		{
			char *file_s = find_slashr(files[i]);
			char *list_s = find_slashr(list[i]);
			if(list_s != NULL || file_s != NULL)
			{
				if(list_s - list[i] != file_s - files[i] ||
						strnoscmp(files[i], list[i], list_s - list[i]) != 0)
				{
					if(file_s == NULL)
						ui_sb_errf("Name \"%s\" contains slash", list[i]);
					else
						ui_sb_errf("Won't move \"%s\" file", files[i]);
					curr_stats.save_msg = 1;
					return 0;
				}
			}
		}

		if(list[i][0] != '\0' && is_in_string_array(list, i, list[i]))
		{
			ui_sb_errf("Name \"%s\" duplicates", list[i]);
			curr_stats.save_msg = 1;
			return 0;
		}
	}

	return 1;
}

int
fops_is_rename_list_ok(char *files[], char is_dup[], int len, char *list[])
{
	int i;
	const char *const work_dir = flist_get_dir(curr_view);
	for(i = 0; i < len; ++i)
	{
		int j;

		const int check_result =
			fops_check_file_rename(work_dir, files[i], list[i], ST_NONE);
		if(check_result < 0)
		{
			continue;
		}

		for(j = 0; j < len; ++j)
		{
			if(strcmp(list[i], files[j]) == 0 && !is_dup[j])
			{
				is_dup[j] = 1;
				break;
			}
		}
		if(j >= len && check_result == 0)
		{
			/* Invoke fops_check_file_rename() again, but this time to produce error
			 * message. */
			(void)fops_check_file_rename(work_dir, files[i], list[i], ST_STATUS_BAR);
			break;
		}
	}
	return i >= len;
}

int
fops_check_file_rename(const char dir[], const char old[], const char new[],
		SignalType signal_type)
{
	if(!is_file_name_changed(old, new))
	{
		return -1;
	}

	if(path_exists_at(dir, new, NODEREF) && stroscmp(old, new) != 0 &&
			!is_case_change(old, new))
	{
		switch(signal_type)
		{
			case ST_STATUS_BAR:
				ui_sb_errf("File \"%s\" already exists", new);
				curr_stats.save_msg = 1;
				break;
			case ST_DIALOG:
				show_error_msg("File exists",
						"That file already exists. Will not overwrite.");
				break;

			default:
				assert(signal_type == ST_NONE && "Unhandled signaling type");
				break;
		}
		return 0;
	}

	return 1;
}

/* Checks whether file name change was performed.  Returns non-zero if change is
 * detected, otherwise zero is returned. */
static int
is_file_name_changed(const char old[], const char new[])
{
	/* Empty new name means reuse of the old name (rename cancellation).  Names
	 * are always compared in a case sensitive way, so that changes in case of
	 * letters triggers rename operation even for systems where paths are case
	 * insensitive. */
	return (new[0] != '\0' && strcmp(old, new) != 0);
}

char **
fops_grab_marked_files(view_t *view, size_t *nmarked)
{
	char **marked = NULL;
	dir_entry_t *entry = NULL;
	*nmarked = 0;
	while(iter_marked_entries(view, &entry))
	{
		*nmarked = add_to_string_array(&marked, *nmarked, 1, entry->name);
	}
	return marked;
}

int
fops_is_dir_entry(const char full_path[], const struct dirent *dentry)
{
#ifndef _WIN32
	struct stat s;
	const int type = get_dirent_type(dentry, full_path);
	if(type != DT_UNKNOWN)
	{
		return type == DT_DIR;
	}
	if(os_lstat(full_path, &s) == 0 && s.st_ino != 0)
	{
		return (s.st_mode&S_IFMT) == S_IFDIR;
	}
	return 0;
#else
	return is_dir(full_path);
#endif
}

int
fops_enqueue_marked_files(ops_t *ops, view_t *view, const char dst_hint[],
		int to_trash)
{
	int nmarked_files = 0;
	dir_entry_t *entry = NULL;

	ui_cancellation_enable();

	while(iter_marked_entries(view, &entry) && !ui_cancellation_requested())
	{
		char full_path[PATH_MAX + 1];

		get_full_path_of(entry, sizeof(full_path), full_path);

		if(to_trash)
		{
			char *const trash_dir = pick_trash_dir(entry->origin);
			ops_enqueue(ops, full_path, trash_dir);
			free(trash_dir);
		}
		else
		{
			ops_enqueue(ops, full_path, dst_hint);
		}

		++nmarked_files;
	}

	ui_cancellation_disable();

	return nmarked_files;
}

ops_t *
fops_get_ops(OPS main_op, const char descr[], const char base_dir[],
		const char target_dir[])
{
	ops_t *const ops = ops_alloc(main_op, 0, descr, base_dir, target_dir);
	if(ops->use_system_calls)
	{
		progress_data_t *const pdata = alloc_progress_data(ops->bg, ops);
		const io_cancellation_t cancellation = { .hook = &ui_cancellation_hook };
		ops->estim = ioeta_alloc(pdata, cancellation);
	}
	return ops;
}

/* Implementation of cancellation hook for I/O unit. */
static int
ui_cancellation_hook(void *arg)
{
	return ui_cancellation_requested();
}

void
fops_progress_msg(const char text[], int ready, int total)
{
	if(!cfg.use_system_calls)
	{
		char msg[strlen(text) + 32];

		sprintf(msg, "%s %d/%d", text, ready + 1, total);
		show_progress(msg, 1);
		curr_stats.save_msg = 2;
	}
}

const char *
fops_get_dst_name(const char src_path[], int from_trash)
{
	if(from_trash)
	{
		return get_real_name_from_trash_name(src_path);
	}
	return get_last_path_component(src_path);
}

int
fops_can_read_marked_files(view_t *view)
{
	dir_entry_t *entry;

	if(is_unc_path(view->curr_dir))
	{
		return 1;
	}

	entry = NULL;
	while(iter_marked_entries(view, &entry))
	{
		char full_path[PATH_MAX + 1];
		get_full_path_of(entry, sizeof(full_path), full_path);

		/* We can copy links even when they are broken, so it's OK to don't check
		 * them (otherwise access() fails for broken links). */
		if(entry->type == FT_LINK || os_access(full_path, R_OK) == 0)
		{
			continue;
		}

		show_error_msgf("Access denied",
				"You don't have read permissions on \"%s\"", full_path);
		flist_sel_stash(view);
		redraw_view(view);
		return 0;
	}
	return 1;
}

int
fops_check_dir_path(const view_t *view, const char path[], char buf[],
		size_t buf_len)
{
	if(path[0] == '/' || path[0] == '~')
	{
		char *const expanded_path = expand_tilde(path);
		copy_str(buf, buf_len, expanded_path);
		free(expanded_path);
	}
	else
	{
		snprintf(buf, buf_len, "%s/%s", fops_get_dst_dir(view, -1), path);
	}

	if(is_dir(buf))
	{
		return 1;
	}

	copy_str(buf, buf_len, fops_get_dst_dir(view, -1));
	return 0;
}

char **
fops_edit_list(size_t count, char *orig[], int *nlines, int load_always)
{
	char rename_file[PATH_MAX + 1];
	char **list = NULL;
	mode_t saved_umask;

	*nlines = 0;

	generate_tmp_file_name("vifm.rename", rename_file, sizeof(rename_file));

	/* Allow temporary file to be only readable and writable by current user. */
	saved_umask = umask(~0600);
	if(write_file_of_lines(rename_file, orig, count) != 0)
	{
		(void)umask(saved_umask);
		show_error_msgf("Error Getting List Of Renames",
				"Can't create temporary file \"%s\": %s", rename_file, strerror(errno));
		return NULL;
	}
	(void)umask(saved_umask);

	if(vim_view_file(rename_file, -1, -1, 0) != 0)
	{
		show_error_msgf("Error Editing File", "Editing of file \"%s\" failed.",
				rename_file);
	}
	else
	{
		list = read_file_of_lines(rename_file, nlines);
		if(list == NULL)
		{
			show_error_msgf("Error Getting List Of Renames",
					"Can't open temporary file \"%s\": %s", rename_file, strerror(errno));
		}

		if(!load_always)
		{
			size_t i = count - 1U;
			if((size_t)*nlines == count)
			{
				for(i = 0U; i < count; ++i)
				{
					if(strcmp(list[i], orig[i]) != 0)
					{
						break;
					}
				}
			}

			if(i == count)
			{
				free_string_array(list, *nlines);
				list = NULL;
				*nlines = 0;
			}
		}
	}

	unlink(rename_file);
	return list;
}

void
fops_bg_ops_init(ops_t *ops, bg_op_t *bg_op)
{
	ops->bg_op = bg_op;
	if(ops->estim != NULL)
	{
		progress_data_t *const pdata = ops->estim->param;
		pdata->bg_op = bg_op;
	}
}

ops_t *
fops_get_bg_ops(OPS main_op, const char descr[], const char dir[])
{
	ops_t *const ops = ops_alloc(main_op, 1, descr, dir, dir);
	if(ops->use_system_calls)
	{
		progress_data_t *const pdata = alloc_progress_data(ops->bg, NULL);
		const io_cancellation_t no_cancellation = {};
		ops->estim = ioeta_alloc(pdata, no_cancellation);
	}
	return ops;
}

/* Allocates progress data with specified parameters and initializes all the
 * rest of structure fields with default values. */
static progress_data_t *
alloc_progress_data(int bg, void *info)
{
	progress_data_t *const pdata = malloc(sizeof(*pdata));

	pdata->bg = bg;
	pdata->ops = info;

	pdata->last_progress = -1;
	pdata->last_stage = (IoPs)-1;
	pdata->dialog = 0;
	pdata->width = 0;

	return pdata;
}

void
fops_free_ops(ops_t *ops)
{
	if(ops == NULL)
	{
		return;
	}

	if(!ops->bg)
	{
		ui_drain_input();
	}

	if(ops->use_system_calls)
	{
		if(!ops->bg && ops->errors != NULL)
		{
			char *const title = format_str("Encountered errors on %s",
					ops_describe(ops));
			show_error_msg(title, ops->errors);
			free(title);
		}

		free(ops->estim->param);
	}
	ops_free(ops);
}

int
fops_mv_file(const char src[], const char src_dir[], const char dst[],
		const char dst_dir[], OPS op, int cancellable, ops_t *ops)
{
	char full_src[PATH_MAX + 1], full_dst[PATH_MAX + 1];

	to_canonic_path(src, src_dir, full_src, sizeof(full_src));
	to_canonic_path(dst, dst_dir, full_dst, sizeof(full_dst));

	return fops_mv_file_f(full_src, full_dst, op, 0, cancellable, ops);
}

int
fops_mv_file_f(const char src[], const char dst[], OPS op, int bg,
		int cancellable, ops_t *ops)
{
	int result;

	/* Compare case sensitive strings even on Windows to let user rename file
	 * changing only case of some characters. */
	if(strcmp(src, dst) == 0)
	{
		return 0;
	}

	result = perform_operation(op, ops, cancellable ? NULL : (void *)1, src, dst);
	if(result == 0 && !bg)
	{
		un_group_add_op(op, NULL, NULL, src, dst);
	}
	return result;
}

void
fops_free_bg_args(bg_args_t *args)
{
	free_string_array(args->list, args->nlines);
	free_string_array(args->sel_list, args->sel_list_len);
	free(args->is_in_trash);
	fops_free_ops(args->ops);
	free(args);
}

void
fops_prepare_for_bg_task(view_t *view, bg_args_t *args)
{
	dir_entry_t *entry;

	entry = NULL;
	while(iter_marked_entries(view, &entry))
	{
		char full_path[PATH_MAX + 1];

		get_full_path_of(entry, sizeof(full_path), full_path);
		args->sel_list_len = add_to_string_array(&args->sel_list,
				args->sel_list_len, 1, full_path);
	}

	ui_view_reset_selection_and_reload(view);
}

void
fops_append_marked_files(view_t *view, char buf[], char **fnames)
{
	const int custom_fnames = (fnames != NULL);
	size_t len = strlen(buf);
	dir_entry_t *entry = NULL;
	while(iter_marked_entries(view, &entry) && len < COMMAND_GROUP_INFO_LEN)
	{
		fops_append_fname(buf, len, entry->name);
		len = strlen(buf);

		if(custom_fnames)
		{
			const char *const custom_fname = *fnames++;

			strncat(buf, " to ", COMMAND_GROUP_INFO_LEN - len - 1);
			len = strlen(buf);
			strncat(buf, custom_fname, COMMAND_GROUP_INFO_LEN - len - 1);
			len = strlen(buf);
		}
	}
}

void
fops_append_fname(char buf[], size_t len, const char fname[])
{
	if(buf[len - 2] != ':')
	{
		strncat(buf, ", ", COMMAND_GROUP_INFO_LEN - len - 1);
		len = strlen(buf);
	}
	strncat(buf, fname, COMMAND_GROUP_INFO_LEN - len - 1);
}

const char *
fops_get_cancellation_suffix(void)
{
	return ui_cancellation_requested() ? " (cancelled)" : "";
}

int
fops_view_can_be_changed(const view_t *view)
{
	/* TODO: maybe add check whether directory of specific entry is writable for
	 *       custom views. */
	return flist_custom_active(view)
	    || fops_is_dir_writable(DR_CURRENT, view->curr_dir);
}

int
fops_view_can_be_extended(const view_t *view, int at)
{
	if(flist_custom_active(view) && !cv_tree(view->custom.type))
	{
		show_error_msg("Operation error",
				"Custom view can't handle this operation.");
		return 0;
	}

	return fops_is_dir_writable(DR_DESTINATION, fops_get_dst_dir(view, at));
}

const char *
fops_get_dst_dir(const view_t *view, int at)
{
	if(flist_custom_active(view) && cv_tree(view->custom.type))
	{
		if(at < 0)
		{
			at = view->list_pos;
		}
		else if(at >= view->list_rows)
		{
			at = view->list_rows - 1;
		}
		return view->dir_entry[at].origin;
	}
	return flist_get_dir(view);
}

int
fops_is_dir_writable(DirRole dir_role, const char path[])
{
	if(is_dir_writable(path))
	{
		return 1;
	}

	if(dir_role == DR_DESTINATION)
	{
		show_error_msg("Operation error", "Destination directory is not writable");
	}
	else
	{
		show_error_msg("Operation error", "Current directory is not writable");
	}
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
