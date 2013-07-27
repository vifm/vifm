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

#include <ctype.h> /* isdigit() */
#include <stdio.h> /* fscanf() fgets() fputc() snprintf() */
#include <string.h> /* memset() strcpy() strtol() strcmp() strchr() strlen() */

#include "../engine/cmds.h"
#include "../utils/file_streams.h"
#include "../utils/filter.h"
#include "../utils/fs.h"
#include "../utils/fs_limits.h"
#include "../utils/log.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../utils/utils.h"
#include "../bookmarks.h"
#include "../commands.h"
#include "../dir_stack.h"
#include "../filelist.h"
#include "../filetype.h"
#include "../opt_handlers.h"
#include "../registers.h"
#include "../status.h"
#include "../trash.h"
#include "../ui.h"
#include "config.h"
#include "info_chars.h"

static void get_sort_info(FileView *view, const char line[]);
static void inc_history(char ***hist, int *num, int *len);
static void get_history(FileView *view, int reread, const char *dir,
		const char *file, int pos);
static void set_view_property(FileView *view, char type, const char value[]);
static int copy_file(const char src[], const char dst[]);
static int copy_file_internal(FILE *const src, FILE *const dst);
static void update_info_file(const char filename[]);
static char * read_vifminfo_line(FILE *fp, char buffer[]);
static void remove_leading_whitespace(char line[]);
static const char * escape_spaces(const char *str);
static void put_sort_info(FILE *fp, char leading_char, const FileView *view);
static int read_possible_possible_pos(FILE *f);
static size_t add_to_int_array(int **array, size_t len, int what);

void
read_info_file(int reread)
{
	/* TODO: refactor this function read_info_file() */

	FILE *fp;
	char info_file[PATH_MAX];
	char *line = NULL, *line2 = NULL, *line3 = NULL, *line4 = NULL;

	snprintf(info_file, sizeof(info_file), "%s/vifminfo", cfg.config_dir);

	if((fp = fopen(info_file, "r")) == NULL)
		return;

	while((line = read_vifminfo_line(fp, line)) != NULL)
	{
		const char type = line[0];
		const char *const line_val = line + 1;

		if(type == LINE_TYPE_COMMENT || type == '\0')
			continue;

		if(type == LINE_TYPE_OPTION)
		{
			if(line_val[0] == '[' || line_val[0] == ']')
			{
				FileView *v = curr_view;
				curr_view = (line_val[0] == '[') ? &lwin : &rwin;
				process_set_args(line_val + 1);
				curr_view = v;
			}
			else
			{
				process_set_args(line_val);
			}
		}
		else if(type == LINE_TYPE_FILETYPE)
		{
			if((line2 = read_vifminfo_line(fp, line2)) != NULL)
			{
				/* This is to prevent old builtin fake associations to be loaded. */
				if(!ends_with(line2, "}" VIFM_PSEUDO_CMD))
				{
					set_programs(line_val, line2, 0,
							curr_stats.env_type == ENVTYPE_EMULATOR_WITH_X);
				}
			}
		}
		else if(type == LINE_TYPE_XFILETYPE)
		{
			if((line2 = read_vifminfo_line(fp, line2)) != NULL)
			{
				set_programs(line_val, line2, 1,
						curr_stats.env_type == ENVTYPE_EMULATOR_WITH_X);
			}
		}
		else if(type == LINE_TYPE_FILEVIEWER)
		{
			if((line2 = read_vifminfo_line(fp, line2)) != NULL)
			{
				set_fileviewer(line_val, line2);
			}
		}
		else if(type == LINE_TYPE_COMMAND)
		{
			if((line2 = read_vifminfo_line(fp, line2)) != NULL)
			{
				char *cmdadd_cmd;
				if((cmdadd_cmd = format_str("command %s %s", line_val, line2)) != NULL)
				{
					exec_commands(cmdadd_cmd, curr_view, GET_COMMAND);
					free(cmdadd_cmd);
				}
			}
		}
		else if(type == LINE_TYPE_BOOKMARK)
		{
			if((line2 = read_vifminfo_line(fp, line2)) != NULL)
			{
				if((line3 = read_vifminfo_line(fp, line3)) != NULL)
				{
					add_bookmark(line_val[0], line2, line3);
				}
			}
		}
		else if(type == LINE_TYPE_ACTIVE_VIEW)
		{
			/* don't change active view on :restart command */
			if(line_val[0] == 'r' && !reread)
			{
				update_view_title(&lwin);
				update_view_title(&rwin);
				curr_view = &rwin;
				other_view = &lwin;
			}
		}
		else if(type == LINE_TYPE_QUICK_VIEW_STATE)
		{
			const int i = atoi(line_val);
			curr_stats.view = (i == 1);
		}
		else if(type == LINE_TYPE_WIN_COUNT)
		{
			const int i = atoi(line_val);
			cfg.show_one_window = (i == 1);
			curr_stats.number_of_windows = (i == 1) ? 1 : 2;
		}
		else if(type == LINE_TYPE_SPLIT_ORIENTATION)
		{
			curr_stats.split = (line_val[0] == 'v') ? VSPLIT : HSPLIT;
		}
		else if(type == LINE_TYPE_SPLIT_POSITION)
		{
			curr_stats.splitter_pos = atof(line_val);
		}
		else if(type == LINE_TYPE_LWIN_SORT)
		{
			get_sort_info(&lwin, line_val);
		}
		else if(type == LINE_TYPE_RWIN_SORT)
		{
			get_sort_info(&rwin, line_val);
		}
		else if(type == LINE_TYPE_LWIN_HIST)
		{
			int pos;

			if(line_val[0] == '\0')
			{
				if(reread)
					continue;
				if(lwin.history_num > 0)
					strcpy(lwin.curr_dir, lwin.history[lwin.history_pos].dir);
				continue;
			}

			if((line2 = read_vifminfo_line(fp, line2)) == NULL)
				continue;

			pos = read_possible_possible_pos(fp);
			get_history(&lwin, reread, line_val, line2, pos);
		}
		else if(type == LINE_TYPE_RWIN_HIST)
		{
			int pos;

			if(line_val[0] == '\0')
			{
				if(reread)
					continue;
				if(rwin.history_num > 0)
					strcpy(rwin.curr_dir, rwin.history[rwin.history_pos].dir);
				continue;
			}

			if((line2 = read_vifminfo_line(fp, line2)) == NULL)
				continue;

			pos = read_possible_possible_pos(fp);
			get_history(&rwin, reread, line_val, line2, pos);
		}
		else if(type == LINE_TYPE_CMDLINE_HIST)
		{
			inc_history(&cfg.cmd_history, &cfg.cmd_history_num, &cfg.history_len);
			save_command_history(line_val);
		}
		else if(type == LINE_TYPE_SEARCH_HIST)
		{
			inc_history(&cfg.search_history, &cfg.search_history_num,
					&cfg.history_len);
			save_search_history(line_val);
		}
		else if(type == LINE_TYPE_PROMPT_HIST)
		{
			inc_history(&cfg.prompt_history, &cfg.prompt_history_num,
					&cfg.history_len);
			save_prompt_history(line_val);
		}
		else if(type == LINE_TYPE_DIR_STACK)
		{
			if((line2 = read_vifminfo_line(fp, line2)) != NULL)
			{
				if((line3 = read_vifminfo_line(fp, line3)) != NULL)
				{
					if((line4 = read_vifminfo_line(fp, line4)) != NULL)
					{
						push_to_dirstack(line_val, line2, line3 + 1, line4);
					}
				}
			}
		}
		else if(type == LINE_TYPE_TRASH)
		{
			if((line2 = read_vifminfo_line(fp, line2)) != NULL)
			{
				if(!path_exists_at(cfg.trash_dir, line_val))
					continue;
				add_to_trash(line2, line_val);
			}
		}
		else if(type == LINE_TYPE_REG)
		{
			append_to_register(line_val[0], line_val + 1);
		}
		else if(type == LINE_TYPE_LWIN_FILT)
		{
			(void)replace_string(&lwin.prev_name_filter, line_val);
			(void)filter_set(&lwin.name_filter, line_val);
		}
		else if(type == LINE_TYPE_RWIN_FILT)
		{
			(void)replace_string(&rwin.prev_name_filter, line_val);
			(void)filter_set(&rwin.name_filter, line_val);
		}
		else if(type == LINE_TYPE_LWIN_FILT_INV)
		{
			const int i = atoi(line_val);
			lwin.invert = (i != 0);
		}
		else if(type == LINE_TYPE_RWIN_FILT_INV)
		{
			const int i = atoi(line_val);
			rwin.invert = (i != 0);
		}
		else if(type == LINE_TYPE_USE_SCREEN)
		{
			const int i = atoi(line_val);
			set_use_screen(i != 0);
		}
		else if(type == LINE_TYPE_COLORSCHEME)
		{
			strcpy(curr_stats.color_scheme, line_val);
		}
		else if(type == LINE_TYPE_LWIN_SPECIFIC || type == LINE_TYPE_RWIN_SPECIFIC)
		{
			FileView *view = (type == LINE_TYPE_LWIN_SPECIFIC) ? &lwin : &rwin;
			set_view_property(view, line_val[0], line_val + 1);
		}
	}

	free(line);
	free(line2);
	free(line3);
	free(line4);
	fclose(fp);
}

/* Parses sort description line of the view and initialized its sort field. */
static void
get_sort_info(FileView *view, const char line[])
{
	int j = 0;
	while(*line != '\0' && j < SORT_OPTION_COUNT)
	{
		char *endptr;
		const int sort_opt = strtol(line, &endptr, 10);
		if(endptr != line)
		{
			line = endptr;
			view->sort[j++] = MIN(LAST_SORT_OPTION, MAX(-LAST_SORT_OPTION, sort_opt));
		}
		else
		{
			line++;
		}
		while(*line == ',')
		{
			line++;
		}
	}
	memset(&view->sort[j], NO_SORT_OPTION, sizeof(view->sort) - j);

	reset_view_sort(view);
}

static void
inc_history(char ***hist, int *num, int *len)
{
	void *p;

	if(*num != *len)
		return;

	p = realloc(*hist, sizeof(char*)*(*len + 1));
	if(p == NULL)
		return;

	(*len)++;
	*hist = p;
}

static void
get_history(FileView *view, int reread, const char *dir, const char *file,
		int pos)
{
	if(view->history_num == cfg.history_len)
	{
		resize_history(MAX(0, cfg.history_len + 1));
	}

	if(!reread)
		view->list_rows = 1;
	save_view_history(view, dir, file, pos);
	if(!reread)
		view->list_rows = 0;
}

/* Sets view property specified by the type to the value. */
static void
set_view_property(FileView *view, char type, const char value[])
{
	if(type == PROP_TYPE_DOTFILES)
	{
		const int bool_val = atoi(value);
		view->hide_dot = bool_val;
	}
	else if(type == PROP_TYPE_AUTO_FILTER)
	{
		if(filter_set(&view->auto_filter, value) != 0)
		{
			LOG_ERROR_MSG("Error setting auto filename filter to: %s", value);
		}
	}
	else
	{
		LOG_ERROR_MSG("Unknown view property type (%c) with value: %s", type,
				value);
	}
}

void
write_info_file(void)
{
	char info_file[PATH_MAX];
	char tmp_file[PATH_MAX];

	(void)snprintf(info_file, sizeof(info_file), "%s/vifminfo", cfg.config_dir);
	(void)snprintf(tmp_file, sizeof(tmp_file), "%s_%u", info_file, get_pid());

	if(access(info_file, R_OK) != 0 || copy_file(info_file, tmp_file) == 0)
	{
		update_info_file(tmp_file);

		if(rename_file(tmp_file, info_file) != 0)
		{
			LOG_ERROR_MSG("Can't replace vifminfo file with its temporary copy");
			(void)remove(tmp_file);
		}
	}
}

/* Copies the src file to the dst location.  Returns zero on success. */
static int
copy_file(const char src[], const char dst[])
{
	FILE *const src_fp = fopen(src, "rb");
	FILE *const dst_fp = fopen(dst, "wb");
	int result;

	result = copy_file_internal(src_fp, dst_fp);

	if(dst_fp != NULL)
	{
		(void)fclose(dst_fp);
	}
	if(src_fp != NULL)
	{
		(void)fclose(src_fp);
	}

	if(result != 0)
	{
		(void)remove(dst);
	}

	return result;
}

/* Internal sub-function of the copy_file() function.  Returns zero on
 * success. */
static int
copy_file_internal(FILE *const src, FILE *const dst)
{
	char buffer[4*1024];
	size_t nread;

	if(src == NULL || dst == NULL)
	{
		return 1;
	}

	while((nread = fread(&buffer[0], 1, sizeof(buffer), src)))
	{
		if(fwrite(&buffer[0], 1, nread, dst) != nread)
		{
			break;
		}
	}

	return nread > 0;
}

/* Reads contents of the filename file as an info file and updates it with the
 * state of current instance. */
static void
update_info_file(const char filename[])
{
	/* TODO: refactor this function update_info_file() */

	FILE *fp;
	char ** list;
	int nlist = -1;
	char **ft = NULL, **fx = NULL , **fv = NULL, **cmds = NULL, **marks = NULL;
	char **lh = NULL, **rh = NULL, **cmdh = NULL, **srch = NULL, **regs = NULL;
	int *lhp = NULL, *rhp = NULL;
	size_t nlhp = 0, nrhp = 0;
	char **prompt = NULL, **trash = NULL;
	int nft = 0, nfx = 0, nfv = 0, ncmds = 0, nmarks = 0, nlh = 0, nrh = 0;
	int ncmdh = 0, nsrch = 0, nregs = 0, nprompt = 0, ntrash = 0;
	int i;

	if(cfg.vifm_info == 0)
		return;

	list = list_udf();
	while(list[++nlist] != NULL);

	if((fp = fopen(filename, "r")) != NULL)
	{
		char *line = NULL, *line2 = NULL, *line3 = NULL;
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
					assoc_record_t prog;
					if(get_default_program_for_file(line_val, &prog))
					{
						free_assoc_record(&prog);
						continue;
					}
					nft = add_to_string_array(&ft, nft, 2, line_val, line2);
				}
			}
			else if(type == LINE_TYPE_XFILETYPE)
			{
				if((line2 = read_vifminfo_line(fp, line2)) != NULL)
				{
					assoc_record_t x_prog;
					if(get_default_program_for_file(line_val, &x_prog))
					{
						assoc_record_t console_prog;
						if(get_default_program_for_file(line_val, &console_prog))
						{
							if(strcmp(x_prog.command, console_prog.command) == 0)
							{
								free_assoc_record(&console_prog);
								free_assoc_record(&x_prog);
								continue;
							}
						}
						free_assoc_record(&x_prog);
					}
					nfx = add_to_string_array(&fx, nfx, 2, line_val, line2);
				}
			}
			else if(type == LINE_TYPE_FILEVIEWER)
			{
				if((line2 = read_vifminfo_line(fp, line2)) != NULL)
				{
					if(get_viewer_for_file(line_val) != NULL)
						continue;
					nfv = add_to_string_array(&fv, nfv, 2, line_val, line2);
				}
			}
			else if(type == LINE_TYPE_COMMAND)
			{
				if(line_val[0] == '\0')
					continue;
				if((line2 = read_vifminfo_line(fp, line2)) != NULL)
				{
					const char *p = line_val;
					for(i = 0; i < nlist; i += 2)
					{
						int cmp = strcmp(list[i], p);
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
			else if(type == LINE_TYPE_LWIN_HIST)
			{
				if(line_val[0] == '\0')
					continue;
				if((line2 = read_vifminfo_line(fp, line2)) != NULL)
				{
					int pos;

					if(lwin.history_pos + nlh/2 == cfg.history_len - 1)
						continue;
					if(is_in_view_history(&lwin, line_val))
						continue;

					pos = read_possible_possible_pos(fp);
					nlh = add_to_string_array(&lh, nlh, 2, line_val, line2);
					if(nlh/2 > nlhp)
					{
						nlhp = add_to_int_array(&lhp, nlhp, pos);
						nlhp = MIN(nlh/2, nlhp);
					}
				}
			}
			else if(type == LINE_TYPE_RWIN_HIST)
			{
				if(line_val[0] == '\0')
					continue;
				if((line2 = read_vifminfo_line(fp, line2)) != NULL)
				{
					int pos;

					if(rwin.history_pos + nrh/2 == cfg.history_len - 1)
						continue;
					if(is_in_view_history(&rwin, line_val))
						continue;

					pos = read_possible_possible_pos(fp);
					nrh = add_to_string_array(&rh, nrh, 2, line_val, line2);
					if(nrh/2 > nrhp)
					{
						nrhp = add_to_int_array(&rhp, nrhp, pos);
						nrhp = MIN(nrh/2, nrhp);
					}
				}
			}
			else if(type == LINE_TYPE_BOOKMARK)
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
						const char mark_str[] = { mark, '\0' };
						if(!char_is_one_of(valid_bookmarks, mark))
							continue;
						if(!is_bookmark_empty(mark2index(mark)))
							continue;
						nmarks = add_to_string_array(&marks, nmarks, 3, mark_str, line2,
								line3);
					}
				}
			}
			else if(type == LINE_TYPE_TRASH)
			{
				if((line2 = read_vifminfo_line(fp, line2)) != NULL)
				{
					if(!path_exists_at(cfg.trash_dir, line_val))
						continue;
					if(is_in_trash(line_val))
						continue;
					ntrash = add_to_string_array(&trash, ntrash, 2, line_val, line2);
				}
			}
			else if(type == LINE_TYPE_CMDLINE_HIST)
			{
				if(cfg.cmd_history_num >= 0 && is_in_string_array(cfg.cmd_history,
						cfg.cmd_history_num + 1, line_val))
					continue;
				ncmdh = add_to_string_array(&cmdh, ncmdh, 1, line_val);
			}
			else if(type == LINE_TYPE_SEARCH_HIST)
			{
				if(cfg.search_history_num >= 0 && is_in_string_array(cfg.search_history,
						cfg.search_history_num + 1, line_val))
					continue;
				nsrch = add_to_string_array(&srch, nsrch, 1, line_val);
			}
			else if(type == LINE_TYPE_PROMPT_HIST)
			{
				if(cfg.prompt_history_num >= 0 && is_in_string_array(cfg.prompt_history,
						cfg.prompt_history_num + 1, line_val))
					continue;
				nprompt = add_to_string_array(&prompt, nprompt, 1, line_val);
			}
			else if(type == LINE_TYPE_REG)
			{
				if(register_exists(line_val[0]))
					continue;
				nregs = add_to_string_array(&regs, nregs, 1, line);
			}
		}
		free(line);
		free(line2);
		free(line3);
		fclose(fp);
	}

	if((fp = fopen(filename, "w")) == NULL)
	{
		return;
	}

	fprintf(fp, "# You can edit this file by hand, but it's recommended not to do that.\n");

	if(cfg.vifm_info & VIFMINFO_OPTIONS)
	{
		fputs("\n# Options:\n", fp);
		fprintf(fp, "=aproposprg=%s\n", escape_spaces(cfg.apropos_prg));
		fprintf(fp, "=%sautochpos\n", cfg.auto_ch_pos ? "" : "no");
		fprintf(fp, "=columns=%d\n", cfg.columns);
		fprintf(fp, "=%sconfirm\n", cfg.confirm ? "" : "no");
		fprintf(fp, "=cpoptions=%s%s%s\n",
				cfg.filter_inverted_by_default ? "f" : "",
				cfg.selection_is_primary ? "s" : "",
				cfg.tab_switches_pane ? "t" : "");
		fprintf(fp, "=%sfastrun\n", cfg.fast_run ? "" : "no");
		fprintf(fp, "=findprg=%s\n", escape_spaces(cfg.find_prg));
		fprintf(fp, "=%sfollowlinks\n", cfg.follow_links ? "" : "no");
		fprintf(fp, "=fusehome=%s\n", escape_spaces(cfg.fuse_home));
		fprintf(fp, "=%sgdefault\n", cfg.gdefault ? "" : "no");
		fprintf(fp, "=grepprg=%s\n", escape_spaces(cfg.grep_prg));
		fprintf(fp, "=history=%d\n", cfg.history_len);
		fprintf(fp, "=%shlsearch\n", cfg.hl_search ? "" : "no");
		fprintf(fp, "=%siec\n", cfg.use_iec_prefixes ? "" : "no");
		fprintf(fp, "=%signorecase\n", cfg.ignore_case ? "" : "no");
		fprintf(fp, "=%sincsearch\n", cfg.inc_search ? "" : "no");
		fprintf(fp, "=%slaststatus\n", cfg.last_status ? "" : "no");
		fprintf(fp, "=lines=%d\n", cfg.lines);
		fprintf(fp, "=locateprg=%s\n", escape_spaces(cfg.locate_prg));
		fprintf(fp, "=rulerformat=%s\n", escape_spaces(cfg.ruler_format));
		fprintf(fp, "=%srunexec\n", cfg.auto_execute ? "" : "no");
		fprintf(fp, "=%sscrollbind\n", cfg.scroll_bind ? "" : "no");
		fprintf(fp, "=scrolloff=%d\n", cfg.scroll_off);
		fprintf(fp, "=shell=%s\n", escape_spaces(cfg.shell));
		fprintf(fp, "=shortmess=%s\n", cfg.trunc_normal_sb_msgs ? "T" : "");
#ifndef _WIN32
		fprintf(fp, "=slowfs=%s\n", escape_spaces(cfg.slow_fs_list));
#endif
		fprintf(fp, "=%ssmartcase\n", cfg.smart_case ? "" : "no");
		fprintf(fp, "=%ssortnumbers\n", cfg.sort_numbers ? "" : "no");
		fprintf(fp, "=statusline=%s\n", escape_spaces(cfg.status_line));
		fprintf(fp, "=tabstop=%d\n", cfg.tab_stop);
		fprintf(fp, "=timefmt=%s\n", escape_spaces(cfg.time_format + 1));
		fprintf(fp, "=timeoutlen=%d\n", cfg.timeout_len);
		fprintf(fp, "=%strash\n", cfg.use_trash ? "" : "no");
		fprintf(fp, "=undolevels=%d\n", cfg.undo_levels);
		fprintf(fp, "=vicmd=%s%s\n", escape_spaces(cfg.vi_command),
				cfg.vi_cmd_bg ? " &" : "");
		fprintf(fp, "=vixcmd=%s%s\n", escape_spaces(cfg.vi_x_command),
				cfg.vi_cmd_bg ? " &" : "");
		fprintf(fp, "=%swrapscan\n", cfg.wrap_scan ? "" : "no");
		fprintf(fp, "=[viewcolumns=%s\n", escape_spaces(lwin.view_columns));
		fprintf(fp, "=]viewcolumns=%s\n", escape_spaces(rwin.view_columns));
		fprintf(fp, "=[%slsview\n", lwin.ls_view ? "" : "no");
		fprintf(fp, "=]%slsview\n", rwin.ls_view ? "" : "no");

		fprintf(fp, "%s", "=dotdirs=");
		if(cfg.dot_dirs & DD_ROOT_PARENT)
			fprintf(fp, "%s", "rootparent,");
		if(cfg.dot_dirs & DD_NONROOT_PARENT)
			fprintf(fp, "%s", "nonrootparent,");
		fprintf(fp, "\n");

		fprintf(fp, "=classify=%s\n", escape_spaces(classify_to_str()));

		fprintf(fp, "=vifminfo=options");
		if(cfg.vifm_info & VIFMINFO_FILETYPES)
			fprintf(fp, ",filetypes");
		if(cfg.vifm_info & VIFMINFO_COMMANDS)
			fprintf(fp, ",commands");
		if(cfg.vifm_info & VIFMINFO_BOOKMARKS)
			fprintf(fp, ",bookmarks");
		if(cfg.vifm_info & VIFMINFO_TUI)
			fprintf(fp, ",tui");
		if(cfg.vifm_info & VIFMINFO_DHISTORY)
			fprintf(fp, ",dhistory");
		if(cfg.vifm_info & VIFMINFO_STATE)
			fprintf(fp, ",state");
		if(cfg.vifm_info & VIFMINFO_CS)
			fprintf(fp, ",cs");
		if(cfg.vifm_info & VIFMINFO_SAVEDIRS)
			fprintf(fp, ",savedirs");
		if(cfg.vifm_info & VIFMINFO_CHISTORY)
			fprintf(fp, ",chistory");
		if(cfg.vifm_info & VIFMINFO_SHISTORY)
			fprintf(fp, ",shistory");
		if(cfg.vifm_info & VIFMINFO_PHISTORY)
			fprintf(fp, ",phistory");
		if(cfg.vifm_info & VIFMINFO_DIRSTACK)
			fprintf(fp, ",dirstack");
		if(cfg.vifm_info & VIFMINFO_REGISTERS)
			fprintf(fp, ",registers");
		fprintf(fp, "\n");

		fprintf(fp, "=%svimhelp\n", cfg.use_vim_help ? "" : "no");
		fprintf(fp, "=%swildmenu\n", cfg.wild_menu ? "" : "no");
		fprintf(fp, "=%swrap\n", cfg.wrap_quick_view ? "" : "no");
	}

	if(cfg.vifm_info & VIFMINFO_FILETYPES)
	{
		fputs("\n# Filetypes:\n", fp);
		for(i = 0; i < filetypes.count; i++)
		{
			int j;
			assoc_t ft_assoc = filetypes.list[i];
			for(j = 0; j < ft_assoc.records.count; j++)
			{
				assoc_record_t ft_record = ft_assoc.records.list[j];
				/* The type check is to prevent builtin fake associations to be written
				 * into vifminfo file */
				if(ft_record.command[0] != '\0' && ft_record.type != ART_BUILTIN)
				{
					fprintf(fp, ".%s\n\t{%s}%s\n", ft_assoc.pattern,
							ft_record.description, ft_record.command);
				}
			}
		}
		for(i = 0; i < nft; i += 2)
			fprintf(fp, ".%s\n\t%s\n", ft[i], ft[i + 1]);

		fputs("\n# X Filetypes:\n", fp);
		for(i = 0; i < xfiletypes.count; i++)
		{
			int j;
			assoc_t xft_assoc = xfiletypes.list[i];
			for(j = 0; j < xft_assoc.records.count; j++)
			{
				assoc_record_t xft_record = xft_assoc.records.list[j];
				if(xft_record.command[0] != '\0')
				{
					fprintf(fp, "x%s\n\t{%s}%s\n", xft_assoc.pattern,
							xft_record.description, xft_record.command);
				}
			}
		}
		for(i = 0; i < nfx; i += 2)
			fprintf(fp, ".%s\n\t%s\n", fx[i], fx[i + 1]);

		fputs("\n# Fileviewers:\n", fp);
		for(i = 0; i < fileviewers.count; i++)
		{
			int j;
			assoc_t fv_assoc = fileviewers.list[i];
			for(j = 0; j < fv_assoc.records.count; j++)
			{
				assoc_record_t fv_record = fileviewers.list[i].records.list[j];
				if(fv_record.command[0] != '\0')
				{
					fprintf(fp, ",%s\n\t%s\n", fv_assoc.pattern, fv_record.command);
				}
			}
		}
		for(i = 0; i < nfv; i += 2)
			fprintf(fp, ",%s\n\t%s\n", fv[i], fv[i + 1]);
	}

	if(cfg.vifm_info & VIFMINFO_COMMANDS)
	{
		fputs("\n# Commands:\n", fp);
		for(i = 0; list[i] != NULL; i += 2)
			fprintf(fp, "!%s\n\t%s\n", list[i], list[i + 1]);
		for(i = 0; i < ncmds; i += 2)
			fprintf(fp, "!%s\n\t%s\n", cmds[i], cmds[i + 1]);
	}

	if(cfg.vifm_info & VIFMINFO_BOOKMARKS)
	{
		int len = init_active_bookmarks(valid_bookmarks);

		fputs("\n# Bookmarks:\n", fp);
		for(i = 0; i < len; i++)
		{
			int j = active_bookmarks[i];
			if(is_spec_bookmark(j))
				continue;
			fprintf(fp, "'%c\n\t%s\n\t", index2mark(j), bookmarks[j].directory);
			fprintf(fp, "%s\n", bookmarks[j].file);
		}
		for(i = 0; i < nmarks; i += 3)
			fprintf(fp, "'%c\n\t%s\n\t%s\n", marks[i][0], marks[i + 1], marks[i + 2]);
	}

	if(cfg.vifm_info & VIFMINFO_TUI)
	{
		fputs("\n# TUI:\n", fp);
		fprintf(fp, "a%c\n", (curr_view == &rwin) ? 'r' : 'l');
		fprintf(fp, "q%d\n", curr_stats.view);
		fprintf(fp, "v%d\n", curr_stats.number_of_windows);
		fprintf(fp, "o%c\n", (curr_stats.split == VSPLIT) ? 'v' : 'h');
		fprintf(fp, "m%d\n", curr_stats.splitter_pos);

		put_sort_info(fp, 'l', &lwin);
		put_sort_info(fp, 'r', &rwin);
	}

	if((cfg.vifm_info & VIFMINFO_DHISTORY) && cfg.history_len > 0)
	{
		save_view_history(&lwin, NULL, NULL, -1);
		fputs("\n# Left window history (oldest to newest):\n", fp);
		for(i = 0; i < nlh; i += 2)
			fprintf(fp, "d%s\n\t%s\n%d\n", lh[i], lh[i + 1], lhp[i/2]);
		for(i = 0; i <= lwin.history_pos; i++)
			fprintf(fp, "d%s\n\t%s\n%d\n", lwin.history[i].dir, lwin.history[i].file,
					lwin.history[i].rel_pos);
		if(cfg.vifm_info & VIFMINFO_SAVEDIRS)
			fprintf(fp, "d\n");

		save_view_history(&rwin, NULL, NULL, -1);
		fputs("\n# Right window history (oldest to newest):\n", fp);
		for(i = 0; i < nrh; i += 2)
			fprintf(fp, "D%s\n\t%s\n%d\n", rh[i], rh[i + 1], rhp[i/2]);
		for(i = 0; i <= rwin.history_pos; i++)
			fprintf(fp, "D%s\n\t%s\n%d\n", rwin.history[i].dir, rwin.history[i].file,
					rwin.history[i].rel_pos);
		if(cfg.vifm_info & VIFMINFO_SAVEDIRS)
			fprintf(fp, "D\n");
	}

	if(cfg.vifm_info & VIFMINFO_CHISTORY)
	{
		fputs("\n# Command line history (oldest to newest):\n", fp);
		for(i = 0; i < MIN(ncmdh, cfg.history_len - cfg.cmd_history_num); i++)
			fprintf(fp, ":%s\n", cmdh[i]);
		for(i = cfg.cmd_history_num; i >= 0; i--)
			fprintf(fp, ":%s\n", cfg.cmd_history[i]);
	}

	if(cfg.vifm_info & VIFMINFO_SHISTORY)
	{
		fputs("\n# Search history (oldest to newest):\n", fp);
		for(i = 0; i < nsrch; i++)
			fprintf(fp, "/%s\n", srch[i]);
		for(i = cfg.search_history_num; i >= 0; i--)
			fprintf(fp, "/%s\n", cfg.search_history[i]);
	}

	if(cfg.vifm_info & VIFMINFO_PHISTORY)
	{
		fputs("\n# Prompt history (oldest to newest):\n", fp);
		for(i = 0; i < nprompt; i++)
			fprintf(fp, "p%s\n", prompt[i]);
		for(i = cfg.prompt_history_num; i >= 0; i--)
			fprintf(fp, "p%s\n", cfg.prompt_history[i]);
	}

	if(cfg.vifm_info & VIFMINFO_REGISTERS)
	{
		fputs("\n# Registers:\n", fp);
		for(i = 0; i < nregs; i++)
			fprintf(fp, "%s\n", regs[i]);
		for(i = 0; valid_registers[i] != '\0'; i++)
		{
			int j;
			registers_t *reg = find_register(valid_registers[i]);
			if(reg == NULL)
				continue;
			for(j = 0; j < reg->num_files; j++)
			{
				if(reg->files[j] == NULL)
					continue;
				fprintf(fp, "\"%c%s\n", reg->name, reg->files[j]);
			}
		}
	}

	if(cfg.vifm_info & VIFMINFO_DIRSTACK)
	{
		fputs("\n# Directory stack (oldest to newest):\n", fp);
		for(i = 0; i < stack_top; i++)
		{
			fprintf(fp, "S%s\n\t%s\n", stack[i].lpane_dir, stack[i].lpane_file);
			fprintf(fp, "S%s\n\t%s\n", stack[i].rpane_dir, stack[i].rpane_file);
		}
	}

	fputs("\n# Trash content:\n", fp);
	for(i = 0; i < nentries; i++)
		fprintf(fp, "t%s\n\t%s\n", trash_list[i].trash_name, trash_list[i].path);
	for(i = 0; i < ntrash; i += 2)
		fprintf(fp, "t%s\n\t%s\n", trash[i], trash[i + 1]);

	if(cfg.vifm_info & VIFMINFO_STATE)
	{
		fputs("\n# State:\n", fp);
		fprintf(fp, "f%s\n", lwin.name_filter.raw);
		fprintf(fp, "i%d\n", lwin.invert);
		fprintf(fp, "[.%d\n", lwin.hide_dot);
		fprintf(fp, "[F%s\n", lwin.auto_filter.raw);
		fprintf(fp, "F%s\n", rwin.name_filter.raw);
		fprintf(fp, "I%d\n", rwin.invert);
		fprintf(fp, "].%d\n", rwin.hide_dot);
		fprintf(fp, "]F%s\n", rwin.auto_filter.raw);
		fprintf(fp, "s%d\n", cfg.use_screen);
	}

	if(cfg.vifm_info & VIFMINFO_CS)
	{
		fputs("\n# Color scheme:\n", fp);
		fprintf(fp, "c%s\n", cfg.cs.name);
	}

	fclose(fp);

	free_string_array(ft, nft);
	free_string_array(fv, nfv);
	free_string_array(fx, nfx);
	free_string_array(cmds, ncmds);
	free_string_array(marks, nmarks);
	free_string_array(list, nlist);
	free_string_array(lh, nlh);
	free_string_array(rh, nrh);
	free(lhp);
	free(rhp);
	free_string_array(cmdh, ncmdh);
	free_string_array(srch, nsrch);
	free_string_array(regs, nregs);
	free_string_array(prompt, nprompt);
	free_string_array(trash, ntrash);
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

/* Returns pointer to a statically allocated buffer */
static const char *
escape_spaces(const char *str)
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
put_sort_info(FILE *fp, char leading_char, const FileView *view)
{
	int i = -1;
	fputc(leading_char, fp);
	while(++i < SORT_OPTION_COUNT && abs(view->sort[i]) <= LAST_SORT_OPTION)
	{
		int last_option = i >= SORT_OPTION_COUNT - 1;
		last_option = last_option || abs(view->sort[i + 1]) > LAST_SORT_OPTION;
		fprintf(fp, "%d%s", view->sort[i], last_option ? "" : ",");
	}
	fputc('\n', fp);
}

static int
read_possible_possible_pos(FILE *f)
{
	char c;
	int result;

	c = getc(f);
	ungetc(c, f);
	if(!isdigit(c) || fscanf(f, "%d\n", &result) != 1)
	{
		result = -1;
	}

	return result;
}

static size_t
add_to_int_array(int **array, size_t len, int what)
{
	int *p;

	p = realloc(*array, sizeof(int)*(len + 1));
	if(p != NULL)
	{
		*array = p;
		(*array)[len++] = what;
	}

	return len;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
