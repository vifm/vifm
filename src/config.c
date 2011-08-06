/* vifm
 * Copyright (C) 2001 Ken Steen.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include "../config.h"

#define CP_HELP "cp " PACKAGE_DATA_DIR "/vifm-help.txt ~/.vifm"
#define CP_RC "cp " PACKAGE_DATA_DIR "/vifmrc ~/.vifm"

#include <ctype.h> /* isalnum */
#include <stdio.h> /* FILE */
#include <stdlib.h> /* getenv */
#include <string.h>
#include <sys/stat.h> /* mkdir */
#include <unistd.h> /* chdir */

#include "bookmarks.h"
#include "color_scheme.h"
#include "commands.h"
#include "cmds.h"
#include "config.h"
#include "fileops.h"
#include "filelist.h"
#include "filetype.h"
#include "menus.h"
#include "opt_handlers.h"
#include "registers.h"
#include "status.h"
#include "utils.h"

#define MAX_LEN 1024

Config cfg;

void
init_config(void)
{
	char *p;

	cfg.num_bookmarks = 0;
	cfg.command_num = 0;
	cfg.filetypes_num = 0;
	cfg.xfiletypes_num = 0;
	cfg.fileviewers_num = 0;
	cfg.nmapped_num = 0;
	cfg.vim_filter = 0;
	cfg.show_one_window = 0;

	cfg.search_history_len = 15;
	cfg.search_history_num = -1;
	cfg.search_history = (char **)calloc(cfg.search_history_len, sizeof(char*));
	cfg.cmd_history_len = 15;
	cfg.cmd_history_num = -1;
	cfg.cmd_history = (char **)calloc(cfg.cmd_history_len, sizeof(char *));
	cfg.auto_execute = 0;
	cfg.time_format = strdup(" %m/%d %H:%M");
	cfg.wrap_quick_view = 1;
	cfg.color_scheme_num = 0;
	cfg.color_scheme_cur = 0;
	cfg.color_scheme = 1;
	cfg.use_iec_prefixes = 0;
	cfg.undo_levels = 100;
	cfg.sort_numbers = 0;
	cfg.follow_links = 1;
	cfg.fast_run = 0;
	cfg.confirm = 1;
	cfg.vi_command = strdup("vim");
	cfg.vi_x_command = strdup("");
	cfg.use_trash = 1;
	cfg.fuse_home = strdup("/tmp/vifm_FUSE");
	cfg.use_screen = 0;
	cfg.history_len = 15;
	cfg.use_vim_help = 0;
	cfg.invert_cur_line = 0;
	cfg.wild_menu = 0;
	cfg.ignore_case = 0;
	cfg.smart_case = 0;
	cfg.hl_search = 1;
	cfg.vifm_info = VIFMINFO_BOOKMARKS;

	p = getenv("SHELL");
	if(p == NULL || *p == '\0')
		cfg.shell = strdup("sh");
	else
		cfg.shell = strdup(p);

	/* Maximum argument length to pass to the shell */
	if((cfg.max_args = sysconf(_SC_ARG_MAX)) == 0)
		cfg.max_args = 4096; /* POSIX MINIMUM */
}

static void
create_help_file(void)
{
	char command[PATH_MAX];

	snprintf(command, sizeof(command), CP_HELP);
	fprintf(stderr, "%s", command);
	file_exec(command);
}

static void
create_rc_file(void)
{
	char command[PATH_MAX];

	snprintf(command, sizeof(command), CP_RC);
	file_exec(command);
	add_bookmark('H', cfg.home_dir, "../");
	add_bookmark('z', cfg.config_dir, "../");
}

void
set_config_dir(void)
{
	char *home_dir;
	FILE *f;
	char help_file[PATH_MAX];
	char rc_file[PATH_MAX];
	char *escaped;

	home_dir = getenv("HOME");
	if(home_dir == NULL)
		return;

	snprintf(rc_file, sizeof(rc_file), "%s/.vifm/vifmrc", home_dir);
	snprintf(help_file, sizeof(help_file), "%s/.vifm/vifm-help_txt", home_dir);
	snprintf(cfg.home_dir, sizeof(cfg.home_dir), "%s/", home_dir);
	snprintf(cfg.config_dir, sizeof(cfg.config_dir), "%s/.vifm", home_dir);
	snprintf(cfg.trash_dir, sizeof(cfg.trash_dir), "%s/.vifm/Trash", home_dir);
	snprintf(cfg.log_file, sizeof(cfg.log_file), "%s/.vifm/log", home_dir);

	escaped = escape_filename(cfg.trash_dir, 0, 0);
	strcpy(cfg.escaped_trash_dir, escaped);
	free(escaped);

	if(chdir(cfg.config_dir))
	{
		if(mkdir(cfg.config_dir, 0777))
			return;
		if(mkdir(cfg.trash_dir, 0777))
			return;
		if((f = fopen(help_file, "r")) == NULL)
			create_help_file();
		if((f = fopen(rc_file, "r")) == NULL)
			create_rc_file();
	}
}

static void
load_view_defaults(FileView *view)
{
	strncpy(view->regexp, "\\..~$", sizeof(view->regexp) - 1);

	view->filename_filter = strdup("");
	view->prev_filter = strdup("");
	view->invert = TRUE;

	view->sort_type = SORT_BY_NAME;
}

void
load_default_configuration(void)
{
	read_color_scheme_file();

	load_view_defaults(&lwin);
	load_view_defaults(&rwin);
}

const char *
conv_udf_name(const char *cmd)
{
	static const char *nums[] = {
		"ZERO", "ONE", "TWO", "THREE", "FOUR",
		"FIVE", "SIX", "SEVEN", "EIGHT", "NINE"
	};
	static char buf[256];
	char *p = buf;
	while(*cmd != '\0')
		if(isdigit(*cmd))
		{
			if(p + strlen(nums[*cmd - '0']) + 1 >= buf + sizeof(buf))
				break;
			strcpy(p, nums[*cmd - '0']);
			p += strlen(p);
			cmd++;
		}
		else
		{
			*p++ = *cmd++;
		}
	*p = '\0';
	return buf;
}

static void
prepare_line(char *line)
{
	int i;

	chomp(line);
	i = 0;
	while(isspace(line[i]))
		i++;
	if(i > 0)
		memmove(line, line + i, strlen(line + i) + 1);
}

void
read_info_file(void)
{
	FILE *fp;
	char info_file[PATH_MAX];
	char line[MAX_LEN], line2[MAX_LEN], line3[MAX_LEN];

	snprintf(info_file, sizeof(info_file), "%s/vifminfo", cfg.config_dir);

	if((fp = fopen(info_file, "r")) == NULL)
		return;

	while(fgets(line, sizeof(line), fp) == line)
	{
		prepare_line(line);
		if(line[0] == '#' || line[0] == '\0')
			continue;

		if(line[0] == '=') /* option */
		{
			process_set_args(line + 1);
		}
		else if(line[0] == '.') /* filetype */
		{
			if(fgets(line2, sizeof(line2), fp) == line2)
			{
				prepare_line(line2);
				set_programs(line + 1, line2, 0);
			}
		}
		else if(line[0] == 'x') /* xfiletype */
		{
			if(fgets(line2, sizeof(line2), fp) == line2)
			{
				prepare_line(line2);
				set_programs(line + 1, line2, 1);
			}
		}
		else if(line[0] == ',') /* fileviewer */
		{
			if(fgets(line2, sizeof(line2), fp) == line2)
			{
				prepare_line(line2);
				set_fileviewer(line + 1, line2);
			}
		}
		else if(line[0] == '!') /* command */
		{
			if(fgets(line2, sizeof(line2), fp) == line2)
			{
				char buf[MAX_LEN*2];
				prepare_line(line2);
				snprintf(buf, sizeof(buf), "command %s %s", line + 1, line2);
				exec_commands(buf, curr_view, 0);
			}
		}
		else if(line[0] == '\'') /* bookmark */
		{
			if(fgets(line2, sizeof(line2), fp) == line2)
			{
				prepare_line(line2);
				if(fgets(line3, sizeof(line3), fp) == line3)
				{
					prepare_line(line3);
					add_bookmark(line[1], line2, line3);
				}
			}
		}
		else if(line[0] == 'v') /* number of windows */
		{
			int i = atoi(line + 1);
			cfg.show_one_window = (i == 1);
			curr_stats.number_of_windows = (i == 1) ? 1 : 2;
		}
		else if(line[0] == 'l') /* left pane sort */
		{
			int i = atoi(line + 1);
			lwin.sort_descending = (i < 0);
			lwin.sort_type = abs(i) - 1;
		}
		else if(line[0] == 'r') /* right pane sort */
		{
			int i = atoi(line + 1);
			rwin.sort_descending = (i < 0);
			rwin.sort_type = abs(i) - 1;
		}
		else if(line[0] == 'd') /* left pane history */
		{
			if(line[1] == '\0')
			{
				strcpy(lwin.curr_dir, lwin.history[lwin.history_pos].dir);
				continue;
			}
			if(fgets(line2, sizeof(line2), fp) != line2)
				continue;
			prepare_line(line2);

			if(lwin.history_num == cfg.history_len - 1)
			{
				cfg.history_len++;
				lwin.history = realloc(lwin.history, sizeof(history_t)*cfg.history_len);
				rwin.history = realloc(rwin.history, sizeof(history_t)*cfg.history_len);
			}

			lwin.list_rows = 1;
			save_view_history(&lwin, line + 1, line2);
			lwin.list_rows = 0;
		}
		else if(line[0] == 'D') /* right pane history */
		{
			if(line[1] == '\0')
			{
				strcpy(rwin.curr_dir, rwin.history[rwin.history_pos].dir);
				continue;
			}
			if(fgets(line2, sizeof(line2), fp) != line2)
				continue;
			prepare_line(line2);

			if(lwin.history_num == cfg.history_len - 1)
			{
				cfg.history_len++;
				lwin.history = realloc(lwin.history, sizeof(history_t)*cfg.history_len);
				rwin.history = realloc(rwin.history, sizeof(history_t)*cfg.history_len);
			}

			rwin.list_rows = 1;
			save_view_history(&rwin, line + 1, line2);
			rwin.list_rows = 0;
		}
		else if(line[0] == 'f') /* left pane filter */
		{
			lwin.filename_filter = (char *)realloc(lwin.filename_filter,
					strlen(line + 1) + 1);
			strcpy(lwin.filename_filter, line + 1);
			lwin.prev_filter = (char *)realloc(lwin.prev_filter,
					strlen(line + 1) + 1);
			strcpy(lwin.prev_filter, line + 1);
		}
		else if(line[0] == 'F') /* right pane filter */
		{
			rwin.filename_filter = (char *)realloc(rwin.filename_filter,
					strlen(line + 1) + 1);
			strcpy(rwin.filename_filter, line + 1);
			rwin.prev_filter = (char *)realloc(rwin.prev_filter,
					strlen(line + 1) + 1);
			strcpy(rwin.prev_filter, line + 1);
		}
		else if(line[0] == 'i') /* left pane filter inverted */
		{
			int i = atoi(line + 1);
			lwin.invert = (i != 0);
		}
		else if(line[0] == 'I') /* right pane filter inverted */
		{
			int i = atoi(line + 1);
			rwin.invert = (i != 0);
		}
		else if(line[0] == 's') /* use screen program */
		{
			int i = atoi(line + 1);
			cfg.use_screen = (i != 0);
		}
		else if(line[0] == 'c') /* default color scheme */
		{
			cfg.color_scheme_cur = find_color_scheme(line + 1);
			if(cfg.color_scheme_cur < 0)
				cfg.color_scheme_cur = 0;
			cfg.color_scheme = 1 + MAXNUM_COLOR*cfg.color_scheme_cur;
		}
	}

	fclose(fp);
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

void
write_info_file(void)
{
	FILE *fp;
	char info_file[PATH_MAX];
	char ** list;
	int nlist = -1;
	char **ft = NULL, **fx = NULL , **fv = NULL, **cmds = NULL, **marks = NULL;
	char **lh = NULL, **rh = NULL;
	int nft = 0, nfx = 0, nfv = 0, ncmds = 0, nmarks = 0, nlh = 0, nrh = 0;
	int i;
	int is_console;

	if(cfg.vifm_info == 0)
		return;

	snprintf(info_file, sizeof(info_file), "%s/vifminfo", cfg.config_dir);

	list = list_udf();
	while(list[++nlist] != NULL);

	if((fp = fopen(info_file, "r")) != NULL)
	{
		char line[MAX_LEN], line2[MAX_LEN], line3[MAX_LEN];

		is_console = curr_stats.is_console;
		while(fgets(line, sizeof(line), fp) == line)
		{
			prepare_line(line);
			if(line[0] == '#' || line[0] == '\0')
				continue;

			if(line[0] == '.') /* filetype */
			{
				if(fgets(line2, sizeof(line2), fp) == line2)
				{
					char *p;
					curr_stats.is_console = 1;
					if((p = get_default_program_for_file(line + 1)) != NULL)
					{
						free(p);
						continue;
					}
					prepare_line(line2);
					nft = add_to_string_array(&ft, nft, 2, line + 1, line2);
				}
			}
			else if(line[0] == 'x') /* xfiletype */
			{
				if(fgets(line2, sizeof(line2), fp) == line2)
				{
					char *p;
					curr_stats.is_console = 0;
					if((p = get_default_program_for_file(line + 1)) != NULL)
					{
						char *t;
						curr_stats.is_console = 1;
						t = get_default_program_for_file(line + 1);
						if(strcmp(p, t) == 0)
						{
							free(t);
							free(p);
							continue;
						}
						free(t);
						free(p);
					}
					prepare_line(line2);
					nfx = add_to_string_array(&fx, nfx, 2, line + 1, line2);
				}
			}
			else if(line[0] == ',') /* fileviewer */
			{
				if(fgets(line2, sizeof(line2), fp) == line2)
				{
					if(get_viewer_for_file(line + 1) != NULL)
						continue;
					prepare_line(line2);
					nfv = add_to_string_array(&fv, nfv, 2, line + 1, line2);
				}
			}
			else if(line[0] == '!') /* command */
			{
				if(line[1] == '\0')
					continue;
				if(fgets(line2, sizeof(line2), fp) == line2)
				{
					char *p = line + 1;
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
					prepare_line(line2);
					ncmds = add_to_string_array(&cmds, ncmds, 2, line + 1, line2);
				}
			}
			else if(line[0] == 'd') /* left view directory history */
			{
				if(line[1] == '\0')
					continue;
				if(fgets(line2, sizeof(line2), fp) == line2)
				{
					if(lwin.history_pos + nlh/2 == cfg.history_len - 1)
						continue;
					if(is_in_view_history(&lwin, line + 1))
						continue;
					prepare_line(line2);
					nlh = add_to_string_array(&lh, nlh, 2, line + 1, line2);
				}
			}
			else if(line[0] == 'D') /* right view directory history */
			{
				if(fgets(line2, sizeof(line2), fp) == line2)
				{
					if(rwin.history_pos + nrh/2 == cfg.history_len - 1)
						continue;
					if(is_in_view_history(&rwin, line + 1))
						continue;
					prepare_line(line2);
					nrh = add_to_string_array(&rh, nrh, 2, line + 1, line2);
				}
			}
			else if(line[0] == '\'') /* bookmark */
			{
				line[2] = '\0';
				if(fgets(line2, sizeof(line2), fp) == line2)
				{
					prepare_line(line2);
					if(fgets(line3, sizeof(line3), fp) == line3)
					{
						if(is_bookmark(mark2index(line[1])))
							continue;
						prepare_line(line3);
						nmarks = add_to_string_array(&marks, nmarks, 2, line + 1, line2,
								line3);
					}
				}
			}
			curr_stats.is_console = is_console;
		}
		fclose(fp);
	}

	if((fp = fopen(info_file, "w")) == NULL)
		return;

	fprintf(fp, "# You can edit this file by hand, but it's recommended not to do that.\n");

	if(cfg.vifm_info & VIFMINFO_OPTIONS)
	{
		fputs("\n# Options:\n", fp);
		fprintf(fp, "=%sconfirm\n", cfg.confirm ? "" : "no");
		fprintf(fp, "=%sfastrun\n", cfg.fast_run ? "" : "no");
		fprintf(fp, "=%sfollowlinks\n", cfg.follow_links ? "" : "no");
		fprintf(fp, "=fusehome=%s\n", escape_spaces(cfg.fuse_home));
		fprintf(fp, "=history=%d\n", cfg.history_len);
		fprintf(fp, "=%shlsearch\n", cfg.hl_search ? "" : "no");
		fprintf(fp, "=%siec\n", cfg.use_iec_prefixes ? "" : "no");
		fprintf(fp, "=%signorecase\n", cfg.ignore_case ? "" : "no");
		fprintf(fp, "=%sreversecol\n", cfg.invert_cur_line ? "" : "no");
		fprintf(fp, "=%srunexec\n", cfg.auto_execute ? "" : "no");
		fprintf(fp, "=shell=%s\n", escape_spaces(cfg.shell));
		fprintf(fp, "=%ssmartcase\n", cfg.smart_case ? "" : "no");
		fprintf(fp, "=%ssortnumbers\n", cfg.sort_numbers ? "" : "no");
		fprintf(fp, "=timefmt=%s\n", escape_spaces(cfg.time_format + 1));
		fprintf(fp, "=%strash\n", cfg.use_trash ? "" : "no");
		fprintf(fp, "=undolevels=%d\n", cfg.undo_levels);
		fprintf(fp, "=vicmd=%s\n", escape_spaces(cfg.vi_command));
		fprintf(fp, "=vixcmd=%s\n", escape_spaces(cfg.vi_x_command));

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
		fprintf(fp, "\n");

		fprintf(fp, "=%svimhelp\n", cfg.use_vim_help ? "" : "no");
		fprintf(fp, "=%swildmenu\n", cfg.wild_menu ? "" : "no");
		fprintf(fp, "=%swrap\n", cfg.wrap_quick_view ? "" : "no");
	}

	if(cfg.vifm_info & VIFMINFO_FILETYPES)
	{
		fputs("\n# Filetypes:\n", fp);
		for(i = 0; i < cfg.filetypes_num; i++)
		{
			if(filetypes[i].com[0] != '\0')
				fprintf(fp, ".%s\n\t%s\n", filetypes[i].ext, filetypes[i].com);
		}
		for(i = 0; i < nft; i += 2)
			fprintf(fp, ".%s\n\t%s\n", ft[i], ft[i + 1]);

		fputs("\n# X Filetypes:\n", fp);
		for(i = 0; i < cfg.xfiletypes_num; i++)
		{
			if(xfiletypes[i].com[0] != '\0')
				fprintf(fp, ".%s\n\t%s\n", xfiletypes[i].ext, xfiletypes[i].com);
		}
		for(i = 0; i < nfx; i += 2)
			fprintf(fp, ".%s\n\t%s\n", fx[i], fx[i + 1]);

		fputs("\n# Fileviewers:\n", fp);
		for(i = 0; i < cfg.fileviewers_num; i++)
		{
			if(fileviewers[i].com[0] != '\0')
				fprintf(fp, ",%s\n\t%s\n", fileviewers[i].ext, fileviewers[i].com);
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
			fprintf(fp, "'%c\n\t%s\n\t", index2mark(j),
					escape_spaces(bookmarks[j].directory));
			fprintf(fp, "%s\n", escape_spaces(bookmarks[j].file));
		}
		for(i = 0; i < nmarks; i += 3)
			fprintf(fp, "'%c\n\t%s\n\t%s\n", marks[i][0], marks[i + 1], marks[i + 2]);
	}

	if(cfg.vifm_info & VIFMINFO_TUI)
	{
		fputs("\n# TUI:\n", fp);
		fprintf(fp, "v%d\n", curr_stats.number_of_windows);
		fprintf(fp, "l%s%d\n", lwin.sort_descending ? "-" : "", lwin.sort_type + 1);
		fprintf(fp, "r%s%d\n", rwin.sort_descending ? "-" : "", rwin.sort_type + 1);
	}

	if((cfg.vifm_info & VIFMINFO_DHISTORY) && cfg.history_len > 0)
	{
		save_view_history(&lwin, NULL, NULL);
		fputs("\n# Left window history (oldest to newest):\n", fp);
		for(i = 0; i < nlh; i += 2)
			fprintf(fp, "d%s\n\t%s\n", lh[i], lh[i + 1]);
		for(i = 0; i <= lwin.history_pos; i++)
			fprintf(fp, "d%s\n\t%s\n", lwin.history[i].dir, lwin.history[i].file);
		if(cfg.vifm_info & VIFMINFO_SAVEDIRS)
			fprintf(fp, "d\n");

		save_view_history(&rwin, NULL, NULL);
		fputs("\n# Right window history (oldest to newest):\n", fp);
		for(i = 0; i < nrh; i += 2)
			fprintf(fp, "D%s\n\t%s\n", rh[i], rh[i + 1]);
		for(i = 0; i <= rwin.history_pos; i++)
			fprintf(fp, "D%s\n\t%s\n", rwin.history[i].dir, rwin.history[i].file);
		if(cfg.vifm_info & VIFMINFO_SAVEDIRS)
			fprintf(fp, "D\n");
	}

	if(cfg.vifm_info & VIFMINFO_STATE)
	{
		fputs("\n# State:\n", fp);
		fprintf(fp, "f%s\n", lwin.filename_filter);
		fprintf(fp, "i%d\n", lwin.invert);
		fprintf(fp, "F%s\n", rwin.filename_filter);
		fprintf(fp, "I%d\n", rwin.invert);
		fprintf(fp, "s%d\n", cfg.use_screen);
	}

	if(cfg.vifm_info & VIFMINFO_CS)
	{
		fputs("\n# Color scheme:\n", fp);
		fprintf(fp, "c%s\n", col_schemes[cfg.color_scheme_cur].name);
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
}

void
exec_config(void)
{
	FILE *fp;
	char config_file[PATH_MAX];
	char line[MAX_LEN*2];
	char next_line[MAX_LEN];

	snprintf(config_file, sizeof(config_file), "%s/vifmrc", cfg.config_dir);

	if((fp = fopen(config_file, "r")) == NULL)
		return;

	if(fgets(line, MAX_LEN, fp) != NULL)
	{
		for(;;)
		{
			char *p;

			if((p = fgets(next_line, sizeof(next_line), fp)) != NULL)
			{
				do
				{
					while(isspace(*p))
						p++;
					chomp(p);
					if(*p == '"')
						continue;
					else if(*p == '\\')
						strncat(line, p + 1, sizeof(line));
					else
						break;
				}
				while((p = fgets(next_line, sizeof(next_line), fp)) != NULL);
			}
			chomp(line);
			exec_commands(line, curr_view, 0);
			if(p == NULL)
				break;
			strcpy(line, p);
		}
	}

	fclose(fp);
}

int
is_old_config(void)
{
	char config_file[PATH_MAX];
	FILE *fp;
	char line[MAX_LEN];

	snprintf(config_file, sizeof(config_file), "%s/vifmrc", cfg.config_dir);

	if((fp = fopen(config_file, "r")) == NULL)
		return 0;

	while(fgets(line, sizeof(line), fp))
	{
		int i = 0;
		while(isspace(line[i]))
			i++;
		if(line[i] == '#')
			return 1;
	}

	fclose(fp);

	return 0;
}

const char *
get_vicmd(void)
{
	if(curr_stats.is_console)
		return cfg.vi_command;
	else if(cfg.vi_x_command[0] != '\0')
		return cfg.vi_x_command;
	else
		return cfg.vi_command;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
