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
#include <locale.h> /* setlocale() LC_ALL */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* FILE fpos_t fclose() fgetpos() fgets() fprintf() fputc()
                      fscanf() fsetpos() snprintf() */
#include <stdlib.h> /* abs() free() */
#include <string.h> /* memcpy() memset() strtol() strcmp() strchr() strlen() */
#include <time.h> /* time_t time() */

#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "../compat/reallocarray.h"
#include "../engine/cmds.h"
#include "../engine/completion.h"
#include "../engine/options.h"
#include "../io/iop.h"
#include "../ui/fileview.h"
#include "../ui/tabs.h"
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
#include "../utils/test_helpers.h"
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

/**
 * Schema-like description of vifminfo.json data:
 *  gtabs = [ {
 *      panes = [ {
 *          ptabs = [ {
 *              history = [ {
 *                  dir = "/some/directory"
 *                  file = "file.png"
 *                  relpos = 28
 *                  ts = 1440801895 # timestamp (optional)
 *              } ]
 *              filters = {
 *                  dot = true
 *                  manual = ""
 *                  auto = ""
 *                  invert = false
 *              }
 *              name = "ptab-name"
 *              options = [ "opt1=val1", "opt2=val2" ]
 *              last-location = "/some/path"
 *              sorting = [ 1, -2, 3 ]
 *              preview = false
 *          } ]
 *          active-ptab = 0
 *      } ]
 *      splitter = {
 *          pos = -1
 *          ratio = 0.5
 *          orientation = "v" # or "h"
 *          expanded = false  # only mode
 *      }
 *      name = "gtab-name"
 *      active-pane = 0
 *      preview = false
 *  } ]
 *  regs = {
 *      "a" = [ "/path1", "/path2" ]
 *  }
 *  trash = [ {
 *      trashed = "/trash/0_file"
 *      original = "/file"
 *  } ]
 *  bmarks = {
 *      "/bookmarked/path" = {
 *          tags = "tag1,tag2"
 *          ts = 1440801895 # timestamp
 *      }
 *  }
 *  marks = {
 *      "h" = {
 *          dir = "/path"
 *          file = "file.jpg"
 *          ts = 1440801895 # timestamp
 *      }
 *  }
 *  cmds = {
 *      "cmd-name" = "echo hi"
 *  }
 *  viewers = [ {
 *      matchers = "{*.jpg}"
 *      cmd = "echo hi"
 *  } ]
 *  assocs = [ {
 *      matchers = "{*.jpg}"
 *      cmd = "echo hi"
 *  } ]
 *  xassocs = [ {
 *      matchers = "{*.jpg}"
 *      cmd = "echo hi"
 *  } ]
 *  dir-stack = [ {
 *      left-dir = "/left/dir"
 *      left-file = "left-file"
 *      right-dir = "/right/dir"
 *      right-file = "right-file"
 *  } ]
 *  options = [ "opt1=val1", "opt2=val2" ]
 *  cmd-hist = [ {
 *      text = "item1"
 *      ts = 1440801895 # timestamp (optional)
 *  } ]
 *  search-hist = [ {
 *      text = "item1"
 *      ts = 1440801895 # timestamp (optional)
 *  } ]
 *  prompt-hist = [ {
 *      text = "item1"
 *      ts = 1440801895 # timestamp (optional)
 *  } ]
 *  lfilt-hist = [ {
 *      text = "item1"
 *      ts = 1440801895 # timestamp (optional)
 *  } ]
 *  exprreg-hist = [ {
 *      text = "item1"
 *      ts = 1440801895 # timestamp (optional)
 *  } ]
 *  active-gtab = 0
 *  use-term-multiplexer = true
 *  color-scheme = "almost-default"
 *
 * Elements in history arrays are stored oldest to newest.
 *
 * Timestamps are used for merging.  However, there are two kinds of merging
 * that can happen:
 *  - elements of dictionaries are merged individually with each other
 *  - for elements of arrays timestamps act more like generation numbers and
 *    while merging happens per element, effectively it's generations (defined
 *    by time of storing of the array) which are being merged
 */

static JSON_Value * read_legacy_info_file(const char info_file[]);
static void load_state(JSON_Object *root, int reread);
static void load_gtabs(JSON_Object *root, int reread);
static tab_layout_t load_gtab_layout(const JSON_Object *gtab, int apply,
		int reread);
static void load_pane(JSON_Object *pane, view_t *view, int right, int reread);
static void load_ptab(JSON_Object *ptab, view_t *view, int reread);
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
static void load_history(JSON_Object *root, const char node[], hist_t *hist);
static void load_sorting(JSON_Object *ptab, view_t *view);
static void ensure_history_not_full(hist_t *hist);
static void put_dhistory_entry(view_t *view, int reread, const char dir[],
		const char file[], int rel_pos, time_t timestamp);
static void set_manual_filter(view_t *view, const char value[]);
TSTATIC void write_info_file(void);
static int copy_file(const char src[], const char dst[]);
static void update_info_file(const char filename[], int vinfo, int merge);
TSTATIC char * drop_locale(void);
TSTATIC void restore_locale(char locale[]);
TSTATIC JSON_Value * serialize_state(int vinfo);
TSTATIC void merge_states(int vinfo, int session_load, JSON_Object *current,
		const JSON_Object *admixture);
static void merge_tabs(int vinfo, int session_load, JSON_Object *current,
		const JSON_Object *admixture);
static void merge_dhistory(int session_load, JSON_Object *current,
		const JSON_Object *admixture);
static void merge_dhistory_by_order(JSON_Object *current,
		const JSON_Object *admixture);
static JSON_Object ** merge_timestamped_data(const JSON_Array *current,
		const JSON_Array *updated);
static JSON_Object ** make_timestamp_ordering(const JSON_Array *array);
static int timestamp_cmp(const void *a, const void *b);
static void merge_assocs(JSON_Object *current, const JSON_Object *admixture,
		const char node[], assoc_list_t *assocs);
static void merge_commands(JSON_Object *current, const JSON_Object *admixture);
static void merge_marks(JSON_Object *current, const JSON_Object *admixture);
static void merge_bmarks(JSON_Object *current, const JSON_Object *admixture);
static void merge_history(int session_load, JSON_Object *current,
		const JSON_Object *admixture, const char node[]);
static void merge_history_by_order(JSON_Object *current,
		const JSON_Object *admixture, const char node[]);
static void merge_regs(JSON_Object *current, const JSON_Object *admixture);
static void merge_dir_stack(JSON_Object *current, const JSON_Object *admixture);
static void merge_options(JSON_Object *current, const JSON_Object *admixture);
static void merge_trash(JSON_Object *current, const JSON_Object *admixture);
static void store_gtab(int vinfo, JSON_Object *gtab, const char name[],
		const tab_layout_t *layout, view_t *left, view_t *right);
static void store_pane(int vinfo, JSON_Object *pane, view_t *view, int right);
static void store_ptab(int vinfo, JSON_Object *ptab, const char name[],
		int preview, view_t *view);
static void store_filters(JSON_Object *view_data, const view_t *view);
static void store_history(JSON_Object *root, const char node[],
		const hist_t *hist);
static void store_global_options(JSON_Object *root);
static void store_view_options(JSON_Object *parent, const view_t *view);
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
static char * convert_old_trash_path(const char trash_path[]);
static void store_dhistory(JSON_Object *obj, view_t *view);
static char * read_vifminfo_line(FILE *fp, char buffer[]);
static void remove_leading_whitespace(char line[]);
static const char * escape_spaces(const char *str);
static void store_sort_info(JSON_Object *obj, const view_t *view);
static int read_optional_number(FILE *f);
static int read_number(const char line[], long *value);
static JSON_Array * add_array(JSON_Object *obj, const char key[]);
static JSON_Object * add_object(JSON_Object *obj, const char key[]);
static JSON_Object * append_object(JSON_Array *arr);
static int get_bool(const JSON_Object *obj, const char key[], int *value);
static int get_int(const JSON_Object *obj, const char key[], int *value);
static int get_double(const JSON_Object *obj, const char key[], double *value);
static int get_str(const JSON_Object *obj, const char key[],
		const char **value);
static void set_bool(JSON_Object *obj, const char key[], int value);
static void set_int(JSON_Object *obj, const char key[], int value);
static void set_double(JSON_Object *obj, const char key[], double value);
static void set_str(JSON_Object *obj, const char key[], const char value[]);
static void append_int(JSON_Array *array, int value);
static void append_dstr(JSON_Array *array, char value[]);
static void clone_missing(JSON_Object *obj, const JSON_Object *source);
static void clone_object(JSON_Object *parent, const JSON_Object *object,
		const char node[]);
static void clone_array(JSON_Object *parent, const JSON_Array *array,
		const char node[]);
static void set_session(const char new_session[]);
static void write_session_file(void);
static void store_file(const char path[], filemon_t *mon, int vinfo);
static void get_session_dir(char buf[], size_t buf_size);

/* Monitor to check for changes of vifminfo file. */
static filemon_t vifminfo_mon;
/* Monitor to check for changes of file that backs current session. */
static filemon_t session_mon;
/* Callback to be invoked when active session has changed.  Can be NULL. */
static sessions_changed session_changed_cb;

void
state_store(void)
{
	write_info_file();

	if(sessions_active())
	{
		write_session_file();
	}
}

void
state_load(int reread)
{
	char info_file[PATH_MAX + 16];
	snprintf(info_file, sizeof(info_file), "%s/vifminfo.json", cfg.config_dir);

	char *locale = drop_locale();
	JSON_Value *state = json_parse_file(info_file);
	restore_locale(locale);

	if(state == NULL)
	{
		char legacy_info_file[PATH_MAX + 16];
		snprintf(legacy_info_file, sizeof(legacy_info_file), "%s/vifminfo",
				cfg.config_dir);
		state = read_legacy_info_file(legacy_info_file);
	}
	if(state == NULL)
	{
		return;
	}

	load_state(json_object(state), reread);
	json_value_free(state);

	(void)filemon_from_file(info_file, FMT_MODIFIED, &vifminfo_mon);

	dir_stack_freeze();
}

/* Reads legacy barely-structured vifminfo format as a JSON.  Returns JSON
 * value or NULL on error. */
static JSON_Value *
read_legacy_info_file(const char info_file[])
{
	FILE *fp = os_fopen(info_file, "r");
	if(fp == NULL)
	{
		return NULL;
	}

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

	JSON_Array *gtabs = add_array(root, "gtabs");
	JSON_Object *gtab = append_object(gtabs);

	JSON_Object *splitter = add_object(gtab, "splitter");

	JSON_Array *panes = add_array(gtab, "panes");
	JSON_Object *left = append_object(panes);
	JSON_Object *right = append_object(panes);

	JSON_Array *left_tabs = add_array(left, "ptabs");
	JSON_Object *left_tab = append_object(left_tabs);

	JSON_Array *right_tabs = add_array(right, "ptabs");
	JSON_Object *right_tab = append_object(right_tabs);

	JSON_Array *left_history = add_array(left_tab, "history");
	JSON_Array *right_history = add_array(right_tab, "history");

	JSON_Object *left_filters = add_object(left_tab, "filters");
	JSON_Object *right_filters = add_object(right_tab, "filters");

	JSON_Array *left_options = add_array(left_tab, "options");
	JSON_Array *right_options = add_array(right_tab, "options");

	char *last_location = NULL;
	char *line = NULL, *line2 = NULL, *line3 = NULL, *line4 = NULL;
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
			JSON_Array *array = NULL;
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
					long long int timestamp = read_optional_number(fp);
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
			set_int(gtab, "active-pane", (line_val[0] == 'l' ? 0 : 1));
		}
		else if(type == LINE_TYPE_QUICK_VIEW_STATE)
		{
			set_bool(gtab, "preview", atoi(line_val));
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
		else if(type == LINE_TYPE_LWIN_SORT || type == LINE_TYPE_RWIN_SORT)
		{
			JSON_Object *tab = (type == LINE_TYPE_LWIN_SORT ? left_tab : right_tab);
			JSON_Array *sorting = add_array(tab, "sorting");
			char *part = line + 1, *state = NULL;
			while((part = split_and_get(part, ',', &state)) != NULL)
			{
				char *endptr;
				int sort_key = strtol(part, &endptr, 10);
				if(*endptr == '\0')
				{
					append_int(sorting, sort_key);
				}
			}
		}
		else if(type == LINE_TYPE_LWIN_HIST || type == LINE_TYPE_RWIN_HIST)
		{
			if(line_val[0] == '\0')
			{
				JSON_Object *ptab = (type == LINE_TYPE_LWIN_HIST)
				                  ? left_tab : right_tab;
				set_str(ptab, "last-location", last_location);
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

				replace_string(&last_location, line_val);
			}
		}
		else if(type == LINE_TYPE_CMDLINE_HIST)
		{
			set_str(append_object(cmd_hist), "text", line_val);
		}
		else if(type == LINE_TYPE_SEARCH_HIST)
		{
			set_str(append_object(search_hist), "text", line_val);
		}
		else if(type == LINE_TYPE_PROMPT_HIST)
		{
			set_str(append_object(prompt_hist), "text", line_val);
		}
		else if(type == LINE_TYPE_FILTER_HIST)
		{
			set_str(append_object(lfilt_hist), "text", line_val);
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
	free(last_location);
	fclose(fp);

	return root_value;
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

	load_gtabs(root, reread);

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
	load_history(root, "cmd-hist", &curr_stats.cmd_hist);
	load_history(root, "exprreg-hist", &curr_stats.exprreg_hist);
	load_history(root, "search-hist", &curr_stats.search_hist);
	load_history(root, "prompt-hist", &curr_stats.prompt_hist);
	load_history(root, "lfilt-hist", &curr_stats.filter_hist);
}

/* Loads global tabs from JSON. */
static void
load_gtabs(JSON_Object *root, int reread)
{
	JSON_Array *gtabs = json_object_get_array(root, "gtabs");

	if(reread)
	{
		if(json_array_get_count(gtabs) > 1)
		{
			return;
		}

		JSON_Object *gtab = json_array_get_object(gtabs, 0);
		JSON_Array *panes = json_object_get_array(gtab, "panes");
		JSON_Object *lpane = json_array_get_object(panes, 0);
		JSON_Object *rpane = json_array_get_object(panes, 1);
		JSON_Array *lptabs = json_object_get_array(lpane, "ptabs");
		JSON_Array *rptabs = json_object_get_array(rpane, "ptabs");

		if(json_array_get_count(lptabs) > 1 || json_array_get_count(rptabs) > 1)
		{
			return;
		}
	}

	int i, n;
	view_t *left = &lwin, *right = &rwin;
	for(i = 0, n = json_array_get_count(gtabs); i < n; ++i)
	{
		JSON_Object *gtab = json_array_get_object(gtabs, i);

		tab_layout_t layout = load_gtab_layout(gtab, i == 0, reread);

		const char *name = NULL;
		(void)get_str(gtab, "name", &name);

		if(i == 0)
		{
			tabs_rename(&lwin, name);
		}
		else
		{
			if(tabs_setup_gtab(name, &layout, &left, &right) != 0)
			{
				break;
			}
		}

		JSON_Array *panes = json_object_get_array(gtab, "panes");
		load_pane(json_array_get_object(panes, 0), left, 0, reread);
		load_pane(json_array_get_object(panes, 1), right, 1, reread);
	}

	int active_gtab;
	if(!cfg.pane_tabs && get_int(root, "active-gtab", &active_gtab))
	{
		tabs_goto(active_gtab);
	}
}

/* Loads (possibly applying it in the progress) layout information of a global
 * tab. */
static tab_layout_t
load_gtab_layout(const JSON_Object *gtab, int apply, int reread)
{
	tab_layout_t layout = {
		.active_pane = 0,
		.only_mode = 0,
		.split = VSPLIT,
		.splitter_pos = -1,
		.splitter_ratio = -1,
		.preview = 0,
	};

	const JSON_Object *splitter = json_object_get_object(gtab, "splitter");

	const char *split_kind;
	if(get_str(splitter, "orientation", &split_kind))
	{
		layout.split = (split_kind[0] == 'v' ? VSPLIT : HSPLIT);
		if(apply)
		{
			curr_stats.split = layout.split;
		}
	}
	if(get_int(splitter, "pos", &layout.splitter_pos) && apply)
	{
		stats_set_splitter_pos(layout.splitter_pos);
	}
	if(get_double(splitter, "ratio", &layout.splitter_ratio) && apply)
	{
		stats_set_splitter_ratio(layout.splitter_ratio);
	}
	if(get_bool(splitter, "expanded", &layout.only_mode) && apply && !reread)
	{
		curr_stats.number_of_windows = (layout.only_mode ? 1 : 2);
	}

	if(get_int(gtab, "active-pane", &layout.active_pane) && apply && !reread)
	{
		view_t *active = (layout.active_pane == 1 ? &rwin : &lwin);
		if(curr_view != active)
		{
			swap_view_roles();
			ui_views_update_titles();
		}
	}
	if(get_bool(gtab, "preview", &layout.preview) && apply)
	{
		stats_set_quickview(layout.preview);
	}

	return layout;
}

/* Loads a pane (consists of pane tabs) from JSON. */
static void
load_pane(JSON_Object *pane, view_t *view, int right, int reread)
{
	JSON_Array *ptabs = json_object_get_array(pane, "ptabs");

	int n = json_array_get_count(ptabs);
	if(n > 1)
	{
		/* More than one pane tab means that we need to change tab scope option. */
		cfg.pane_tabs = 1;
		load_tabscope_option();
	}

	tab_layout_t layout;
	tabs_layout_fill(&layout);

	int i;
	view_t *side = (right ? &rwin : &lwin);
	for(i = 0; i < n; ++i)
	{
		JSON_Object *ptab = json_array_get_object(ptabs, i);

		const char *name = NULL;
		(void)get_str(ptab, "name", &name);

		int preview = layout.preview;
		(void)get_bool(ptab, "preview", &preview);

		if(i == 0)
		{
			if(cfg.pane_tabs)
			{
				tabs_rename(side, name);
				stats_set_quickview(preview);
			}
		}
		else
		{
			view = tabs_setup_ptab(side, name, preview);
			if(view == NULL)
			{
				break;
			}
		}

		load_ptab(ptab, view, reread);
	}

	int active_ptab;
	if(cfg.pane_tabs && get_int(pane, "active-ptab", &active_ptab))
	{
		view_t *v = curr_view;
		curr_view = (right ? &rwin : &lwin);
		tabs_goto(active_ptab);
		curr_view = v;
	}
}

/* Loads a pane tab  from JSON. */
static void
load_ptab(JSON_Object *ptab, view_t *view, int reread)
{
	load_dhistory(ptab, view, reread);
	load_filters(ptab, view);

	view_t *v = curr_view;
	curr_view = view;
	load_options(ptab);
	curr_view = v;

	load_sorting(ptab, view);
}

/* Loads directory history of a view from JSON. */
static void
load_dhistory(JSON_Object *info, view_t *view, int reread)
{
	JSON_Array *history = json_object_get_array(info, "history");

	int i, n;
	for(i = 0, n = json_array_get_count(history); i < n; ++i)
	{
		JSON_Object *entry = json_array_get_object(history, i);

		const char *dir, *file;
		int rel_pos;
		if(get_str(entry, "dir", &dir) && get_str(entry, "file", &file) &&
				get_int(entry, "relpos", &rel_pos))
		{
			/* Timestamp is optional here. */
			double ts = -1;
			get_double(entry, "ts", &ts);
			put_dhistory_entry(view, reread, dir, file, rel_pos < 0 ? 0 : rel_pos,
					(time_t)ts);
		}
	}

	const char *last_location;
	if(!reread && get_str(info, "last-location", &last_location))
	{
		copy_str(view->curr_dir, sizeof(view->curr_dir), last_location);
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

	/* Nothing to do if there is no "invert" key. */
	(void)get_bool(filters, "invert", &view->invert);

	int dot;
	if(get_bool(filters, "dot", &dot))
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
		const char *opt_str = json_array_get_string(options, i);
		if(opt_str != NULL)
		{
			process_set_args(opt_str, 1, 1);
		}
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
		if(cmd != NULL)
		{
			char *cmdadd_cmd = format_str("command %s %s", name, cmd);
			if(cmdadd_cmd != NULL)
			{
				exec_commands(cmdadd_cmd, curr_view, CIT_COMMAND);
				free(cmdadd_cmd);
			}
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
			marks_setup_user(curr_view, name[0], dir, file, (time_t)ts);
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
			const char *file = json_array_get_string(files, j);
			if(file != NULL)
			{
				regs_append(name[0], file);
			}
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
load_history(JSON_Object *root, const char node[], hist_t *hist)
{
	JSON_Array *entries = json_object_get_array(root, node);

	int i, n;
	for(i = 0, n = json_array_get_count(entries); i < n; ++i)
	{
		JSON_Object *entry = json_array_get_object(entries, i);
		const char *text;
		if(get_str(entry, "text", &text))
		{
			/* Timestamp is optional here. */
			double ts = -1;
			get_double(entry, "ts", &ts);

			ensure_history_not_full(hist);
			hist_add(hist, text, (time_t)ts);
		}
	}
}

/* Loads view sorting from JSON. */
static void
load_sorting(JSON_Object *ptab, view_t *view)
{
	JSON_Array *sorting = json_object_get_array(ptab, "sorting");
	if(sorting == NULL)
	{
		return;
	}

	signed char *const sort = curr_stats.restart_in_progress
	                        ? ui_view_sort_list_get(view, view->sort)
	                        : view->sort;

	int i, n;
	int j = 0;
	for(i = 0, n = json_array_get_count(sorting); i < n && i < SK_COUNT; ++i)
	{
		JSON_Value *val = json_array_get_value(sorting, i);
		if(json_value_get_type(val) == JSONNumber)
		{
			int sort_key = json_value_get_number(val);
			view->sort_g[j++] = MIN(SK_LAST, MAX(-SK_LAST, sort_key));
		}
	}

	memset(&view->sort_g[j], SK_NONE, sizeof(view->sort_g) - j);
	if(j == 0)
	{
		view->sort_g[0] = SK_DEFAULT;
	}
	memcpy(sort, view->sort_g, sizeof(view->sort));

	fview_sorting_updated(view);
}

/* Checks that history has at least one more empty slot or extends history by
 * one more element. */
static void
ensure_history_not_full(hist_t *hist)
{
	if(hist->size == cfg.history_len)
	{
		cfg_resize_histories(cfg.history_len + 1);
		assert(hist->size < hist->capacity && "Failed to resize history.");
	}
}

/* Puts single history entry from vifminfo into the view. */
static void
put_dhistory_entry(view_t *view, int reread, const char dir[],
		const char file[], int rel_pos, time_t timestamp)
{
	if(view->history_num == cfg.history_len)
	{
		cfg_resize_histories(cfg.history_len + 1);
	}

	const int list_rows = view->list_rows;
	if(!reread)
	{
		view->list_rows = 1;
	}
	flist_hist_setup(view, dir, file, rel_pos, timestamp);
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

/* Writes vifminfo file updating it with state of the current instance. */
TSTATIC void
write_info_file(void)
{
	char info_file[PATH_MAX + 16];
	snprintf(info_file, sizeof(info_file), "%s/vifminfo.json", cfg.config_dir);

	store_file(info_file, &vifminfo_mon, cfg.vifm_info);
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

	return (iop_cp(&args) == IO_RES_SUCCEEDED ? 0 : 1);
}

/* Reads contents of the filename file as a JSON info file and updates it with
 * the state of current instance. */
static void
update_info_file(const char filename[], int vinfo, int merge)
{
	char *locale = drop_locale();
	JSON_Value *current = serialize_state(vinfo);

	if(merge)
	{
		JSON_Value *admixture = json_parse_file(filename);
		if(admixture != NULL)
		{
			merge_states(vinfo, 0, json_object(current), json_object(admixture));
			json_value_free(admixture);
		}
	}

	if(json_serialize_to_file(current, filename) == JSONError)
	{
		LOG_ERROR_MSG("Error storing state to: %s", filename);
	}

	json_value_free(current);
	restore_locale(locale);
}

/* Replaces current locale with C locale and returns string to be passed to
 * restore_locale() to get previous state back. */
TSTATIC char *
drop_locale(void)
{
	char *current = setlocale(LC_ALL, NULL);
	if(current != NULL)
	{
		current = strdup(current);
	}

	(void)setlocale(LC_ALL, "C");

	return current;
}

/* Restores locale state stored by drop_locale(). */
TSTATIC void
restore_locale(char locale[])
{
	if(locale != NULL)
	{
		(void)setlocale(LC_ALL, locale);
		free(locale);
	}
}

/* Serializes specified state of current instance into a JSON object.  Returns
 * the object. */
TSTATIC JSON_Value *
serialize_state(int vinfo)
{
	JSON_Value *root_value = json_value_init_object();
	JSON_Object *root = json_object(root_value);

	JSON_Array *gtabs = add_array(root, "gtabs");

	if(cfg.pane_tabs || !(vinfo & VINFO_TABS))
	{
		tab_layout_t layout;
		tabs_layout_fill(&layout);
		store_gtab(vinfo, append_object(gtabs), NULL, &layout, &lwin, &rwin);
	}
	else
	{
		int i;
		for(i = 0; i < tabs_count(&lwin); ++i)
		{
			tab_info_t left_tab_info, right_tab_info;
			tabs_enum(&lwin, i, &left_tab_info);
			tabs_enum(&rwin, i, &right_tab_info);
			store_gtab(vinfo, append_object(gtabs), left_tab_info.name,
					&left_tab_info.layout, left_tab_info.view, right_tab_info.view);
		}
		set_int(root, "active-gtab", tabs_current(&lwin));
	}

	store_trash(root);

	if(vinfo & VINFO_OPTIONS)
	{
		store_global_options(root);
	}

	if(vinfo & VINFO_FILETYPES)
	{
		store_assocs(root, "assocs", &filetypes);
		store_assocs(root, "xassocs", &xfiletypes);
		store_assocs(root, "viewers", &fileviewers);
	}

	if(vinfo & VINFO_COMMANDS)
	{
		store_cmds(root);
	}

	if(vinfo & VINFO_MARKS)
	{
		store_marks(root);
	}

	if(vinfo & VINFO_BOOKMARKS)
	{
		store_bmarks(root);
	}

	if(vinfo & VINFO_CHISTORY)
	{
		store_history(root, "cmd-hist", &curr_stats.cmd_hist);
	}

	if(vinfo & VINFO_EHISTORY)
	{
		store_history(root, "exprreg-hist", &curr_stats.exprreg_hist);
	}

	if(vinfo & VINFO_SHISTORY)
	{
		store_history(root, "search-hist", &curr_stats.search_hist);
	}

	if(vinfo & VINFO_PHISTORY)
	{
		store_history(root, "prompt-hist", &curr_stats.prompt_hist);
	}

	if(vinfo & VINFO_FHISTORY)
	{
		store_history(root, "lfilt-hist", &curr_stats.filter_hist);
	}

	if(vinfo & VINFO_REGISTERS)
	{
		store_regs(root);
	}

	if(vinfo & VINFO_DIRSTACK)
	{
		store_dir_stack(root);
	}

	if(vinfo & VINFO_STATE)
	{
		set_bool(root, "use-term-multiplexer", cfg.use_term_multiplexer);
	}

	if(vinfo & VINFO_CS)
	{
		set_str(root, "color-scheme", cfg.cs.name);
	}

	return root_value;
}

/* Adds parts of admixture to current state to avoid losing state stored by
 * other instances. */
TSTATIC void
merge_states(int vinfo, int session_load, JSON_Object *current,
		const JSON_Object *admixture)
{
	merge_tabs(vinfo, session_load, current, admixture);

	if(vinfo & VINFO_FILETYPES)
	{
		merge_assocs(current, admixture, "assocs", &filetypes);
		merge_assocs(current, admixture, "xassocs", &xfiletypes);
		merge_assocs(current, admixture, "viewers", &fileviewers);
	}

	if(vinfo & VINFO_COMMANDS)
	{
		merge_commands(current, admixture);
	}

	if(vinfo & VINFO_MARKS)
	{
		merge_marks(current, admixture);
	}

	if(vinfo & VINFO_BOOKMARKS)
	{
		merge_bmarks(current, admixture);
	}

	if(vinfo & VINFO_CHISTORY)
	{
		merge_history(session_load, current, admixture, "cmd-hist");
	}

	if(vinfo & VINFO_EHISTORY)
	{
		merge_history(session_load, current, admixture, "exprreg-hist");
	}

	if(vinfo & VINFO_SHISTORY)
	{
		merge_history(session_load, current, admixture, "search-hist");
	}

	if(vinfo & VINFO_PHISTORY)
	{
		merge_history(session_load, current, admixture, "prompt-hist");
	}

	if(vinfo & VINFO_FHISTORY)
	{
		merge_history(session_load, current, admixture, "lfilt-hist");
	}

	if(vinfo & VINFO_REGISTERS)
	{
		merge_regs(current, admixture);
	}

	if(vinfo & VINFO_DIRSTACK)
	{
		merge_dir_stack(current, admixture);
	}

	if(vinfo & VINFO_OPTIONS)
	{
		merge_options(current, admixture);
	}

	merge_trash(current, admixture);

	if(session_load)
	{
		clone_missing(current, admixture);
	}
}

/* Merges two sets of tabs if there is only one tab at each level (global and
 * pane). */
static void
merge_tabs(int vinfo, int session_load, JSON_Object *current,
		const JSON_Object *admixture)
{
	if(!(vinfo & VINFO_DHISTORY))
	{
		/* There is nothing to merge accept for directory history. */
		return;
	}

	JSON_Array *current_gtabs = json_object_get_array(current, "gtabs");
	JSON_Array *updated_gtabs = json_object_get_array(admixture, "gtabs");
	if(current_gtabs == NULL || json_array_get_count(current_gtabs) == 0)
	{
		clone_array(current, updated_gtabs, "gtabs");
		return;
	}

	if(json_array_get_count(current_gtabs) != 1 ||
			json_array_get_count(updated_gtabs) != 1)
	{
		return;
	}

	JSON_Object *current_gtab = json_array_get_object(current_gtabs, 0);
	JSON_Object *updated_gtab = json_array_get_object(updated_gtabs, 0);

	JSON_Array *current_panes = json_object_get_array(current_gtab, "panes");
	JSON_Array *updated_panes = json_object_get_array(updated_gtab, "panes");
	if(current_panes == NULL || json_array_get_count(current_panes) == 0)
	{
		clone_array(current_gtab, updated_panes, "panes");
		if(session_load)
		{
			clone_missing(current_gtab, updated_gtab);
		}
		return;
	}

	int i;
	for(i = 0; i < 2; ++i)
	{
		JSON_Object *current_pane = json_array_get_object(current_panes, i);
		JSON_Object *updated_pane = json_array_get_object(updated_panes, i);

		JSON_Array *current_ptabs = json_object_get_array(current_pane, "ptabs");
		JSON_Array *updated_ptabs = json_object_get_array(updated_pane, "ptabs");
		if(current_ptabs == NULL)
		{
			clone_array(current_pane, updated_ptabs, "ptabs");
			continue;
		}

		if(json_array_get_count(current_ptabs) == 1 &&
				json_array_get_count(updated_ptabs) == 1)
		{
			JSON_Object *current_ptab = json_array_get_object(current_ptabs, 0);
			JSON_Object *updated_ptab = json_array_get_object(updated_ptabs, 0);
			merge_dhistory(session_load, current_ptab, updated_ptab);
			if(session_load)
			{
				clone_missing(current_ptab, updated_ptab);
			}
		}
	}

	if(session_load)
	{
		clone_missing(current_gtab, updated_gtab);
	}
}

/* Merges two directory histories. */
static void
merge_dhistory(int session_load, JSON_Object *current,
		const JSON_Object *admixture)
{
	JSON_Array *history = json_object_get_array(current, "history");
	JSON_Array *updated = json_object_get_array(admixture, "history");
	if(history == NULL)
	{
		clone_array(current, updated, "history");
		return;
	}

	if(session_load)
	{
		merge_dhistory_by_order(current, admixture);
		return;
	}

	JSON_Object **combined = merge_timestamped_data(history, updated);
	int total = json_array_get_count(history) + json_array_get_count(updated);

	/* Squash adjacent entries that have the same directory leaving the latest
	 * one. */
	if(total > 0)
	{
		const char *last_dir = "";
		(void)get_str(combined[0], "dir", &last_dir);

		int i, j = 0;
		for(i = 1; i < total; ++i)
		{
			const char *curr_dir = "";
			(void)get_str(combined[i], "dir", &curr_dir);
			if(stroscmp(last_dir, curr_dir) != 0)
			{
				++j;
				last_dir = curr_dir;
			}

			combined[j] = combined[i];
		}
		total = j + 1;
	}

	JSON_Value *merged_value = json_value_init_array();
	JSON_Array *merged = json_array(merged_value);

	int i;
	int lower_limit = total - cfg.history_len;
	for(i = MAX(0, lower_limit); i < total; ++i)
	{
		JSON_Value *value = json_object_get_wrapping_value(combined[i]);
		json_array_append_value(merged, json_value_deep_copy(value));
	}

	free(combined);

	json_object_set_value(current, "history", merged_value);
}

/* Merges two directory histories not paying attention to timestamps thus giving
 * priority to data in the current parameter. */
static void
merge_dhistory_by_order(JSON_Object *current, const JSON_Object *admixture)
{
	JSON_Array *history = json_object_get_array(current, "history");
	JSON_Array *updated = json_object_get_array(admixture, "history");

	int i, n;
	JSON_Value *merged_value = json_value_init_array();
	JSON_Array *merged = json_array(merged_value);

	for(i = 0, n = json_array_get_count(updated); i < n; ++i)
	{
		JSON_Value *value = json_array_get_value(updated, i);
		json_array_append_value(merged, json_value_deep_copy(value));
	}

	for(i = 0, n = json_array_get_count(history); i < n; ++i)
	{
		JSON_Value *value = json_array_get_value(history, i);
		json_array_append_value(merged, json_value_deep_copy(value));
	}

	json_object_set_value(current, "history", merged_value);
}

/* Merges two arrays with elements that have "ts" keys with timestamps.  Returns
 * merged array of pointers to children of original arrays. */
static JSON_Object **
merge_timestamped_data(const JSON_Array *current, const JSON_Array *updated)
{
	JSON_Object **current_sorted = make_timestamp_ordering(current);
	JSON_Object **updated_sorted = make_timestamp_ordering(updated);

	int n = json_array_get_count(current);
	int m = json_array_get_count(updated);
	int total = n + m;

	JSON_Object **merged = reallocarray(NULL, total, sizeof(*merged));

	int i = 0, j = 0, k = 0;
	while(k < total && i < n && j < m)
	{
		if(timestamp_cmp(&current_sorted[i], &updated_sorted[j]) <= 0)
		{
			merged[k++] = current_sorted[i++];
		}
		else
		{
			merged[k++] = updated_sorted[j++];
		}
	}
	while(k < total && i < n)
	{
		merged[k++] = current_sorted[i++];
	}
	while(k < total && j < m)
	{
		merged[k++] = updated_sorted[j++];
	}

	free(current_sorted);
	free(updated_sorted);

	return merged;
}

/* Sorts elements of an array of objects according to optional "ts" field of its
 * elements (elements without the field are considered to be the smallest).
 * Returns dynamically allocated array of sorted elements. */
static JSON_Object **
make_timestamp_ordering(const JSON_Array *array)
{
	int n = json_array_get_count(array);
	JSON_Object **sorted = reallocarray(NULL, n, sizeof(*sorted));

	int i;
	for(i = 0; i < n; ++i)
	{
		sorted[i] = json_array_get_object(array, i);
	}

	safe_qsort(sorted, n, sizeof(*sorted), &timestamp_cmp);
	return sorted;
}

/* qsort() comparer that compares JSON_Object's according to values of optional
 * timestamp fields called "ts".  Returns standard -1, 0, 1 for comparisons. */
static int
timestamp_cmp(const void *a, const void *b)
{
	/* Timestamps can be optional. */
	double lhs_ts = -1, rhs_ts = -1;
	get_double(*(const JSON_Object **)a, "ts", &lhs_ts);
	get_double(*(const JSON_Object **)b, "ts", &rhs_ts);
	return lhs_ts - rhs_ts;
}

/* Merges two lists of associations. */
static void
merge_assocs(JSON_Object *current, const JSON_Object *admixture,
		const char node[], assoc_list_t *assocs)
{
	JSON_Array *entries = json_object_get_array(current, node);
	JSON_Array *updated = json_object_get_array(admixture, node);
	if(entries == NULL)
	{
		clone_array(current, updated, node);
		return;
	}

	int i, n;
	for(i = 0, n = json_array_get_count(updated); i < n; ++i)
	{
		JSON_Object *entry = json_array_get_object(updated, i);

		const char *matchers, *cmd;
		if(get_str(entry, "matchers", &matchers) && get_str(entry, "cmd", &cmd))
		{
			if(!ft_assoc_exists(assocs, matchers, cmd))
			{
				JSON_Value *value = json_object_get_wrapping_value(entry);
				json_array_append_value(entries, json_value_deep_copy(value));
			}
		}
	}
}

/* Merges two sets of :commands. */
static void
merge_commands(JSON_Object *current, const JSON_Object *admixture)
{
	JSON_Object *cmds = json_object_get_object(current, "cmds");
	JSON_Object *updated = json_object_get_object(admixture, "cmds");
	if(cmds == NULL)
	{
		clone_object(current, updated, "cmds");
		return;
	}

	int i, n;
	for(i = 0, n = json_object_get_count(updated); i < n; ++i)
	{
		const char *name = json_object_get_name(updated, i);
		if(!json_object_has_value(cmds, name))
		{
			JSON_Value *value = json_object_get_value_at(updated, i);
			json_object_set_value(cmds, name, json_value_deep_copy(value));
		}
	}
}

/* Merges two sets of marks. */
static void
merge_marks(JSON_Object *current, const JSON_Object *admixture)
{
	JSON_Object *marks = json_object_get_object(current, "marks");
	JSON_Object *updated = json_object_get_object(admixture, "marks");
	if(marks == NULL)
	{
		clone_object(current, updated, "marks");
		return;
	}

	int i, n;
	for(i = 0, n = json_object_get_count(updated); i < n; ++i)
	{
		JSON_Object *mark = json_object(json_object_get_value_at(updated, i));

		double ts;
		const char *name = json_object_get_name(updated, i);
		if(get_double(mark, "ts", &ts) &&
				marks_is_older(curr_view, name[0], (time_t)ts))
		{
			JSON_Value *value = json_object_get_wrapping_value(mark);
			json_object_set_value(marks, name, json_value_deep_copy(value));
		}
	}
}

/* Merges two sets of bookmarks. */
static void
merge_bmarks(JSON_Object *current, const JSON_Object *admixture)
{
	JSON_Object *bmarks = json_object_get_object(current, "bmarks");
	JSON_Object *updated = json_object_get_object(admixture, "bmarks");
	if(bmarks == NULL)
	{
		clone_object(current, updated, "bmarks");
		return;
	}

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
merge_history(int session_load, JSON_Object *current,
		const JSON_Object *admixture, const char node[])
{
	JSON_Array *updated = json_object_get_array(admixture, node);
	if(json_array_get_count(updated) == 0)
	{
		return;
	}

	JSON_Array *entries = json_object_get_array(current, node);
	if(entries == NULL)
	{
		clone_array(current, updated, node);
		return;
	}

	if(session_load)
	{
		merge_history_by_order(current, admixture, node);
		return;
	}

	trie_t *trie = trie_create(/*free_func=*/NULL);
	int i, n;

	JSON_Value *combined_value = json_value_init_array();
	JSON_Array *combined = json_array(combined_value);

	for(i = 0, n = json_array_get_count(entries); i < n; ++i)
	{
		const char *text;
		if(get_str(json_array_get_object(entries, i), "text", &text))
		{
			trie_put(trie, text);
		}
	}

	for(i = 0, n = json_array_get_count(updated); i < n; ++i)
	{
		JSON_Object *entry = json_array_get_object(updated, i);

		const char *text;
		if(get_str(entry, "text", &text))
		{
			void *data;
			if(trie_get(trie, text, &data) != 0)
			{
				JSON_Value *value = json_object_get_wrapping_value(entry);
				json_array_append_value(combined, json_value_deep_copy(value));
			}
		}
	}

	trie_free(trie);

	for(i = 0, n = json_array_get_count(entries); i < n; ++i)
	{
		JSON_Value *entry = json_array_get_value(entries, i);
		json_array_append_value(combined, json_value_deep_copy(entry));
	}

	JSON_Object **entries_sorted = make_timestamp_ordering(combined);

	JSON_Value *merged_value = json_value_init_array();
	JSON_Array *merged = json_array(merged_value);

	int lower_limit = n - cfg.history_len;
	for(n = json_array_get_count(combined), i = MAX(0, lower_limit); i < n; ++i)
	{
		JSON_Value *entry = json_object_get_wrapping_value(entries_sorted[i]);
		json_array_append_value(merged, json_value_deep_copy(entry));
	}

	json_value_free(combined_value);
	free(entries_sorted);

	json_object_set_value(current, node, merged_value);
}

/* Merges two states of a particular kind of history not paying attention to
 * timestamps thus giving priority to data in the current parameter. */
static void
merge_history_by_order(JSON_Object *current, const JSON_Object *admixture,
		const char node[])
{
	JSON_Array *entries = json_object_get_array(current, node);
	JSON_Array *updated = json_object_get_array(admixture, node);

	int i, n;
	trie_t *trie = trie_create(/*free_func=*/NULL);

	JSON_Value *merged_value = json_value_init_array();
	JSON_Array *merged = json_array(merged_value);

	for(i = 0, n = json_array_get_count(entries); i < n; ++i)
	{
		const char *text;
		if(get_str(json_array_get_object(entries, i), "text", &text))
		{
			trie_put(trie, text);
		}
	}

	for(i = 0, n = json_array_get_count(updated); i < n; ++i)
	{
		const char *text;
		if(get_str(json_array_get_object(updated, i), "text", &text))
		{
			void *data;
			if(trie_get(trie, text, &data) != 0)
			{
				JSON_Value *entry = json_array_get_value(updated, i);
				json_array_append_value(merged, json_value_deep_copy(entry));
			}
		}
	}

	trie_free(trie);

	for(i = 0, n = json_array_get_count(entries); i < n; ++i)
	{
		JSON_Value *entry = json_array_get_value(entries, i);
		json_array_append_value(merged, json_value_deep_copy(entry));
	}

	json_object_set_value(current, node, merged_value);
}

/* Merges two states of registers. */
static void
merge_regs(JSON_Object *current, const JSON_Object *admixture)
{
	JSON_Object *regs = json_object_get_object(current, "regs");
	JSON_Object *updated = json_object_get_object(admixture, "regs");
	if(regs == NULL)
	{
		clone_object(current, updated, "regs");
		return;
	}

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
merge_dir_stack(JSON_Object *current, const JSON_Object *admixture)
{
	/* Just leave new state as is if was changed since startup. */
	if(!dir_stack_changed())
	{
		JSON_Value *updated = json_object_get_value(admixture, "dir-stack");
		json_object_set_value(current, "dir-stack", json_value_deep_copy(updated));
	}
}

/* Merges two options' states. */
static void
merge_options(JSON_Object *current, const JSON_Object *admixture)
{
	JSON_Array *options = json_object_get_array(current, "options");
	JSON_Array *updated = json_object_get_array(admixture, "options");
	if(options == NULL)
	{
		clone_array(current, updated, "options");
	}
}

/* Merges two trash states. */
static void
merge_trash(JSON_Object *current, const JSON_Object *admixture)
{
	JSON_Array *trash = json_object_get_array(current, "trash");
	JSON_Array *updated = json_object_get_array(admixture, "trash");
	if(trash == NULL)
	{
		clone_array(current, updated, "trash");
		return;
	}

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
store_gtab(int vinfo, JSON_Object *gtab, const char name[], const
		tab_layout_t *layout, view_t *left, view_t *right)
{
	set_str(gtab, "name", name);

	JSON_Array *panes = add_array(gtab, "panes");
	store_pane(vinfo, append_object(panes), left, 0);
	store_pane(vinfo, append_object(panes), right, 1);

	if(vinfo & VINFO_TUI)
	{
		set_int(gtab, "active-pane", layout->active_pane);
		if(!cfg.pane_tabs)
		{
			set_bool(gtab, "preview", layout->preview);
		}

		JSON_Object *splitter = add_object(gtab, "splitter");
		set_int(splitter, "pos", layout->splitter_pos);
		set_double(splitter, "ratio", layout->splitter_ratio);
		set_str(splitter, "orientation", layout->split == VSPLIT ? "v" : "h");
		set_bool(splitter, "expanded", layout->only_mode);
	}
}

/* Serializes a view into JSON table. */
static void
store_pane(int vinfo, JSON_Object *pane, view_t *view, int right)
{
	JSON_Array *ptabs = add_array(pane, "ptabs");

	if(cfg.pane_tabs && (vinfo & VINFO_TABS))
	{
		int i;
		tab_info_t tab_info;
		for(i = 0; tabs_enum(view, i, &tab_info); ++i)
		{
			store_ptab(vinfo, append_object(ptabs), tab_info.name,
					tab_info.layout.preview, tab_info.view);
		}
		set_int(pane, "active-ptab", tabs_current(right ? &rwin : &lwin));
	}
	else
	{
		tab_layout_t layout;
		tabs_layout_fill(&layout);
		store_ptab(vinfo, append_object(ptabs), NULL, layout.preview, view);
	}
}

/* Serializes a pane tab into JSON table. */
static void
store_ptab(int vinfo, JSON_Object *ptab, const char name[], int preview,
		view_t *view)
{
	set_str(ptab, "name", name);

	if((vinfo & VINFO_DHISTORY) && cfg.history_len > 0)
	{
		store_dhistory(ptab, view);
	}

	if(vinfo & VINFO_STATE)
	{
		store_filters(ptab, view);
	}

	if(vinfo & VINFO_OPTIONS)
	{
		store_view_options(ptab, view);
	}

	if(vinfo & VINFO_SAVEDIRS)
	{
		set_str(ptab, "last-location", flist_get_dir(view));
	}

	if(vinfo & VINFO_TUI)
	{
		store_sort_info(ptab, view);
		set_bool(ptab, "preview", preview);
	}
}

/* Serializes filters of a view into JSON table. */
static void
store_filters(JSON_Object *view_data, const view_t *view)
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
	if(hist->size <= 0)
	{
		return;
	}

	/* Same timestamp for all entries created during this session. */
	time_t ts = time(NULL);

	int i;
	JSON_Array *entries = add_array(root, node);
	for(i = hist->size - 1; i >= 0; i--)
	{
		JSON_Object *entry = append_object(entries);
		set_str(entry, "text", hist->items[i].text);

		if(hist->items[i].timestamp == (time_t)-1)
		{
			set_double(entry, "ts", ts);
		}
		else
		{
			set_double(entry, "ts", hist->items[i].timestamp);
		}
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
	append_dstr(options, format_str("%sautocd", cfg.auto_cd ? "" : "no"));
	append_dstr(options, format_str("%schaselinks", cfg.chase_links ? "" : "no"));
	append_dstr(options, format_str("columns=%d", cfg.columns));
	append_dstr(options, format_str("cpoptions=%s",
			escape_spaces(vle_opts_get("cpoptions", OPT_GLOBAL))));
	append_dstr(options, format_str("deleteprg=%s",
				escape_spaces(cfg.delete_prg)));
	append_dstr(options, format_str("%sfastrun", cfg.fast_run ? "" : "no"));
	append_dstr(options, format_str("fillchars+=vborder:%s,hborder:%s",
				escape_spaces(cfg.vborder_filler),
				escape_spaces(cfg.hborder_filler)));
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
	append_dstr(options, format_str("previewoptions=%s",
				escape_spaces(vle_opts_get("previewoptions", OPT_GLOBAL))));
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
	append_dstr(options, format_str("tablabel=%s",
			escape_spaces(vle_opts_get("tablabel", OPT_GLOBAL))));
	append_dstr(options, format_str("tabprefix=%s",
				escape_spaces(vle_opts_get("tabprefix", OPT_GLOBAL))));
	append_dstr(options, format_str("tabscope=%s",
			escape_spaces(vle_opts_get("tabscope", OPT_GLOBAL))));
	append_dstr(options, format_str("tabstop=%d", cfg.tab_stop));
	append_dstr(options, format_str("tabsuffix=%s",
				escape_spaces(vle_opts_get("tabsuffix", OPT_GLOBAL))));
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
	append_dstr(options, format_str("sessionoptions=%s",
			escape_spaces(vle_opts_get("sessionoptions", OPT_GLOBAL))));

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
store_view_options(JSON_Object *parent, const view_t *view)
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
	const int len = marks_list_active(curr_view, marks_all, active_marks);

	int i;
	for(i = 0; i < len; ++i)
	{
		const int index = active_marks[i];
		const char m = marks_resolve_index(index);
		if(!marks_is_special(index))
		{
			const mark_t *const mark = marks_by_index(curr_view, index);

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
	if(trash_list_size > 0)
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

/* Stores history of the view into JSON representation. */
static void
store_dhistory(JSON_Object *obj, view_t *view)
{
	flist_hist_save(view);

	JSON_Array *history = add_array(obj, "history");

	/* Same timestamp for all entries created during this session. */
	time_t ts = time(NULL);

	int i;
	for(i = 0; i <= view->history_pos && i < view->history_num; ++i)
	{
		JSON_Object *entry = append_object(history);
		set_str(entry, "dir", view->history[i].dir);
		set_str(entry, "file", view->history[i].file);
		set_int(entry, "relpos", view->history[i].rel_pos);

		if(view->history[i].timestamp == (time_t)-1)
		{
			set_double(entry, "ts", ts);
		}
		else
		{
			set_double(entry, "ts", view->history[i].timestamp);
		}
	}
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

/* Puts sort description line of the view into JSON representation. */
static void
store_sort_info(JSON_Object *obj, const view_t *view)
{
	int i = 0;
	JSON_Array *sorting = add_array(obj, "sorting");
	const signed char *const sort = ui_view_sort_list_get(view, view->sort_g);
	while(i < SK_COUNT && abs(sort[i]) <= SK_LAST)
	{
		append_int(sorting, sort[i++]);
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
		return 1;
	}
	return 0;
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
		return 1;
	}
	return 0;
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
		return 1;
	}
	return 0;
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
		return 1;
	}
	return 0;
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

/* Appends an integer to an array. */
static void
append_int(JSON_Array *array, int value)
{
	json_array_append_number(array, value);
}

/* Appends value of a dynamically allocated string to an array, freeing the
 * string afterwards. */
static void
append_dstr(JSON_Array *array, char value[])
{
	json_array_append_string(array, value);
	free(value);
}

/* Clones fields of source that are missing obj into obj. */
static void
clone_missing(JSON_Object *obj, const JSON_Object *source)
{
	int i, n;
	for(i = 0, n = json_object_get_count(source); i < n; ++i)
	{
		const char *name = json_object_get_name(source, i);
		if(!json_object_has_value(obj, name))
		{
			JSON_Value *value = json_object_get_value_at(source, i);
			json_object_set_value(obj, name, json_value_deep_copy(value));
		}
	}
}

/* Clones some object (can be NULL) into specified node of another object. */
static void
clone_object(JSON_Object *parent, const JSON_Object *object, const char node[])
{
	JSON_Value *value = json_object_get_wrapping_value(object);
	json_object_set_value(parent, node, json_value_deep_copy(value));
}

/* Clones some array (can be NULL) into specified node of another object. */
static void
clone_array(JSON_Object *parent, const JSON_Array *array, const char node[])
{
	JSON_Value *value = json_array_get_wrapping_value(array);
	json_object_set_value(parent, node, json_value_deep_copy(value));
}

void
sessions_set_callback(sessions_changed callback)
{
	session_changed_cb = callback;
}

int
sessions_create(const char name[])
{
	if(sessions_current_is(name))
	{
		/* Active session might not exist on a disk yet. */
		return 1;
	}

	char sessions_dir[PATH_MAX + 16];
	get_session_dir(sessions_dir, sizeof(sessions_dir));
	char session_file[PATH_MAX + 32];
	snprintf(session_file, sizeof(session_file), "%s/%s.json", sessions_dir,
			name);
	if(path_exists(session_file, NODEREF))
	{
		return 1;
	}

	set_session(name);
	filemon_reset(&session_mon);
	return 0;
}

int
sessions_stop(void)
{
	if(!sessions_active())
	{
		return 1;
	}

	set_session(NULL);
	return 0;
}

const char *
sessions_current(void)
{
	return (cfg.session == NULL ? "" : cfg.session);
}

int
sessions_current_is(const char name[])
{
	return (sessions_active() && stroscmp(sessions_current(), name) == 0);
}

int
sessions_active(void)
{
	return (cfg.session != NULL);
}

int
sessions_exists(const char name[])
{
	char sessions_dir[PATH_MAX + 16];
	get_session_dir(sessions_dir, sizeof(sessions_dir));
	char session_file[PATH_MAX + 32];
	snprintf(session_file, sizeof(session_file), "%s/%s.json", sessions_dir,
			name);

	return (!is_dir(session_file) && path_exists(session_file, DEREF));
}

int
sessions_load(const char name[])
{
	char sessions_dir[PATH_MAX + 16];
	get_session_dir(sessions_dir, sizeof(sessions_dir));
	char session_file[PATH_MAX + 32];
	snprintf(session_file, sizeof(session_file), "%s/%s.json", sessions_dir,
			name);

	char *locale = drop_locale();
	JSON_Value *session = json_parse_file(session_file);

	if(session == NULL)
	{
		restore_locale(locale);
		state_load(1);
		set_session(NULL);
		return 1;
	}

	char info_file[PATH_MAX + 16];
	snprintf(info_file, sizeof(info_file), "%s/vifminfo.json", cfg.config_dir);
	JSON_Value *common = json_parse_file(info_file);
	restore_locale(locale);

	if(common != NULL)
	{
		merge_states(FULL_VINFO, 1, json_object(session), json_object(common));
		json_value_free(common);

		(void)filemon_from_file(info_file, FMT_MODIFIED, &vifminfo_mon);
	}

	load_state(json_object(session), 0);
	json_value_free(session);

	set_session(name);
	(void)filemon_from_file(session_file, FMT_MODIFIED, &session_mon);

	return 0;
}

/* Changes active session.  The parameter can be NULL. */
static void
set_session(const char new_session[])
{
	update_string(&cfg.session, new_session);
	if(session_changed_cb != NULL)
	{
		session_changed_cb(sessions_current());
	}
}

/* Writes session file updating it with state of the current instance if
 * necessary.  Writing is skipped if set state stored per session is empty. */
static void
write_session_file(void)
{
	if(cfg.session_options == EMPTY_VINFO)
	{
		return;
	}

	char sessions_dir[PATH_MAX + 16];
	get_session_dir(sessions_dir, sizeof(sessions_dir));
	(void)create_path(sessions_dir, S_IRWXU);

	char session_file[PATH_MAX + 32];
	snprintf(session_file, sizeof(session_file), "%s/%s.json", sessions_dir,
			cfg.session);

	store_file(session_file, &session_mon, cfg.session_options);
}

/* Writes file updating it with state of the current instance if necessary. */
static void
store_file(const char path[], filemon_t *mon, int vinfo)
{
	char tmp_file[PATH_MAX + 64];
	snprintf(tmp_file, sizeof(tmp_file), "%s_%u", path, get_pid());

	if(os_access(path, R_OK) != 0 || copy_file(path, tmp_file) == 0)
	{
		filemon_t current_mon;
		int file_changed = filemon_from_file(path, FMT_MODIFIED, &current_mon) != 0
		                || !filemon_equal(mon, &current_mon);

		update_info_file(tmp_file, vinfo, file_changed);
		(void)filemon_from_file(tmp_file, FMT_MODIFIED, mon);

		if(rename_file(tmp_file, path) != 0)
		{
			LOG_ERROR_MSG("Can't replace \"%s\" file with updated temporary", path);
			(void)remove(tmp_file);
		}
	}
}

int
sessions_remove(const char name[])
{
	char sessions_dir[PATH_MAX + 16];
	get_session_dir(sessions_dir, sizeof(sessions_dir));
	char session_file[PATH_MAX + 32];
	snprintf(session_file, sizeof(session_file), "%s/%s.json", sessions_dir,
			name);
	return (is_dir(session_file) || remove(session_file) != 0);
}

void
sessions_complete(const char prefix[])
{
	char sessions_dir[PATH_MAX + 16];
	get_session_dir(sessions_dir, sizeof(sessions_dir));

	int len = 0;
	char **list = list_regular_files(sessions_dir, NULL, &len);

	size_t prefix_len = strlen(prefix);
	int i;
	for(i = 0; i < len; ++i)
	{
		if(list[i][0] != '.' && cut_suffix(list[i], ".json"))
		{
			if(strnoscmp(list[i], prefix, prefix_len) == 0)
			{
				vle_compl_put_match(list[i], "");
				list[i] = NULL;
			}
		}
	}

	free_string_array(list, len);

	vle_compl_finish_group();
	vle_compl_add_last_match(prefix);
}

/* Fills buffer with the path at which sessions are stored. */
static void
get_session_dir(char buf[], size_t buf_size)
{
	snprintf(buf, buf_size, "%s/sessions", cfg.config_dir);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
