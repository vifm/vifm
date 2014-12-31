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

#include <stddef.h> /* size_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* strcpy() strlen() */

#include "cfg/config.h"
#include "engine/completion.h"
#include "menus/menus.h"
#include "ui/ui.h"
#include "utils/fs.h"
#include "utils/fs_limits.h"
#include "utils/macros.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/tree.h"
#include "color_manager.h"
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
	[OTHER_LINE_COLOR]   = "OtherLine",
};
ARRAY_GUARD(HI_GROUPS, MAXNUM_COLOR);

char *LIGHT_COLOR_NAMES[8] = {
	[COLOR_BLACK] = "lightblack",
	[COLOR_RED] = "lightred",
	[COLOR_GREEN] = "lightgreen",
	[COLOR_YELLOW] = "lightyellow",
	[COLOR_BLUE] = "lightblue",
	[COLOR_MAGENTA] = "lightmagenta",
	[COLOR_CYAN] = "lightcyan",
	[COLOR_WHITE] = "lightwhite",
};

char *XTERM256_COLOR_NAMES[256] = {
	[COLOR_BLACK]   = "black",
	[COLOR_RED]     = "red",
	[COLOR_GREEN]   = "green",
	[COLOR_YELLOW]  = "yellow",
	[COLOR_BLUE]    = "blue",
	[COLOR_MAGENTA] = "magenta",
	[COLOR_CYAN]    = "cyan",
	[COLOR_WHITE]   = "white",
	[8]  = "lightblack",
	[9]  = "lightred",
	[10] = "lightgreen",
	[11] = "lightyellow",
	[12] = "lightblue",
	[13] = "lightmagenta",
	[14] = "lightcyan",
	[15] = "lightwhite",
	[16] = "Grey0",
	[17] = "NavyBlue",
	[18] = "DarkBlue",
	[19] = "Blue3",
	[20] = "Blue3_2",
	[21] = "Blue1",
	[22] = "DarkGreen",
	[23] = "DeepSkyBlue4",
	[24] = "DeepSkyBlue4_2",
	[25] = "DeepSkyBlue4_3",
	[26] = "DodgerBlue3",
	[27] = "DodgerBlue2",
	[28] = "Green4",
	[29] = "SpringGreen4",
	[30] = "Turquoise4",
	[31] = "DeepSkyBlue3",
	[32] = "DeepSkyBlue3_2",
	[33] = "DodgerBlue1",
	[34] = "Green3",
	[35] = "SpringGreen3",
	[36] = "DarkCyan",
	[37] = "LightSeaGreen",
	[38] = "DeepSkyBlue2",
	[39] = "DeepSkyBlue1",
	[40] = "Green3_2",
	[41] = "SpringGreen3_2",
	[42] = "SpringGreen2",
	[43] = "Cyan3",
	[44] = "DarkTurquoise",
	[45] = "Turquoise2",
	[46] = "Green1",
	[47] = "SpringGreen2_2",
	[48] = "SpringGreen1",
	[49] = "MediumSpringGreen",
	[50] = "Cyan2",
	[51] = "Cyan1",
	[52] = "DarkRed",
	[53] = "DeepPink4",
	[54] = "Purple4",
	[55] = "Purple4_2",
	[56] = "Purple3",
	[57] = "BlueViolet",
	[58] = "Orange4",
	[59] = "Grey37",
	[60] = "MediumPurple4",
	[61] = "SlateBlue3",
	[62] = "SlateBlue3_2",
	[63] = "RoyalBlue1",
	[64] = "Chartreuse4",
	[65] = "DarkSeaGreen4",
	[66] = "PaleTurquoise4",
	[67] = "SteelBlue",
	[68] = "SteelBlue3",
	[69] = "CornflowerBlue",
	[70] = "Chartreuse3",
	[71] = "DarkSeaGreen4_2",
	[72] = "CadetBlue",
	[73] = "CadetBlue_2",
	[74] = "SkyBlue3",
	[75] = "SteelBlue1",
	[76] = "Chartreuse3_2",
	[77] = "PaleGreen3",
	[78] = "SeaGreen3",
	[79] = "Aquamarine3",
	[80] = "MediumTurquoise",
	[81] = "SteelBlue1_2",
	[82] = "Chartreuse2",
	[83] = "SeaGreen2",
	[84] = "SeaGreen1",
	[85] = "SeaGreen1_2",
	[86] = "Aquamarine1",
	[87] = "DarkSlateGray2",
	[88] = "DarkRed_2",
	[89] = "DeepPink4_2",
	[90] = "DarkMagenta",
	[91] = "DarkMagenta_2",
	[92] = "DarkViolet",
	[93] = "Purple",
	[94] = "Orange4_2",
	[95] = "LightPink4",
	[96] = "Plum4",
	[97] = "MediumPurple3",
	[98] = "MediumPurple3_2",
	[99] = "SlateBlue1",
	[100] = "Yellow4",
	[101] = "Wheat4",
	[102] = "Grey53",
	[103] = "LightSlateGrey",
	[104] = "MediumPurple",
	[105] = "LightSlateBlue",
	[106] = "Yellow4_2",
	[107] = "DarkOliveGreen3",
	[108] = "DarkSeaGreen",
	[109] = "LightSkyBlue3",
	[110] = "LightSkyBlue3_2",
	[111] = "SkyBlue2",
	[112] = "Chartreuse2_2",
	[113] = "DarkOliveGreen3_2",
	[114] = "PaleGreen3_2",
	[115] = "DarkSeaGreen3",
	[116] = "DarkSlateGray3",
	[117] = "SkyBlue1",
	[118] = "Chartreuse1",
	[119] = "LightGreen_2",
	[120] = "LightGreen_3",
	[121] = "PaleGreen1",
	[122] = "Aquamarine1_2",
	[123] = "DarkSlateGray1",
	[124] = "Red3",
	[125] = "DeepPink4_3",
	[126] = "MediumVioletRed",
	[127] = "Magenta3",
	[128] = "DarkViolet_2",
	[129] = "Purple_2",
	[130] = "DarkOrange3",
	[131] = "IndianRed",
	[132] = "HotPink3",
	[133] = "MediumOrchid3",
	[134] = "MediumOrchid",
	[135] = "MediumPurple2",
	[136] = "DarkGoldenrod",
	[137] = "LightSalmon3",
	[138] = "RosyBrown",
	[139] = "Grey63",
	[140] = "MediumPurple2_2",
	[141] = "MediumPurple1",
	[142] = "Gold3",
	[143] = "DarkKhaki",
	[144] = "NavajoWhite3",
	[145] = "Grey69",
	[146] = "LightSteelBlue3",
	[147] = "LightSteelBlue",
	[148] = "Yellow3",
	[149] = "DarkOliveGreen3_3",
	[150] = "DarkSeaGreen3_2",
	[151] = "DarkSeaGreen2",
	[152] = "LightCyan3",
	[153] = "LightSkyBlue1",
	[154] = "GreenYellow",
	[155] = "DarkOliveGreen2",
	[156] = "PaleGreen1_2",
	[157] = "DarkSeaGreen2_2",
	[158] = "DarkSeaGreen1",
	[159] = "PaleTurquoise1",
	[160] = "Red3_2",
	[161] = "DeepPink3",
	[162] = "DeepPink3_2",
	[163] = "Magenta3_2",
	[164] = "Magenta3_3",
	[165] = "Magenta2",
	[166] = "DarkOrange3_2",
	[167] = "IndianRed_2",
	[168] = "HotPink3_2",
	[169] = "HotPink2",
	[170] = "Orchid",
	[171] = "MediumOrchid1",
	[172] = "Orange3",
	[173] = "LightSalmon3_2",
	[174] = "LightPink3",
	[175] = "Pink3",
	[176] = "Plum3",
	[177] = "Violet",
	[178] = "Gold3_2",
	[179] = "LightGoldenrod3",
	[180] = "Tan",
	[181] = "MistyRose3",
	[182] = "Thistle3",
	[183] = "Plum2",
	[184] = "Yellow3_2",
	[185] = "Khaki3",
	[186] = "LightGoldenrod2",
	[187] = "LightYellow3",
	[188] = "Grey84",
	[189] = "LightSteelBlue1",
	[190] = "Yellow2",
	[191] = "DarkOliveGreen1",
	[192] = "DarkOliveGreen1_2",
	[193] = "DarkSeaGreen1_2",
	[194] = "Honeydew2",
	[195] = "LightCyan1",
	[196] = "Red1",
	[197] = "DeepPink2",
	[198] = "DeepPink1",
	[199] = "DeepPink1_2",
	[200] = "Magenta2_2",
	[201] = "Magenta1",
	[202] = "OrangeRed1",
	[203] = "IndianRed1",
	[204] = "IndianRed1_2",
	[205] = "HotPink",
	[206] = "HotPink_2",
	[207] = "MediumOrchid1_2",
	[208] = "DarkOrange",
	[209] = "Salmon1",
	[210] = "LightCoral",
	[211] = "PaleVioletRed1",
	[212] = "Orchid2",
	[213] = "Orchid1",
	[214] = "Orange1",
	[215] = "SandyBrown",
	[216] = "LightSalmon1",
	[217] = "LightPink1",
	[218] = "Pink1",
	[219] = "Plum1",
	[220] = "Gold1",
	[221] = "LightGoldenrod2_2",
	[222] = "LightGoldenrod2_3",
	[223] = "NavajoWhite1",
	[224] = "MistyRose1",
	[225] = "Thistle1",
	[226] = "Yellow1",
	[227] = "LightGoldenrod1",
	[228] = "Khaki1",
	[229] = "Wheat1",
	[230] = "Cornsilk1",
	[231] = "Grey100",
	[232] = "Grey3",
	[233] = "Grey7",
	[234] = "Grey11",
	[235] = "Grey15",
	[236] = "Grey19",
	[237] = "Grey23",
	[238] = "Grey27",
	[239] = "Grey30",
	[240] = "Grey35",
	[241] = "Grey39",
	[242] = "Grey42",
	[243] = "Grey46",
	[244] = "Grey50",
	[245] = "Grey54",
	[246] = "Grey58",
	[247] = "Grey62",
	[248] = "Grey66",
	[249] = "Grey70",
	[250] = "Grey74",
	[251] = "Grey78",
	[252] = "Grey82",
	[253] = "Grey85",
	[254] = "Grey89",
	[255] = "Grey93",
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
	[OTHER_LINE_COLOR]   = { -1,            -1,          -1                      },
};
ARRAY_GUARD(default_colors, MAXNUM_COLOR);

static void restore_primary_color_scheme(const col_scheme_t *cs);
static void reset_to_default_color_scheme(col_scheme_t *cs);
static void reset_color_scheme_colors(col_scheme_t *cs);
static void load_color_pairs(int base, col_scheme_t *cs);
static void ensure_dirs_tree_exists(void);

static tree_t dirs = NULL_TREE;

void
check_color_scheme(col_scheme_t *cs)
{
	if(cs->state == CSS_BROKEN)
	{
		int i;
		for(i = 0; i < ARRAY_LEN(default_colors); i++)
		{
			cs->color[i].fg = default_colors[i][0];
			cs->color[i].bg = default_colors[i][1];
			cs->color[i].attr = default_colors[i][2];
		}

		cs->state = CSS_DEFAULTED;
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

void
write_color_scheme_file(void)
{
	FILE *fp;
	char colors_dir[PATH_MAX];
	int i;

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

	for(i = 0; i < MAXNUM_COLOR; ++i)
	{
		char fg_buf[16], bg_buf[16];

		if(i == OTHER_LINE_COLOR)
		{
			/* Skip OtherLine as there is no way to express defaults. */
			continue;
		}

		color_to_str(cfg.cs.color[i].fg, sizeof(fg_buf), fg_buf);
		color_to_str(cfg.cs.color[i].bg, sizeof(bg_buf), bg_buf);

		fprintf(fp, "highlight %s cterm=%s ctermfg=%s ctermbg=%s\n", HI_GROUPS[i],
				attrs_to_str(cfg.cs.color[i].attr), fg_buf, bg_buf);
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
	else if(color >= 0 && color < ARRAY_LEN(XTERM256_COLOR_NAMES))
	{
		copy_str(str_buf, buf_len, XTERM256_COLOR_NAMES[color]);
	}
	else
	{
		snprintf(str_buf, buf_len, "%d", color);
	}
}

int
load_primary_color_scheme(const char name[])
{
	col_scheme_t prev_cs;
	char full[PATH_MAX];

	if(!color_scheme_exists(name))
	{
		show_error_msgf("Color Scheme", "Invalid color scheme name: \"%s\"", name);
		return 0;
	}

	prev_cs = cfg.cs;
	curr_stats.cs_base = DCOLOR_BASE;
	curr_stats.cs = &cfg.cs;
	cfg.cs.state = CSS_LOADING;

	snprintf(full, sizeof(full), "%s/colors/%s", cfg.config_dir, name);
	if(source_file(full) != 0)
	{
		restore_primary_color_scheme(&prev_cs);
		show_error_msgf("Color Scheme Sourcing",
				"Errors loading colors cheme: \"%s\"", name);
		cfg.cs.state = CSS_NORMAL;
		return 0;
	}
	copy_str(cfg.cs.name, sizeof(cfg.cs.name), name);
	check_color_scheme(&cfg.cs);

	update_attributes();

	if(curr_stats.load_stage >= 2 && cfg.cs.state == CSS_DEFAULTED)
	{
		restore_primary_color_scheme(&prev_cs);
		show_error_msg("Color Scheme Error", "Not supported by the terminal");
		return 0;
	}

	cfg.cs.state = CSS_NORMAL;
	return 0;
}

/* Restore previous state of primary color scheme. */
static void
restore_primary_color_scheme(const col_scheme_t *cs)
{
	cfg.cs = *cs;
	load_color_scheme_colors();
	update_screen(UT_FULL);
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

	reset_to_default_color_scheme(&cfg.cs);
	reset_to_default_color_scheme(&lwin.cs);
	lwin.color_scheme = LCOLOR_BASE;
	reset_to_default_color_scheme(&rwin.cs);
	rwin.color_scheme = RCOLOR_BASE;

	load_color_pairs(DCOLOR_BASE, &cfg.cs);
	load_color_pairs(LCOLOR_BASE, &lwin.cs);
	load_color_pairs(RCOLOR_BASE, &rwin.cs);
}

/* Completely resets the cs to builtin default color scheme.  Changes: colors,
 * name, state. */
static void
reset_to_default_color_scheme(col_scheme_t *cs)
{
	reset_color_scheme_colors(cs);

	copy_str(cs->name, sizeof(cs->name), DEF_CS_NAME);
	copy_str(cs->dir, sizeof(cs->dir), "/");

	cs->state = CSS_NORMAL;
}

void
reset_color_scheme(int color_base, col_scheme_t *cs)
{
	reset_color_scheme_colors(cs);
	load_color_pairs(color_base, cs);
}

/* Resets color scheme to default builtin values. */
static void
reset_color_scheme_colors(col_scheme_t *cs)
{
	int i;

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
	{
		return DCOLOR_BASE;
	}

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

/* Loads color scheme settings into color pairs. */
static void
load_color_pairs(int base, col_scheme_t *cs)
{
	int i;
	for(i = 0; i < MAXNUM_COLOR; i++)
	{
		cs->pair[i] = colmgr_get_pair(cs->color[i].fg, cs->color[i].bg);
	}
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
				vle_compl_add_match(schemes[i]);
			}
		}
	}

	free_string_array(schemes, schemes_len);

	vle_compl_finish_group();
	vle_compl_add_last_match(name);
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
	{
		base->fg = mixup->fg;
	}

	if(mixup->bg != -1)
	{
		base->bg = mixup->bg;
	}

	if(mixup->attr != -1)
	{
		base->attr = mixup->attr;
	}
}

int
is_color_set(const col_attr_t *color)
{
	return color->fg != -1 || color->bg != -1 || color->attr != -1;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
