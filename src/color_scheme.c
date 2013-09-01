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

#include "color_scheme.h"

#include <curses.h>

#include <sys/stat.h>
#include <dirent.h> /* DIR */
#include <unistd.h>

#include <ctype.h>
#include <stddef.h> /* size_t */
#include <stdio.h> /* snprintf() */
#include <string.h> /* strcpy() strlen() */

#include "cfg/config.h"
#include "engine/completion.h"
#include "menus/menus.h"
#include "utils/fs.h"
#include "utils/fs_limits.h"
#include "utils/macros.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/tree.h"
#include "filelist.h"
#include "status.h"

char *HI_GROUPS[] = {
	[WIN_COLOR]          = "Win",
	[DIRECTORY_COLOR]    = "Directory",
	[LINK_COLOR]         = "Link",
	[BROKEN_LINK_COLOR]  = "BrokenLink",
	[SOCKET_COLOR]       = "Socket",
	[DEVICE_COLOR]       = "Device",
	[FIFO_COLOR]         = "Fifo",
	[EXECUTABLE_COLOR]   = "Executable",
	[SELECTED_COLOR]     = "Selected",
	[CURR_LINE_COLOR]    = "CurrLine",
	[TOP_LINE_COLOR]     = "TopLine",
	[TOP_LINE_SEL_COLOR] = "TopLineSel",
	[STATUS_LINE_COLOR]  = "StatusLine",
	[MENU_COLOR]         = "WildMenu",
	[CMD_LINE_COLOR]     = "CmdLine",
	[ERROR_MSG_COLOR]    = "ErrorMsg",
	[BORDER_COLOR]       = "Border",
};
ARRAY_GUARD(HI_GROUPS, MAXNUM_COLOR - 2);

char *COLOR_NAMES[8] = {
	[COLOR_BLACK]   = "black",
	[COLOR_RED]     = "red",
	[COLOR_GREEN]   = "green",
	[COLOR_YELLOW]  = "yellow",
	[COLOR_BLUE]    = "blue",
	[COLOR_MAGENTA] = "magenta",
	[COLOR_CYAN]    = "cyan",
	[COLOR_WHITE]   = "white",
};

char *LIGHT_COLOR_NAMES[8] = {
	[COLOR_BLACK]   = "lightblack",
	[COLOR_RED]     = "lightred",
	[COLOR_GREEN]   = "lightgreen",
	[COLOR_YELLOW]  = "lightyellow",
	[COLOR_BLUE]    = "lightblue",
	[COLOR_MAGENTA] = "lightmagenta",
	[COLOR_CYAN]    = "lightcyan",
	[COLOR_WHITE]   = "lightwhite",
};

static const int default_colors[][3] = {
	                      /* fg             bg           attr */
	[WIN_COLOR]          = { COLOR_WHITE,   COLOR_BLACK, 0                       },
	[DIRECTORY_COLOR]    = { COLOR_CYAN,    -1,          A_BOLD                  },
	[LINK_COLOR]         = { COLOR_YELLOW,  -1,          A_BOLD                  },
	[BROKEN_LINK_COLOR]  = { COLOR_RED,     -1,          A_BOLD                  },
	[SOCKET_COLOR]       = { COLOR_MAGENTA, -1,          A_BOLD                  },
	[DEVICE_COLOR]       = { COLOR_RED,     -1,          A_BOLD                  },
	[FIFO_COLOR]         = { COLOR_CYAN,    -1,          A_BOLD                  },
	[EXECUTABLE_COLOR]   = { COLOR_GREEN,   -1,          A_BOLD                  },
	[SELECTED_COLOR]     = { COLOR_MAGENTA, -1,          A_BOLD                  },
	[CURR_LINE_COLOR]    = { -1,            COLOR_BLUE,  A_BOLD                  },
	[TOP_LINE_COLOR]     = { COLOR_BLACK,   COLOR_WHITE, 0                       },
	[TOP_LINE_SEL_COLOR] = { COLOR_BLACK,   -1,          A_BOLD                  },
	[STATUS_LINE_COLOR]  = { COLOR_BLACK,   COLOR_WHITE, A_BOLD                  },
	[MENU_COLOR]         = { COLOR_WHITE,   COLOR_BLACK, A_UNDERLINE | A_REVERSE },
	[CMD_LINE_COLOR]     = { COLOR_WHITE,   COLOR_BLACK, 0                       },
	[ERROR_MSG_COLOR]    = { COLOR_RED,     COLOR_BLACK, 0                       },
	[BORDER_COLOR]       = { COLOR_BLACK,   COLOR_WHITE, 0                       },
};
ARRAY_GUARD(default_colors, MAXNUM_COLOR - 2);

static void init_color_scheme(col_scheme_t *cs);
static void load_color_pairs(int base, const col_scheme_t *cs);
static void ensure_dirs_tree_exists(void);

static tree_t dirs = NULL_TREE;

void
check_color_scheme(col_scheme_t *cs)
{
	int i;

	if(cs->defaulted >= 0)
		return;

	cs->defaulted = 1;
	for(i = 0; i < ARRAY_LEN(default_colors); i++)
	{
		cs->color[i].fg = default_colors[i][0];
		cs->color[i].bg = default_colors[i][1];
		cs->color[i].attr = default_colors[i][2];
	}
}

char **
list_color_schemes(int *len)
{
	char colors_dir[PATH_MAX];
	snprintf(colors_dir, sizeof(colors_dir), "%s/colors", cfg.config_dir);
	return list_regular_files(colors_dir, len);
}

int
color_scheme_exists(const char name[])
{
	char full_path[PATH_MAX];
	snprintf(full_path, sizeof(full_path), "%s/colors/%s", cfg.config_dir, name);
	return is_regular_file(full_path);
}

/* This function is called only when colorschemes file doesn't exist */
void
write_color_scheme_file(void)
{
	FILE *fp;
	char colors_dir[PATH_MAX];
	int y;

	snprintf(colors_dir, sizeof(colors_dir), "%s/colors", cfg.config_dir);
	if(make_dir(colors_dir, 0777) != 0)
		return;

	strncat(colors_dir, "/Default", sizeof(colors_dir) - strlen(colors_dir) - 1);
	if((fp = fopen(colors_dir, "w")) == NULL)
		return;

	fprintf(fp, "\" You can edit this file by hand.\n");
	fprintf(fp, "\" The \" character at the beginning of a line comments out the line.\n");
	fprintf(fp, "\" Blank lines are ignored.\n\n");

	fprintf(fp, "\" The Default color scheme is used for any directory that does not have\n");
	fprintf(fp, "\" a specified scheme and for parts of user interface like menus. A\n");
	fprintf(fp, "\" color scheme set for a base directory will also\n");
	fprintf(fp, "\" be used for the sub directories.\n\n");

	fprintf(fp, "\" The standard ncurses colors are:\n");
	fprintf(fp, "\" Default = -1 = None, can be used for transparency or default color\n");
	fprintf(fp, "\" Black = 0\n");
	fprintf(fp, "\" Red = 1\n");
	fprintf(fp, "\" Green = 2\n");
	fprintf(fp, "\" Yellow = 3\n");
	fprintf(fp, "\" Blue = 4\n");
	fprintf(fp, "\" Magenta = 5\n");
	fprintf(fp, "\" Cyan = 6\n");
	fprintf(fp, "\" White = 7\n\n");

	fprintf(fp, "\" Light versions of colors are also available (set bold attribute):\n");
	fprintf(fp, "\" LightBlack\n");
	fprintf(fp, "\" LightRed\n");
	fprintf(fp, "\" LightGreen\n");
	fprintf(fp, "\" LightYellow\n");
	fprintf(fp, "\" LightBlue\n");
	fprintf(fp, "\" LightMagenta\n");
	fprintf(fp, "\" LightCyan\n");
	fprintf(fp, "\" LightWhite\n\n");

	fprintf(fp, "\" Available attributes (some of them can be combined):\n");
	fprintf(fp, "\" bold\n");
	fprintf(fp, "\" underline\n");
	fprintf(fp, "\" reverse or inverse\n");
	fprintf(fp, "\" standout\n");
	fprintf(fp, "\" none\n\n");

	fprintf(fp, "\" Vifm supports 256 colors you can use color numbers 0-255\n");
	fprintf(fp, "\" (requires properly set up terminal: set your TERM environment variable\n");
	fprintf(fp, "\" (directly or using resources) to some color terminal name (e.g.\n");
	fprintf(fp, "\" xterm-256color) from /usr/lib/terminfo/; you can check current number\n");
	fprintf(fp, "\" of colors in your terminal with tput colors command)\n\n");

	fprintf(fp, "\" highlight group cterm=attrs ctermfg=foreground_color ctermbg=background_color\n\n");

	for(y = 0; y < MAXNUM_COLOR - 2; y++)
	{
		char fg_buf[16], bg_buf[16];

		color_to_str(cfg.cs.color[y].fg, sizeof(fg_buf), fg_buf);
		color_to_str(cfg.cs.color[y].bg, sizeof(bg_buf), bg_buf);

		fprintf(fp, "highlight %s cterm=%s ctermfg=%s ctermbg=%s\n", HI_GROUPS[y],
				attrs_to_str(cfg.cs.color[y].attr), fg_buf, bg_buf);
	}

	fclose(fp);
}

void
color_to_str(int color, size_t buf_len, char str_buf[])
{
	if(color == -1)
	{
		copy_str(str_buf, buf_len, "default");
	}
	else if(color >= 0 && color < ARRAY_LEN(COLOR_NAMES))
	{
		copy_str(str_buf, buf_len, COLOR_NAMES[color]);
	}
	else
	{
		snprintf(str_buf, buf_len, "%d", color);
	}
}

void
load_color_scheme_colors(void)
{
	ensure_dirs_tree_exists();

	load_color_pairs(DCOLOR_BASE, &cfg.cs);
	load_color_pairs(LCOLOR_BASE, &lwin.cs);
	load_color_pairs(RCOLOR_BASE, &rwin.cs);
}

void
load_def_scheme(void)
{
	tree_free(dirs);
	dirs = NULL_TREE;

	init_color_scheme(&cfg.cs);
	init_color_scheme(&lwin.cs);
	lwin.color_scheme = LCOLOR_BASE;
	init_color_scheme(&rwin.cs);
	rwin.color_scheme = RCOLOR_BASE;

	load_color_pairs(DCOLOR_BASE, &cfg.cs);
	load_color_pairs(LCOLOR_BASE, &lwin.cs);
	load_color_pairs(RCOLOR_BASE, &rwin.cs);
}

static void
init_color_scheme(col_scheme_t *cs)
{
	int i;
	snprintf(cs->name, sizeof(cs->name), "%s", "built-in default");
	snprintf(cs->dir, sizeof(cs->dir), "%s", "/");
	cs->defaulted = 0;

	for(i = 0; i < ARRAY_LEN(default_colors); i++)
	{
		cs->color[i].fg = default_colors[i][0];
		cs->color[i].bg = default_colors[i][1];
		cs->color[i].attr = default_colors[i][2];
	}
	for(i = ARRAY_LEN(default_colors); i < MAXNUM_COLOR; i++)
	{
		cs->color[i].fg = -1;
		cs->color[i].bg = -1;
		cs->color[i].attr = 0;
	}
}

/* The return value is the color scheme base number for the colorpairs.
 *
 * The color scheme with the longest matching directory path is the one that
 * should be returned.
 */
int
check_directory_for_color_scheme(int left, const char *dir)
{
	char *p;
	char t;

	union
	{
		char *name;
		tree_val_t buf;
	}u;

	if(dirs == NULL_TREE)
		return DCOLOR_BASE;

	curr_stats.cs_base = left ? LCOLOR_BASE : RCOLOR_BASE;
	curr_stats.cs = left ? &lwin.cs : &rwin.cs;
	*curr_stats.cs = cfg.cs;

	p = (char *)dir;
	do
	{
		char full[PATH_MAX];
		t = *p;
		*p = '\0';

		if(tree_get_data(dirs, dir, &u.buf) != 0 || !color_scheme_exists(u.name))
		{
			*p = t;
			if((p = strchr(p + 1, '/')) == NULL)
				p = (char *)dir + strlen(dir);
			continue;
		}

		snprintf(full, sizeof(full), "%s/colors/%s", cfg.config_dir, u.name);
		(void)source_file(full);

		*p = t;
		if((p = strchr(p + 1, '/')) == NULL)
			p = (char *)dir + strlen(dir);
	}
	while(t != '\0');

	check_color_scheme(curr_stats.cs);
	load_color_pairs(curr_stats.cs_base, curr_stats.cs);
	curr_stats.cs_base = DCOLOR_BASE;
	curr_stats.cs = &cfg.cs;

	return left ? LCOLOR_BASE : RCOLOR_BASE;
}

static void
load_color_pairs(int base, const col_scheme_t *cs)
{
	int i;
	for(i = 0; i < MAXNUM_COLOR; i++)
		init_pair(base + i, cs->color[i].fg, cs->color[i].bg);
}

void
complete_colorschemes(const char name[])
{
	int i;
	size_t len;
	int schemes_len;
	char **schemes;

	len = strlen(name);
	schemes = list_color_schemes(&schemes_len);

	for(i = 0; i < schemes_len; i++)
	{
		if(schemes[i][0] != '.' || name[0] == '.')
		{
			if(strnoscmp(name, schemes[i], len) == 0)
			{
				add_completion(schemes[i]);
			}
		}
	}

	free_string_array(schemes, schemes_len);

	completion_group_end();
	add_completion(name);
}

const char *
attrs_to_str(int attrs)
{
	static char result[64];
	result[0] = '\0';
	if(attrs == 0)
		strcpy(result, "none,");
	if((attrs & A_BOLD) == A_BOLD)
		strcat(result, "bold,");
	if((attrs & A_UNDERLINE) == A_UNDERLINE)
		strcat(result, "underline,");
	if((attrs & A_REVERSE) == A_REVERSE)
		strcat(result, "reverse,");
	if((attrs & A_STANDOUT) == A_STANDOUT)
		strcat(result, "standout,");
	if(result[0] != '\0')
		result[strlen(result) - 1] = '\0';
	return result;
}

void
assoc_dir(const char *name, const char *dir)
{
	union
	{
		char *s;
		tree_val_t l;
	}u = {
		.s = strdup(name),
	};

	ensure_dirs_tree_exists();

	if(tree_set_data(dirs, dir, u.l) != 0)
		free(u.s);
}

static void
ensure_dirs_tree_exists(void)
{
	if(dirs == NULL_TREE)
	{
		dirs = tree_create(1, 1);
	}
}

void
mix_colors(col_attr_t *base, const col_attr_t *mixup)
{
	if(mixup->fg != -1)
		base->fg = mixup->fg;
	if(mixup->bg != -1)
		base->bg = mixup->bg;
	base->attr = mixup->attr;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
