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

#include "info.h"

#include <assert.h> /* assert() */
#include <ctype.h> /* isdigit() */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* FILE fpos_t fclose() fgetpos() fgets() fprintf() fputc()
                      fscanf() fsetpos() snprintf() */
#include <stdlib.h> /* abs() free() */
#include <string.h> /* memcpy() memset() strtol() strcmp() strchr() strlen() */

#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "../compat/reallocarray.h"
#include "../engine/cmds.h"
#include "../engine/options.h"
#include "../io/iop.h"
#include "../ui/fileview.h"
#include "../ui/ui.h"
#include "../utils/file_streams.h"
#include "../utils/filemon.h"
#include "../utils/filter.h"
#include "../utils/fs.h"
#include "../utils/hist.h"
#include "../utils/log.h"
#include "../utils/macros.h"
#include "../utils/matcher.h"
#include "../utils/matchers.h"
#include "../utils/parson.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../utils/trie.h"
#include "../utils/utils.h"
#include "../bmarks.h"
#include "../cmd_core.h"
#include "../dir_stack.h"
#include "../filelist.h"
#include "../flist_hist.h"
#include "../filetype.h"
#include "../filtering.h"
#include "../marks.h"
#include "../opt_handlers.h"
#include "../registers.h"
#include "../status.h"
#include "../trash.h"
#include "config.h"
#include "info_chars.h"

static void load_state(JSON_Object *root, int reread);
static void load_gtab(JSON_Object *gtab, int reread);
static void load_pane(JSON_Object *pane, view_t *view, int reread);
static void load_dhistory(JSON_Object *info, view_t *view, int reread);
static void load_filters(JSON_Object *pane, view_t *view);
static void load_options(JSON_Object *parent);
static void load_assocs(JSON_Object *root, const char node[], int for_x);
static void load_viewers(JSON_Object *root);
static void load_cmds(JSON_Object *root);
static void load_marks(JSON_Object *root);
static void load_bmarks(JSON_Object *root);
static void load_regs(JSON_Object *root);
static void load_dir_stack(JSON_Object *root);
static void load_trash(JSON_Object *root);
static void load_history(JSON_Object *root, const char node[], hist_t *hist,
		void (*saver)(const char[]));
static void get_sort_info(view_t *view, const char line[]);
static void append_to_history(hist_t *hist, void (*saver)(const char[]),
		const char item[]);
static void ensure_history_not_full(hist_t *hist);
static void get_history(view_t *view, int reread, const char dir[],
		const char file[], int rel_pos);
static void set_manual_filter(view_t *view, const char value[]);
static int copy_file(const char src[], const char dst[]);
static void update_info_file(const char filename[], int merge);
static void update_info_file_json(const char filename[], int merge);
static JSON_Value * serialize_state(void);
static void merge_states(JSON_Object *current, JSON_Object *admixture);
static void merge_tabs(JSON_Object *current, JSON_Object *admixture);
static void merge_dhistory(JSON_Object *current, JSON_Object *admixture,
		const view_t *view);
static void merge_commands(JSON_Object *current, JSON_Object *admixture);
static void merge_marks(JSON_Object *current, JSON_Object *admixture);
static void merge_bmarks(JSON_Object *current, JSON_Object *admixture);
static void merge_history(JSON_Object *current, JSON_Object *admixture,
		const char node[]);
static void merge_regs(JSON_Object *current, JSON_Object *admixture);
static void merge_dir_stack(JSON_Object *current, JSON_Object *admixture);
static void merge_trash(JSON_Object *current, JSON_Object *admixture);
static void store_gtab(JSON_Object *gtab);
static void store_view(JSON_Object *view_data, view_t *view);
static void store_filters(JSON_Object *view_data, view_t *view);
static void store_history(JSON_Object *root, const char node[],
		const hist_t *hist);
static void store_global_options(JSON_Object *root);
static void store_view_options(JSON_Object *parent, view_t *view);
static void store_assocs(JSON_Object *root, const char node[],
		assoc_list_t *assocs);
static void store_cmds(JSON_Object *root);
static void store_marks(JSON_Object *root);
static void store_bmarks(JSON_Object *root);
static void store_bmark(const char path[], const char tags[], time_t timestamp,
		void *arg);
static void store_regs(JSON_Object *root);
static void store_dir_stack(JSON_Object *root);
static void store_trash(JSON_Object *root);
static void process_hist_entry(view_t *view, const char dir[],
		const char file[], int pos, char ***lh, int *nlh, int **lhp, size_t *nlhp);
static char * convert_old_trash_path(const char trash_path[]);
static void write_options(FILE *fp);
static void write_assocs(FILE *fp, const char str[], char mark,
		assoc_list_t *assocs, int prev_count, char *prev[]);
static void write_doubling_commas(FILE *fp, const char str[]);
static void write_commands(FILE *fp, char *cmds_list[], char *cmds[],
		int ncmds);
static void write_marks(FILE *fp, const char non_conflicting_marks[],
		char *marks[], const int timestamps[], int nmarks);
static void write_bmarks(FILE *fp, char *bmarks[], const int timestamps[],
		int nbmarks);
static void write_bmark(const char path[], const char tags[], time_t timestamp,
		void *arg);
static void write_tui_state(FILE *fp);
static void write_view_history(FILE *fp, view_t *view, const char str[],
		char mark, int prev_count, char *prev[], int pos[]);
static void store_dhistory(JSON_Object *obj, view_t *view);
static void write_history(FILE *fp, const char str[], char mark, int prev_count,
		char *prev[], const hist_t *hist);
static void write_registers(FILE *fp, char *regs[], int nregs);
static void write_dir_stack(FILE *fp, char *old_dir_stack[],
		int nold_dir_stack);
static void write_trash(FILE *fp, char *trash[], int ntrash);
static void write_general_state(FILE *fp);
static char * read_vifminfo_line(FILE *fp, char buffer[]);
static void remove_leading_whitespace(char line[]);
static const char * escape_spaces(const char *str);
static void put_sort_info(FILE *fp, char leading_char, const view_t *view);
static void store_sort_info(JSON_Object *obj, const view_t *view);
static void make_sort_info(const view_t *view, char buf[], size_t buf_size);
static int read_optional_number(FILE *f);
static int read_number(const char line[], long *value);
static size_t add_to_int_array(int **array, size_t len, int what);
static JSON_Array * add_array(JSON_Object *obj, const char key[]);
static JSON_Object * add_object(JSON_Object *obj, const char key[]);
static JSON_Object * append_object(JSON_Array *arr);
static int get_bool(const JSON_Object *obj, const char key[], int *value);
static int get_int(const JSON_Object *obj, const char key[], int *value);
static int get_double(const JSON_Object *obj, const char key[], double *value);
static int get_str(const JSON_Object *obj, const char key[], const char **value);
static void set_bool(JSON_Object *obj, const char key[], int value);
static void set_int(JSON_Object *obj, const char key[], int value);
static void set_double(JSON_Object *obj, const char key[], double value);
static void set_str(JSON_Object *obj, const char key[], const char value[]);
static void append_dstr(JSON_Array *array, char value[]);

/* Monitor to check for changes of vifminfo file. */
static filemon_t vifminfo_mon;

void
read_info_file(int reread)
{
	/* TODO: refactor this function read_info_file() */

	FILE *fp;
	char info_file[PATH_MAX + 16];
	char *line = NULL, *line2 = NULL, *line3 = NULL, *line4 = NULL;

	snprintf(info_file, sizeof(info_file), "%s/vifminfo", cfg.config_dir);

	if((fp = os_fopen(info_file, "r")) == NULL)
		return;

	(void)filemon_from_file(info_file, FMT_MODIFIED, &vifminfo_mon);

	JSON_Value *root_value = json_value_init_object();
	JSON_Object *root = json_object(root_value);

	JSON_Array *options = add_array(root, "options");
	JSON_Array *assocs = add_array(root, "assocs");
	JSON_Array *xassocs = add_array(root, "xassocs");
	JSON_Array *viewers = add_array(root, "viewers");
	JSON_Object *cmds = add_object(root, "cmds");
	JSON_Object *marks = add_object(root, "marks");
	JSON_Object *bmarks = add_object(root, "bmarks");
	JSON_Array *cmd_hist = add_array(root, "cmd-hist");
	JSON_Array *search_hist = add_array(root, "search-hist");
	JSON_Array *prompt_hist = add_array(root, "prompt-hist");
	JSON_Array *lfilt_hist = add_array(root, "lfilt-hist");
	JSON_Array *dir_stack = add_array(root, "dir-stack");
	JSON_Array *trash = add_array(root, "trash");
	JSON_Object *regs = add_object(root, "regs");

	JSON_Array *outer_tabs = add_array(root, "tabs");
	JSON_Object *outer_tab = append_object(outer_tabs);

	JSON_Object *splitter = add_object(outer_tab, "splitter");

	JSON_Array *panes = add_array(outer_tab, "panes");
	JSON_Object *left = append_object(panes);
	JSON_Object *right = append_object(panes);

	JSON_Array *left_tabs = add_array(left, "tabs");
	JSON_Object *left_tab = append_object(left_tabs);

	JSON_Array *right_tabs = add_array(right, "tabs");
	JSON_Object *right_tab = append_object(right_tabs);

	JSON_Array *left_history = add_array(left_tab, "history");
	JSON_Array *right_history = add_array(right_tab, "history");

	JSON_Object *left_filters = add_object(left_tab, "filters");
	JSON_Object *right_filters = add_object(right_tab, "filters");

	JSON_Array *left_options = add_array(left_tab, "options");
	JSON_Array *right_options = add_array(right_tab, "options");

	while((line = read_vifminfo_line(fp, line)) != NULL)
	{
		const char type = line[0];
		const char *const line_val = line + 1;

		if(type == LINE_TYPE_COMMENT || type == '\0')
			continue;

		if(type == LINE_TYPE_OPTION)
		{
			if(line_val[0] == '[')
			{
				json_array_append_string(left_options, line_val + 1);
			}
			else if(line_val[0] == ']')
			{
				json_array_append_string(right_options, line_val + 1);
			}
			else
			{
				json_array_append_string(options, line_val);
			}
		}
		else if(type == LINE_TYPE_FILETYPE || type == LINE_TYPE_XFILETYPE ||
				type == LINE_TYPE_FILEVIEWER)
		{
			JSON_Array *array;
			switch(type)
			{
				case LINE_TYPE_FILETYPE:   array = assocs; break;
				case LINE_TYPE_XFILETYPE:  array = xassocs; break;
				case LINE_TYPE_FILEVIEWER: array = viewers; break;
			}

			if((line2 = read_vifminfo_line(fp, line2)) != NULL)
			{
				/* Prevent loading of old builtin fake associations. */
				if(type != LINE_TYPE_FILEVIEWER &&
						ends_with(line2, "}" VIFM_PSEUDO_CMD))
				{
					continue;
				}

				JSON_Object *entry = append_object(array);
				set_str(entry, "matchers", line_val);
				set_str(entry, "cmd", line2);
			}
		}
		else if(type == LINE_TYPE_COMMAND)
		{
			if((line2 = read_vifminfo_line(fp, line2)) != NULL)
			{
				json_object_set_string(cmds, line_val, line2);
			}
		}
		else if(type == LINE_TYPE_MARK)
		{
			if((line2 = read_vifminfo_line(fp, line2)) != NULL)
			{
				if((line3 = read_vifminfo_line(fp, line3)) != NULL)
				{
					int timestamp = read_optional_number(fp);
					if(timestamp == -1)
					{
						timestamp = time(NULL);
					}

					char name[] = { line_val[0], '\0' };
					JSON_Object *mark = add_object(marks, name);
					set_str(mark, "dir", line2);
					set_str(mark, "file", line3);
					set_double(mark, "ts", timestamp);
				}
			}
		}
		else if(type == LINE_TYPE_BOOKMARK)
		{
			if((line2 = read_vifminfo_line(fp, line2)) != NULL)
			{
				long timestamp;
				if((line3 = read_vifminfo_line(fp, line3)) != NULL &&
						read_number(line3, &timestamp))
				{
					JSON_Object *bmark = add_object(bmarks, line_val);
					set_str(bmark, "tags", line2);
					set_double(bmark, "ts", timestamp);
				}
			}
		}
		else if(type == LINE_TYPE_ACTIVE_VIEW)
		{
			set_int(outer_tab, "active-pane", (line_val[0] == 'l' ? 0 : 1));
		}
		else if(type == LINE_TYPE_QUICK_VIEW_STATE)
		{
			set_bool(outer_tab, "preview", atoi(line_val));
		}
		else if(type == LINE_TYPE_WIN_COUNT)
		{
			set_bool(splitter, "expanded", atoi(line_val) == 1);
		}
		else if(type == LINE_TYPE_SPLIT_ORIENTATION)
		{
			set_str(splitter, "orientation", line_val[0] == 'v' ? "v" : "h");
		}
		else if(type == LINE_TYPE_SPLIT_POSITION)
		{
			set_int(splitter, "pos", atoi(line_val));
		}
		else if(type == LINE_TYPE_LWIN_SORT)
		{
			set_str(left_tab, "sorting", line_val);
		}
		else if(type == LINE_TYPE_RWIN_SORT)
		{
			set_str(right_tab, "sorting", line_val);
		}
		else if(type == LINE_TYPE_LWIN_HIST || type == LINE_TYPE_RWIN_HIST)
		{
			if(line_val[0] == '\0')
			{
				JSON_Object *itab = (type == LINE_TYPE_LWIN_HIST)
				                  ? left_tab : right_tab;
				set_bool(itab, "restore-last-location", 1);
			}
			else if((line2 = read_vifminfo_line(fp, line2)) != NULL)
			{
				const int rel_pos = read_optional_number(fp);

				JSON_Array *hist = (type == LINE_TYPE_LWIN_HIST) ? left_history
				                                                 : right_history;
				JSON_Object *entry = append_object(hist);
				set_str(entry, "dir", line_val);
				set_str(entry, "file", line2);
				set_int(entry, "relpos", rel_pos);
			}
		}
		else if(type == LINE_TYPE_CMDLINE_HIST)
		{
			json_array_append_string(cmd_hist, line_val);
		}
		else if(type == LINE_TYPE_SEARCH_HIST)
		{
			json_array_append_string(search_hist, line_val);
		}
		else if(type == LINE_TYPE_PROMPT_HIST)
		{
			json_array_append_string(prompt_hist, line_val);
		}
		else if(type == LINE_TYPE_FILTER_HIST)
		{
			json_array_append_string(lfilt_hist, line_val);
		}
		else if(type == LINE_TYPE_DIR_STACK)
		{
			if((line2 = read_vifminfo_line(fp, line2)) != NULL)
			{
				if((line3 = read_vifminfo_line(fp, line3)) != NULL)
				{
					if((line4 = read_vifminfo_line(fp, line4)) != NULL)
					{
						JSON_Object *entry = append_object(dir_stack);
						set_str(entry, "left-dir", line_val);
						set_str(entry, "left-file", line2);
						set_str(entry, "right-dir", line3 + 1);
						set_str(entry, "right-file", line4);
					}
				}
			}
		}
		else if(type == LINE_TYPE_TRASH)
		{
			if((line2 = read_vifminfo_line(fp, line2)) != NULL)
			{
				char *const trash_name = convert_old_trash_path(line_val);

				JSON_Object *entry = append_object(trash);
				set_str(entry, "trashed", trash_name);
				set_str(entry, "original", line2);

				free(trash_name);
			}
		}
		else if(type == LINE_TYPE_REG)
		{
			char *pos = strchr(valid_registers, line_val[0]);
			if(pos != NULL)
			{
				char name[] = { line_val[0], '\0' };
				JSON_Array *files = json_object_get_array(regs, name);
				if(files == NULL)
				{
					files = add_array(regs, name);
				}

				json_array_append_string(files, line_val + 1);
			}
		}
		else if(type == LINE_TYPE_LWIN_FILT)
		{
			set_str(left_filters, "manual", line_val);
		}
		else if(type == LINE_TYPE_RWIN_FILT)
		{
			set_str(right_filters, "manual", line_val);
		}
		else if(type == LINE_TYPE_LWIN_FILT_INV)
		{
			set_bool(left_filters, "invert", atoi(line_val));
		}
		else if(type == LINE_TYPE_RWIN_FILT_INV)
		{
			set_bool(right_filters, "invert", atoi(line_val));
		}
		else if(type == LINE_TYPE_USE_SCREEN)
		{
			set_bool(root, "use-term-multiplexer", atoi(line_val));
		}
		else if(type == LINE_TYPE_COLORSCHEME)
		{
			set_str(root, "color-scheme", line_val);
		}
		else if(type == LINE_TYPE_LWIN_SPECIFIC || type == LINE_TYPE_RWIN_SPECIFIC)
		{
			JSON_Object *info = (type == LINE_TYPE_LWIN_SPECIFIC)
			                  ? left_tab : right_tab;

			if(line_val[0] == PROP_TYPE_DOTFILES)
			{
				set_bool(info, "dot", atoi(line_val + 1));
			}
			else if(line_val[0] == PROP_TYPE_AUTO_FILTER)
			{
				set_str(info, "auto", line_val + 1);
			}
		}
	}

	free(line);
	free(line2);
	free(line3);
	free(line4);
	fclose(fp);

	load_state(root, reread);
	json_value_free(root_value);

	dir_stack_freeze();
}

/* Loads state of the application from JSON. */
static void
load_state(JSON_Object *root, int reread)
{
	int use_term_multiplexer;
	if(get_bool(root, "use-term-multiplexer", &use_term_multiplexer))
	{
		cfg_set_use_term_multiplexer(use_term_multiplexer);
	}

	const char *cs;
	if(get_str(root, "color-scheme", &cs))
	{
		copy_str(curr_stats.color_scheme, sizeof(curr_stats.color_scheme), cs);
	}

	JSON_Array *gtabs = json_object_get_array(root, "tabs");
	int i, n;
	for(i = 0, n = json_array_get_count(gtabs); i < n; ++i)
	{
		/* TODO: switch to appropriate global tab. */
		load_gtab(json_array_get_object(gtabs, i), reread);
	}

	load_options(root);
	load_assocs(root, "assocs", 0);
	load_assocs(root, "xassocs", 1);
	load_viewers(root);
	load_cmds(root);
	load_marks(root);
	load_bmarks(root);
	load_regs(root);
	load_dir_stack(root);
	load_trash(root);
	load_history(root, "cmd-hist", &curr_stats.cmd_hist, &hists_commands_save);
	load_history(root, "search-hist", &curr_stats.search_hist, &hists_search_save);
	load_history(root, "prompt-hist", &curr_stats.prompt_hist, &hists_prompt_save);
	load_history(root, "lfilt-hist", &curr_stats.filter_hist, &hists_filter_save);
}

/* Loads a global tab from JSON. */
static void
load_gtab(JSON_Object *gtab, int reread)
{
	JSON_Array *panes = json_object_get_array(gtab, "panes");
	load_pane(json_array_get_object(panes, 0), &lwin, reread);
	load_pane(json_array_get_object(panes, 1), &rwin, reread);

	int preview;
	if(get_bool(gtab, "preview", &preview))
	{
		stats_set_quickview(preview);
	}

	JSON_Object *splitter = json_object_get_object(gtab, "splitter");

	const char *split_kind;
	if(get_str(splitter, "orientation", &split_kind))
	{
		curr_stats.split = (split_kind[0] == 'v' ? VSPLIT : HSPLIT);
	}
	get_int(splitter, "pos", &curr_stats.splitter_pos);

	/* Don't change some properties on :restart command. */
	if(!reread)
	{
		int active_pane;
		if(get_int(gtab, "active-pane", &active_pane) && active_pane == 1)
		{
			/* TODO: why is this not the last statement in the block? */
			ui_views_update_titles();

			curr_view = &rwin;
			other_view = &lwin;
		}

		int expanded;
		if(get_bool(splitter, "expanded", &expanded))
		{
			curr_stats.number_of_windows = (expanded ? 1 : 2);
		}
	}
}

/* Loads a pane (consists of pane tabs) from JSON. */
static void
load_pane(JSON_Object *pane, view_t *view, int reread)
{
	JSON_Array *itabs = json_object_get_array(pane, "tabs");
	int i, n;
	for(i = 0, n = json_array_get_count(itabs); i < n; ++i)
	{
		/* TODO: switch to appropriate pane tab. */

		JSON_Object *itab = json_array_get_object(itabs, i);

		load_dhistory(itab, view, reread);
		load_filters(itab, view);

		view_t *v = curr_view;
		curr_view = view;
		load_options(itab);
		curr_view = v;

		const char *sorting;
		if(get_str(itab, "sorting", &sorting))
		{
			get_sort_info(view, sorting);
		}
	}
}

/* Loads directory history of a view from JSON. */
static void
load_dhistory(JSON_Object *info, view_t *view, int reread)
{
	JSON_Array *history = json_object_get_array(info, "history");

	int i, n;
	const char *last_dir = NULL;
	for(i = 0, n = json_array_get_count(history); i < n; ++i)
	{
		JSON_Object *entry = json_array_get_object(history, i);

		const char *dir, *file;
		int rel_pos;
		if(get_str(entry, "dir", &dir) && get_str(entry, "file", &file) &&
				get_int(entry, "relpos", &rel_pos));
		{
			get_history(view, reread, dir, file, rel_pos < 0 ? 0 : rel_pos);
			last_dir = dir;
		}
	}

	int restore_last_location;
	if(get_bool(info, "restore-last-location", &restore_last_location))
	{
		if(!reread && restore_last_location && last_dir != NULL)
		{
			copy_str(view->curr_dir, sizeof(view->curr_dir), last_dir);
		}
	}
}

/* Loads state of filters of a view from JSON. */
static void
load_filters(JSON_Object *pane, view_t *view)
{
	JSON_Object *filters = json_object_get_object(pane, "filters");
	if(filters == NULL)
	{
		return;
	}

	get_bool(filters, "invert", &view->invert);

	int dot;
	if(get_bool(filters, "invert", &dot))
	{
		dot_filter_set(view, !dot);
	}

	const char *filter;

	if(get_str(filters, "manual", &filter))
	{
		set_manual_filter(view, filter);
	}

	if(get_str(filters, "auto", &filter))
	{
		if(filter_set(&view->auto_filter, filter) != 0)
		{
			LOG_ERROR_MSG("Error setting auto filename filter to: %s", filter);
		}
	}
}

/* Loads options from JSON. */
static void
load_options(JSON_Object *parent)
{
	JSON_Array *options = json_object_get_array(parent, "options");

	int i, n;
	for(i = 0, n = json_array_get_count(options); i < n; ++i)
	{
		process_set_args(json_array_get_string(options, i), 1, 1);
	}
}

/* Loads file associations from JSON. */
static void
load_assocs(JSON_Object *root, const char node[], int for_x)
{
	JSON_Array *entries = json_object_get_array(root, node);

	int i, n;
	int in_x = (curr_stats.exec_env_type == EET_EMULATOR_WITH_X);
	for(i = 0, n = json_array_get_count(entries); i < n; ++i)
	{
		JSON_Object *entry = json_array_get_object(entries, i);

		const char *matchers, *cmd;
		if(get_str(entry, "matchers", &matchers) && get_str(entry, "cmd", &cmd))
		{
			char *error;
			matchers_t *const ms = matchers_alloc(matchers, 0, 1, "", &error);
			if(ms == NULL)
			{
				LOG_ERROR_MSG("Error with matchers of an assoc `%s`: %s", matchers,
						error);
				free(error);
			}
			else
			{
				ft_set_programs(ms, cmd, for_x, in_x);
			}
		}
	}
}

/* Loads file viewers from JSON. */
static void
load_viewers(JSON_Object *root)
{
	JSON_Array *viewers = json_object_get_array(root, "viewers");

	int i, n;
	for(i = 0, n = json_array_get_count(viewers); i < n; ++i)
	{
		JSON_Object *viewer = json_array_get_object(viewers, i);

		const char *matchers, *cmd;
		if(get_str(viewer, "matchers", &matchers) && get_str(viewer, "cmd", &cmd))
		{
			char *error;
			matchers_t *const ms = matchers_alloc(matchers, 0, 1, "", &error);
			if(ms == NULL)
			{
				LOG_ERROR_MSG("Error with matchers of a viewer `%s`: %s", matchers,
						error);
				free(error);
			}
			else
			{
				ft_set_viewers(ms, cmd);
			}
		}
	}
}

/* Loads :commands from JSON. */
static void
load_cmds(JSON_Object *root)
{
	JSON_Object *cmds = json_object_get_object(root, "cmds");

	int i, n;
	for(i = 0, n = json_object_get_count(cmds); i < n; ++i)
	{
		const char *name = json_object_get_name(cmds, i);
		const char *cmd = json_string(json_object_get_value_at(cmds, i));

		char *cmdadd_cmd = format_str("command %s %s", name, cmd);
		if(cmdadd_cmd != NULL)
		{
			exec_commands(cmdadd_cmd, curr_view, CIT_COMMAND);
			free(cmdadd_cmd);
		}
	}
}

/* Loads marks from JSON. */
static void
load_marks(JSON_Object *root)
{
	JSON_Object *marks = json_object_get_object(root, "marks");

	int i, n;
	for(i = 0, n = json_object_get_count(marks); i < n; ++i)
	{
		const char *name = json_object_get_name(marks, i);
		JSON_Object *mark =  json_object(json_object_get_value_at(marks, i));

		const char *dir, *file;
		double ts;
		if(get_str(mark, "dir", &dir) && get_str(mark, "file", &file) &&
				get_double(mark, "ts", &ts))
		{
			setup_user_mark(name[0], dir, file, (time_t)ts);
		}
	}
}

/* Loads bookmarks from JSON. */
static void
load_bmarks(JSON_Object *root)
{
	JSON_Object *bmarks = json_object_get_object(root, "bmarks");

	int i, n;
	for(i = 0, n = json_object_get_count(bmarks); i < n; ++i)
	{
		const char *path = json_object_get_name(bmarks, i);
		JSON_Object *bmark =  json_object(json_object_get_value_at(bmarks, i));

		const char *tags;
		double ts;
		if(get_str(bmark, "tags", &tags) && get_double(bmark, "ts", &ts))
		{
			if(bmarks_setup(path, tags, (time_t)ts) != 0)
			{
				LOG_ERROR_MSG("Can't add a bookmark: %s (%s)", path, tags);
			}
		}
	}
}

/* Loads registers from JSON. */
static void
load_regs(JSON_Object *root)
{
	JSON_Object *regs = json_object_get_object(root, "regs");

	int i, n;
	for(i = 0, n = json_object_get_count(regs); i < n; ++i)
	{
		int j, m;
		const char *name = json_object_get_name(regs, i);
		JSON_Array *files = json_array(json_object_get_value_at(regs, i));
		for(j = 0, m = json_array_get_count(files); j < m; ++j)
		{
			regs_append(name[0], json_array_get_string(files, j));
		}
	}
}

/* Loads directory stack from JSON. */
static void
load_dir_stack(JSON_Object *root)
{
	JSON_Array *entries = json_object_get_array(root, "dir-stack");

	int i, n;
	for(i = 0, n = json_array_get_count(entries); i < n; ++i)
	{
		JSON_Object *entry = json_array_get_object(entries, i);

		const char *left_dir, *left_file, *right_dir, *right_file;
		if(get_str(entry, "left-dir", &left_dir) &&
				get_str(entry, "left-file", &left_file) &&
				get_str(entry, "right-dir", &right_dir) &&
				get_str(entry, "right-file", &right_file))
		{
			dir_stack_push(left_dir, left_file, right_dir, right_file);
		}
	}
}

/* Loads trash from JSON. */
static void
load_trash(JSON_Object *root)
{
	JSON_Array *trash = json_object_get_array(root, "trash");

	int i, n;
	for(i = 0, n = json_array_get_count(trash); i < n; ++i)
	{
		JSON_Object *entry = json_array_get_object(trash, i);

		const char *trashed, *original;
		if(get_str(entry, "trashed", &trashed) &&
				get_str(entry, "original", &original))
		{
			(void)trash_add_entry(original, trashed);
		}
	}
}

/* Loads history data from JSON. */
static void
load_history(JSON_Object *root, const char node[], hist_t *hist,
		void (*saver)(const char[]))
{
	JSON_Array *entries = json_object_get_array(root, node);

	int i, n;
	for(i = 0, n = json_array_get_count(entries); i < n; ++i)
	{
		const char *item = json_array_get_string(entries, i);
		append_to_history(&curr_stats.filter_hist, saver, item);
	}
}

/* Parses sort description line of the view and initialized its sort field. */
static void
get_sort_info(view_t *view, const char line[])
{
	signed char *const sort = curr_stats.restart_in_progress
	                        ? ui_view_sort_list_get(view, view->sort)
	                        : view->sort;

	int j = 0;
	while(*line != '\0' && j < SK_COUNT)
	{
		char *endptr;
		const int sort_opt = strtol(line, &endptr, 10);
		if(endptr != line)
		{
			line = endptr;
			view->sort_g[j++] = MIN(SK_LAST, MAX(-SK_LAST, sort_opt));
		}
		else
		{
			line++;
		}
		line = skip_char(line, ',');
	}
	memset(&view->sort_g[j], SK_NONE, sizeof(view->sort_g) - j);
	if(j == 0)
	{
		view->sort_g[0] = SK_DEFAULT;
	}
	memcpy(sort, view->sort_g, sizeof(view->sort));

	fview_sorting_updated(view);
}

/* Appends item to the hist extending the history to fit it if needed. */
static void
append_to_history(hist_t *hist, void (*saver)(const char[]),
		const char item[])
{
	ensure_history_not_full(hist);
	saver(item);
}

/* Checks that history has at least one more empty slot or extends history by
 * one more element. */
static void
ensure_history_not_full(hist_t *hist)
{
	if(hist->pos + 1 == cfg.history_len)
	{
		cfg_resize_histories(cfg.history_len + 1);
		assert(hist->pos + 1 != cfg.history_len && "Failed to resize history.");
	}
}

/* Loads single history entry from vifminfo into the view. */
static void
get_history(view_t *view, int reread, const char dir[], const char file[],
		int rel_pos)
{
	const int list_rows = view->list_rows;

	if(view->history_num == cfg.history_len)
	{
		cfg_resize_histories(cfg.history_len + 1);
	}

	if(!reread)
	{
		view->list_rows = 1;
	}
	flist_hist_save(view, dir, file, rel_pos);
	if(!reread)
	{
		view->list_rows = list_rows;
	}
}

/* Sets manual filter of the view and its previous state to given value. */
static void
set_manual_filter(view_t *view, const char value[])
{
	char *error;
	matcher_t *matcher;

	(void)replace_string(&view->prev_manual_filter, value);
	matcher = matcher_alloc(value, FILTER_DEF_CASE_SENSITIVITY, 0, "", &error);
	free(error);

	/* If setting filter value has failed, try to setup an empty value instead. */
	if(matcher == NULL)
	{
		(void)replace_string(&view->prev_manual_filter, "");
		matcher = matcher_alloc("", FILTER_DEF_CASE_SENSITIVITY, 0, "", &error);
		free(error);
		assert(matcher != NULL && "Can't init manual filter.");
	}

	matcher_free(view->manual_filter);
	view->manual_filter = matcher;
}

void
write_info_file(void)
{
	char info_file[PATH_MAX + 16];
	char tmp_file[PATH_MAX + 16];

	(void)snprintf(info_file, sizeof(info_file), "%s/vifminfo", cfg.config_dir);
	(void)snprintf(tmp_file, sizeof(tmp_file), "%s_%u", info_file, get_pid());

	if(os_access(info_file, R_OK) != 0 || copy_file(info_file, tmp_file) == 0)
	{
		filemon_t current_vifminfo_mon;
		int vifminfo_changed;

		vifminfo_changed =
			filemon_from_file(info_file, FMT_MODIFIED, &current_vifminfo_mon) != 0 ||
			!filemon_equal(&vifminfo_mon, &current_vifminfo_mon);

		update_info_file(tmp_file, vifminfo_changed);
		(void)filemon_from_file(tmp_file, FMT_MODIFIED, &vifminfo_mon);

		if(rename_file(tmp_file, info_file) != 0)
		{
			LOG_ERROR_MSG("Can't replace vifminfo file with its temporary copy");
			(void)remove(tmp_file);
		}
	}

	char json_path[PATH_MAX + 1];
	snprintf(json_path, sizeof(json_path), "%s.json", info_file);
	update_info_file_json(json_path, 1);
}

/* Copies the src file to the dst location.  Returns zero on success. */
static int
copy_file(const char src[], const char dst[])
{
	io_args_t args = {
		.arg1.src = src,
		.arg2.dst = dst,
		.arg3.crs = IO_CRS_REPLACE_FILES,
	};

	return iop_cp(&args);
}

/* Reads contents of the filename file as an info file and updates it with the
 * state of current instance. */
static void
update_info_file(const char filename[], int merge)
{
	/* TODO: refactor this function update_info_file() */

	FILE *fp;
	char **cmds_list;
	int ncmds_list = -1;
	char **ft = NULL, **fx = NULL, **fv = NULL, **cmds = NULL, **marks = NULL;
	char **lh = NULL, **rh = NULL, **cmdh = NULL, **srch = NULL, **regs = NULL;
	int *lhp = NULL, *rhp = NULL, *bt = NULL, *bmt = NULL;
	char **prompt = NULL, **filter = NULL, **trash = NULL;
	char **bmarks = NULL;
	int nft = 0, nfx = 0, nfv = 0, ncmds = 0, nmarks = 0, nlh = 0, nrh = 0;
	int ncmdh = 0, nsrch = 0, nregs = 0, nprompt = 0, nfilter = 0, ntrash = 0;
	int nbmarks = 0;
	char **dir_stack = NULL;
	int ndir_stack = 0;
	char *non_conflicting_marks;

	if(cfg.vifm_info == 0)
		return;

	cmds_list = vle_cmds_list_udcs();
	ncmds_list = count_strings(cmds_list);

	non_conflicting_marks = strdup(valid_marks);

	if(merge && (fp = os_fopen(filename, "r")) != NULL)
	{
		size_t nlhp = 0UL, nrhp = 0UL, nbt = 0UL, nbmt = 0UL;
		char *line = NULL, *line2 = NULL, *line3 = NULL, *line4 = NULL;
		while((line = read_vifminfo_line(fp, line)) != NULL)
		{
			const char type = line[0];
			const char *const line_val = line + 1;

			if(type == LINE_TYPE_COMMENT || type == '\0')
				continue;

			if(type == LINE_TYPE_FILETYPE)
			{
				if((line2 = read_vifminfo_line(fp, line2)) != NULL)
				{
					if(!ft_assoc_exists(&filetypes, line_val, line2))
					{
						nft = add_to_string_array(&ft, nft, 2, line_val, line2);
					}
				}
			}
			else if(type == LINE_TYPE_XFILETYPE)
			{
				if((line2 = read_vifminfo_line(fp, line2)) != NULL)
				{
					if(!ft_assoc_exists(&xfiletypes, line_val, line2))
					{
						nfx = add_to_string_array(&fx, nfx, 2, line_val, line2);
					}
				}
			}
			else if(type == LINE_TYPE_FILEVIEWER)
			{
				if((line2 = read_vifminfo_line(fp, line2)) != NULL)
				{
					if(!ft_assoc_exists(&fileviewers, line_val, line2))
					{
						nfv = add_to_string_array(&fv, nfv, 2, line_val, line2);
					}
				}
			}
			else if(type == LINE_TYPE_COMMAND)
			{
				if(line_val[0] == '\0')
					continue;
				if((line2 = read_vifminfo_line(fp, line2)) != NULL)
				{
					int i;
					const char *p = line_val;
					for(i = 0; i < ncmds_list; i += 2)
					{
						int cmp = strcmp(cmds_list[i], p);
						if(cmp < 0)
							continue;
						if(cmp == 0)
							p = NULL;
						break;
					}
					if(p == NULL)
						continue;
					ncmds = add_to_string_array(&cmds, ncmds, 2, line_val, line2);
				}
			}
			else if(type == LINE_TYPE_LWIN_HIST || type == LINE_TYPE_RWIN_HIST)
			{
				if(line_val[0] == '\0')
					continue;
				if((line2 = read_vifminfo_line(fp, line2)) != NULL)
				{
					const int pos = read_optional_number(fp);

					if(type == LINE_TYPE_LWIN_HIST)
					{
						process_hist_entry(&lwin, line_val, line2, pos, &lh, &nlh, &lhp,
								&nlhp);
					}
					else
					{
						process_hist_entry(&rwin, line_val, line2, pos, &rh, &nrh, &rhp,
								&nrhp);
					}
				}
			}
			else if(type == LINE_TYPE_MARK)
			{
				const char mark = line_val[0];
				if(line_val[1] != '\0')
				{
					LOG_ERROR_MSG("Expected end of line, but got: %s", line_val + 1);
				}
				if((line2 = read_vifminfo_line(fp, line2)) != NULL)
				{
					if((line3 = read_vifminfo_line(fp, line3)) != NULL)
					{
						const int timestamp = read_optional_number(fp);
						const char mark_str[] = { mark, '\0' };

						if(!char_is_one_of(valid_marks, mark))
						{
							continue;
						}

						if(is_mark_older(mark, timestamp))
						{
							char *const pos = strchr(non_conflicting_marks, mark);
							if(pos != NULL)
							{
								nmarks = add_to_string_array(&marks, nmarks, 3, mark_str, line2,
										line3);
								nbt = add_to_int_array(&bt, nbt, timestamp);

								*pos = '\xff';
							}
						}
					}
				}
			}
			else if(type == LINE_TYPE_BOOKMARK)
			{
				if((line2 = read_vifminfo_line(fp, line2)) != NULL)
				{
					if((line3 = read_vifminfo_line(fp, line3)) != NULL)
					{
						long timestamp;
						if(read_number(line3, &timestamp) &&
								bmark_is_older(line_val, timestamp))
						{
							nbmarks = add_to_string_array(&bmarks, nbmarks, 2, line_val,
									line2);
							nbmt = add_to_int_array(&bmt, nbmt, timestamp);
						}
					}
				}
			}
			else if(type == LINE_TYPE_TRASH)
			{
				if((line2 = read_vifminfo_line(fp, line2)) != NULL)
				{
					char *const trash_name = convert_old_trash_path(line_val);
					if(!trash_has_entry(line2, trash_name))
					{
						ntrash = add_to_string_array(&trash, ntrash, 2, trash_name, line2);
					}
					free(trash_name);
				}
			}
			else if(type == LINE_TYPE_CMDLINE_HIST)
			{
				if(!hist_contains(&curr_stats.cmd_hist, line_val))
				{
					ncmdh = add_to_string_array(&cmdh, ncmdh, 1, line_val);
				}
			}
			else if(type == LINE_TYPE_SEARCH_HIST)
			{
				if(!hist_contains(&curr_stats.search_hist, line_val))
				{
					nsrch = add_to_string_array(&srch, nsrch, 1, line_val);
				}
			}
			else if(type == LINE_TYPE_PROMPT_HIST)
			{
				if(!hist_contains(&curr_stats.prompt_hist, line_val))
				{
					nprompt = add_to_string_array(&prompt, nprompt, 1, line_val);
				}
			}
			else if(type == LINE_TYPE_FILTER_HIST)
			{
				if(!hist_contains(&curr_stats.filter_hist, line_val))
				{
					nfilter = add_to_string_array(&filter, nfilter, 1, line_val);
				}
			}
			else if(type == LINE_TYPE_DIR_STACK)
			{
				if((line2 = read_vifminfo_line(fp, line2)) != NULL)
				{
					if((line3 = read_vifminfo_line(fp, line3)) != NULL)
					{
						if((line4 = read_vifminfo_line(fp, line4)) != NULL)
						{
							ndir_stack = add_to_string_array(&dir_stack, ndir_stack, 4,
									line_val, line2, line3 + 1, line4);
						}
					}
				}
			}
			else if(type == LINE_TYPE_REG)
			{
				if(regs_exists(line_val[0]))
				{
					continue;
				}
				nregs = add_to_string_array(&regs, nregs, 1, line);
			}
		}
		free(line);
		free(line2);
		free(line3);
		free(line4);
		fclose(fp);
	}

	if((fp = os_fopen(filename, "w")) != NULL)
	{
		fprintf(fp, "# You can edit this file by hand, but it's recommended not to "
				"do that.\n");

		if(cfg.vifm_info & VINFO_OPTIONS)
		{
			write_options(fp);
		}

		if(cfg.vifm_info & VINFO_FILETYPES)
		{
			write_assocs(fp, "Filetypes", LINE_TYPE_FILETYPE, &filetypes, nft, ft);
			write_assocs(fp, "X Filetypes", LINE_TYPE_XFILETYPE, &xfiletypes, nfx,
					fx);
			write_assocs(fp, "Fileviewers", LINE_TYPE_FILEVIEWER, &fileviewers, nfv,
					fv);
		}

		if(cfg.vifm_info & VINFO_COMMANDS)
		{
			write_commands(fp, cmds_list, cmds, ncmds);
		}

		if(cfg.vifm_info & VINFO_MARKS)
		{
			write_marks(fp, non_conflicting_marks, marks, bt, nmarks);
		}

		if(cfg.vifm_info & VINFO_BOOKMARKS)
		{
			write_bmarks(fp, bmarks, bmt, nbmarks);
		}

		if(cfg.vifm_info & VINFO_TUI)
		{
			write_tui_state(fp);
		}

		if((cfg.vifm_info & VINFO_DHISTORY) && cfg.history_len > 0)
		{
			write_view_history(fp, &lwin, "Left", LINE_TYPE_LWIN_HIST, nlh, lh, lhp);
			write_view_history(fp, &rwin, "Right", LINE_TYPE_RWIN_HIST, nrh, rh, rhp);
		}

		if(cfg.vifm_info & VINFO_CHISTORY)
		{
			write_history(fp, "Command line", LINE_TYPE_CMDLINE_HIST,
					MIN(ncmdh, cfg.history_len - curr_stats.cmd_hist.pos), cmdh,
					&curr_stats.cmd_hist);
		}

		if(cfg.vifm_info & VINFO_SHISTORY)
		{
			write_history(fp, "Search", LINE_TYPE_SEARCH_HIST, nsrch, srch,
					&curr_stats.search_hist);
		}

		if(cfg.vifm_info & VINFO_PHISTORY)
		{
			write_history(fp, "Prompt", LINE_TYPE_PROMPT_HIST, nprompt, prompt,
					&curr_stats.prompt_hist);
		}

		if(cfg.vifm_info & VINFO_FHISTORY)
		{
			write_history(fp, "Local filter", LINE_TYPE_FILTER_HIST, nfilter, filter,
					&curr_stats.filter_hist);
		}

		if(cfg.vifm_info & VINFO_REGISTERS)
		{
			write_registers(fp, regs, nregs);
		}

		if(cfg.vifm_info & VINFO_DIRSTACK)
		{
			write_dir_stack(fp, dir_stack, ndir_stack);
		}

		write_trash(fp, trash, ntrash);

		if(cfg.vifm_info & VINFO_STATE)
		{
			write_general_state(fp);
		}

		if(cfg.vifm_info & VINFO_CS)
		{
			fputs("\n# Color scheme:\n", fp);
			fprintf(fp, "c%s\n", cfg.cs.name);
		}

		fclose(fp);
	}

	free_string_array(ft, nft);
	free_string_array(fv, nfv);
	free_string_array(fx, nfx);
	free_string_array(cmds, ncmds);
	free_string_array(marks, nmarks);
	free_string_array(cmds_list, ncmds_list);
	free_string_array(lh, nlh);
	free_string_array(rh, nrh);
	free(lhp);
	free(rhp);
	free(bt);
	free(bmt);
	free_string_array(cmdh, ncmdh);
	free_string_array(srch, nsrch);
	free_string_array(regs, nregs);
	free_string_array(prompt, nprompt);
	free_string_array(filter, nfilter);
	free_string_array(trash, ntrash);
	free_string_array(bmarks, nbmarks);
	free_string_array(dir_stack, ndir_stack);
	free(non_conflicting_marks);
}

/* Reads contents of the filename file as a JSON info file and updates it with
 * the state of current instance. */
static void
update_info_file_json(const char filename[], int merge)
{
	JSON_Value *current = serialize_state();

	if(merge)
	{
		JSON_Value *admixture = json_parse_file(filename);
		merge_states(json_object(current), json_object(admixture));
		json_value_free(admixture);
	}

	if(json_serialize_to_file(current, filename) == JSONError)
	{
		LOG_ERROR_MSG("Error storing state to: %s", filename);
	}

	json_value_free(current);
}

/* Serializes state of current instance into a JSON object.  Returns the
 * object. */
static JSON_Value *
serialize_state(void)
{
	JSON_Value *root_value = json_value_init_object();
	JSON_Object *root = json_object(root_value);

	JSON_Array *outer_tabs = add_array(root, "tabs");
	JSON_Object *outer_tab = append_object(outer_tabs);

	store_gtab(outer_tab);

	store_trash(root);

	if(cfg.vifm_info & VINFO_OPTIONS)
	{
		store_global_options(root);
	}

	if(cfg.vifm_info & VINFO_FILETYPES)
	{
		store_assocs(root, "assocs", &filetypes);
		store_assocs(root, "xassocs", &xfiletypes);
		store_assocs(root, "viewers", &fileviewers);
	}

	if(cfg.vifm_info & VINFO_COMMANDS)
	{
		store_cmds(root);
	}

	if(cfg.vifm_info & VINFO_MARKS)
	{
		store_marks(root);
	}

	if(cfg.vifm_info & VINFO_BOOKMARKS)
	{
		store_bmarks(root);
	}

	if(cfg.vifm_info & VINFO_CHISTORY)
	{
		store_history(root, "cmd-hist", &curr_stats.cmd_hist);
	}

	if(cfg.vifm_info & VINFO_SHISTORY)
	{
		store_history(root, "search-hist", &curr_stats.search_hist);
	}

	if(cfg.vifm_info & VINFO_PHISTORY)
	{
		store_history(root, "prompt-hist", &curr_stats.prompt_hist);
	}

	if(cfg.vifm_info & VINFO_FHISTORY)
	{
		store_history(root, "lfilt-hist", &curr_stats.filter_hist);
	}

	if(cfg.vifm_info & VINFO_REGISTERS)
	{
		store_regs(root);
	}

	if(cfg.vifm_info & VINFO_DIRSTACK)
	{
		store_dir_stack(root);
	}

	if(cfg.vifm_info & VINFO_STATE)
	{
		set_bool(root, "use-term-multiplexer", cfg.use_term_multiplexer);
	}

	if(cfg.vifm_info & VINFO_CS)
	{
		set_str(root, "color-scheme", cfg.cs.name);
	}

	return root_value;
}

/* Adds parts of admixture to current state to avoid losing state stored by
 * other instances. */
static void
merge_states(JSON_Object *current, JSON_Object *admixture)
{
	merge_tabs(current, admixture);

	if(cfg.vifm_info & VINFO_COMMANDS)
	{
		merge_commands(current, admixture);
	}

	if(cfg.vifm_info & VINFO_MARKS)
	{
		merge_marks(current, admixture);
	}

	if(cfg.vifm_info & VINFO_BOOKMARKS)
	{
		merge_bmarks(current, admixture);
	}

	if(cfg.vifm_info & VINFO_CHISTORY)
	{
		merge_history(current, admixture, "cmd-hist");
	}

	if(cfg.vifm_info & VINFO_SHISTORY)
	{
		merge_history(current, admixture, "search-hist");
	}

	if(cfg.vifm_info & VINFO_PHISTORY)
	{
		merge_history(current, admixture, "prompt-hist");
	}

	if(cfg.vifm_info & VINFO_FHISTORY)
	{
		merge_history(current, admixture, "lfilt-hist");
	}

	if(cfg.vifm_info & VINFO_REGISTERS)
	{
		merge_regs(current, admixture);
	}

	if(cfg.vifm_info & VINFO_DIRSTACK)
	{
		merge_dir_stack(current, admixture);
	}

	merge_trash(current, admixture);
}

/* Merges two sets of tabs if there is only one tab at each level (global and
 * pane). */
static void
merge_tabs(JSON_Object *current, JSON_Object *admixture)
{
	if(!(cfg.vifm_info & VINFO_DHISTORY))
	{
		/* There is nothing to merge accept for directory history. */
		return;
	}

	JSON_Array *current_gtabs = json_object_get_array(current, "tabs");
	JSON_Array *updated_gtabs = json_object_get_array(admixture, "tabs");
	if(json_array_get_count(current_gtabs) != 1 ||
			json_array_get_count(updated_gtabs) != 1)
	{
		return;
	}

	JSON_Object *current_gtab = json_array_get_object(current_gtabs, 0);
	JSON_Object *updated_gtab = json_array_get_object(updated_gtabs, 0);

	JSON_Array *current_panes = json_object_get_array(current_gtab, "panes");
	JSON_Array *updated_panes = json_object_get_array(updated_gtab, "panes");

	int i;
	for(i = 0; i < 2; ++i)
	{
		JSON_Object *current_pane = json_array_get_object(current_panes, i);
		JSON_Object *updated_pane = json_array_get_object(updated_panes, i);

		JSON_Array *current_itabs = json_object_get_array(current_pane, "tabs");
		JSON_Array *updated_itabs = json_object_get_array(updated_pane, "tabs");

		if(json_array_get_count(current_itabs) == 1 &&
				json_array_get_count(updated_itabs) == 1)
		{
			const view_t *view = (i == 0 ? &lwin : &rwin);
			merge_dhistory(json_array_get_object(current_itabs, 0),
					json_array_get_object(updated_itabs, 0), view);
		}
	}
}

/* Merges two directory histories. */
static void
merge_dhistory(JSON_Object *current, JSON_Object *admixture, const view_t *view)
{
	JSON_Array *history = json_object_get_array(current, "history");
	JSON_Array *updated = json_object_get_array(admixture, "history");

	int extra_space = cfg.history_len - 1 - view->history_pos;
	if(extra_space == 0 || json_array_get_count(updated) == 0)
	{
		return;
	}

	int i, n;
	JSON_Value *merged_value = json_value_init_array();
	JSON_Array *merged = json_array(merged_value);

	for(i = 0, n = json_array_get_count(updated); i < n; ++i)
	{
		JSON_Object *entry = json_array_get_object(updated, i);

		const char *dir;
		if(get_str(entry, "dir", &dir))
		{
			if(!flist_hist_contains(view, dir) && is_dir(dir))
			{
				JSON_Value *value = json_object_get_wrapping_value(entry);
				json_array_append_value(merged, json_value_deep_copy(value));
			}
		}
	}

	for(i = 0, n = json_array_get_count(history); i < n; ++i)
	{
		JSON_Object *entry = json_array_get_object(history, i);
		JSON_Value *value = json_object_get_wrapping_value(entry);
		json_array_append_value(merged, json_value_deep_copy(value));
	}

	json_object_set_value(current, "history", merged_value);
}

/* Merges two sets of :commands. */
static void
merge_commands(JSON_Object *current, JSON_Object *admixture)
{
	JSON_Object *cmds = json_object_get_object(current, "cmds");
	JSON_Object *updated = json_object_get_object(admixture, "cmds");

	int i, n;
	for(i = 0, n = json_object_get_count(updated); i < n; ++i)
	{
		const char *name = json_object_get_name(updated, i);
		if(!json_object_has_value(cmds, name))
		{
			JSON_Value *value = json_object_get_value_at(updated, i);
			json_object_set_string(cmds, name, json_string(value));
		}
	}
}

/* Merges two sets of marks. */
static void
merge_marks(JSON_Object *current, JSON_Object *admixture)
{
	JSON_Object *bmarks = json_object_get_object(current, "marks");
	JSON_Object *updated = json_object_get_object(admixture, "marks");

	int i, n;
	for(i = 0, n = json_object_get_count(updated); i < n; ++i)
	{
		JSON_Object *mark = json_object(json_object_get_value_at(updated, i));

		double ts;
		const char *name = json_object_get_name(updated, i);
		if(get_double(mark, "ts", &ts) && is_mark_older(name[0], (time_t)ts))
		{
			JSON_Value *value = json_object_get_wrapping_value(mark);
			json_object_set_value(bmarks, name, json_value_deep_copy(value));
		}
	}
}

/* Merges two sets of bookmarks. */
static void
merge_bmarks(JSON_Object *current, JSON_Object *admixture)
{
	JSON_Object *bmarks = json_object_get_object(current, "bmarks");
	JSON_Object *updated = json_object_get_object(admixture, "bmarks");

	int i, n;
	for(i = 0, n = json_object_get_count(updated); i < n; ++i)
	{
		JSON_Object *bmark = json_object(json_object_get_value_at(updated, i));

		double ts;
		const char *path = json_object_get_name(updated, i);
		if(get_double(bmark, "ts", &ts) && bmark_is_older(path, (time_t)ts))
		{
			JSON_Value *value = json_object_get_wrapping_value(bmark);
			json_object_set_value(bmarks, path, json_value_deep_copy(value));
		}
	}
}

/* Merges two states of a particular kind of history. */
static void
merge_history(JSON_Object *current, JSON_Object *admixture, const char node[])
{
	JSON_Array *updated = json_object_get_array(admixture, node);
	if(json_array_get_count(updated) == 0)
	{
		return;
	}

	int i, n;
	JSON_Array *entries = json_object_get_array(current, node);
	trie_t *trie = trie_create();

	JSON_Value *merged_value = json_value_init_array();
	JSON_Array *merged = json_array(merged_value);

	for(i = 0, n = json_array_get_count(entries); i < n; ++i)
	{
		const char *entry = json_array_get_string(entries, i);
		trie_put(trie, entry);
	}

	for(i = 0, n = json_array_get_count(updated); i < n; ++i)
	{
		void *data;
		const char *entry = json_array_get_string(updated, i);
		if(trie_get(trie, entry, &data) != 0)
		{
			json_array_append_string(merged, entry);
		}
	}

	for(i = 0, n = json_array_get_count(entries); i < n; ++i)
	{
		json_array_append_string(merged, json_array_get_string(entries, i));
	}

	trie_free(trie);

	json_object_set_value(current, node, merged_value);
}

/* Merges two states of registers. */
static void
merge_regs(JSON_Object *current, JSON_Object *admixture)
{
	JSON_Object *regs = json_object_get_object(current, "regs");
	JSON_Object *updated = json_object_get_object(admixture, "regs");

	int i, n;
	for(i = 0, n = json_object_get_count(updated); i < n; ++i)
	{
		const char *name = json_object_get_name(updated, i);
		if(!json_object_has_value(regs, name))
		{
			JSON_Value *reg = json_object_get_value_at(updated, i);
			json_object_set_value(regs, name, json_value_deep_copy(reg));
		}
	}
}

/* Merges two directory stack states. */
static void
merge_dir_stack(JSON_Object *current, JSON_Object *admixture)
{
	/* Just leave new state as is if was changed since startup. */
	if(!dir_stack_changed())
	{
		JSON_Value *updated = json_object_get_value(admixture, "dir-stack");
		json_object_set_value(current, "dir-stack", json_value_deep_copy(updated));
	}
}

/* Merges two trash states. */
static void
merge_trash(JSON_Object *current, JSON_Object *admixture)
{
	JSON_Array *trash = json_object_get_array(current, "trash");
	JSON_Array *updated = json_object_get_array(admixture, "trash");

	int i, n;
	for(i = 0, n = json_array_get_count(updated); i < n; ++i)
	{
		JSON_Object *entry = json_array_get_object(updated, i);

		const char *trashed, *original;
		if(get_str(entry, "trashed", &trashed) &&
				get_str(entry, "original", &original))
		{
			if(!trash_has_entry(original, trashed))
			{
				JSON_Value *value = json_object_get_wrapping_value(entry);
				json_array_append_value(trash, json_value_deep_copy(value));
			}
		}
	}
}

/* Serializes a global tab into JSON table. */
static void
store_gtab(JSON_Object *gtab)
{
	JSON_Object *splitter = add_object(gtab, "splitter");

	JSON_Array *panes = add_array(gtab, "panes");

	JSON_Object *left = append_object(panes);
	JSON_Array *left_tabs = add_array(left, "tabs");
	JSON_Object *left_tab = append_object(left_tabs);

	JSON_Object *right = append_object(panes);
	JSON_Array *right_tabs = add_array(right, "tabs");
	JSON_Object *right_tab = append_object(right_tabs);

	set_int(gtab, "active-pane", (curr_view == &lwin ? 0 : 1));

	set_bool(gtab, "preview", curr_stats.preview.on);

	store_view(left_tab, &lwin);
	store_view(right_tab, &rwin);

	if(cfg.vifm_info & VINFO_TUI)
	{
		set_int(splitter, "pos", curr_stats.splitter_pos);
		set_str(splitter, "orientation", curr_stats.split == VSPLIT ? "v" : "h");
		set_bool(splitter, "expanded", curr_stats.number_of_windows == 1);
	}
}

/* Serializes a view into JSON table. */
static void
store_view(JSON_Object *view_data, view_t *view)
{
	if((cfg.vifm_info & VINFO_DHISTORY) && cfg.history_len > 0)
	{
		store_dhistory(view_data, view);
	}

	if(cfg.vifm_info & VINFO_STATE)
	{
		store_filters(view_data, view);
	}

	if(cfg.vifm_info & VINFO_OPTIONS)
	{
		store_view_options(view_data, view);
	}

	store_sort_info(view_data, view);
}

/* Serializes filters of a view into JSON table. */
static void
store_filters(JSON_Object *view_data, view_t *view)
{
	JSON_Object *filters = add_object(view_data, "filters");
	set_bool(filters, "invert", view->invert);
	set_bool(filters, "dot", view->hide_dot);
	set_str(filters, "manual", matcher_get_expr(view->manual_filter));
	set_str(filters, "auto", view->auto_filter.raw);
}

/* Serializes a history into JSON. */
static void
store_history(JSON_Object *root, const char node[], const hist_t *hist)
{
	if(hist->pos < 0)
	{
		return;
	}

	int i;
	JSON_Array *entries = add_array(root, node);
	for(i = hist->pos; i >= 0; i--)
	{
		json_array_append_string(entries, hist->items[i]);
	}
}

/* Serializes options into JSON table. */
static void
store_global_options(JSON_Object *root)
{
	JSON_Array *options = add_array(root, "options");

	append_dstr(options, format_str("aproposprg=%s",
				escape_spaces(cfg.apropos_prg)));
	append_dstr(options, format_str("%sautochpos", cfg.auto_ch_pos ? "" : "no"));
	append_dstr(options, format_str("cdpath=%s", cfg.cd_path));
	append_dstr(options, format_str("%schaselinks", cfg.chase_links ? "" : "no"));
	append_dstr(options, format_str("columns=%d", cfg.columns));
	append_dstr(options, format_str("cpoptions=%s",
			escape_spaces(vle_opts_get("cpoptions", OPT_GLOBAL))));
	append_dstr(options, format_str("deleteprg=%s",
				escape_spaces(cfg.delete_prg)));
	append_dstr(options, format_str("%sfastrun", cfg.fast_run ? "" : "no"));
	if(strcmp(cfg.border_filler, " ") != 0)
	{
		append_dstr(options, format_str("fillchars+=vborder:%s",
					cfg.border_filler));
	}
	append_dstr(options, format_str("findprg=%s", escape_spaces(cfg.find_prg)));
	append_dstr(options, format_str("%sfollowlinks",
				cfg.follow_links ? "" : "no"));
	append_dstr(options, format_str("fusehome=%s", escape_spaces(cfg.fuse_home)));
	append_dstr(options, format_str("%sgdefault", cfg.gdefault ? "" : "no"));
	append_dstr(options, format_str("grepprg=%s", escape_spaces(cfg.grep_prg)));
	append_dstr(options, format_str("histcursor=%s",
			escape_spaces(vle_opts_get("histcursor", OPT_GLOBAL))));
	append_dstr(options, format_str("history=%d", cfg.history_len));
	append_dstr(options, format_str("%shlsearch", cfg.hl_search ? "" : "no"));
	append_dstr(options, format_str("%siec",
				cfg.sizefmt.ieci_prefixes ? "" : "no"));
	append_dstr(options, format_str("%signorecase", cfg.ignore_case ? "" : "no"));
	append_dstr(options, format_str("%sincsearch", cfg.inc_search ? "" : "no"));
	append_dstr(options, format_str("%slaststatus",
				cfg.display_statusline ? "" : "no"));
	append_dstr(options, format_str("%stitle", cfg.set_title ? "" : "no"));
	append_dstr(options, format_str("lines=%d", cfg.lines));
	append_dstr(options, format_str("locateprg=%s",
				escape_spaces(cfg.locate_prg)));
	append_dstr(options, format_str("mediaprg=%s",
				escape_spaces(cfg.media_prg)));
	append_dstr(options, format_str("mintimeoutlen=%d", cfg.min_timeout_len));
	append_dstr(options, format_str("%squickview",
				curr_stats.preview.on ? "" : "no"));
	append_dstr(options, format_str("rulerformat=%s",
				escape_spaces(cfg.ruler_format)));
	append_dstr(options, format_str("%srunexec", cfg.auto_execute ? "" : "no"));
	append_dstr(options, format_str("%sscrollbind", cfg.scroll_bind ? "" : "no"));
	append_dstr(options, format_str("scrolloff=%d", cfg.scroll_off));
	append_dstr(options, format_str("shell=%s", escape_spaces(cfg.shell)));
	append_dstr(options, format_str("shellcmdflag=%s",
				escape_spaces(cfg.shell_cmd_flag)));
	append_dstr(options, format_str("shortmess=%s",
			escape_spaces(vle_opts_get("shortmess", OPT_GLOBAL))));
	append_dstr(options, format_str("showtabline=%s",
			escape_spaces(vle_opts_get("showtabline", OPT_GLOBAL))));
	append_dstr(options, format_str("sizefmt=%s",
			escape_spaces(vle_opts_get("sizefmt", OPT_GLOBAL))));
#ifndef _WIN32
	append_dstr(options, format_str("slowfs=%s",
				escape_spaces(cfg.slow_fs_list)));
#endif
	append_dstr(options, format_str("%ssmartcase", cfg.smart_case ? "" : "no"));
	append_dstr(options, format_str("%ssortnumbers",
				cfg.sort_numbers ? "" : "no"));
	append_dstr(options, format_str("statusline=%s",
				escape_spaces(cfg.status_line)));
	append_dstr(options, format_str("syncregs=%s",
			escape_spaces(vle_opts_get("syncregs", OPT_GLOBAL))));
	append_dstr(options, format_str("tabscope=%s",
			escape_spaces(vle_opts_get("tabscope", OPT_GLOBAL))));
	append_dstr(options, format_str("tabstop=%d", cfg.tab_stop));
	append_dstr(options, format_str("timefmt=%s",
				escape_spaces(cfg.time_format)));
	append_dstr(options, format_str("timeoutlen=%d", cfg.timeout_len));
	append_dstr(options, format_str("%strash", cfg.use_trash ? "" : "no"));
	append_dstr(options, format_str("tuioptions=%s",
			escape_spaces(vle_opts_get("tuioptions", OPT_GLOBAL))));
	append_dstr(options, format_str("undolevels=%d", cfg.undo_levels));
	append_dstr(options, format_str("vicmd=%s%s", escape_spaces(cfg.vi_command),
			cfg.vi_cmd_bg ? " &" : ""));
	append_dstr(options, format_str("vixcmd=%s%s",
				escape_spaces(cfg.vi_x_command), cfg.vi_cmd_bg ? " &" : ""));
	append_dstr(options, format_str("%swrapscan", cfg.wrap_scan ? "" : "no"));

	append_dstr(options, format_str("confirm=%s",
			escape_spaces(vle_opts_get("confirm", OPT_GLOBAL))));
	append_dstr(options, format_str("dotdirs=%s",
			escape_spaces(vle_opts_get("dotdirs", OPT_GLOBAL))));
	append_dstr(options, format_str("caseoptions=%s",
			escape_spaces(vle_opts_get("caseoptions", OPT_GLOBAL))));
	append_dstr(options, format_str("suggestoptions=%s",
			escape_spaces(vle_opts_get("suggestoptions", OPT_GLOBAL))));
	append_dstr(options, format_str("iooptions=%s",
			escape_spaces(vle_opts_get("iooptions", OPT_GLOBAL))));

	append_dstr(options, format_str("dirsize=%s",
				cfg.view_dir_size == VDS_SIZE ? "size" : "nitems"));

	const char *str = classify_to_str();
	append_dstr(options, format_str("classify=%s",
				escape_spaces(str == NULL ? "" : str)));

	append_dstr(options, format_str("vifminfo=%s",
			escape_spaces(vle_opts_get("vifminfo", OPT_GLOBAL))));

	append_dstr(options, format_str("%svimhelp", cfg.use_vim_help ? "" : "no"));
	append_dstr(options, format_str("%swildmenu", cfg.wild_menu ? "" : "no"));
	append_dstr(options, format_str("wildstyle=%s",
				cfg.wild_popup ? "popup" : "bar"));
	append_dstr(options, format_str("wordchars=%s",
			escape_spaces(vle_opts_get("wordchars", OPT_GLOBAL))));
	append_dstr(options, format_str("%swrap", cfg.wrap_quick_view ? "" : "no"));
}

/* Serializes view-specific options into JSON table. */
static void
store_view_options(JSON_Object *parent, view_t *view)
{
	JSON_Array *options = add_array(parent, "options");

	append_dstr(options, format_str("viewcolumns=%s",
				escape_spaces(lwin.view_columns_g)));
	append_dstr(options, format_str("sortgroups=%s",
				escape_spaces(lwin.sort_groups_g)));
	append_dstr(options, format_str("lsoptions=%s",
				lwin.ls_transposed_g ? "transposed" : ""));
	append_dstr(options, format_str("%slsview", lwin.ls_view_g ? "" : "no"));
	append_dstr(options, format_str("milleroptions=lsize:%d,csize:%d,rsize:%d",
			lwin.miller_ratios_g[0], lwin.miller_ratios_g[1],
			lwin.miller_ratios_g[2]));
	append_dstr(options, format_str("%smillerview",
				lwin.miller_view_g ? "" : "no"));
	append_dstr(options, format_str("%snumber",
				(lwin.num_type_g & NT_SEQ) ? "" : "no"));
	append_dstr(options, format_str("numberwidth=%d", lwin.num_width_g));
	append_dstr(options, format_str("%srelativenumber",
				(lwin.num_type_g & NT_REL) ? "" : "no"));
	append_dstr(options, format_str("%sdotfiles", lwin.hide_dot_g ? "" : "no"));
	append_dstr(options, format_str("previewprg=%s",
				escape_spaces(lwin.preview_prg_g)));
}

/* Serializes file associations into JSON table. */
static void
store_assocs(JSON_Object *root, const char node[], assoc_list_t *assocs)
{
	int i;
	JSON_Array *entries = add_array(root, node);
	for(i = 0; i < assocs->count; ++i)
	{
		int j;
		assoc_t assoc = assocs->list[i];
		for(j = 0; j < assoc.records.count; ++j)
		{
			assoc_record_t ft_record = assoc.records.list[j];

			/* The type check is to prevent builtin fake associations to be written
			 * into vifminfo file. */
			if(ft_record.command[0] == '\0' || ft_record.type == ART_BUILTIN)
			{
				continue;
			}

			char *doubled_commas_cmd = double_char(ft_record.command, ',');

			JSON_Object *entry = append_object(entries);
			set_str(entry, "matchers", matchers_get_expr(assoc.matchers));

			if(ft_record.description[0] == '\0')
			{
				set_str(entry, "cmd", doubled_commas_cmd);
			}
			else
			{
				char *cmd = format_str("{%s}%s", ft_record.description,
						doubled_commas_cmd);
				if(cmd != NULL)
				{
					set_str(entry, "cmd", cmd);
					free(cmd);
				}
			}

			free(doubled_commas_cmd);
		}
	}
}

/* Serializes :commands into JSON table. */
static void
store_cmds(JSON_Object *root)
{
	int i;
	JSON_Object *cmds = add_object(root, "cmds");
	char **cmds_list = vle_cmds_list_udcs();
	for(i = 0; cmds_list[i] != NULL; i += 2)
	{
		json_object_set_string(cmds, cmds_list[i], cmds_list[i + 1]);
	}
}

/* Serializes marks into JSON table. */
static void
store_marks(JSON_Object *root)
{
	JSON_Object *marks = add_object(root, "marks");

	int active_marks[NUM_MARKS];
	const int len = init_active_marks(valid_marks, active_marks);

	int i;
	for(i = 0; i < len; ++i)
	{
		const int index = active_marks[i];
		const char m = index2mark(index);
		if(!is_spec_mark(index))
		{
			const mark_t *const mark = get_mark(index);

			char name[] = { m, '\0' };
			JSON_Object *entry = add_object(marks, name);
			set_str(entry, "dir", mark->directory);
			set_str(entry, "file", mark->file);
			set_double(entry, "ts", (double)mark->timestamp);
		}
	}
}

/* Serializes bookmarks into JSON table. */
static void
store_bmarks(JSON_Object *root)
{
	JSON_Object *bmarks = add_object(root, "bmarks");
	bmarks_list(&store_bmark, bmarks);
}

/* bmarks_list() callback that writes a bookmark into JSON. */
static void
store_bmark(const char path[], const char tags[], time_t timestamp, void *arg)
{
	JSON_Object *bmarks = arg;

	JSON_Object *bmark = add_object(bmarks, path);
	set_str(bmark, "tags", tags);
	set_double(bmark, "ts", timestamp);
}

/* Serializes registers into JSON table. */
static void
store_regs(JSON_Object *root)
{
	int i;
	JSON_Object *regs = add_object(root, "regs");
	for(i = 0; valid_registers[i] != '\0'; ++i)
	{
		const reg_t *const reg = regs_find(valid_registers[i]);
		if(reg == NULL || reg->nfiles == 0)
		{
			continue;
		}

		char name[] = { valid_registers[i], '\0' };
		JSON_Array *files = add_array(regs, name);

		int j;
		for(j = 0; j < reg->nfiles; ++j)
		{
			if(reg->files[j] != NULL)
			{
				json_array_append_string(files, reg->files[j]);
			}
		}
	}
}

/* Serializes directory stack into JSON table. */
static void
store_dir_stack(JSON_Object *root)
{
	unsigned int i;
	JSON_Array *entries = add_array(root, "dir-stack");
	for(i = 0U; i < dir_stack_top; ++i)
	{
		dir_stack_entry_t *const entry = &dir_stack[i];

		JSON_Object *info = append_object(entries);
		set_str(info, "left-dir", entry->lpane_dir);
		set_str(info, "left-file", entry->lpane_file);
		set_str(info, "right-dir", entry->rpane_dir);
		set_str(info, "right-file", entry->rpane_file);
	}
}

/* Serializes trash into JSON table. */
static void
store_trash(JSON_Object *root)
{
	int i;
	JSON_Array *trash = add_array(root, "trash");
	for(i = 0; i < trash_list_size; ++i)
	{
		JSON_Object *entry = append_object(trash);
		set_str(entry, "trashed", trash_list[i].trash_name);
		set_str(entry, "original", trash_list[i].path);
	}
}

/* Handles single directory history entry, possibly skipping merging it in. */
static void
process_hist_entry(view_t *view, const char dir[], const char file[], int pos,
		char ***lh, int *nlh, int **lhp, size_t *nlhp)
{
	if(view->history_pos + *nlh/2 == cfg.history_len - 1 ||
			flist_hist_contains(view, dir) || !is_dir(dir))
	{
		return;
	}

	*nlh = add_to_string_array(lh, *nlh, 2, dir, file);
	if(*nlh/2U > *nlhp)
	{
		*nlhp = add_to_int_array(lhp, *nlhp, pos);
		*nlhp = MIN(*nlh/2U, *nlhp);
	}
}

/* Performs conversions on files in trash required for partial backward
 * compatibility.  Returns newly allocated string that should be freed by the
 * caller. */
static char *
convert_old_trash_path(const char trash_path[])
{
	if(!is_path_absolute(trash_path) && is_dir_writable(cfg.trash_dir))
	{
		char *const full_path = format_str("%s/%s", cfg.trash_dir, trash_path);
		if(path_exists(full_path, NODEREF))
		{
			return full_path;
		}
		free(full_path);
	}
	return strdup(trash_path);
}

/* Writes current values of all options into vifminfo file. */
static void
write_options(FILE *fp)
{
	const char *str;

	fputs("\n# Options:\n", fp);
	fprintf(fp, "=aproposprg=%s\n", escape_spaces(cfg.apropos_prg));
	fprintf(fp, "=%sautochpos\n", cfg.auto_ch_pos ? "" : "no");
	fprintf(fp, "=cdpath=%s\n", cfg.cd_path);
	fprintf(fp, "=%schaselinks\n", cfg.chase_links ? "" : "no");
	fprintf(fp, "=columns=%d\n", cfg.columns);
	fprintf(fp, "=cpoptions=%s\n",
			escape_spaces(vle_opts_get("cpoptions", OPT_GLOBAL)));
	fprintf(fp, "=deleteprg=%s\n", escape_spaces(cfg.delete_prg));
	fprintf(fp, "=%sfastrun\n", cfg.fast_run ? "" : "no");
	if(strcmp(cfg.border_filler, " ") != 0)
	{
		fprintf(fp, "=fillchars+=vborder:%s\n", cfg.border_filler);
	}
	fprintf(fp, "=findprg=%s\n", escape_spaces(cfg.find_prg));
	fprintf(fp, "=%sfollowlinks\n", cfg.follow_links ? "" : "no");
	fprintf(fp, "=fusehome=%s\n", escape_spaces(cfg.fuse_home));
	fprintf(fp, "=%sgdefault\n", cfg.gdefault ? "" : "no");
	fprintf(fp, "=grepprg=%s\n", escape_spaces(cfg.grep_prg));
	fprintf(fp, "=histcursor=%s\n",
			escape_spaces(vle_opts_get("histcursor", OPT_GLOBAL)));
	fprintf(fp, "=history=%d\n", cfg.history_len);
	fprintf(fp, "=%shlsearch\n", cfg.hl_search ? "" : "no");
	fprintf(fp, "=%siec\n", cfg.sizefmt.ieci_prefixes ? "" : "no");
	fprintf(fp, "=%signorecase\n", cfg.ignore_case ? "" : "no");
	fprintf(fp, "=%sincsearch\n", cfg.inc_search ? "" : "no");
	fprintf(fp, "=%slaststatus\n", cfg.display_statusline ? "" : "no");
	fprintf(fp, "=%stitle\n", cfg.set_title ? "" : "no");
	fprintf(fp, "=lines=%d\n", cfg.lines);
	fprintf(fp, "=locateprg=%s\n", escape_spaces(cfg.locate_prg));
	fprintf(fp, "=mediaprg=%s\n", escape_spaces(cfg.media_prg));
	fprintf(fp, "=mintimeoutlen=%d\n", cfg.min_timeout_len);
	fprintf(fp, "=%squickview\n", curr_stats.preview.on ? "" : "no");
	fprintf(fp, "=rulerformat=%s\n", escape_spaces(cfg.ruler_format));
	fprintf(fp, "=%srunexec\n", cfg.auto_execute ? "" : "no");
	fprintf(fp, "=%sscrollbind\n", cfg.scroll_bind ? "" : "no");
	fprintf(fp, "=scrolloff=%d\n", cfg.scroll_off);
	fprintf(fp, "=shell=%s\n", escape_spaces(cfg.shell));
	fprintf(fp, "=shellcmdflag=%s\n", escape_spaces(cfg.shell_cmd_flag));
	fprintf(fp, "=shortmess=%s\n",
			escape_spaces(vle_opts_get("shortmess", OPT_GLOBAL)));
	fprintf(fp, "=showtabline=%s\n",
			escape_spaces(vle_opts_get("showtabline", OPT_GLOBAL)));
	fprintf(fp, "=sizefmt=%s\n",
			escape_spaces(vle_opts_get("sizefmt", OPT_GLOBAL)));
#ifndef _WIN32
	fprintf(fp, "=slowfs=%s\n", escape_spaces(cfg.slow_fs_list));
#endif
	fprintf(fp, "=%ssmartcase\n", cfg.smart_case ? "" : "no");
	fprintf(fp, "=%ssortnumbers\n", cfg.sort_numbers ? "" : "no");
	fprintf(fp, "=statusline=%s\n", escape_spaces(cfg.status_line));
	fprintf(fp, "=syncregs=%s\n",
			escape_spaces(vle_opts_get("syncregs", OPT_GLOBAL)));
	fprintf(fp, "=tabscope=%s\n",
			escape_spaces(vle_opts_get("tabscope", OPT_GLOBAL)));
	fprintf(fp, "=tabstop=%d\n", cfg.tab_stop);
	fprintf(fp, "=timefmt=%s\n", escape_spaces(cfg.time_format));
	fprintf(fp, "=timeoutlen=%d\n", cfg.timeout_len);
	fprintf(fp, "=%strash\n", cfg.use_trash ? "" : "no");
	fprintf(fp, "=tuioptions=%s\n",
			escape_spaces(vle_opts_get("tuioptions", OPT_GLOBAL)));
	fprintf(fp, "=undolevels=%d\n", cfg.undo_levels);
	fprintf(fp, "=vicmd=%s%s\n", escape_spaces(cfg.vi_command),
			cfg.vi_cmd_bg ? " &" : "");
	fprintf(fp, "=vixcmd=%s%s\n", escape_spaces(cfg.vi_x_command),
			cfg.vi_cmd_bg ? " &" : "");
	fprintf(fp, "=%swrapscan\n", cfg.wrap_scan ? "" : "no");
	fprintf(fp, "=[viewcolumns=%s\n", escape_spaces(lwin.view_columns_g));
	fprintf(fp, "=]viewcolumns=%s\n", escape_spaces(rwin.view_columns_g));
	fprintf(fp, "=[sortgroups=%s\n", escape_spaces(lwin.sort_groups_g));
	fprintf(fp, "=]sortgroups=%s\n", escape_spaces(rwin.sort_groups_g));
	fprintf(fp, "=[lsoptions=%s\n", lwin.ls_transposed_g ? "transposed" : "");
	fprintf(fp, "=]lsoptions=%s\n", rwin.ls_transposed_g ? "transposed" : "");
	fprintf(fp, "=[%slsview\n", lwin.ls_view_g ? "" : "no");
	fprintf(fp, "=]%slsview\n", rwin.ls_view_g ? "" : "no");
	fprintf(fp, "=[milleroptions=lsize:%d,csize:%d,rsize:%d\n",
			lwin.miller_ratios_g[0], lwin.miller_ratios_g[1],
			lwin.miller_ratios_g[2]);
	fprintf(fp, "=]milleroptions=lsize:%d,csize:%d,rsize:%d\n",
			rwin.miller_ratios_g[0], rwin.miller_ratios_g[1],
			rwin.miller_ratios_g[2]);
	fprintf(fp, "=[%smillerview\n", lwin.miller_view_g ? "" : "no");
	fprintf(fp, "=]%smillerview\n", rwin.miller_view_g ? "" : "no");
	fprintf(fp, "=[%snumber\n", (lwin.num_type_g & NT_SEQ) ? "" : "no");
	fprintf(fp, "=]%snumber\n", (rwin.num_type_g & NT_SEQ) ? "" : "no");
	fprintf(fp, "=[numberwidth=%d\n", lwin.num_width_g);
	fprintf(fp, "=]numberwidth=%d\n", rwin.num_width_g);
	fprintf(fp, "=[%srelativenumber\n", (lwin.num_type_g & NT_REL) ? "" : "no");
	fprintf(fp, "=]%srelativenumber\n", (rwin.num_type_g & NT_REL) ? "" : "no");
	fprintf(fp, "=[%sdotfiles\n", lwin.hide_dot_g ? "" : "no");
	fprintf(fp, "=]%sdotfiles\n", rwin.hide_dot_g ? "" : "no");
	fprintf(fp, "=[previewprg=%s\n", escape_spaces(lwin.preview_prg_g));
	fprintf(fp, "=]previewprg=%s\n", escape_spaces(rwin.preview_prg_g));

	fprintf(fp, "=confirm=%s\n",
			escape_spaces(vle_opts_get("confirm", OPT_GLOBAL)));
	fprintf(fp, "=dotdirs=%s\n",
			escape_spaces(vle_opts_get("dotdirs", OPT_GLOBAL)));
	fprintf(fp, "=caseoptions=%s\n",
			escape_spaces(vle_opts_get("caseoptions", OPT_GLOBAL)));
	fprintf(fp, "=suggestoptions=%s\n",
			escape_spaces(vle_opts_get("suggestoptions", OPT_GLOBAL)));
	fprintf(fp, "=iooptions=%s\n",
			escape_spaces(vle_opts_get("iooptions", OPT_GLOBAL)));

	fprintf(fp, "=dirsize=%s", cfg.view_dir_size == VDS_SIZE ? "size" : "nitems");

	str = classify_to_str();
	fprintf(fp, "=classify=%s\n", escape_spaces(str == NULL ? "" : str));

	fprintf(fp, "=vifminfo=%s\n",
			escape_spaces(vle_opts_get("vifminfo", OPT_GLOBAL)));

	fprintf(fp, "=%svimhelp\n", cfg.use_vim_help ? "" : "no");
	fprintf(fp, "=%swildmenu\n", cfg.wild_menu ? "" : "no");
	fprintf(fp, "=wildstyle=%s\n", cfg.wild_popup ? "popup" : "bar");
	fprintf(fp, "=wordchars=%s\n",
			escape_spaces(vle_opts_get("wordchars", OPT_GLOBAL)));
	fprintf(fp, "=%swrap\n", cfg.wrap_quick_view ? "" : "no");
}

/* Stores list of associations to the file. */
static void
write_assocs(FILE *fp, const char str[], char mark, assoc_list_t *assocs,
		int prev_count, char *prev[])
{
	int i;

	fprintf(fp, "\n# %s:\n", str);

	for(i = 0; i < assocs->count; ++i)
	{
		int j;

		assoc_t assoc = assocs->list[i];

		for(j = 0; j < assoc.records.count; ++j)
		{
			assoc_record_t ft_record = assoc.records.list[j];

			/* The type check is to prevent builtin fake associations to be written
			 * into vifminfo file. */
			if(ft_record.command[0] == '\0' || ft_record.type == ART_BUILTIN)
			{
				continue;
			}

			if(ft_record.description[0] == '\0')
			{
				fprintf(fp, "%c%s\n\t", mark, matchers_get_expr(assoc.matchers));
			}
			else
			{
				fprintf(fp, "%c%s\n\t{%s}", mark, matchers_get_expr(assoc.matchers),
						ft_record.description);
			}
			write_doubling_commas(fp, ft_record.command);
			fputc('\n', fp);
		}
	}

	for(i = 0; i < prev_count; i += 2)
	{
		fprintf(fp, "%c%s\n\t%s\n", mark, prev[i], prev[i + 1]);
	}
}

/* Prints the string into the file doubling commas in process. */
static void
write_doubling_commas(FILE *fp, const char str[])
{
	while(*str != '\0')
	{
		if(*str == ',')
		{
			fputc(',', fp);
		}
		fputc(*str++, fp);
	}
}

/* Writes user-defined commands to vifminfo file.  cmds_list is a NULL
 * terminated list of commands existing in current session, cmds is a list of
 * length ncmds with unseen commands read from vifminfo. */
static void
write_commands(FILE *fp, char *cmds_list[], char *cmds[], int ncmds)
{
	int i;

	fputs("\n# Commands:\n", fp);
	for(i = 0; cmds_list[i] != NULL; i += 2)
	{
		fprintf(fp, "!%s\n\t%s\n", cmds_list[i], cmds_list[i + 1]);
	}
	for(i = 0; i < ncmds; i += 2)
	{
		fprintf(fp, "!%s\n\t%s\n", cmds[i], cmds[i + 1]);
	}
}

/* Writes marks to vifminfo file.  marks is a list of length nmarks marks read
 * from vifminfo. */
static void
write_marks(FILE *fp, const char non_conflicting_marks[], char *marks[],
		const int timestamps[], int nmarks)
{
	int active_marks[NUM_MARKS];
	const int len = init_active_marks(valid_marks, active_marks);
	int i;

	fputs("\n# Marks:\n", fp);
	for(i = 0; i < len; ++i)
	{
		const int index = active_marks[i];
		const char m = index2mark(index);
		if(!is_spec_mark(index) && char_is_one_of(non_conflicting_marks, m))
		{
			const mark_t *const mark = get_mark(index);

			fprintf(fp, "%c%c\n", LINE_TYPE_MARK, m);
			fprintf(fp, "\t%s\n", mark->directory);
			fprintf(fp, "\t%s\n", mark->file);
			fprintf(fp, "%lld\n", (long long)mark->timestamp);
		}
	}
	for(i = 0; i < nmarks; i += 3)
	{
		fprintf(fp, "%c%c\n", LINE_TYPE_MARK, marks[i][0]);
		fprintf(fp, "\t%s\n", marks[i + 1]);
		fprintf(fp, "\t%s\n", marks[i + 2]);
		fprintf(fp, "%d\n", timestamps[i/3]);
	}
}

/* Writes bookmarks to vifminfo file.  bmarks is a list of length nbmarks marks
 * read from vifminfo. */
static void
write_bmarks(FILE *fp, char *bmarks[], const int timestamps[], int nbmarks)
{
	int i;

	fputs("\n# Bookmarks:\n", fp);

	bmarks_list(&write_bmark, fp);

	for(i = 0; i < nbmarks; i += 2)
	{
		fprintf(fp, "%c%s\n", LINE_TYPE_BOOKMARK, bmarks[i]);
		fprintf(fp, "\t%s\n", bmarks[i + 1]);
		fprintf(fp, "\t%d\n", timestamps[i/2]);
	}
}

/* bmarks_list() callback that writes a bookmark into vifminfo. */
static void
write_bmark(const char path[], const char tags[], time_t timestamp, void *arg)
{
	FILE *const fp = arg;
	fprintf(fp, "%c%s\n", LINE_TYPE_BOOKMARK, path);
	fprintf(fp, "\t%s\n", tags);
	fprintf(fp, "\t%d\n", (int)timestamp);
}

/* Writes state of the TUI to vifminfo file. */
static void
write_tui_state(FILE *fp)
{
	fputs("\n# TUI:\n", fp);
	fprintf(fp, "%c%c\n", LINE_TYPE_ACTIVE_VIEW,
			(curr_view == &rwin) ? 'r' : 'l');
	fprintf(fp, "%c%u\n", LINE_TYPE_QUICK_VIEW_STATE, curr_stats.preview.on);
	fprintf(fp, "%c%d\n", LINE_TYPE_WIN_COUNT, curr_stats.number_of_windows);
	fprintf(fp, "%c%c\n", LINE_TYPE_SPLIT_ORIENTATION,
			(curr_stats.split == VSPLIT) ? 'v' : 'h');
	fprintf(fp, "%c%d\n", LINE_TYPE_SPLIT_POSITION, curr_stats.splitter_pos);

	put_sort_info(fp, LINE_TYPE_LWIN_SORT, &lwin);
	put_sort_info(fp, LINE_TYPE_RWIN_SORT, &rwin);
}

/* Stores history of the view to the file. */
static void
write_view_history(FILE *fp, view_t *view, const char str[], char mark,
		int prev_count, char *prev[], int pos[])
{
	int i;
	flist_hist_save(view, NULL, NULL, -1);
	fprintf(fp, "\n# %s window history (oldest to newest):\n", str);
	for(i = 0; i < prev_count; i += 2)
	{
		fprintf(fp, "%c%s\n\t%s\n%d\n", mark, prev[i], prev[i + 1], pos[i/2]);
	}
	for(i = 0; i <= view->history_pos && i < view->history_num; i++)
	{
		fprintf(fp, "%c%s\n\t%s\n%d\n", mark, view->history[i].dir,
				view->history[i].file, view->history[i].rel_pos);
	}
	if(cfg.vifm_info & VINFO_SAVEDIRS)
	{
		fprintf(fp, "%c\n", mark);
	}
}

/* Stores history of the view into JSON representation. */
static void
store_dhistory(JSON_Object *obj, view_t *view)
{
	flist_hist_save(view, NULL, NULL, -1);

	JSON_Array *history = add_array(obj, "history");

	int i;
	for(i = 0; i <= view->history_pos && i < view->history_num; ++i)
	{
		JSON_Object *entry = append_object(history);
		set_str(entry, "dir", view->history[i].dir);
		set_str(entry, "file", view->history[i].file);
		set_int(entry, "relpos", view->history[i].rel_pos);
	}

	set_bool(obj, "restore-last-location", cfg.vifm_info & VINFO_SAVEDIRS);
}

/* Stores history items to the file. */
static void
write_history(FILE *fp, const char str[], char mark, int prev_count,
		char *prev[], const hist_t *hist)
{
	int i;
	fprintf(fp, "\n# %s history (oldest to newest):\n", str);
	for(i = 0; i < prev_count; i++)
	{
		fprintf(fp, "%c%s\n", mark, prev[i]);
	}
	for(i = hist->pos; i >= 0; i--)
	{
		fprintf(fp, "%c%s\n", mark, hist->items[i]);
	}
}

/* Writes registers to vifminfo file.  regs is a list of length nregs registers
 * read from vifminfo. */
static void
write_registers(FILE *fp, char *regs[], int nregs)
{
	int i;

	fputs("\n# Registers:\n", fp);
	for(i = 0; i < nregs; i++)
	{
		fprintf(fp, "%s\n", regs[i]);
	}
	for(i = 0; valid_registers[i] != '\0'; i++)
	{
		const reg_t *const reg = regs_find(valid_registers[i]);
		if(reg != NULL)
		{
			int j;
			for(j = 0; j < reg->nfiles; ++j)
			{
				if(reg->files[j] != NULL)
				{
					fprintf(fp, "%c%c%s\n", LINE_TYPE_REG, reg->name, reg->files[j]);
				}
			}
		}
	}
}

/* Writes directory stack to vifminfo file.  old_dir_stack is a list of length
 * nold_dir_stack entries (4 lines per entry) read from vifminfo. */
static void
write_dir_stack(FILE *fp, char *old_dir_stack[], int nold_dir_stack)
{
	fputs("\n# Directory stack (oldest to newest):\n", fp);
	if(dir_stack_changed())
	{
		unsigned int i;
		for(i = 0U; i < dir_stack_top; ++i)
		{
			dir_stack_entry_t *const entry = &dir_stack[i];
			fprintf(fp, "S%s\n\t%s\n", entry->lpane_dir, entry->lpane_file);
			fprintf(fp, "S%s\n\t%s\n", entry->rpane_dir, entry->rpane_file);
		}
	}
	else
	{
		int i;
		for(i = 0; i < nold_dir_stack; i += 4)
		{
			fprintf(fp, "S%s\n\t%s\n", old_dir_stack[i], old_dir_stack[i + 1]);
			fprintf(fp, "S%s\n\t%s\n", old_dir_stack[i + 2], old_dir_stack[i + 3]);
		}
	}
}

/* Writes trash entries to vifminfo file.  trash is a list of length ntrash
 * entries read from vifminfo. */
static void
write_trash(FILE *fp, char *trash[], int ntrash)
{
	int i;
	fputs("\n# Trash content:\n", fp);
	for(i = 0; i < trash_list_size; i++)
	{
		fprintf(fp, "%c%s\n\t%s\n", LINE_TYPE_TRASH, trash_list[i].trash_name,
				trash_list[i].path);
	}
	for(i = 0; i < ntrash; i += 2)
	{
		fprintf(fp, "%c%s\n\t%s\n", LINE_TYPE_TRASH, trash[i], trash[i + 1]);
	}
}

/* Writes general state to vifminfo file. */
static void
write_general_state(FILE *fp)
{
	fputs("\n# State:\n", fp);

	fprintf(fp, "%c%s\n", LINE_TYPE_LWIN_FILT,
			matcher_get_expr(lwin.manual_filter));
	fprintf(fp, "%c%d\n", LINE_TYPE_LWIN_FILT_INV, lwin.invert);
	fprintf(fp, "%c%c%d\n", LINE_TYPE_LWIN_SPECIFIC, PROP_TYPE_DOTFILES,
			lwin.hide_dot);
	fprintf(fp, "%c%c%s\n", LINE_TYPE_LWIN_SPECIFIC, PROP_TYPE_AUTO_FILTER,
			lwin.auto_filter.raw);

	fprintf(fp, "%c%s\n", LINE_TYPE_RWIN_FILT,
			matcher_get_expr(rwin.manual_filter));
	fprintf(fp, "%c%d\n", LINE_TYPE_RWIN_FILT_INV, rwin.invert);
	fprintf(fp, "%c%c%d\n", LINE_TYPE_RWIN_SPECIFIC, PROP_TYPE_DOTFILES,
			rwin.hide_dot);
	fprintf(fp, "%c%c%s\n", LINE_TYPE_RWIN_SPECIFIC, PROP_TYPE_AUTO_FILTER,
			rwin.auto_filter.raw);

	fprintf(fp, "%c%d\n", LINE_TYPE_USE_SCREEN, cfg.use_term_multiplexer);
}

/* Reads line from configuration file.  Takes care of trailing newline character
 * (removes it) and leading whitespace.  Buffer should be NULL or valid memory
 * buffer allocated on heap.  Returns reallocated buffer or NULL on error or
 * when end of file is reached. */
static char *
read_vifminfo_line(FILE *fp, char buffer[])
{
	if((buffer = read_line(fp, buffer)) != NULL)
	{
		remove_leading_whitespace(buffer);
	}
	return buffer;
}

/* Removes leading whitespace from the line in place. */
static void
remove_leading_whitespace(char line[])
{
	const char *const non_whitespace = skip_whitespace(line);
	if(non_whitespace != line)
	{
		memmove(line, non_whitespace, strlen(non_whitespace) + 1);
	}
}

/* Escapes spaces in the string.  Returns pointer to a statically allocated
 * buffer. */
static const char *
escape_spaces(const char str[])
{
	static char buf[4096];
	char *p;

	p = buf;
	while(*str != '\0')
	{
		if(*str == '\\' || *str == ' ')
			*p++ = '\\';
		*p++ = *str;

		str++;
	}
	*p = '\0';
	return buf;
}

/* Writes sort description line of the view to the fp file prepending the
 * leading_char to it. */
static void
put_sort_info(FILE *fp, char leading_char, const view_t *view)
{
	char buf[SK_LAST*5 + 1];
	make_sort_info(view, buf, sizeof(buf));

	fprintf(fp, "%c%s\n", leading_char, buf);
}

/* Puts sort description line of the view into JSON representation. */
static void
store_sort_info(JSON_Object *obj, const view_t *view)
{
	char buf[SK_LAST*5 + 1];
	make_sort_info(view, buf, sizeof(buf));

	set_str(obj, "sorting", buf);
}

/* Builds a string describing sorting state of a view in the buffer. */
static void
make_sort_info(const view_t *view, char buf[], size_t buf_size)
{
	size_t len = 0U;

	const signed char *const sort = ui_view_sort_list_get(view, view->sort_g);

	int i = -1;
	while(++i < SK_COUNT && abs(sort[i]) <= SK_LAST)
	{
		int is_last_option = i >= SK_COUNT - 1 || abs(sort[i + 1]) > SK_LAST;

		char piece[10];
		snprintf(piece, sizeof(piece), "%d%s", sort[i], is_last_option ? "" : ",");

		sstrappend(buf, &len, buf_size, piece);
	}
}

/* Ensures that the next character of the stream is a digit and reads a number.
 * Returns read number or -1 in case there is no digit. */
static int
read_optional_number(FILE *f)
{
	int num;
	int nread;
	int c;
	fpos_t pos;

	if(fgetpos(f, &pos) != 0)
	{
		return -1;
	}

	c = getc(f);
	if(c == EOF)
	{
		return -1;
	}
	ungetc(c, f);

	if(!isdigit(c) && c != '-' && c != '+')
	{
		return -1;
	}

	nread = fscanf(f, "%30d\n", &num);
	if(nread != 1)
	{
		fsetpos(f, &pos);
		return -1;
	}

	return num;
}

/* Converts line to number.  Returns non-zero on success and zero otherwise. */
static int
read_number(const char line[], long *value)
{
	char *endptr;
	*value = strtol(line, &endptr, 10);
	return *line != '\0' && *endptr == '\0';
}

static size_t
add_to_int_array(int **array, size_t len, int what)
{
	int *p;

	p = reallocarray(*array, len + 1, sizeof(*p));
	if(p != NULL)
	{
		*array = p;
		(*array)[len++] = what;
	}

	return len;
}

/* Adds a new child array to an object and returns a pointer to it. */
static JSON_Array *
add_array(JSON_Object *obj, const char key[])
{
	JSON_Value *array = json_value_init_array();
	json_object_set_value(obj, key, array);
	return json_array(array);
}

/* Adds a new child object to an object and returns a pointer to it. */
static JSON_Object *
add_object(JSON_Object *obj, const char key[])
{
	JSON_Value *object = json_value_init_object();
	json_object_set_value(obj, key, object);
	return json_object(object);
}

/* Appends a new object to an array and returns a pointer to it. */
static JSON_Object *
append_object(JSON_Array *arr)
{
	JSON_Value *object = json_value_init_object();
	json_array_append_value(arr, object);
	return json_object(object);
}

/* Assigns value of a boolean key from a table to *value.  Returns non-zero if
 * value was assigned and zero otherwise and doesn't change *value. */
static int
get_bool(const JSON_Object *obj, const char key[], int *value)
{
	JSON_Value *val = json_object_get_value(obj, key);
	if(json_value_get_type(val) == JSONBoolean)
	{
		*value = json_value_get_boolean(val);
	}
	return (val != NULL);
}

/* Assigns value of an integer key from a table to *value.  Returns non-zero if
 * value was assigned and zero otherwise and doesn't change *value. */
static int
get_int(const JSON_Object *obj, const char key[], int *value)
{
	JSON_Value *val = json_object_get_value(obj, key);
	if(json_value_get_type(val) == JSONNumber)
	{
		*value = json_value_get_number(val);
	}
	return (val != NULL);
}

/* Assigns value of a double key from a table to *value.  Returns non-zero if
 * value was assigned and zero otherwise and doesn't change *value. */
static int
get_double(const JSON_Object *obj, const char key[], double *value)
{
	JSON_Value *val = json_object_get_value(obj, key);
	if(json_value_get_type(val) == JSONNumber)
	{
		*value = json_value_get_number(val);
	}
	return (val != NULL);
}

/* Assigns value of a string key from a table to *value.  Returns non-zero if
 * value was assigned and zero otherwise and doesn't change *value. */
static int
get_str(const JSON_Object *obj, const char key[], const char **value)
{
	JSON_Value *val = json_object_get_value(obj, key);
	if(json_value_get_type(val) == JSONString)
	{
		*value = json_value_get_string(val);
	}
	return (val != NULL);
}

/* Assigns value to a boolean key in a table. */
static void
set_bool(JSON_Object *obj, const char key[], int value)
{
	json_object_set_boolean(obj, key, value);
}

/* Assigns value to an integer key in a table. */
static void
set_int(JSON_Object *obj, const char key[], int value)
{
	json_object_set_number(obj, key, value);
}

/* Assigns value to a double key in a table. */
static void
set_double(JSON_Object *obj, const char key[], double value)
{
	json_object_set_number(obj, key, value);
}

/* Assigns value to a string key in a table. */
static void
set_str(JSON_Object *obj, const char key[], const char value[])
{
	json_object_set_string(obj, key, value);
}

/* Appends value of a dynamically allocated string to an array, freeing the
 * string afterwards. */
static void
append_dstr(JSON_Array *array, char value[])
{
	json_array_append_string(array, value);
	free(value);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
