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

#include <assert.h> /* assert() */
#include <limits.h> /* INT_MIN */
#include <stddef.h> /* NULL */
#include <string.h>
#include <time.h> /* time_t time() */

#include "cfg/config.h"
#include "compat/fs_limits.h"
#include "compat/pthread.h"
#include "compat/reallocarray.h"
#include "modes/modes.h"
#include "ui/colors.h"
#include "ui/ui.h"
#include "utils/env.h"
#include "utils/fsdata.h"
#include "utils/log.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/utils.h"
#include "cmd_completion.h"
#include "cmd_core.h"
#include "filelist.h"
#include "filetype.h"
#include "opt_handlers.h"

/* Environment variables by which application hosted by terminal multiplexer can
 * identify the host. */
#define SCREEN_ENVVAR "STY"
#define TMUX_ENVVAR "TMUX"

/* dcache entry. */
typedef struct
{
	uint64_t value;   /* Stored value. */
	time_t timestamp; /* When the value was set. */
}
dcache_data_t;

static void load_def_values(status_t *stats, config_t *config);
static void determine_fuse_umount_cmd(status_t *stats);
static void set_gtk_available(status_t *stats);
static int reset_dircache(void);
static void set_last_cmdline_command(const char cmd[]);
static void save_into_history(const char item[], hist_t *hist, int len);
static void size_updater(void *data, void *arg);

status_t curr_stats;

static int pending_redraw;
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

	(void)hist_init(&curr_stats.cmd_hist, config->history_len);
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
	size_t i;

	pending_redraw = 0;

	stats->need_update = UT_NONE;
	stats->last_char = 0;
	stats->save_msg = 0;
	stats->use_register = 0;
	stats->curr_register = -1;
	stats->register_saved = 0;
	stats->number_of_windows = 2;
	stats->use_input_bar = 1;
	stats->drop_new_dir_hist = 0;
	stats->load_stage = 0;
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

	stats->msg_head = 0;
	stats->msg_tail = 0;
	stats->save_msg_in_list = 1;
	stats->allow_sb_msg_truncation = 1;
	for(i = 0U; i < ARRAY_LEN(stats->msgs); ++i)
	{
		update_string(&stats->msgs[i], NULL);
	}

	stats->scroll_bind_off = 0;
	stats->split = VSPLIT;
	stats->splitter_pos = -1;

	stats->sourcing_state = SOURCING_NONE;

	stats->restart_in_progress = 0;

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

	stats->preview_hint = NULL;

	stats->global_local_settings = 0;

	stats->history_size = 0;

	stats->ipc = NULL;

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
stats_redraw_schedule(void)
{
	pending_redraw = 1;
}

int
stats_redraw_fetch(void)
{
	if(pending_redraw)
	{
		pending_redraw = 0;
		return 1;
	}
	return 0;
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
stats_silence_ui(int more)
{
	if(more)
	{
		++silent_ui;
	}
	else if(--silent_ui == 0)
	{
		if(silence_skipped_updates)
		{
			silence_skipped_updates = 0;
			pending_redraw = 0;
			modes_redraw();
		}
	}

	assert(silent_ui >= 0 && "Unbalanced calls to stats_silence_ui()");
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
hists_resize(int new_size)
{
	const int old_size = curr_stats.history_size;
	const int delta = new_size - old_size;

	if(new_size <= 0)
	{
		hist_reset(&curr_stats.search_hist, old_size);
		hist_reset(&curr_stats.cmd_hist, old_size);
		hist_reset(&curr_stats.prompt_hist, old_size);
		hist_reset(&curr_stats.filter_hist, old_size);
		curr_stats.history_size = 0;
		return;
	}

	curr_stats.history_size = new_size;

	if(delta < 0)
	{
		hist_trunc(&curr_stats.search_hist, new_size, -delta);
		hist_trunc(&curr_stats.cmd_hist, new_size, -delta);
		hist_trunc(&curr_stats.prompt_hist, new_size, -delta);
		hist_trunc(&curr_stats.filter_hist, new_size, -delta);
	}

	curr_stats.cmd_hist.items = reallocarray(curr_stats.cmd_hist.items, new_size,
			sizeof(char *));
	curr_stats.search_hist.items = reallocarray(curr_stats.search_hist.items,
			new_size, sizeof(char *));
	curr_stats.prompt_hist.items = reallocarray(curr_stats.prompt_hist.items,
			new_size, sizeof(char *));
	curr_stats.filter_hist.items = reallocarray(curr_stats.filter_hist.items,
			new_size, sizeof(char *));

	if(delta > 0)
	{
		const size_t str_item_len = sizeof(char *)*delta;

		memset(curr_stats.cmd_hist.items + old_size, 0, str_item_len);
		memset(curr_stats.search_hist.items + old_size, 0, str_item_len);
		memset(curr_stats.prompt_hist.items + old_size, 0, str_item_len);
		memset(curr_stats.filter_hist.items + old_size, 0, str_item_len);
	}
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
		save_into_history(command, &curr_stats.cmd_hist, curr_stats.history_size);
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
hists_search_save(const char pattern[])
{
	save_into_history(pattern, &curr_stats.search_hist, curr_stats.history_size);
}

void
hists_prompt_save(const char input[])
{
	save_into_history(input, &curr_stats.prompt_hist, curr_stats.history_size);
}

void
hists_filter_save(const char input[])
{
	save_into_history(input, &curr_stats.filter_hist, curr_stats.history_size);
}

/* Adaptor for the hist_add() function, which handles signed history length. */
static void
save_into_history(const char item[], hist_t *hist, int len)
{
	if(len >= 0)
	{
		hist_add(hist, item, len);
	}
}

const char *
hists_search_last(void)
{
	return hist_is_empty(&curr_stats.search_hist)
	     ? ""
	     : curr_stats.search_hist.items[0];
}

void
dcache_get_at(const char path[], uint64_t *size, uint64_t *nitems)
{
	/* Initialization to make condition false by default. */
	dcache_data_t size_data, nitems_data;

	pthread_mutex_lock(&dcache_size_mutex);
	if(fsdata_get(dcache_size, path, &size_data, sizeof(size_data)) != 0)
	{
		size_data.value = DCACHE_UNKNOWN;
	}
	pthread_mutex_unlock(&dcache_size_mutex);

	pthread_mutex_lock(&dcache_nitems_mutex);
	if(fsdata_get(dcache_nitems, path, &nitems_data, sizeof(nitems_data)) != 0)
	{
		nitems_data.value = DCACHE_UNKNOWN;
	}
	pthread_mutex_unlock(&dcache_nitems_mutex);

	if(size != NULL)
	{
		*size = size_data.value;
	}
	if(nitems != NULL)
	{
		*nitems = nitems_data.value;
	}
}

void
dcache_get_of(const dir_entry_t *entry, dcache_result_t *size,
		dcache_result_t *nitems)
{
	dcache_data_t size_data, nitems_data;

	char full_path[PATH_MAX + 1];
	get_full_path_of(entry, sizeof(full_path), full_path);

	size->value = DCACHE_UNKNOWN;
	size->is_valid = 0;

	nitems->value = DCACHE_UNKNOWN;
	nitems->is_valid = 0;

	pthread_mutex_lock(&dcache_size_mutex);
	if(fsdata_get(dcache_size, full_path, &size_data, sizeof(size_data)) == 0)
	{
		size->value = size_data.value;
		/* We check strictly for less than to handle scenario when multiple changes
		 * occurred during the same second. */
		size->is_valid = (entry->mtime < size_data.timestamp);
	}
	pthread_mutex_unlock(&dcache_size_mutex);

	pthread_mutex_lock(&dcache_nitems_mutex);
	if(fsdata_get(dcache_nitems, full_path, &nitems_data,
				sizeof(nitems_data)) == 0)
	{
		nitems->value = nitems_data.value;
		/* We check strictly for less than to handle scenario when multiple changes
		 * occurred during the same second. */
		nitems->is_valid = (entry->mtime < nitems_data.timestamp);
	}
	pthread_mutex_unlock(&dcache_nitems_mutex);
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
dcache_set_at(const char path[], uint64_t size, uint64_t nitems)
{
	int ret = 0;
	const time_t ts = time(NULL);

	if(size != DCACHE_UNKNOWN)
	{
		const dcache_data_t data = { .value = size, .timestamp = ts };

		pthread_mutex_lock(&dcache_size_mutex);
		ret |= fsdata_set(dcache_size, path, &data, sizeof(data));
		pthread_mutex_unlock(&dcache_size_mutex);
	}

	if(nitems != DCACHE_UNKNOWN)
	{
		const dcache_data_t data = { .value = nitems, .timestamp = ts };

		pthread_mutex_lock(&dcache_nitems_mutex);
		ret |= fsdata_set(dcache_nitems, path, &data, sizeof(data));
		pthread_mutex_unlock(&dcache_nitems_mutex);
	}

	return ret;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
