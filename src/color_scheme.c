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

#include <sys/stat.h>

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

#define MAX_LEN 1024

Col_scheme *col_schemes;

static const int default_colors[][2] = {
	{7, 0}, /* MENU_COLOR */
	{0, 7}, /* BORDER_COLOR */
	{7, 0}, /* WIN_COLOR */
	{7, 0}, /* STATUS_BAR_COLOR */
	{7, 4}, /* CURR_LINE_COLOR */
	{6, 0}, /* DIRECTORY_COLOR */
	{3, 0}, /* LINK_COLOR */
	{5, 0}, /* SOCKET_COLOR */
	{1, 0}, /* DEVICE_COLOR */
	{2, 0}, /* EXECUTABLE_COLOR */
	{5, 0}, /* SELECTED_COLOR */
	{4, 0}, /* CURRENT_COLOR */
};

static int _gnuc_unused default_colors_size_guard[
	(ARRAY_LEN(default_colors) == MAXNUM_COLOR) ? 1 : -1
];

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
	char config_file[PATH_MAX];
	int x, y;
	char buf[128];
	char fg_buf[64];
	char bg_buf[64];

	snprintf(config_file, sizeof(config_file), "%s/colorschemes", cfg.config_dir);

	if((fp = fopen(config_file, "w")) == NULL)
		return;

	fprintf(fp, "# You can edit this file by hand.\n");
	fprintf(fp, "# The # character at the beginning of a line comments out the line.\n");
	fprintf(fp, "# Blank lines are ignored.\n\n");

	fprintf(fp, "# The Default color scheme is used for any directory that does not have\n");
	fprintf(fp, "# a specified scheme.	A color scheme set for a base directory will also\n");
	fprintf(fp, "# be used for the sub directories.\n\n");

	fprintf(fp, "# The standard ncurses colors are: \n");
	fprintf(fp, "# Default = -1 can be used for transparency\n");
	fprintf(fp, "# Black = 0\n");
	fprintf(fp, "# Red = 1\n");
	fprintf(fp, "# Green = 2\n");
	fprintf(fp, "# Yellow = 3\n");
	fprintf(fp, "# Blue = 4\n");
	fprintf(fp, "# Magenta = 5\n");
	fprintf(fp, "# Cyan = 6\n");
	fprintf(fp, "# White = 7\n\n");

	fprintf(fp, "# Vifm supports 256 colors you can use color numbers 0-255\n");
	fprintf(fp, "# (requires properly set up terminal: set your TERM environment variable to\n");
	fprintf(fp, "# some color terminal name (e.g. xterm-256color) from /usr/lib/terminfo/;\n");
	fprintf(fp, "# you can check current number of colors in your terminal with tput colors\n");
	fprintf(fp, "# command)\n\n");

	fprintf(fp, "# COLORSCHEME=OneWordDescription\n");
	fprintf(fp, "# DIRECTORY=/Full/Path/To/Base/Directory\n");
	fprintf(fp, "# COLOR=Window_name=foreground_color_number=background_color_number\n\n");

	for(x = 0; x < cfg.color_scheme_num; x++)
	{
		fprintf(fp, "\nCOLORSCHEME=%s\n", col_schemes[x].name);
		fprintf(fp, "DIRECTORY=%s\n", col_schemes[x].dir);

		for(y = 0; y < MAXNUM_COLOR; y++)
		{
			static const char *ELEM_NAMES[] = {
				"MENU",
				"BORDER",
				"WIN",
				"STATUS_BAR",
				"CURR_LINE",
				"DIRECTORY",
				"LINK",
				"SOCKET",
				"DEVICE",
				"EXECUTABLE",
				"SELECTED",
				"CURRENT",
			};
			static const char *COLOR_STR[] = {
				"default",
				"black",
				"red",
				"green",
				"yellow",
				"blue",
				"magenta",
				"cyan",
				"white",
			};
			int t;

			snprintf(buf, sizeof(buf), "%s", ELEM_NAMES[y]);

			t = col_schemes[x].color[y].fg + 1;
			if((size_t)t < sizeof(COLOR_STR)/sizeof(COLOR_STR[0]))
				snprintf(fg_buf, sizeof(fg_buf), "%s", COLOR_STR[t]);
			else
				snprintf(fg_buf, sizeof(fg_buf), "%d", t + 1);

			t = col_schemes[x].color[y].bg + 1;
			if((size_t)t < sizeof(COLOR_STR)/sizeof(COLOR_STR[0]))
				snprintf(bg_buf, sizeof(bg_buf), "%s", COLOR_STR[t]);
			else
				snprintf(bg_buf, sizeof(bg_buf), "%d", t + 1);

			fprintf(fp, "COLOR=%s=%s=%s\n", buf, fg_buf, bg_buf);
		}
	}

	fclose(fp);
	return;
}

static void
init_color_scheme(int num, Col_scheme *cs)
{
	int i;
	strcpy(cs->dir, "");

	for(i = 0; i < MAXNUM_COLOR; i++)
	{
		cs->color[i].fg = default_colors[i][0];
		cs->color[i].bg = default_colors[i][1];
	}
}

static void
load_default_colors()
{
	init_color_scheme(0, &col_schemes[0]);

	snprintf(col_schemes[0].name, NAME_MAX, "Default");
	snprintf(col_schemes[0].dir, PATH_MAX, "/");
}

/*
 * convert possible <color_name> to <int>
 */
static int
colname2int(char col[])
{
	/* test if col[] is a number... */
	if(isdigit(col[0]))
		return atoi(col);

	/* otherwise convert */
	if(!strcmp(col, "black"))
		return 0;
	if(!strcmp(col, "red"))
		return 1;
	if(!strcmp(col, "green"))
		return 2;
	if(!strcmp(col, "yellow"))
		return 3;
	if(!strcmp(col, "blue"))
		return 4;
	if(!strcmp(col, "magenta"))
		return 5;
	if(!strcmp(col, "cyan"))
		return 6;
	if(!strcmp(col, "white"))
		return 7;
	/* return default color */
	return -1;
}

static void
add_color(char s1[], char s2[], char s3[])
{
	int fg, bg;
	int scheme;
	const int x = cfg.color_scheme_num - 1;
	int y;

	fg = colname2int(s2);
	bg = colname2int(s3);

	scheme = x * MAXNUM_COLOR;

	y = -1;
	if(!strcmp(s1, "MENU"))
		y = MENU_COLOR;
	else if(!strcmp(s1, "BORDER"))
		y = BORDER_COLOR;
	else if(!strcmp(s1, "WIN"))
		y = WIN_COLOR;
	else if(!strcmp(s1, "STATUS_BAR"))
		y = STATUS_BAR_COLOR;
	else if(!strcmp(s1, "CURR_LINE"))
		y = CURR_LINE_COLOR;
	else if(!strcmp(s1, "DIRECTORY"))
		y = DIRECTORY_COLOR;
	else if(!strcmp(s1, "LINK"))
		y = LINK_COLOR;
	else if(!strcmp(s1, "SOCKET"))
		y = SOCKET_COLOR;
	else if(!strcmp(s1, "DEVICE"))
		y = DEVICE_COLOR;
	else if(!strcmp(s1, "EXECUTABLE"))
		y = EXECUTABLE_COLOR;
	else if(!strcmp(s1, "SELECTED"))
		y = SELECTED_COLOR;
	else if(!strcmp(s1, "CURRENT"))
		y = CURRENT_COLOR;
	else
		return;

	col_schemes[x].color[y].fg = fg;
	col_schemes[x].color[y].bg = bg;
}

void
read_color_scheme_file(void)
{
	FILE *fp;
	char config_file[PATH_MAX];
	char line[MAX_LEN];
	char *s1 = NULL;
	char *s2 = NULL;
	char *s3 = NULL;
	char *sx = NULL;
	int args;

	snprintf(config_file, sizeof(config_file), "%s/colorschemes", cfg.config_dir);

	if((fp = fopen(config_file, "r")) == NULL)
	{
		load_default_colors();

		cfg.color_scheme_num++;
		write_color_scheme_file();
		return;
	}

	while(fgets(line, MAX_LEN, fp))
	{
		args = 0;

		if(line[0] == '#')
			continue;

		if((sx = s1 = strchr(line, '=')) != NULL)
		{
			s1++;
			chomp(s1);
			*sx = '\0';
			args = 1;
		}
		else
			continue;
		if((sx = s2 = strchr(s1, '=')) != NULL)
		{
			s2++;
			chomp(s2);
			*sx = '\0';
			args = 2;
		}
		if((args == 2) && ((sx = s3 = strchr(s2, '=')) != NULL))
		{
			s3++;
			chomp(s3);
			*sx = '\0';
			args = 3;
		}

		if(args == 1)
		{
			if(!strcmp(line, "COLORSCHEME"))
			{
				cfg.color_scheme_num++;

				if(cfg.color_scheme_num > 8)
					break;

				init_color_scheme(cfg.color_scheme_num - 1,
						&col_schemes[cfg.color_scheme_num - 1]);

				snprintf(col_schemes[cfg.color_scheme_num - 1].name, NAME_MAX, "%s",
						s1);

				continue;
			}
			if(!strcmp(line, "DIRECTORY"))
			{
				Col_scheme* cs;

				cs = col_schemes + cfg.color_scheme_num - 1;
				snprintf(cs->dir, PATH_MAX, "%s", s1);
				if(strcmp(cs->dir, "/") != 0)
					chosp(cs->dir);
				continue;
			}
		}
		if(!strcmp(line, "COLOR") && args == 3)
			add_color(s1, s2, s3);
	}

	fclose(fp);
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
check_directory_for_color_scheme(const char *dir)
{
	int i;
	int max_len = 0;
	int max_index = -1;

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

	if(max_index == -1)
		return cfg.color_scheme_cur*MAXNUM_COLOR;

	return max_index*MAXNUM_COLOR;
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
