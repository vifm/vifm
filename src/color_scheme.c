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

#include <curses.h>

#include <dirent.h> /* DIR */
#include <sys/stat.h>
#include <unistd.h>

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "color_scheme.h"
#include "completion.h"
#include "config.h"
#include "macros.h"
#include "menus.h"
#include "status.h"
#include "utils.h"

Col_scheme *col_schemes;

char *HI_GROUPS[] = {
	"Menu",
	"Border",
	"Win",
	"StatusBar",
	"CurrLine",
	"Directory",
	"Link",
	"Socket",
	"Device",
	"Executable",
	"Selected",
	"BrokenLink",
	"TopLine",
	"StatusLine",
	"Fifo",
	"ErrorMsg",
};

static int _gnuc_unused HI_GROUPS_size_guard[
	(ARRAY_LEN(HI_GROUPS) + 2 == MAXNUM_COLOR) ? 1 : -1
];

char *COLOR_NAMES[8] = {
	"black",
	"red",
	"green",
	"yellow",
	"blue",
	"magenta",
	"cyan",
	"white",
};

int COLOR_VALS[8] = {
	COLOR_BLACK,
	COLOR_RED,
	COLOR_GREEN,
	COLOR_YELLOW,
	COLOR_BLUE,
	COLOR_MAGENTA,
	COLOR_CYAN,
	COLOR_WHITE,
};

static const int default_colors[][2] = {
	{ COLOR_WHITE,   COLOR_BLACK }, /* MENU_COLOR */
	{ COLOR_BLACK,   COLOR_WHITE }, /* BORDER_COLOR */
	{ COLOR_WHITE,   COLOR_BLACK }, /* WIN_COLOR */
	{ COLOR_WHITE,   COLOR_BLACK }, /* STATUS_BAR_COLOR */
	{ COLOR_WHITE,   COLOR_BLUE  }, /* CURR_LINE_COLOR */
	{ COLOR_CYAN,    COLOR_BLACK }, /* DIRECTORY_COLOR */
	{ COLOR_YELLOW,  COLOR_BLACK }, /* LINK_COLOR */
	{ COLOR_MAGENTA, COLOR_BLACK }, /* SOCKET_COLOR */
	{ COLOR_RED,     COLOR_BLACK }, /* DEVICE_COLOR */
	{ COLOR_GREEN,   COLOR_BLACK }, /* EXECUTABLE_COLOR */
	{ COLOR_MAGENTA, COLOR_BLACK }, /* SELECTED_COLOR */
	{ COLOR_RED,     COLOR_BLACK }, /* BROKEN_LINK_COLOR */
	{ COLOR_BLACK,   COLOR_WHITE }, /* TOP_LINE_COLOR */
	{ COLOR_BLACK,   COLOR_WHITE }, /* STATUS_LINE_COLOR */
	{ COLOR_CYAN,    COLOR_BLACK }, /* FIFO_COLOR */
	{ COLOR_RED,     COLOR_BLACK }, /* ERROR_MSG_COLOR */
};

static int _gnuc_unused default_colors_size_guard[
	(ARRAY_LEN(default_colors) + 2 == MAXNUM_COLOR) ? 1 : -1
];

static void
init_color_scheme(Col_scheme *cs)
{
	int i;
	strcpy(cs->dir, "/");
	cs->defaulted = 0;

	for(i = 0; i < MAXNUM_COLOR; i++)
	{
		cs->color[i].fg = default_colors[i][0];
		cs->color[i].bg = default_colors[i][1];
	}
}

static void
check_color_scheme(Col_scheme *cs)
{
	int need_correction = 0;
	int i;
	for(i = 0; i < ARRAY_LEN(cs->color) - 2; i++)
	{
		if(cs->color[i].bg >= COLORS || cs->color[i].fg >= COLORS)
		{
			need_correction = 1;
			break;
		}
	}

	if(!need_correction)
		return;

	cs->defaulted = 1;
	for(i = 0; i < ARRAY_LEN(cs->color); i++)
	{
		cs->color[i].fg = default_colors[i][0];
		cs->color[i].bg = default_colors[i][1];
	}
}

void
check_color_schemes(void)
{
	int i;

	cfg.color_scheme_num = cfg.color_scheme_num;

	for(i = 0; i < cfg.color_scheme_num; i++)
		check_color_scheme(col_schemes + i);
}

int
add_color_scheme(const char *name, const char *directory)
{
	void *p = realloc(col_schemes, sizeof(Col_scheme)*(cfg.color_scheme_num + 1));
	if(p == NULL)
	{
		(void)show_error_msg("Memory Error", "Unable to allocate enough memory");
		return 1;
	}

	cfg.color_scheme_num++;
	init_color_scheme(&col_schemes[cfg.color_scheme_num - 1]);
	snprintf(col_schemes[cfg.color_scheme_num - 1].name, NAME_MAX, "%s", name);
	if(directory != NULL)
		snprintf(col_schemes[cfg.color_scheme_num - 1].dir, PATH_MAX, "%s",
				directory);
	load_color_schemes();
	return 0;
}

int
find_color_scheme(const char *name)
{
	int i;
	for(i = 0; i < cfg.color_scheme_num; i++)
	{
		if(strcmp(col_schemes[i].name, name) == 0)
			return i;
	}
	return -1;
}

/* This function is called only when colorschemes file doesn't exist */
static void
write_color_scheme_file(void)
{
	FILE *fp;
	char colors_dir[PATH_MAX];
	int x, y;

	snprintf(colors_dir, sizeof(colors_dir), "%s/colors", cfg.config_dir);
	if(mkdir(colors_dir, 0777) != 0)
		return;

	strcat(colors_dir, "/Default");
	if((fp = fopen(colors_dir, "w")) == NULL)
		return;

	fprintf(fp, "\" You can edit this file by hand.\n");
	fprintf(fp, "\" The \" character at the beginning of a line comments out the line.\n");
	fprintf(fp, "\" Blank lines are ignored.\n\n");

	fprintf(fp, "\" The Default color scheme is used for any directory that does not have\n");
	fprintf(fp, "\" a specified scheme and for parts of user interface like menus. A\n");
	fprintf(fp, "\" color scheme set for a base directory will also\n");
	fprintf(fp, "\" be used for the sub directories.\n\n");

	fprintf(fp, "\" The standard ncurses colors are: \n");
	fprintf(fp, "\" Default = -1 can be used for transparency\n");
	fprintf(fp, "\" Black = 0\n");
	fprintf(fp, "\" Red = 1\n");
	fprintf(fp, "\" Green = 2\n");
	fprintf(fp, "\" Yellow = 3\n");
	fprintf(fp, "\" Blue = 4\n");
	fprintf(fp, "\" Magenta = 5\n");
	fprintf(fp, "\" Cyan = 6\n");
	fprintf(fp, "\" White = 7\n\n");

	fprintf(fp, "\" Vifm supports 256 colors you can use color numbers 0-255\n");
	fprintf(fp, "\" (requires properly set up terminal: set your TERM environment variable\n");
	fprintf(fp, "\" (directly or using resources) to some color terminal name (e.g.\n");
	fprintf(fp, "\" xterm-256color) from /usr/lib/terminfo/; you can check current number\n");
	fprintf(fp, "\" of colors in your terminal with tput colors command)\n\n");

	fprintf(fp, "\" colorscheme! OneWordDescription /Full/Path/To/Base/Directory\n");
	fprintf(fp, "\" highlight group ctermfg=foreground_color_ ctermbg=background_color\n\n");

	x = 0;
	fprintf(fp, "\ncolorscheme! '%s' '%s'\n", col_schemes[x].name,
			col_schemes[x].dir);

	for(y = 0; y < MAXNUM_COLOR - 2; y++)
	{
		char fg_buf[16], bg_buf[16];
		int fg = col_schemes[x].color[y].fg;
		int bg = col_schemes[x].color[y].bg;

		if(fg == -1)
			strcpy(fg_buf, "default");
		else if(fg < ARRAY_LEN(COLOR_NAMES))
			strcpy(fg_buf, COLOR_NAMES[fg]);
		else
			snprintf(fg_buf, sizeof(fg_buf), "%d", fg);

		if(bg == -1)
			strcpy(bg_buf, "default");
		else if(bg < ARRAY_LEN(COLOR_NAMES))
			strcpy(bg_buf, COLOR_NAMES[bg]);
		else
			snprintf(bg_buf, sizeof(bg_buf), "%d", bg);

		fprintf(fp, "highlight %s ctermfg=%s ctermbg=%s\n", HI_GROUPS[y], fg_buf,
				bg_buf);
	}

	fclose(fp);
}

static void
load_default_colors(void)
{
	init_color_scheme(&col_schemes[0]);

	snprintf(col_schemes[0].name, NAME_MAX, "Default");
	snprintf(col_schemes[0].dir, PATH_MAX, "/");
}

void
read_color_schemes(void)
{
	char colors_dir[PATH_MAX];
	DIR *dir;
	struct dirent *d;

	snprintf(colors_dir, sizeof(colors_dir), "%s/colors", cfg.config_dir);

	dir = opendir(colors_dir);
	if(dir == NULL)
	{
		load_default_colors();
		cfg.color_scheme_num = 1;

		write_color_scheme_file();
		return;
	}

	while((d = readdir(dir)) != NULL)
	{
		char full[PATH_MAX];

		if(d->d_type != DT_REG && d->d_type != DT_LNK)
			continue;

		if(d->d_name[0] == '.')
			continue;

		snprintf(full, sizeof(full), "%s/%s", colors_dir, d->d_name);
		source_file(full);
	}
	closedir(dir);
}

static void
load_color_pairs(int base, const Col_scheme *cs)
{
	int i;
	for(i = 0; i < MAXNUM_COLOR; i++)
	{
		if(i == MENU_COLOR)
			init_pair(base + i, cs->color[i].bg, cs->color[i].fg);
		else
			init_pair(base + i, cs->color[i].fg, cs->color[i].bg);
	}
}

void
load_color_schemes(void)
{
	load_color_pairs(DCOLOR_BASE, col_schemes + cfg.color_scheme_cur);
}

/* The return value is the color scheme base number for the colorpairs.
 * There are 12 color pairs for each color scheme.
 *
 * Default returns 0;
 * Second color scheme returns 12
 * Third color scheme returns 24
 *
 * The color scheme with the longest matching directory path is the one that
 * should be returned.
 */
int
check_directory_for_color_scheme(int left, const char *dir)
{
	int i;
	int max_len = 0;
	int max_index = -1;
	int base = left ? LCOLOR_BASE : RCOLOR_BASE;

	for(i = 0; i < cfg.color_scheme_num; i++)
	{
		size_t len = strlen(col_schemes[i].dir);

		if(path_starts_with(dir, col_schemes[i].dir))
		{
			if(len > max_len)
			{
				max_len = len;
				max_index = i;
			}
		}
	}

	if(path_starts_with(dir, col_schemes[cfg.color_scheme_cur].dir) &&
			max_len == strlen(col_schemes[cfg.color_scheme_cur].dir))
		max_index = cfg.color_scheme_cur;

	if(max_index == -1)
		max_index = cfg.color_scheme_cur;

	load_color_pairs(base, col_schemes + max_index);
	return base;
}

void
complete_colorschemes(const char *name)
{
	size_t len;
	int i;

	len = strlen(name);

	for(i = 0; i < cfg.color_scheme_num; i++)
	{
		if(strncmp(name, col_schemes[i].name, len) == 0)
			add_completion(col_schemes[i].name);
	}
	completion_group_end();
	add_completion(name);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
