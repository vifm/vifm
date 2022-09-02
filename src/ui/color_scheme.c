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
#include <regex.h> /* regexec() */

#include <assert.h> /* assert() */
#include <limits.h> /* INT_MAX */
#include <math.h> /* abs() */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() */
#include <stdlib.h> /* free() */
#include <string.h> /* strcpy() strlen() */

#include "../cfg/config.h"
#include "../compat/dtype.h"
#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "../compat/reallocarray.h"
#include "../engine/completion.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../utils/fs.h"
#include "../utils/fsddata.h"
#include "../utils/macros.h"
#include "../utils/matchers.h"
#include "../utils/str.h"
#include "../utils/string_array.h"
#include "../utils/utils.h"
#include "../status.h"
#include "color_manager.h"
#include "statusbar.h"
#include "ui.h"

char *HI_GROUPS[] = {
	[WIN_COLOR]          = "Win",
	[DIRECTORY_COLOR]    = "Directory",
	[LINK_COLOR]         = "Link",
	[BROKEN_LINK_COLOR]  = "BrokenLink",
	[HARD_LINK_COLOR]    = "HardLink",
	[SOCKET_COLOR]       = "Socket",
	[DEVICE_COLOR]       = "Device",
	[FIFO_COLOR]         = "Fifo",
	[EXECUTABLE_COLOR]   = "Executable",
	[SELECTED_COLOR]     = "Selected",
	[CURR_LINE_COLOR]    = "CurrLine",
	[TOP_LINE_COLOR]     = "TopLine",
	[TOP_LINE_SEL_COLOR] = "TopLineSel",
	[STATUS_LINE_COLOR]  = "StatusLine",
	[WILD_MENU_COLOR]    = "WildMenu",
	[CMD_LINE_COLOR]     = "CmdLine",
	[ERROR_MSG_COLOR]    = "ErrorMsg",
	[BORDER_COLOR]       = "Border",
	[OTHER_LINE_COLOR]   = "OtherLine",
	[JOB_LINE_COLOR]     = "JobLine",
	[SUGGEST_BOX_COLOR]  = "SuggestBox",
	[MISMATCH_COLOR]     = "CmpMismatch",
	[UNMATCHED_COLOR]    = "CmpUnmatched",
	[BLANK_COLOR]        = "CmpBlank",
	[AUX_WIN_COLOR]      = "AuxWin",
	[TAB_LINE_COLOR]     = "TabLine",
	[TAB_LINE_SEL_COLOR] = "TabLineSel",
	[USER1_COLOR]        = "User1",
	[USER2_COLOR]        = "User2",
	[USER3_COLOR]        = "User3",
	[USER4_COLOR]        = "User4",
	[USER5_COLOR]        = "User5",
	[USER6_COLOR]        = "User6",
	[USER7_COLOR]        = "User7",
	[USER8_COLOR]        = "User8",
	[USER9_COLOR]        = "User9",
	[OTHER_WIN_COLOR]    = "OtherWin",
	[LINE_NUM_COLOR]     = "LineNr",
	[ODD_LINE_COLOR]     = "OddLine",
};
ARRAY_GUARD(HI_GROUPS, MAXNUM_COLOR);

const char *HI_GROUPS_DESCR[] = {
	[WIN_COLOR]          = "base highlight of every window",
	[DIRECTORY_COLOR]    = "directories and links to directories",
	[LINK_COLOR]         = "symbolic links",
	[BROKEN_LINK_COLOR]  = "dangling symbolic links",
	[HARD_LINK_COLOR]    = "regular files with more than one hard link",
	[SOCKET_COLOR]       = "sockets",
	[DEVICE_COLOR]       = "device file",
	[FIFO_COLOR]         = "pipe",
	[EXECUTABLE_COLOR]   = "executable file",
	[SELECTED_COLOR]     = "selected item",
	[CURR_LINE_COLOR]    = "cursor in active pane",
	[TOP_LINE_COLOR]     = "base top line highlight",
	[TOP_LINE_SEL_COLOR] = "active window title",
	[STATUS_LINE_COLOR]  = "status line",
	[WILD_MENU_COLOR]    = "completion box",
	[CMD_LINE_COLOR]     = "status/command-line bar",
	[ERROR_MSG_COLOR]    = "error message",
	[BORDER_COLOR]       = "horizontal borders",
	[OTHER_LINE_COLOR]   = "cursor in inactive pane",
	[JOB_LINE_COLOR]     = "job bar",
	[SUGGEST_BOX_COLOR]  = "suggestion box",
	[MISMATCH_COLOR]     = "mismatched diff entries",
	[UNMATCHED_COLOR]    = "diff entry with no match in the other pane",
	[BLANK_COLOR]        = "entry placeholder in a diff",
	[AUX_WIN_COLOR]      = "auxiliary part of window",
	[TAB_LINE_COLOR]     = "tab line",
	[TAB_LINE_SEL_COLOR] = "tip of selected tab",
	[USER1_COLOR]        = "user color #1",
	[USER2_COLOR]        = "user color #2",
	[USER3_COLOR]        = "user color #3",
	[USER4_COLOR]        = "user color #4",
	[USER5_COLOR]        = "user color #5",
	[USER6_COLOR]        = "user color #6",
	[USER7_COLOR]        = "user color #7",
	[USER8_COLOR]        = "user color #8",
	[USER9_COLOR]        = "user color #9",
	[OTHER_WIN_COLOR]    = "additional highlighting of inactive pane",
	[LINE_NUM_COLOR]     = "color of line number column in panes",
	[ODD_LINE_COLOR]     = "color of every second entry line in a pane",
};
ARRAY_GUARD(HI_GROUPS_DESCR, ARRAY_LEN(HI_GROUPS));

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

/* Maps 256 colors into corresponding #RRGGBB values.  The array is offset by
 * one to allow handling of default color (-1). */
static const int XTERM256_PALETTE[] = {
	-1,
	0, 1, 2, 3, 4, 5, 6, 7,
	0x808080, 0xff0000, 0x00ff00, 0xffff00, 0x0000ff, 0xff00ff, 0x00ffff,
	0xffffff, 0x000000, 0x00005f, 0x000087, 0x0000af, 0x0000d7, 0x0000ff,
	0x005f00, 0x005f5f, 0x005f87, 0x005faf, 0x005fd7, 0x005fff, 0x008700,
	0x00875f, 0x008787, 0x0087af, 0x0087d7, 0x0087ff, 0x00af00, 0x00af5f,
	0x00af87, 0x00afaf, 0x00afd7, 0x00afff, 0x00d700, 0x00d75f, 0x00d787,
	0x00d7af, 0x00d7d7, 0x00d7ff, 0x00ff00, 0x00ff5f, 0x00ff87, 0x00ffaf,
	0x00ffd7, 0x00ffff, 0x5f0000, 0x5f005f, 0x5f0087, 0x5f00af, 0x5f00d7,
	0x5f00ff, 0x5f5f00, 0x5f5f5f, 0x5f5f87, 0x5f5faf, 0x5f5fd7, 0x5f5fff,
	0x5f8700, 0x5f875f, 0x5f8787, 0x5f87af, 0x5f87d7, 0x5f87ff, 0x5faf00,
	0x5faf5f, 0x5faf87, 0x5fafaf, 0x5fafd7, 0x5fafff, 0x5fd700, 0x5fd75f,
	0x5fd787, 0x5fd7af, 0x5fd7d7, 0x5fd7ff, 0x5fff00, 0x5fff5f, 0x5fff87,
	0x5fffaf, 0x5fffd7, 0x5fffff, 0x870000, 0x87005f, 0x870087, 0x8700af,
	0x8700d7, 0x8700ff, 0x875f00, 0x875f5f, 0x875f87, 0x875faf, 0x875fd7,
	0x875fff, 0x878700, 0x87875f, 0x878787, 0x8787af, 0x8787d7, 0x8787ff,
	0x87af00, 0x87af5f, 0x87af87, 0x87afaf, 0x87afd7, 0x87afff, 0x87d700,
	0x87d75f, 0x87d787, 0x87d7af, 0x87d7d7, 0x87d7ff, 0x87ff00, 0x87ff5f,
	0x87ff87, 0x87ffaf, 0x87ffd7, 0x87ffff, 0xaf0000, 0xaf005f, 0xaf0087,
	0xaf00af, 0xaf00d7, 0xaf00ff, 0xaf5f00, 0xaf5f5f, 0xaf5f87, 0xaf5faf,
	0xaf5fd7, 0xaf5fff, 0xaf8700, 0xaf875f, 0xaf8787, 0xaf87af, 0xaf87d7,
	0xaf87ff, 0xafaf00, 0xafaf5f, 0xafaf87, 0xafafaf, 0xafafd7, 0xafafff,
	0xafd700, 0xafd75f, 0xafd787, 0xafd7af, 0xafd7d7, 0xafd7ff, 0xafff00,
	0xafff5f, 0xafff87, 0xafffaf, 0xafffd7, 0xafffff, 0xd70000, 0xd7005f,
	0xd70087, 0xd700af, 0xd700d7, 0xd700ff, 0xd75f00, 0xd75f5f, 0xd75f87,
	0xd75faf, 0xd75fd7, 0xd75fff, 0xd78700, 0xd7875f, 0xd78787, 0xd787af,
	0xd787d7, 0xd787ff, 0xdfaf00, 0xdfaf5f, 0xdfaf87, 0xdfafaf, 0xdfafdf,
	0xdfafff, 0xdfdf00, 0xdfdf5f, 0xdfdf87, 0xdfdfaf, 0xdfdfdf, 0xdfdfff,
	0xdfff00, 0xdfff5f, 0xdfff87, 0xdfffaf, 0xdfffdf, 0xdfffff, 0xff0000,
	0xff005f, 0xff0087, 0xff00af, 0xff00df, 0xff00ff, 0xff5f00, 0xff5f5f,
	0xff5f87, 0xff5faf, 0xff5fdf, 0xff5fff, 0xff8700, 0xff875f, 0xff8787,
	0xff87af, 0xff87df, 0xff87ff, 0xffaf00, 0xffaf5f, 0xffaf87, 0xffafaf,
	0xffafdf, 0xffafff, 0xffdf00, 0xffdf5f, 0xffdf87, 0xffdfaf, 0xffdfdf,
	0xffdfff, 0xffff00, 0xffff5f, 0xffff87, 0xffffaf, 0xffffdf, 0xffffff,
	0x080808, 0x121212, 0x1c1c1c, 0x262626, 0x303030, 0x3a3a3a, 0x444444,
	0x4e4e4e, 0x585858, 0x626262, 0x6c6c6c, 0x767676, 0x808080, 0x8a8a8a,
	0x949494, 0x9e9e9e, 0xa8a8a8, 0xb2b2b2, 0xbcbcbc, 0xc6c6c6, 0xd0d0d0,
	0xdadada, 0xe4e4e4, 0xeeeeee,
};
ARRAY_GUARD(XTERM256_PALETTE, 1 + 256);

/* Default color scheme definition. */
static const col_attr_t default_cs[] = {
	                      /* fg             bg           attr */
	[WIN_COLOR]          = { COLOR_WHITE,   COLOR_BLACK, 0                       },
	[DIRECTORY_COLOR]    = { COLOR_CYAN,    -1,          A_BOLD                  },
	[LINK_COLOR]         = { COLOR_YELLOW,  -1,          A_BOLD                  },
	[BROKEN_LINK_COLOR]  = { COLOR_RED,     -1,          A_BOLD                  },
	[HARD_LINK_COLOR]    = { COLOR_YELLOW,  -1,          0                       },
	[SOCKET_COLOR]       = { COLOR_MAGENTA, -1,          A_BOLD                  },
	[DEVICE_COLOR]       = { COLOR_RED,     -1,          A_BOLD                  },
	[FIFO_COLOR]         = { COLOR_CYAN,    -1,          A_BOLD                  },
	[EXECUTABLE_COLOR]   = { COLOR_GREEN,   -1,          A_BOLD                  },
	[SELECTED_COLOR]     = { COLOR_MAGENTA, -1,          A_BOLD                  },
	[CURR_LINE_COLOR]    = { -1,            -1,          A_BOLD | A_REVERSE      },
	[TOP_LINE_COLOR]     = { COLOR_BLACK,   COLOR_WHITE, 0                       },
	[TOP_LINE_SEL_COLOR] = { COLOR_BLACK,   -1,          A_BOLD                  },
	[STATUS_LINE_COLOR]  = { COLOR_BLACK,   COLOR_WHITE, A_BOLD                  },
	[WILD_MENU_COLOR]    = { COLOR_WHITE,   COLOR_BLACK, A_UNDERLINE | A_REVERSE },
	[CMD_LINE_COLOR]     = { COLOR_WHITE,   COLOR_BLACK, 0                       },
	[ERROR_MSG_COLOR]    = { COLOR_RED,     COLOR_BLACK, 0                       },
	[BORDER_COLOR]       = { COLOR_BLACK,   COLOR_WHITE, 0                       },
	[OTHER_LINE_COLOR]   = { -1,            -1,          -1                      },
	[JOB_LINE_COLOR]     = { COLOR_BLACK,   COLOR_WHITE, A_BOLD | A_REVERSE      },
	[SUGGEST_BOX_COLOR]  = { -1,            -1,          A_BOLD                  },
	[MISMATCH_COLOR]     = { COLOR_WHITE,   COLOR_RED,   A_BOLD                  },
	[UNMATCHED_COLOR]    = { COLOR_WHITE,   COLOR_GREEN, A_BOLD                  },
	[BLANK_COLOR]        = { -1,            -1,          -1                      },
	[AUX_WIN_COLOR]      = { -1,            -1,          -1                      },
	[TAB_LINE_COLOR]     = { COLOR_WHITE,   COLOR_BLACK, 0                       },
	[TAB_LINE_SEL_COLOR] = { -1,            -1,          A_BOLD | A_REVERSE      },
	[USER1_COLOR]        = { -1,            -1,          -1                      },
	[USER2_COLOR]        = { -1,            -1,          -1                      },
	[USER3_COLOR]        = { -1,            -1,          -1                      },
	[USER4_COLOR]        = { -1,            -1,          -1                      },
	[USER5_COLOR]        = { -1,            -1,          -1                      },
	[USER6_COLOR]        = { -1,            -1,          -1                      },
	[USER7_COLOR]        = { -1,            -1,          -1                      },
	[USER8_COLOR]        = { -1,            -1,          -1                      },
	[USER9_COLOR]        = { -1,            -1,          -1                      },
	[OTHER_WIN_COLOR]    = { -1,            -1,          -1                      },
	[LINE_NUM_COLOR]     = { -1,            -1,          -1                      },
	[ODD_LINE_COLOR]     = { -1,            -1,          -1                      },
};
ARRAY_GUARD(default_cs, MAXNUM_COLOR);

static char ** list_cs_files(int *len);
static void restore_primary_cs(const col_scheme_t *cs);
static void reset_to_default_cs(col_scheme_t *cs);
static void free_cs_highlights(col_scheme_t *cs);
static file_hi_t * clone_cs_highlights(const col_scheme_t *from);
static void reset_cs_colors(col_scheme_t *cs);
static int source_cs(const char name[]);
static void get_cs_path(const char name[], char buf[], size_t buf_size);
static const char * get_global_colors_dir(void);
static void check_cs(col_scheme_t *cs);
static void load_color_pairs(col_scheme_t *cs);
static void ensure_dir_map_exists(void);
static void overlap_cterm_colors(col_attr_t *color,
		const col_attr_t *admixture);
static void overlap_gui_colors(col_attr_t *color, const col_attr_t *admixture);
static col_attr_t convert_to_gui(const col_attr_t *color);
static int color_diff(int a, int b);

/* Mapping of color schemes associations onto file system tree. */
static fsddata_t *dir_map;

int
cs_have_no_extensions(void)
{
	int len;
	char **list = list_cs_files(&len);
	int i;

	/* Check if extensions are in use. */
	for(i = 0; i < len; ++i)
	{
		if(ends_with(list[i], ".vifm"))
		{
			break;
		}
	}

	free_string_array(list, len);
	return (len > 0 && i >= len);
}

void
cs_rename_all(void)
{
	DIR *dir;
	struct dirent *d;

	dir = os_opendir(cfg.colors_dir);
	if(dir == NULL)
	{
		return;
	}

	while((d = os_readdir(dir)) != NULL)
	{
#ifndef _WIN32
		unsigned char type;
#endif
		char full_old_path[PATH_MAX + 32 + NAME_MAX];
		char full_new_path[PATH_MAX + 32 + NAME_MAX];
		snprintf(full_old_path, sizeof(full_old_path), "%s/%s", cfg.colors_dir,
				d->d_name);
		snprintf(full_new_path, sizeof(full_new_path), "%s/%s.vifm", cfg.colors_dir,
				d->d_name);

#ifndef _WIN32
		type = get_dirent_type(d, full_old_path);
		if(type == DT_REG || (type == DT_UNKNOWN && is_regular_file(full_old_path)))
#else
		if(is_regular_file(full_old_path))
#endif
		{
			(void)os_rename(full_old_path, full_new_path);
		}
	}
	os_closedir(dir);
}

char **
cs_list(int *len)
{
	char **const list = list_cs_files(len);
	int i, j;
	int new_names;

	/* Check if extensions are in use. */
	new_names = 0;
	for(i = 0; i < *len; ++i)
	{
		if(ends_with(list[i], ".vifm"))
		{
			new_names = 1;
		}
	}

	/* Remove hidden files, maybe along with old style file names, maybe cut off
	 * extensions from new style file names. */
	j = 0;
	for(i = 0; i < *len; ++i)
	{
		if(list[i][0] == '.' || (new_names && !cut_suffix(list[i], ".vifm")))
		{
			free(list[i]);
		}
		else
		{
			list[j++] = list[i];
		}
	}
	*len = j;

	return list;
}

/* Lists names of all files in color schemes directories.  Allocates an array of
 * strings, which should be freed by the caller.  Always sets *len.  Returns
 * NULL on error. */
static char **
list_cs_files(int *len)
{
	char **list = NULL;
	*len = 0;

	list = list_regular_files(cfg.colors_dir, list, len);
	list = list_regular_files(get_global_colors_dir(), list, len);

	return list;
}

int
cs_exists(const char name[])
{
	char cs_path[PATH_MAX + 32];
	get_cs_path(name, cs_path, sizeof(cs_path));
	return is_regular_file(cs_path);
}

void
cs_write(void)
{
	FILE *fp;
	char def_cs_path[PATH_MAX + 32];
	size_t i;

	if(create_path(cfg.colors_dir, S_IRWXU) != 0)
	{
		/* Do nothing if local colors directory exists or we've failed to create
		 * it. */
		return;
	}

	snprintf(def_cs_path, sizeof(def_cs_path), "%s/Default.vifm", cfg.colors_dir);
	fp = os_fopen(def_cs_path, "w");
	if(fp == NULL)
	{
		return;
	}

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

	fprintf(fp, "\" Light versions of colors are also available (they set bold\n"
	            "\" attribute in terminals with less than 16 colors):\n");
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
	fprintf(fp, "\" italic (on unsupported systems becomes reverse)\n");
	fprintf(fp, "\" combine\n");
	fprintf(fp, "\" none\n\n");

	fprintf(fp, "\" Vifm supports 256 colors you can use color numbers 0-255\n");
	fprintf(fp, "\" (requires properly set up terminal: set your TERM environment variable\n");
	fprintf(fp, "\" (directly or using resources) to some color terminal name (e.g.\n");
	fprintf(fp, "\" xterm-256color) from /usr/lib/terminfo/; you can check current number\n");
	fprintf(fp, "\" of colors in your terminal with tput colors command)\n\n");

	fprintf(fp, "\" highlight group cterm=attrs ctermfg=foreground_color ctermbg=background_color\n\n");

	fprintf(fp, "highlight clear\n\n");

	for(i = 0U; i < ARRAY_LEN(default_cs); ++i)
	{
		char fg_buf[16], bg_buf[16];
		cs_color_to_str(default_cs[i].fg, sizeof(fg_buf), fg_buf, /*is_gui=*/0);
		cs_color_to_str(default_cs[i].bg, sizeof(bg_buf), bg_buf, /*is_gui=*/0);

		if(default_cs[i].attr == -1)
		{
			fprintf(fp, "highlight %s ctermfg=%s ctermbg=%s\n", HI_GROUPS[i], fg_buf,
					bg_buf);
		}
		else
		{
			fprintf(fp, "highlight %s cterm=%s ctermfg=%s ctermbg=%s\n", HI_GROUPS[i],
					cs_attrs_to_str(&default_cs[i], /*gui_part=*/0), fg_buf, bg_buf);
		}
	}

	fclose(fp);
}

void
cs_color_to_str(int color, size_t buf_len, char str_buf[], int is_gui)
{
	int color_limit = (is_gui ? 8 : ARRAY_LEN(XTERM256_COLOR_NAMES));

	if(color == -1)
	{
		copy_str(str_buf, buf_len, "default");
	}
	else if(color >= 0 && color < color_limit)
	{
		copy_str(str_buf, buf_len, XTERM256_COLOR_NAMES[color]);
	}
	else if(is_gui)
	{
		snprintf(str_buf, buf_len, "#%06x", color);
	}
	else
	{
		snprintf(str_buf, buf_len, "%d", color);
	}
}

void
cs_load_primary(const char name[])
{
	col_scheme_t prev_cs = {};

	if(!cs_exists(name))
	{
		show_error_msgf("Color Scheme", "No such color scheme: \"%s\"", name);
		return;
	}

	cs_assign(&prev_cs, &cfg.cs);
	curr_stats.cs = &cfg.cs;
	cfg.cs.state = CSS_LOADING;

	if(source_cs(name) != 0)
	{
		restore_primary_cs(&prev_cs);
		show_error_msgf("Color Scheme Sourcing",
				"An error occurred on sourcing color scheme: \"%s\"", name);
		return;
	}

	check_cs(&cfg.cs);
	if(cfg.cs.state == CSS_DEFAULTED)
	{
		restore_primary_cs(&prev_cs);
		show_error_msgf("Color Scheme Error",
				"\"%s\" color scheme is not supported by the terminal, restored \"%s\"",
				name, prev_cs.name);
		return;
	}

	copy_str(cfg.cs.name, sizeof(cfg.cs.name), name);
	update_attributes();

	free_cs_highlights(&prev_cs);
	cfg.cs.state = CSS_NORMAL;
}

void
cs_load_primary_list(char *names[], int count)
{
	col_scheme_t prev_cs = {};
	cs_assign(&prev_cs, &cfg.cs);

	while(count-- > 0)
	{
		const char *name = *names++;

		cs_assign(&cfg.cs, &prev_cs);
		curr_stats.cs = &cfg.cs;
		cfg.cs.state = CSS_LOADING;

		if(!cs_exists(name))
		{
			continue;
		}

		if(source_cs(name) != 0)
		{
			show_error_msgf("Color Scheme Sourcing",
					"An error occurred on sourcing color scheme: \"%s\"", name);
			continue;
		}

		check_cs(&cfg.cs);
		if(cfg.cs.state == CSS_DEFAULTED)
		{
			continue;
		}

		copy_str(cfg.cs.name, sizeof(cfg.cs.name), name);
		update_attributes();

		free_cs_highlights(&prev_cs);
		cfg.cs.state = CSS_NORMAL;

		return;
	}

	restore_primary_cs(&prev_cs);
}

/* Restore previous state of primary color scheme. */
static void
restore_primary_cs(const col_scheme_t *cs)
{
	free_cs_highlights(&cfg.cs);
	cfg.cs = *cs;
	cs_load_pairs();
	update_screen(UT_FULL);
}

void
cs_load_pairs(void)
{
	ensure_dir_map_exists();

	load_color_pairs(&cfg.cs);
	load_color_pairs(&lwin.cs);
	load_color_pairs(&rwin.cs);
}

void
cs_load_defaults(void)
{
	fsddata_free(dir_map);
	dir_map = NULL;

	lwin.local_cs = 0;
	rwin.local_cs = 0;

	reset_to_default_cs(&cfg.cs);
	reset_to_default_cs(&lwin.cs);
	reset_to_default_cs(&rwin.cs);

	load_color_pairs(&cfg.cs);
	load_color_pairs(&lwin.cs);
	load_color_pairs(&rwin.cs);
}

/* Completely resets the cs to builtin default color scheme.  Changes: colors,
 * name, state. */
static void
reset_to_default_cs(col_scheme_t *cs)
{
	reset_cs_colors(cs);

	copy_str(cs->name, sizeof(cs->name), DEF_CS_NAME);

	cs->state = CSS_NORMAL;
}

void
cs_reset(col_scheme_t *cs)
{
	reset_cs_colors(cs);
	free_cs_highlights(cs);
	load_color_pairs(cs);
}

void
cs_assign(col_scheme_t *to, const col_scheme_t *from)
{
	free_cs_highlights(to);
	*to = *from;
	to->file_hi = clone_cs_highlights(from);
}

/* Resets color scheme to default builtin values. */
static void
reset_cs_colors(col_scheme_t *cs)
{
	size_t i;
	for(i = 0U; i < ARRAY_LEN(default_cs); ++i)
	{
		cs->color[i] = default_cs[i];
	}
}

/* Frees data structures of the color scheme that are related to filename
 * specific highlight. */
static void
free_cs_highlights(col_scheme_t *cs)
{
	int i;

	for(i = 0; i < cs->file_hi_count; ++i)
	{
		matchers_free(cs->file_hi[i].matchers);
	}

	free(cs->file_hi);

	cs->file_hi = NULL;
	cs->file_hi_count = 0;
}

/* Clones filename specific highlight array of the *from color scheme and
 * returns it. */
static file_hi_t *
clone_cs_highlights(const col_scheme_t *from)
{
	int i;
	file_hi_t *const file_hi = reallocarray(NULL, from->file_hi_count + 1,
			sizeof(*file_hi));

	for(i = 0; i < from->file_hi_count; ++i)
	{
		const file_hi_t *const hi = &from->file_hi[i];
		file_hi[i].matchers = matchers_clone(hi->matchers);
		file_hi[i].hi = hi->hi;
	}

	return file_hi;
}

int
cs_load_local(int left, const char dir[])
{
	if(dir_map == NULL || curr_stats.cs != &cfg.cs)
	{
		/* Either have nothing to do, or we're already doing it (sourcing a file can
		 * trigger view redraws). */
		return 0;
	}

	curr_stats.cs = left ? &lwin.cs : &rwin.cs;
	cs_assign(curr_stats.cs, &cfg.cs);

	/* TODO: maybe use split_and_get() here as in io/iop:iop_mkdir(). */
	char *const dir_copy = strdup(dir);
	if(dir_copy == NULL)
	{
		return 0;
	}

	char *p = dir_copy;
	int altered = 0;
	char t;
	do
	{
		void *name;

		t = *p;
		*p = '\0';

		if(fsddata_get(dir_map, dir_copy, &name) == 0 && cs_exists(name))
		{
			(void)source_cs(name);
			altered = 1;
		}

		*p = t;
		p = (t == '\0' ? NULL : until_first(p + 1, '/'));
	}
	while(t != '\0');
	free(dir_copy);

	curr_stats.cs = &cfg.cs;

	if(!altered)
	{
		return 0;
	}

	check_cs(curr_stats.cs);
	load_color_pairs(curr_stats.cs);

	return 1;
}

/* Sources color scheme file.  Returns non-zero on error (e.g. file doesn't
 * exist), and zero otherwise. */
static int
source_cs(const char name[])
{
	char cs_path[PATH_MAX + 1];
	get_cs_path(name, cs_path, sizeof(cs_path));
	return cfg_source_file(cs_path);
}

/* Fill the buffer of specified size with path to color scheme, which might not
 * exist. */
static void
get_cs_path(const char name[], char buf[], size_t buf_size)
{
	snprintf(buf, buf_size, "%s/%s.vifm", cfg.colors_dir, name);
	if(is_regular_file(buf))
	{
		return;
	}

	(void)cut_suffix(buf, ".vifm");
	if(is_regular_file(buf))
	{
		return;
	}

	snprintf(buf, buf_size, "%s/%s.vifm", get_global_colors_dir(),
			name);
	if(is_regular_file(buf))
	{
		return;
	}

	(void)cut_suffix(buf, ".vifm");
}

/* Retrieves path to global directory containing color schemes.  Returns the
 * path. */
static const char *
get_global_colors_dir(void)
{
	static char dir_path[PATH_MAX + 1];
	if(dir_path[0] == '\0')
	{
		snprintf(dir_path, sizeof(dir_path), "%s/colors", get_sys_conf_dir());
	}
	return dir_path;
}

/* Checks whether colorscheme is in unusable state and resets it to normal
 * state. */
static void
check_cs(col_scheme_t *cs)
{
	if(cs->state == CSS_BROKEN)
	{
		reset_cs_colors(cs);
		cs->state = CSS_DEFAULTED;
	}
}

/* Loads color scheme settings into color pairs. */
static void
load_color_pairs(col_scheme_t *cs)
{
	int i;
	for(i = 0; i < MAXNUM_COLOR; ++i)
	{
		cs->pair[i] = cs_load_color(&cs->color[i]);
	}
}

void
cs_complete(const char name[])
{
	int i;
	size_t len;
	int schemes_len;
	char **schemes;

	len = strlen(name);
	schemes = cs_list(&schemes_len);

	for(i = 0; i < schemes_len; ++i)
	{
		if(schemes[i][0] != '.' || name[0] == '.')
		{
			if(strnoscmp(name, schemes[i], len) == 0)
			{
				vle_compl_add_match(schemes[i], "");
			}
		}
	}

	free_string_array(schemes, schemes_len);

	vle_compl_finish_group();
	vle_compl_add_last_match(name);
}

const char *
cs_attrs_to_str(const col_attr_t *color, int gui_part)
{
	static char result[64];

	const int attrs = (gui_part ? color->gui_attr : color->attr);
	const int combine =
		(gui_part ? color->combine_gui_attrs : color->combine_attrs);

	if((attrs == 0 || attrs == -1) && !combine)
	{
		return strcpy(result, "none");
	}

	result[0] = '\0';
	if((attrs & A_BOLD) == A_BOLD)
		strcat(result, "bold,");
	if((attrs & A_UNDERLINE) == A_UNDERLINE)
		strcat(result, "underline,");
	if((attrs & A_REVERSE) == A_REVERSE)
		strcat(result, "reverse,");
	if((attrs & A_STANDOUT) == A_STANDOUT)
		strcat(result, "standout,");
#ifdef HAVE_A_ITALIC_DECL
	if((attrs & A_ITALIC) == A_ITALIC)
		strcat(result, "italic,");
#endif
	if(combine)
		strcat(result, "combine,");
	if(result[0] != '\0')
		result[strlen(result) - 1] = '\0';
	return result;
}

void
cs_assoc_dir(const char name[], const char dir[])
{
	char *const copy = strdup(name);

	ensure_dir_map_exists();

	if(fsddata_set(dir_map, dir, copy) != 0)
	{
		free(copy);
	}
}

/* Makes sure that dir_map variable is initialized. */
static void
ensure_dir_map_exists(void)
{
	if(dir_map == NULL)
	{
		dir_map = fsddata_create(1, 1);
	}
}

void
cs_mix_colors(col_attr_t *color, const col_attr_t *admixture)
{
	if(curr_stats.direct_color)
	{
		*color = convert_to_gui(color);
	}

	const int attr = color->attr;
	const int gui_attr = color->gui_attr;

	cs_overlap_colors(color, admixture);

	if(attr != -1 && admixture->combine_attrs)
	{
		color->attr |= attr;
	}

	if(gui_attr != -1 && admixture->combine_gui_attrs)
	{
		color->gui_attr |= gui_attr;
	}
}

void
cs_overlap_colors(col_attr_t *color, const col_attr_t *admixture)
{
	if(curr_stats.direct_color)
	{
		*color = convert_to_gui(color);
		const col_attr_t gui_admixture = convert_to_gui(admixture);
		overlap_gui_colors(color, &gui_admixture);
	}
	else
	{
		overlap_cterm_colors(color, admixture);
	}
}

/* Overlaps cterm colors of *admixture into the *color.  Non-transparent
 * properties of *admixture are transferred onto *color.  Attributes are never
 * combined. */
static void
overlap_cterm_colors(col_attr_t *color, const col_attr_t *admixture)
{
	if(admixture->fg != -1)
	{
		color->fg = admixture->fg;
	}

	if(admixture->bg != -1)
	{
		color->bg = admixture->bg;
	}

	if(admixture->attr != -1)
	{
		color->attr = admixture->attr;
	}
}

/* Overlaps gui colors of *admixture into the *color.  Non-transparent
 * properties of *admixture are transferred onto *color.  Attributes are never
 * combined. */
static void
overlap_gui_colors(col_attr_t *color, const col_attr_t *admixture)
{
	if(admixture->gui_fg != -1)
	{
		color->gui_fg = admixture->gui_fg;
	}

	if(admixture->gui_bg != -1)
	{
		color->gui_bg = admixture->gui_bg;
	}

	if(admixture->gui_attr != -1)
	{
		color->gui_attr = admixture->gui_attr;
	}
}

void
cs_add_file_hi(struct matchers_t *matchers, const col_attr_t *hi)
{
	void *p;
	file_hi_t *file_hi;
	col_scheme_t *const cs = curr_stats.cs;

	/* Try to find existing record that exactly matches the expression and update
	 * the record. */
	int i;
	const char *expr = matchers_get_expr(matchers);
	for(i = 0; i < cs->file_hi_count; ++i)
	{
		if(strcmp(matchers_get_expr(cs->file_hi[i].matchers), expr) == 0)
		{
			matchers_free(matchers);
			cs->file_hi[i].hi = *hi;
			return;
		}
	}

	p = reallocarray(cs->file_hi, cs->file_hi_count + 1, sizeof(*cs->file_hi));
	if(p == NULL)
	{
		matchers_free(matchers);
		show_error_msg("Color Scheme File Highlight", "Not enough memory");
		return;
	}
	cs->file_hi = p;

	file_hi = &cs->file_hi[cs->file_hi_count];

	file_hi->matchers = matchers;
	file_hi->hi = *hi;

	++cs->file_hi_count;
}

const col_attr_t *
cs_get_file_hi(const col_scheme_t *cs, const char fname[], int *hi_hint)
{
	if(*hi_hint == INT_MAX)
	{
		return NULL;
	}
	if(*hi_hint != -1)
	{
		assert(*hi_hint >= 0 && "Wrong index.");
		assert(*hi_hint < cs->file_hi_count && "Wrong index.");
		return &cs->file_hi[*hi_hint].hi;
	}

	int i;
	for(i = 0; i < cs->file_hi_count; ++i)
	{
		const file_hi_t *const file_hi = &cs->file_hi[i];
		if(matchers_match(file_hi->matchers, fname))
		{
			*hi_hint = i;
			return &file_hi->hi;
		}
	}

	*hi_hint = INT_MAX;
	return NULL;
}

int
cs_del_file_hi(const char matchers_expr[])
{
	col_scheme_t *const cs = curr_stats.cs;

	int i;
	for(i = 0; i < cs->file_hi_count; ++i)
	{
		if(strcmp(matchers_get_expr(cs->file_hi[i].matchers), matchers_expr) == 0)
		{
			matchers_free(cs->file_hi[i].matchers);
			memmove(&cs->file_hi[i], &cs->file_hi[i + 1],
					sizeof(*cs->file_hi)*((cs->file_hi_count - 1) - i));
			--cs->file_hi_count;
			return 1;
		}
	}

	return 0;
}

int
cs_is_color_set(const col_attr_t *color)
{
	return color->fg != -1 || color->bg != -1 || color->attr != -1
	    || color->gui_set;
}

int
cs_load_color(const col_attr_t *color)
{
	if(curr_stats.direct_color)
	{
		if(color->gui_set)
		{
			return colmgr_get_pair(color->gui_fg, color->gui_bg);
		}

		const col_attr_t gui = convert_to_gui(color);
		return colmgr_get_pair(gui.gui_fg, gui.gui_bg);
	}

	return colmgr_get_pair(color->fg, color->bg);
}

/* Converts cterm colors to a corresponding gui direct colors if necessary.
 * Returns direct colors with cterm part erased. */
static col_attr_t
convert_to_gui(const col_attr_t *color)
{
	if(!curr_stats.direct_color || color->gui_set)
	{
		return *color;
	}

	col_attr_t gui = {
		.gui_fg = -1,
		.gui_bg = -1,
		.gui_attr = color->attr,
		.combine_gui_attrs = color->combine_attrs,
		.gui_set = 1,
	};

	if(color->fg >= -1 && 1 + color->fg < (int)ARRAY_LEN(XTERM256_PALETTE))
	{
		gui.gui_fg = XTERM256_PALETTE[1 + color->fg];
	}
	if(color->bg >= -1 && 1 + color->bg < (int)ARRAY_LEN(XTERM256_PALETTE))
	{
		gui.gui_bg = XTERM256_PALETTE[1 + color->bg];
	}

	return gui;
}

int
cs_color_get_attr(const col_attr_t *color)
{
	return (curr_stats.direct_color && color->gui_set) ? color->gui_attr
	                                                   : color->attr;
}

cchar_t
cs_color_to_cchar(const col_attr_t *color, int pair)
{
	if(pair < 0)
	{
		pair = cs_load_color(color);
	}

	cchar_t cch;
	setcchar(&cch, L" ", cs_color_get_attr(color), pair, NULL);
	return cch;
}

void
cs_color_enable_gui(col_attr_t *color)
{
	if(!color->gui_set)
	{
		color->gui_set = 1;
		color->gui_fg = -1;
		color->gui_bg = -1;
		color->gui_attr = 0;
		color->combine_gui_attrs = 0;
	}
}

int
cs_downscale_color(int direct_color)
{
	int i;
	int closest = 0;
	int diff = color_diff(XTERM256_PALETTE[1], direct_color);
	int color_limit = MIN(COLORS, 256);
	for(i = 1; i < color_limit; ++i)
	{
		int d = color_diff(XTERM256_PALETTE[1 + i], direct_color);
		if(d < diff)
		{
			diff = d;
			closest = i;
		}
	}
	return closest;
}

/* Computes distance between two RGB colors.  Returns the distance. */
static int
color_diff(int a, int b)
{
	int rd = abs((a >> 16) - (b >> 16));
	int gd = abs(((a >> 8)&0xff) - ((b >> 8)&0xff));
	int bd = abs((a&0xff) - (b&0xff));
	return rd + gd + bd;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
