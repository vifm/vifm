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

#include <sys/stat.h> /* stat umask() */
#include <sys/types.h> /* mode_t */
#include <fcntl.h>
#include <unistd.h> /* unlink() */

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include <assert.h> /* assert() */
#include <errno.h> /* errno */
#include <stddef.h> /* NULL size_t */
#include <stdint.h> /* uint64_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* free() malloc() */
#include <string.h> /* memset() strcat() strcmp() strdup() strlen() */
#include <time.h> /* clock_gettime() */

#include "cfg/config.h"
#include "compat/dtype.h"
#include "compat/fs_limits.h"
#include "compat/os.h"
#include "int/ext_edit.h"
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
#include "utils/test_helpers.h"
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

/* Maximum value of progress_data_t::last_progress. */
#define IO_MAX_PROGRESS (100*(IO_PRECISION))

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

	int last_progress; /* Progress of the operation during previous call.
	                      Range is [-1; IO_MAX_PROGRESS]. */
	IoPs last_stage;   /* Stage of the operation during previous call. */

	char *progress_bar;     /* String of progress bar. */
	int progress_bar_value; /* Value of progress bar during previous call. */
	int progress_bar_max;   /* Width of progress bar during previous call. */

	/* State of rate calculation. */
	long long last_calc_time; /* Time of last rate calculation. */
	uint64_t last_seen_byte;  /* Position at the time of last call. */
	uint64_t rate;            /* Rate in bytes per millisecond. */
	char *rate_str;           /* Rate formatted as a string. */

	/* Whether progress is displayed in a dialog, rather than on status bar. */
	int dialog;

	int width; /* Maximum reached width of the dialog. */
}
progress_data_t;

static void io_progress_changed(const io_progress_t *state);
static int calc_io_progress(const io_progress_t *state, int *skip);
static void update_io_rate(progress_data_t *pdata, const ioeta_estim_t *estim);
static void update_progress_bar(progress_data_t *pdata,
		const ioeta_estim_t *estim);
static void io_progress_fg(const io_progress_t *state, int progress);
static void io_progress_fg_sb(const io_progress_t *state, int progress);
static void io_progress_bg(const io_progress_t *state, int progress);
static char * format_file_progress(const ioeta_estim_t *estim, int precision);
static void format_pretty_path(const char base_dir[], const char path[],
		char pretty[], size_t pretty_size);
static int is_file_name_changed(const char old[], const char new[]);
static int ui_cancellation_hook(void *arg);
TSTATIC char ** edit_list(struct ext_edit_t *ext_edit, size_t orig_len,
		char *orig[], int *edited_len, int load_always);
TSTATIC progress_data_t * alloc_progress_data(int bg, void *info);
static long long time_in_ms(void);

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
 * the range [-1; IO_MAX_PROGRESS], where -1 means "unknown". */
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
		return (estim->current_item*IO_MAX_PROGRESS)/estim->total_items;
	}
	else if(pdata->last_progress >= IO_MAX_PROGRESS &&
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
		return (estim->current_byte*IO_MAX_PROGRESS)/estim->total_bytes;
	}
}

/* Updates rate of operation. */
static void
update_io_rate(progress_data_t *pdata, const ioeta_estim_t *estim)
{
	long long current_time_ms = time_in_ms();
	long long elapsed_time_ms = current_time_ms - pdata->last_calc_time;

	if(elapsed_time_ms == 0 ||
			(pdata->last_seen_byte != 0 && elapsed_time_ms < 3000))
	{
		/* Rate is updated initially and then once in 3000 milliseconds. */
		return;
	}

	uint64_t bytes_difference = estim->current_byte - pdata->last_seen_byte;
	pdata->rate = bytes_difference/elapsed_time_ms;
	pdata->last_calc_time = current_time_ms;
	pdata->last_seen_byte = estim->current_byte;

	char rate_str[64];
	(void)friendly_size_notation(pdata->rate*1000, sizeof(rate_str) - 8,
			rate_str);
	strcat(rate_str, "/s");
	replace_string(&pdata->rate_str, rate_str);
}

/* Updates progress bar of operation. */
static void
update_progress_bar(progress_data_t *pdata, const ioeta_estim_t *estim)
{
	int max_width = pdata->width - 6;
	if(max_width <= 0)
	{
		return;
	}

	int value = (pdata->last_progress*max_width)/IO_MAX_PROGRESS;
	if(value == pdata->progress_bar_value && max_width == pdata->progress_bar_max)
	{
		return;
	}

	pdata->progress_bar_value = value;
	pdata->progress_bar_max = max_width;

	free(pdata->progress_bar);
	pdata->progress_bar = malloc(max_width + 3);

	pdata->progress_bar[0] = '[';
	memset(pdata->progress_bar + 1, '=', value);
	memset(pdata->progress_bar + 1 + value, ' ', max_width - value);
	pdata->progress_bar[1 + max_width] = ']';
	pdata->progress_bar[1 + max_width + 1] = '\0';
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

	update_io_rate(pdata, estim);

	if(progress < 0)
	{
		/* Simplified message for unknown total size. */
		draw_msgf(title, ctrl_msg, pdata->width,
				"Location: %s\nItem:     %d of %" PRINTF_ULL "\n"
				"Overall:  %s %s\n"
				" \n" /* Space is on purpose to preserve empty line. */
				"file %s\nfrom %s%s",
				replace_home_part(ops->target_dir), item_num,
				(unsigned long long)estim->total_items, total_size_str, pdata->rate_str,
				item_name, src_path, as_part);
	}
	else
	{
		char *const file_progress = format_file_progress(estim, IO_PRECISION);
		update_progress_bar(pdata, estim);

		draw_msgf(title, ctrl_msg, pdata->width,
				"Location: %s\nItem:     %d of %" PRINTF_ULL "\n"
				"Overall:  %s/%s (%2d%%) %s\n"
				"%s\n"
				" \n" /* Space is on purpose to preserve empty line. */
				"file %s\nfrom %s%s%s",
				replace_home_part(ops->target_dir), item_num,
				(unsigned long long)estim->total_items, current_size_str,
				total_size_str, progress/IO_PRECISION, pdata->rate_str,
				pdata->progress_bar, item_name, src_path, as_part, file_progress);

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

	if(estim->item == NULL)
	{
		strcpy(pretty_path, "-");
	}
	else
	{
		format_pretty_path(ops->base_dir, estim->item, pretty_path,
				sizeof(pretty_path));
	}

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
fops_is_name_list_ok(int count, int nlines, char *list[], char *files[],
		char **error)
{
	int i;

	if(nlines < count)
	{
		put_string(error, format_str("Not enough file names (%d/%d)", nlines,
					count));
		return 0;
	}

	if(nlines > count)
	{
		put_string(error, format_str("Too many file names (%d/%d)", nlines, count));
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
				if(list_s == NULL || file_s == NULL ||
						list_s - list[i] != file_s - files[i] ||
						strnoscmp(files[i], list[i], list_s - list[i]) != 0)
				{
					if(file_s == NULL)
					{
						put_string(error, format_str("Name \"%s\" contains slash",
									list[i]));
					}
					else
					{
						put_string(error, format_str("Won't move \"%s\" file", files[i]));
					}
					return 0;
				}
			}
		}

		if(list[i][0] != '\0' && is_in_string_array(list, i, list[i]))
		{
			put_string(error, format_str("Name \"%s\" duplicates", list[i]));
			return 0;
		}
	}

	return 1;
}

int
fops_is_copy_list_ok(const char dst[], int count, char *list[], int force,
		char **error)
{
	if(force)
	{
		return 1;
	}

	int i;
	for(i = 0; i < count; ++i)
	{
		if(path_exists_at(dst, list[i], NODEREF))
		{
			char *escaped = escape_unreadable(list[i]);
			put_string(error, format_str("File \"%s\" already exists", escaped));
			free(escaped);
			return 0;
		}
	}
	return 1;
}

int
fops_is_rename_list_ok(char *files[], char is_dup[], int len, char *list[],
		char **error)
{
	int i;
	const char *const work_dir = flist_get_dir(curr_view);
	for(i = 0; i < len; ++i)
	{
		const int check_result =
			fops_check_file_rename(work_dir, files[i], list[i], error);
		if(check_result < 0)
		{
			continue;
		}

		int j;
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
			break;
		}
		update_string(error, NULL);
	}
	return i >= len;
}

int
fops_check_file_rename(const char dir[], const char old[], const char new[],
		char **error)
{
	if(!is_file_name_changed(old, new))
	{
		return -1;
	}

	if(path_exists_at(dir, new, NODEREF) && stroscmp(old, new) != 0 &&
			!is_case_change(old, new))
	{
		char *escaped = escape_unreadable(new);
		put_string(error, format_str("File \"%s\" already exists", escaped));
		free(escaped);
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
		*nmarked = add_to_string_array(&marked, *nmarked, entry->name);
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
			char *const trash_dir = trash_pick_dir(entry->origin);
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
	ops_t *const ops = ops_alloc(main_op, 0, descr, base_dir, target_dir,
			fops_options_prompt, &prompt_msg);
	if(ops->use_system_calls)
	{
		progress_data_t *const pdata = alloc_progress_data(ops->bg, ops);
		const io_cancellation_t cancellation = { .hook = &ui_cancellation_hook };
		ops->estim = ioeta_alloc(pdata, cancellation);
	}
	ui_cancellation_push_off();
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
		return trash_get_real_name_of(src_path);
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
fops_query_list(size_t orig_len, char *orig[], int *edited_len, int load_always,
		fops_query_verify_func verify, void *verify_data)
{
	ext_edit_t ext_edit = {};
	char **list = NULL;
	int nlines = 0;
	char *error_str = NULL;

	while(1)
	{
		list = edit_list(&ext_edit, orig_len, orig, &nlines, 0);
		if(nlines == 0)
		{
			/* Cancelled. */
			break;
		}

		if(!verify(orig, orig_len, list, nlines, &error_str, verify_data))
		{
			free_string_array(list, nlines);
			list = NULL;
			nlines = 0;

			/* Stray space prevents removal of the line. */
			if(prompt_msgf("Editing error", "%s\n \nRe-edit paths?", error_str))
			{
				update_string(&ext_edit.last_error, error_str);
				continue;
			}
		}

		break;
	}

	free(error_str);
	ext_edit_discard(&ext_edit);

	*edited_len = nlines;
	return list;
}

/* Prompts user with a file containing lines from orig array of length orig_len
 * and returns modified list of strings of length *edited_len or NULL on error
 * or unchanged list unless load_always is non-zero. */
TSTATIC char **
edit_list(ext_edit_t *ext_edit, size_t orig_len, char *orig[], int *edited_len,
		int load_always)
{
	*edited_len = 0;

	char rename_file[PATH_MAX + 1];
	generate_tmp_file_name("vifm.rename", rename_file, sizeof(rename_file));

	strlist_t prepared = ext_edit_prepare(ext_edit, orig, orig_len);

	/* Allow temporary file to be only readable and writable by current user. */
	mode_t saved_umask = umask(~0600);
	const int write_error = (write_file_of_lines(rename_file, prepared.items,
				prepared.nitems) != 0);
	(void)umask(saved_umask);

	free_string_array(prepared.items, prepared.nitems);

	if(write_error)
	{
		show_error_msgf("Error Getting List Of Renames",
				"Can't create temporary file \"%s\": %s", rename_file, strerror(errno));
		return NULL;
	}

	if(vim_view_file(rename_file, -1, -1, 0) != 0)
	{
		unlink(rename_file);
		show_error_msgf("Error Editing File", "Editing of file \"%s\" failed.",
				rename_file);
		return NULL;
	}

	int result_len;
	char **result = read_file_of_lines(rename_file, &result_len);
	unlink(rename_file);
	if(result == NULL)
	{
		show_error_msgf("Error Getting List Of Renames",
				"Can't open temporary file \"%s\": %s", rename_file, strerror(errno));
		return NULL;
	}

	ext_edit_done(ext_edit, orig, orig_len, result, &result_len);

	if(!load_always && string_array_equal(orig, orig_len, result, result_len))
	{
		ext_edit_discard(ext_edit);
		free_string_array(result, result_len);
		return NULL;
	}

	*edited_len = result_len;
	return result;
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
	ops_t *const ops = ops_alloc(main_op, 1, descr, dir, dir, fops_options_prompt,
			&prompt_msg);
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
TSTATIC progress_data_t *
alloc_progress_data(int bg, void *info)
{
	progress_data_t *const pdata = malloc(sizeof(*pdata));

	pdata->bg = bg;
	pdata->ops = info;

	pdata->last_progress = -1;
	pdata->last_stage = (IoPs)-1;

	pdata->progress_bar = strdup("");
	pdata->progress_bar_value = 0;
	pdata->progress_bar_max = 0;

	/* Time of starting the operation to have meaningful first rate. */
	pdata->last_calc_time = time_in_ms();
	pdata->last_seen_byte = 0;
	pdata->rate = 0;
	pdata->rate_str = strdup("? B/s");

	pdata->dialog = 0;
	pdata->width = 0;

	return pdata;
}

/* Retrieves current time in milliseconds. */
static long long
time_in_ms(void)
{
	struct timespec current_time;
	if(clock_gettime(CLOCK_MONOTONIC, &current_time) != 0)
	{
		return 0;
	}

	return current_time.tv_sec*1000 + current_time.tv_nsec/1000000;
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
		ui_cancellation_pop();
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

		progress_data_t *pdata = ops->estim->param;
		free(pdata->progress_bar);
		free(pdata->rate_str);
		free(pdata);
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
	/* Compare case sensitive strings even on Windows to let user rename file
	 * changing only case of some characters. */
	if(strcmp(src, dst) == 0)
	{
		return 0;
	}

	OpsResult result = perform_operation(op, ops, cancellable ? NULL : (void *)1,
			src, dst);
	if(result != OPS_SUCCEEDED)
	{
		return 1;
	}

	if(!bg)
	{
		un_group_add_op(op, NULL, NULL, src, dst);
	}
	return 0;
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
				args->sel_list_len, full_path);
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
	if(!flist_is_fs_backed(view))
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
