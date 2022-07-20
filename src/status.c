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

#include "status.h"

#ifdef HAVE_LIBGTK
#include <gio/gio.h>
#include <gtk/gtk.h>
#undef MAX
#undef MIN
#endif

#include <sys/types.h> /* ino_t */

#include <assert.h> /* assert() */
#include <limits.h> /* INT_MIN */
#include <stddef.h> /* NULL */
#include <string.h> /* memcpy() memmove() */
#include <time.h> /* time_t time() */

#include "cfg/config.h"
#include "compat/fs_limits.h"
#include "compat/pthread.h"
#include "compat/reallocarray.h"
#include "lua/vlua.h"
#include "modes/modes.h"
#include "ui/colors.h"
#include "ui/ui.h"
#include "utils/env.h"
#include "utils/fs.h"
#include "utils/fsdata.h"
#include "utils/log.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/test_helpers.h"
#include "utils/utils.h"
#include "cmd_completion.h"
#include "cmd_core.h"
#include "filelist.h"
#include "filetype.h"
#include "ipc.h"
#include "opt_handlers.h"
#include "plugins.h"

/* Environment variables by which application hosted by terminal multiplexer can
 * identify the host. */
#define SCREEN_ENVVAR "STY"
#define TMUX_ENVVAR "TMUX"

/* dcache entry. */
typedef struct
{
	uint64_t value;   /* Stored value. */
#ifndef _WIN32
	ino_t inode;      /* Inode number. */
#endif
	time_t timestamp; /* When the value was set. */
}
dcache_data_t;

/* Saved view selection. */
typedef struct
{
	char *location; /* Path at which selection was done. */

	char **paths; /* Names of selected files. */
	int length;   /* Number of items in paths. */
}
selection_t;

static void load_def_values(status_t *stats, config_t *config);
static void determine_fuse_umount_cmd(status_t *stats);
static void set_gtk_available(status_t *stats);
static int reset_dircache(void);
static void set_last_cmdline_command(const char cmd[]);
static void dcache_get(const char path[], time_t mtime, uint64_t inode,
		dcache_result_t *size, dcache_result_t *nitems);
static void size_updater(void *data, void *arg);
TSTATIC time_t dcache_get_size_timestamp(const char path[]);
TSTATIC void dcache_set_size_timestamp(const char path[], time_t ts);
static void rotate_right(void *ptr, size_t count, size_t item_len);

status_t curr_stats;

/* Whether redraw operation is scheduled. */
static int pending_redraw;
/* Whether reload operation is scheduled.  Redrawing is then assumed to be
 * scheduled too, as it's part of reloading. */
static int pending_refresh;
static int inside_screen;
static int inside_tmux;

/* Thread-safety guard for dcache_size variable. */
static pthread_mutex_t dcache_size_mutex = PTHREAD_MUTEX_INITIALIZER;
/* Thread-safety guard for dcache_nitems variable. */
static pthread_mutex_t dcache_nitems_mutex = PTHREAD_MUTEX_INITIALIZER;
/* Cache for directory sizes. */
static fsdata_t *dcache_size;
/* Cache for directory item count. */
static fsdata_t *dcache_nitems;

/* Whether UI updates should be "paused" (a counter, not a flag). */
static int silent_ui;
/* Whether silencing UI led to skipping of screen updates. */
static int silence_skipped_updates;

/* Selection history.  Most recently updated entry comes first. */
static selection_t sel_hist[10];

int
stats_init(config_t *config)
{
	inside_screen = !is_null_or_empty(env_get(SCREEN_ENVVAR));
	inside_tmux = !is_null_or_empty(env_get(TMUX_ENVVAR));

	load_def_values(&curr_stats, config);
	determine_fuse_umount_cmd(&curr_stats);
	set_gtk_available(&curr_stats);
	curr_stats.exec_env_type = get_exec_env_type();
	stats_update_shell_type(config->shell);

	update_string(&curr_stats.term_name, env_get("TERM"));

	(void)hist_init(&curr_stats.cmd_hist, config->history_len);
	(void)hist_init(&curr_stats.exprreg_hist, config->history_len);
	(void)hist_init(&curr_stats.search_hist, config->history_len);
	(void)hist_init(&curr_stats.prompt_hist, config->history_len);
	(void)hist_init(&curr_stats.filter_hist, config->history_len);

	hists_resize(config->history_len);

	return stats_reset(config);
}

/* Initializes most fields of the status structure, some are left to be
 * initialized by the stats_reset() function. */
static void
load_def_values(status_t *stats, config_t *config)
{
	pending_redraw = 0;
	pending_refresh = 0;

	stats->last_char = 0;
	stats->save_msg = 0;
	stats->use_register = 0;
	stats->curr_register = -1;
	stats->register_saved = 0;
	stats->number_of_windows = 2;
	stats->use_input_bar = 1;
	stats->drop_new_dir_hist = 0;
	stats->load_stage = 0;
	update_string(&stats->term_name, NULL);
	stats->term_state = TS_NORMAL;
	stats->ch_pos = 1;
	stats->confirmed = 0;
	stats->skip_shellout_redraw = 0;
	stats->cs = &config->cs;
	strcpy(stats->color_scheme, "");

	stats->preview.on = 0;
	stats->preview.kind = VK_TEXTUAL;
	update_string(&stats->preview.cleanup_cmd, NULL);
	stats->preview.clearing = 0;

	stats->direct_color = 0;

	stats->msg_head = 0;
	stats->msg_tail = 0;
	stats->save_msg_in_list = 1;
	stats->allow_sb_msg_truncation = 1;
	size_t i;
	for(i = 0U; i < ARRAY_LEN(stats->msgs); ++i)
	{
		update_string(&stats->msgs[i], NULL);
	}

	stats->scroll_bind_off = 0;
	stats->split = VSPLIT;
	stats->splitter_pos = -1;
	stats->splitter_ratio = 0.5;

	stats->sourcing_state = SOURCING_NONE;

	stats->restart_in_progress = 0;

	stats->reusing_statusline = 0;

	stats->exec_env_type = EET_EMULATOR;

	stats->term_multiplexer = TM_NONE;

	stats->initial_lines = INT_MIN;
	stats->initial_columns = INT_MIN;

	stats->ellipsis = "...";

	stats->shell_type = ST_NORMAL;

	stats->fuse_umount_cmd = "";

	stats->original_stdout = NULL;

	update_string(&stats->chosen_files_out, NULL);
	update_string(&stats->chosen_dir_out , NULL);
	(void)replace_string(&stats->output_delimiter, "\n");

	update_string(&stats->on_choose, NULL);

	update_string(&stats->last_session, NULL);

	stats->preview_hint = NULL;

	stats->global_local_settings = 0;

	stats->history_size = 0;

	ipc_free(stats->ipc);
	plugs_free(stats->plugs);
	vlua_finish(stats->vlua);

	stats->ipc = NULL;
	stats->vlua = NULL;
	stats->plugs = NULL;

#ifdef HAVE_LIBGTK
	stats->gtk_available = 0;
#endif
}

/* Initializes stats->fuse_umount_cmd field of the stats. */
static void
determine_fuse_umount_cmd(status_t *stats)
{
	if(external_command_exists("fusermount"))
	{
		stats->fuse_umount_cmd = "fusermount -u";
	}
	else if(external_command_exists("umount"))
	{
		/* Some systems use regular umount command for FUSE. */
		stats->fuse_umount_cmd = "umount";
	}
	else
	{
		/* Leave default value. */
	}
}

static void
set_gtk_available(status_t *stats)
{
#ifdef HAVE_LIBGTK
	char *argv[] = { "vifm", NULL };
	int argc = ARRAY_LEN(argv) - 1;
	char **ptr = argv;
	stats->gtk_available = gtk_init_check(&argc, &ptr);
#endif
}

int
stats_reset(const config_t *config)
{
	set_last_cmdline_command("");

	curr_stats.initial_lines = config->lines;
	curr_stats.initial_columns = config->columns;

	return reset_dircache();
}

/* Returns non-zero on error. */
static int
reset_dircache(void)
{
	fsdata_free(dcache_size);
	dcache_size = fsdata_create(0, 1);

	fsdata_free(dcache_nitems);
	dcache_nitems = fsdata_create(0, 1);

	return (dcache_size == NULL || dcache_nitems == NULL);
}

void
stats_set_use_multiplexer(int use_term_multiplexer)
{
	if(!use_term_multiplexer)
	{
		curr_stats.term_multiplexer = TM_NONE;
	}
	else if(inside_screen)
	{
		curr_stats.term_multiplexer = TM_SCREEN;
	}
	else if(inside_tmux)
	{
		curr_stats.term_multiplexer = TM_TMUX;
	}
	else
	{
		curr_stats.term_multiplexer = TM_NONE;
	}
}

void
stats_update_shell_type(const char shell_cmd[])
{
	curr_stats.shell_type = get_shell_type(shell_cmd);
}

TermState
stats_update_term_state(int screen_x, int screen_y)
{
	if(screen_x < MIN_TERM_WIDTH || screen_y < MIN_TERM_HEIGHT)
	{
		curr_stats.term_state = TS_TOO_SMALL;
	}
	else if(curr_stats.term_state != TS_NORMAL)
	{
		curr_stats.term_state = TS_BACK_TO_NORMAL;
	}

	return curr_stats.term_state;
}

void
stats_set_chosen_files_out(const char output[])
{
	(void)replace_string(&curr_stats.chosen_files_out, output);
}

void
stats_set_chosen_dir_out(const char output[])
{
	(void)replace_string(&curr_stats.chosen_dir_out, output);
}

void
stats_set_output_delimiter(const char delimiter[])
{
	(void)replace_string(&curr_stats.output_delimiter, delimiter);
}

void
stats_set_on_choose(const char command[])
{
	(void)replace_string(&curr_stats.on_choose, command);
}

int
stats_file_choose_action_set(void)
{
	return !is_null_or_empty(curr_stats.chosen_files_out)
	    || !is_null_or_empty(curr_stats.on_choose);
}

void
stats_save_msg(const char msg[])
{
	if(!curr_stats.save_msg_in_list || msg[0] == '\0')
	{
		return;
	}

	if(curr_stats.msg_tail != curr_stats.msg_head &&
			strcmp(curr_stats.msgs[curr_stats.msg_tail], msg) == 0)
	{
		return;
	}

	curr_stats.msg_tail = (curr_stats.msg_tail + 1)%ARRAY_LEN(curr_stats.msgs);
	if(curr_stats.msg_tail == curr_stats.msg_head)
	{
		free(curr_stats.msgs[curr_stats.msg_head]);
		curr_stats.msg_head = (curr_stats.msg_head + 1)%ARRAY_LEN(curr_stats.msgs);
	}
	curr_stats.msgs[curr_stats.msg_tail] = strdup(msg);
}

void
stats_set_quickview(int on)
{
	curr_stats.preview.on = on;
	load_quickview_option();
}

void
stats_set_splitter_pos(int position)
{
	double max = (curr_stats.split == HSPLIT ? cfg.lines : cfg.columns);
	double ratio = (position < 0 ? 0.5 : (max == INT_MIN ? -1 : position/max));

	curr_stats.splitter_ratio = ratio;
	if(curr_stats.splitter_pos != position)
	{
		curr_stats.splitter_pos = position;
		stats_redraw_later();
	}
}

void
stats_set_splitter_ratio(double ratio)
{
	if(ratio == -1)
	{
		stats_set_splitter_pos(curr_stats.splitter_pos);
		return;
	}

	curr_stats.splitter_ratio = ratio;

	int max = (curr_stats.split == HSPLIT ? cfg.lines : cfg.columns);
	if(max == INT_MIN)
	{
		/* Can't compute position from ratio, so leave it as is. */
		return;
	}

	int position = (ratio == 0.5 ? -1 : max*ratio + 0.5);
	if(curr_stats.splitter_pos != position)
	{
		curr_stats.splitter_pos = position;
		stats_redraw_later();
	}
}

void
stats_redraw_later(void)
{
	pending_redraw = 1;
}

void
stats_refresh_later(void)
{
	pending_refresh = 1;
}

UpdateType
stats_update_fetch(void)
{
	if(pending_refresh)
	{
		pending_refresh = 0;
		pending_redraw = 0;
		return UT_FULL;
	}
	if(pending_redraw)
	{
		pending_redraw = 0;
		return UT_REDRAW;
	}
	return UT_NONE;
}

int
stats_redraw_planned(void)
{
	return pending_refresh != 0
	    || pending_redraw != 0;
}

void
stats_silence_ui(int more)
{
	if(more)
	{
		++silent_ui;
	}
	else if(--silent_ui == 0)
	{
		stats_unsilence_ui();
	}
	else if(silent_ui < 0)
	{
		silent_ui = 0;
	}
}

int
stats_silenced_ui(void)
{
	if(silent_ui)
	{
		silence_skipped_updates = 1;
		return 1;
	}
	return 0;
}

void
stats_unsilence_ui(void)
{
	silent_ui = 0;
	if(silence_skipped_updates)
	{
		silence_skipped_updates = 0;
		pending_redraw = 0;
		modes_redraw();
	}
}

void
hists_resize(int new_size)
{
	curr_stats.history_size = new_size;

	hist_resize(&curr_stats.exprreg_hist, new_size);
	hist_resize(&curr_stats.search_hist, new_size);
	hist_resize(&curr_stats.cmd_hist, new_size);
	hist_resize(&curr_stats.prompt_hist, new_size);
	hist_resize(&curr_stats.filter_hist, new_size);
}

void
hists_commands_save(const char command[])
{
	if(is_history_command(command))
	{
		if(!curr_stats.restart_in_progress && curr_stats.load_stage == 3)
		{
			set_last_cmdline_command(command);
		}
		hist_add(&curr_stats.cmd_hist, command, -1);
	}
}

/* Sets last_cmdline_command field of the status structure. */
static void
set_last_cmdline_command(const char cmd[])
{
	const int err = replace_string(&curr_stats.last_cmdline_command, cmd);
	if(err != 0)
	{
		LOG_ERROR_MSG("replace_string() failed on duplicating: %s", cmd);
	}
	assert(curr_stats.last_cmdline_command != NULL &&
			"The field was not initialized properly");
}

void
hists_exprreg_save(const char pattern[])
{
	hist_add(&curr_stats.exprreg_hist, pattern, -1);
}

void
hists_search_save(const char pattern[])
{
	hist_add(&curr_stats.search_hist, pattern, -1);
}

void
hists_prompt_save(const char input[])
{
	hist_add(&curr_stats.prompt_hist, input, -1);
}

void
hists_filter_save(const char input[])
{
	hist_add(&curr_stats.filter_hist, input, -1);
}

const char *
hists_search_last(void)
{
	return hist_is_empty(&curr_stats.search_hist)
	     ? ""
	     : curr_stats.search_hist.items[0].text;
}

void
dcache_get_at(const char path[], time_t mtime, uint64_t inode, uint64_t *size,
		uint64_t *nitems)
{
	dcache_result_t size_res, nitems_res;
	dcache_get(path, mtime, inode, (size == NULL ? NULL : &size_res),
			(nitems == NULL ? NULL : &nitems_res));

	if(size != NULL)
	{
		*size = (size_res.is_valid ? size_res.value : DCACHE_UNKNOWN);
	}
	if(nitems != NULL)
	{
		*nitems = (nitems_res.is_valid ? nitems_res.value : DCACHE_UNKNOWN);
	}
}

void
dcache_get_of(const dir_entry_t *entry, dcache_result_t *size,
		dcache_result_t *nitems)
{
	if(size == NULL && nitems == NULL)
	{
		return;
	}

	char full_path[PATH_MAX + 1];
	get_full_path_of(entry, sizeof(full_path), full_path);

	uint64_t inode = get_true_inode(entry);
	dcache_get(full_path, entry->mtime, inode, size, nitems);
}

/* Retrieves information about the path checking whether it's outdated.  size
 * and/or nitems can be NULL. */
static void
dcache_get(const char path[], time_t mtime, uint64_t inode,
		dcache_result_t *size, dcache_result_t *nitems)
{
	if(size != NULL)
	{
		size->value = DCACHE_UNKNOWN;
		size->is_valid = 0;

		pthread_mutex_lock(&dcache_size_mutex);
		dcache_data_t size_data;
		if(fsdata_get(dcache_size, path, &size_data, sizeof(size_data)) == 0)
		{
			size->value = size_data.value;
			/* We check strictly for less than to handle scenario when multiple
			 * changes occurred during the same second. */
			size->is_valid = (mtime < size_data.timestamp);
#ifndef _WIN32
			size->is_valid &= (inode == size_data.inode);
#endif
		}
		pthread_mutex_unlock(&dcache_size_mutex);
	}

	if(nitems != NULL)
	{
		nitems->value = DCACHE_UNKNOWN;
		nitems->is_valid = 0;

		pthread_mutex_lock(&dcache_nitems_mutex);
		dcache_data_t nitems_data;
		if(fsdata_get(dcache_nitems, path, &nitems_data, sizeof(nitems_data)) == 0)
		{
			nitems->value = nitems_data.value;
			/* We check strictly for less than to handle scenario when multiple
			 * changes occurred during the same second. */
			nitems->is_valid = (mtime < nitems_data.timestamp);
#ifndef _WIN32
			nitems->is_valid &= (inode == nitems_data.inode);
#endif
		}
		pthread_mutex_unlock(&dcache_nitems_mutex);
	}
}

void
dcache_update_parent_sizes(const char path[], uint64_t by)
{
	pthread_mutex_lock(&dcache_size_mutex);
	(void)fsdata_map_parents(dcache_size, path, &size_updater, &by);
	pthread_mutex_unlock(&dcache_size_mutex);
}

/* Updates cached value by a fixed amount. */
static void
size_updater(void *data, void *arg)
{
	const uint64_t *const by = arg;
	dcache_data_t *const what = data;

	what->value += *by;
}

int
dcache_set_at(const char path[], uint64_t inode, uint64_t size, uint64_t nitems)
{
	int ret = 0;
	const time_t ts = time(NULL);

	if(size != DCACHE_UNKNOWN)
	{
		dcache_data_t data = { .value = size, .timestamp = ts };
#ifndef _WIN32
		data.inode = (ino_t)inode;
#endif

		pthread_mutex_lock(&dcache_size_mutex);
		ret |= fsdata_set(dcache_size, path, &data, sizeof(data));
		pthread_mutex_unlock(&dcache_size_mutex);
	}

	if(nitems != DCACHE_UNKNOWN)
	{
		dcache_data_t data = { .value = nitems, .timestamp = ts };
#ifndef _WIN32
		data.inode = (ino_t)inode;
#endif

		pthread_mutex_lock(&dcache_nitems_mutex);
		ret |= fsdata_set(dcache_nitems, path, &data, sizeof(data));
		pthread_mutex_unlock(&dcache_nitems_mutex);
	}

	return ret;
}

TSTATIC time_t
dcache_get_size_timestamp(const char path[])
{
	dcache_data_t size_data;
	if(fsdata_get(dcache_size, path, &size_data, sizeof(size_data)) == 0)
	{
		return size_data.timestamp;
	}

	return -1;
}

TSTATIC void
dcache_set_size_timestamp(const char path[], time_t ts)
{
	dcache_data_t size_data;
	if(fsdata_get(dcache_size, path, &size_data, sizeof(size_data)) == 0)
	{
		size_data.timestamp = ts;
		(void)fsdata_set(dcache_size, path, &size_data, sizeof(size_data));
	}
}

void
selhist_put(const char location[], char *paths[], int path_count)
{
	if(path_count == 0)
	{
		free_string_array(paths, 0);
		return;
	}

	char *location_copy = strdup(location);
	if(location_copy == NULL)
	{
		free_string_array(paths, path_count);
		return;
	}

	unsigned int i;
	for(i = 0; i < ARRAY_LEN(sel_hist) - 1; ++i)
	{
		if(sel_hist[i].location != NULL &&
				paths_are_same(sel_hist[i].location, location))
		{
			break;
		}
	}

	/* At this point we have either found a match among all but the last element
	 * or will use the last element (whether it's a match doesn't matter). */

	free(sel_hist[i].location);
	free_string_array(sel_hist[i].paths, sel_hist[i].length);

	sel_hist[i].location = location_copy;
	sel_hist[i].paths = paths;
	sel_hist[i].length = path_count;

	rotate_right(sel_hist, i + 1, sizeof(sel_hist[0]));
}

/* Rotates array's elements to the right once: 0 -> 1, ..., (n - 1) -> 0. */
static void
rotate_right(void *ptr, size_t count, size_t item_len)
{
	assert(count > 0 && "Can't rotate if item count is 0!");
	assert(item_len > 0 && "Can't rotate if item size is 0!");

	size_t slice_size = item_len*(count - 1);
	char *p = ptr;

	/* Stash last item. */
	uint8_t buf[item_len];
	memcpy(buf, p + slice_size, item_len);

	/* Move all but the last item. */
	memmove(p + item_len, p, slice_size);

	/* Put last item in front. */
	memcpy(p, buf, item_len);
}

int
selhist_get(const char location[], char ***paths, int *path_count)
{
	unsigned int i;

	for(i = 0; i < ARRAY_LEN(sel_hist); ++i)
	{
		if(sel_hist[i].location != NULL &&
				paths_are_same(sel_hist[i].location, location))
		{
			*paths = copy_string_array(sel_hist[i].paths, sel_hist[i].length);
			*path_count = sel_hist[i].length;
			return 0;
		}
	}

	return 1;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
