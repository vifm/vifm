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
#include "config.h"
#include "menus.h"
#include "status.h"
#include "utils.h"

#define MAX_LEN 1024

Col_scheme *col_schemes;

static void
verify_color_schemes(void)
{
	/* TODO implement */
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

void
load_color_scheme(const char *name)
{
	int i;

	i = find_color_scheme(name);
	if(i < 0)
	{
		show_error_msg("Color Scheme", "Invalid color scheme name");
		return;
	}

	load_color_scheme_id(i);
}

void
load_color_scheme_id(int id)
{
	int x;
	for(x = 0; x < MAXNUM_COLOR; x++)
		init_pair(col_schemes[id].color[x].name - id*MAXNUM_COLOR,
				col_schemes[id].color[x].fg, col_schemes[id].color[x].bg);

	cfg.color_scheme_cur = id;
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
	fprintf(fp, "#	(requires properly set up terminal).\n\n");

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

			col_schemes[x].color[y].name %= MAXNUM_COLOR;
			snprintf(buf, sizeof(buf), ELEM_NAMES[col_schemes[x].color[y].name]);

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
load_default_colors()
{
	snprintf(col_schemes[0].name, NAME_MAX, "Default");
	snprintf(col_schemes[0].dir, PATH_MAX, "/");

	col_schemes[0].color[0].name = MENU_COLOR;
	col_schemes[0].color[0].fg = 7;
	col_schemes[0].color[0].bg = 0;

	col_schemes[0].color[1].name = BORDER_COLOR;
	col_schemes[0].color[1].fg = 0;
	col_schemes[0].color[1].bg = 7;

	col_schemes[0].color[2].name = WIN_COLOR;
	col_schemes[0].color[2].fg = 7;
	col_schemes[0].color[2].bg = 0;

	col_schemes[0].color[3].name = STATUS_BAR_COLOR;
	col_schemes[0].color[3].fg = 7;
	col_schemes[0].color[3].bg = 0;

	col_schemes[0].color[4].name = CURR_LINE_COLOR;
	col_schemes[0].color[4].fg = 7;
	col_schemes[0].color[4].bg = 4;

	col_schemes[0].color[5].name = DIRECTORY_COLOR;
	col_schemes[0].color[5].fg = 6;
	col_schemes[0].color[5].bg = 0;

	col_schemes[0].color[6].name = LINK_COLOR;
	col_schemes[0].color[6].fg = 3;
	col_schemes[0].color[6].bg = 0;

	col_schemes[0].color[7].name = SOCKET_COLOR;
	col_schemes[0].color[7].fg = 5;
	col_schemes[0].color[7].bg = 0;

	col_schemes[0].color[8].name = DEVICE_COLOR;
	col_schemes[0].color[8].fg = 1;
	col_schemes[0].color[8].bg = 0;

	col_schemes[0].color[9].name = EXECUTABLE_COLOR;
	col_schemes[0].color[9].fg = 2;
	col_schemes[0].color[9].bg = 0;

	col_schemes[0].color[10].name = SELECTED_COLOR;
	col_schemes[0].color[10].fg = 5;
	col_schemes[0].color[10].bg = 0;

	col_schemes[0].color[11].name = CURRENT_COLOR;
	col_schemes[0].color[11].fg = 4;
	col_schemes[0].color[11].bg = 0;
}

/*
 * convert possible <color_name> to <int>
 */
static int
colname2int(char col[])
{
 /* test if col[] is a number... */
	 if (isdigit(col[0]))
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

/*
 * add color
 */
void
add_color(char s1[], char s2[], char s3[])
{
	int fg, bg;
	int scheme;
	const int x = cfg.color_scheme_num -1;
	int y = cfg.color_pairs_num;

	fg = colname2int(s2);
	bg = colname2int(s3);

	y %= MAXNUM_COLOR;

	scheme = x * MAXNUM_COLOR;

	if(!strcmp(s1, "MENU"))
		col_schemes[x].color[y].name = scheme + MENU_COLOR;

	if(!strcmp(s1, "BORDER"))
		col_schemes[x].color[y].name = scheme + BORDER_COLOR;

	if(!strcmp(s1, "WIN"))
		col_schemes[x].color[y].name = scheme + WIN_COLOR;

	if(!strcmp(s1, "STATUS_BAR"))
		col_schemes[x].color[y].name = scheme + STATUS_BAR_COLOR;

	if(!strcmp(s1, "CURR_LINE"))
		col_schemes[x].color[y].name = scheme + CURR_LINE_COLOR;

	if(!strcmp(s1, "DIRECTORY"))
		col_schemes[x].color[y].name = scheme + DIRECTORY_COLOR;

	if(!strcmp(s1, "LINK"))
		col_schemes[x].color[y].name = scheme + LINK_COLOR;

	if(!strcmp(s1, "SOCKET"))
		col_schemes[x].color[y].name = scheme + SOCKET_COLOR;

	if(!strcmp(s1, "DEVICE"))
		col_schemes[x].color[y].name = scheme + DEVICE_COLOR;

	if(!strcmp(s1, "EXECUTABLE"))
		col_schemes[x].color[y].name = scheme + EXECUTABLE_COLOR;

	if(!strcmp(s1, "SELECTED"))
		col_schemes[x].color[y].name = scheme + SELECTED_COLOR;

	if(!strcmp(s1, "CURRENT"))
		col_schemes[x].color[y].name = scheme + CURRENT_COLOR;

	col_schemes[x].color[y].fg = fg;
	col_schemes[x].color[y].bg = bg;

	cfg.color_pairs_num++;
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
		cfg.color_pairs_num = MAXNUM_COLOR;
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
				/* TODO
				 check if last colorscheme is complete and pad it before starting
				 a new scheme
				 verify_scheme();

					col_schemes = (Col_scheme *)realloc(col_schemes,
							sizeof(Col_scheme *) +1);
				*/

				snprintf(col_schemes[cfg.color_scheme_num].name, NAME_MAX, "%s", s1);

				cfg.color_scheme_num++;

				if (cfg.color_scheme_num > 8)
					break;

				continue;
			}
			if(!strcmp(line, "DIRECTORY"))
			{
				size_t len;
				Col_scheme* cs = col_schemes + (cfg.color_scheme_num - 1);

				snprintf(cs->dir, PATH_MAX, "%s", s1);
				len = strlen(cs->dir);
				if(cs->dir[len - 1] == '/')
					cs->dir[len - 1] = '\0';
				continue;
			}
		}
		if(!strcmp(line, "COLOR") && args == 3)
		{
			add_color(s1, s2, s3);
		}
	}

	fclose(fp);

	// TODO verify_color_schemes();
	return;
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
	int i, y;
	int max_len = 0;
	int max_index = 0;

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

	return (max_index * MAXNUM_COLOR);
}

char *
complete_colorschemes(const char *name)
{
	static char *buf;
	static int last;

	size_t len;

	if(name != NULL)
	{
		free(buf);
		buf = strdup(name);
		last = -1;
	}

	len = strlen(buf);

	if(last == cfg.color_scheme_num)
		last = -1;

	while(++last < cfg.color_scheme_num)
	{
		if(strncmp(buf, col_schemes[last].name, len) == 0)
			return strdup(col_schemes[last].name);
	}
	return strdup(buf);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
