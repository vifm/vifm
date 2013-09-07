/* vifm
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

#ifdef _WIN32
#include <windows.h>
#endif

#include <curses.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <ctype.h>
#include <errno.h> /* ENOENT */
#include <locale.h> /* setlocale() */
#include <stdio.h> /* snprintf() */
#include <stdlib.h>
#include <string.h> /* strcpy() */

#include "utils/fs_limits.h"
#include "utils/macros.h"
#include "color_scheme.h"

#define MAX_LEN 1024

/* Maximum number of color scheme files to convert supported by the
 * implementation. */
#define MAX_COLOR_SCHEMES 8

enum
{
	VIFMINFO_OPTIONS   = 1 << 0,
	VIFMINFO_FILETYPES = 1 << 1,
	VIFMINFO_COMMANDS  = 1 << 2,
	VIFMINFO_BOOKMARKS = 1 << 3,
	VIFMINFO_TUI       = 1 << 4,
	VIFMINFO_DHISTORY  = 1 << 5,
	VIFMINFO_STATE     = 1 << 6,
	VIFMINFO_CS        = 1 << 7,
	VIFMINFO_SAVEDIRS  = 1 << 8,
};

static int vifminfo = 0x1ff;

static struct
{
	char *vicmd;     /* VI_COMMAND */
	int trash;       /* USE_TRASH */
	int history;     /* HISTORY_LENGTH */
	int followlinks; /* FOLLOW_LINKS */
	int fastrun;     /* FAST_RUN */
	int sortnumbers; /* SORT_NUMBERS */
	int undolevels;  /* UNDO_LEVELS */
	int vimhelp;     /* USE_VIM_HELP */
	int runexec;     /* RUN_EXECUTABLE */
	int iec;         /* USE_IEC_PREFIXES */
	char *fusehome;  /* FUSE_HOME */
	char *timefmt;   /* TIME_STAMP_FORMAT */
}options = {
	.vicmd = "vim",
	.trash = 1,
	.history = 15,
	.followlinks = 1,
	.fastrun = 0,
	.sortnumbers = 0,
	.undolevels = 100,
	.vimhelp = 0,
	.runexec = 0,
	.iec = 0,
	.fusehome = "/tmp/vifm_FUSE",
	.timefmt = " %m/%d %H:%M"
};

static char *color_scheme = "Default"; /* COLOR_SCHEME */

typedef struct
{
	char *description;
	char *extensions;
	char *programs;
	char *viewer;
}filetype;

static filetype *filetypes;
static int nfiletypes;

typedef struct
{
	char *name;
	char *cmd;
}command;

static command *commands;
static int ncommands;

typedef struct
{
	char name;
	char *dir;
	char *file;
}bookmark;

static bookmark *bookmarks;
static int nbookmarks;

static struct
{
	int windows;   /* vnumber of windows */
	int lwin_sort; /* lleft window sort */
	int rwin_sort; /* rright window sort */
}tui = {
	.windows = 2,
	.lwin_sort = 2, /* by name */
	.rwin_sort = 2, /* by name */
};

static char *lwin_dir; /* dleft window directory */
static char *rwin_dir; /* Dright window directory */

static struct
{
	char *lwin_filter; /* fleft window filter */
	int lwin_inverted; /* ileft window filter inverted */
	char *rwin_filter; /* Fright window filter */
	int rwin_inverted; /* Iright window filter inverted */
	int use_screen;    /* suse screen program */
}state = {
	.lwin_filter = NULL,
	.lwin_inverted = 1,
	.rwin_filter = NULL,
	.rwin_inverted = 1,
	.use_screen = 0,
};

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

static struct
{
	int count;
	col_scheme_t array[MAX_COLOR_SCHEMES];
}cs = {
	.count = 0,
};

#ifdef _WIN32
static int is_dir(const char *file);
#endif
static void add_command(const char *name, const char *cmd);
static const char * conv_udf_name(const char *cmd);
static void add_bookmark(char name, const char *dir, const char *file);
static void add_filetype(const char *desc, const char *exts, const char *viewer,
		const char *programs);
static void write_vifmrc(const char *config_file, int comment_out);
static void append_vifmrc(const char *config_file, int comment_out);
static void append_startup(const char *config_file, const char *startup_file);
static void append_vifminfo_option(const char *config_file, int vifm_like);
static void write_vifminfo(const char *config_file, int vifm_like);
static void convert_color_schemes(const char *colorschemes_file,
		const char *colors_dir);
static void read_color_scheme_file(const char *config_file);
static void load_default_colors(void);
static void init_color_scheme(col_scheme_t *cs);
static void add_color(char s1[], char s2[], char s3[]);
static int colname2int(char col[]);
static void write_color_schemes(const char *colors_dir);
static const char * conv_attrs_to_str(int attrs);

static void
chomp(char *text)
{
	size_t len;

	if(text[0] == '\0')
		return;

	len = strlen(text);
	if(text[len - 1] == '\n')
		text[len - 1] = '\0';
}

/* Returns zero on error */
static int
read_config_file(const char *config_file)
{
	FILE *fp;
	char line[4096];
	char *s1 = NULL;
	char *s2 = NULL;
	char *s3 = NULL;
	char *s4 = NULL;
	char *sx = NULL;

	if((fp = fopen(config_file, "r")) == NULL)
		return 0;

	while(fgets(line, sizeof(line), fp))
	{
		int args;

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
		/* COMMAND is handled here so that the command can have an '=' */
		if(!strcmp(line, "COMMAND"))
			add_command(s1, s2);
		else
		{
			if(args == 2 && (sx = s3 = strchr(s2, '=')) != NULL)
			{
				s3++;
				chomp(s3);
				*sx = '\0';
				args = 3;
			}
			if(args == 3 && (sx = s4 = strchr(s3, '=')) != NULL)
			{
				s4++;
				chomp(s4);
				*sx = '\0';
				args = 4;
			}
		}
		if(args == 1)
		{
			if(!strcmp(line, "VI_COMMAND"))
			{
				options.vicmd = strdup(s1);
				continue;
			}
			if(!strcmp(line, "USE_TRASH"))
			{
				options.trash = atoi(s1);
				continue;
			}
			if(!strcmp(line, "UNDO_LEVELS"))
			{
				options.undolevels = atoi(s1);
				continue;
			}
			if(!strcmp(line, "USE_ONE_WINDOW"))
			{
				tui.windows = atoi(s1) ? 1 : 2;
				continue;
			}
			if(!strcmp(line, "USE_SCREEN"))
			{
				state.use_screen = atoi(s1);
				continue;
			}
			if(!strcmp(line, "HISTORY_LENGTH"))
			{
				options.history = atoi(s1);
				continue;
			}
			if(!strcmp(line, "FAST_RUN"))
			{
				options.fastrun = atoi(s1);
				continue;
			}
			if(!strcmp(line, "SORT_NUMBERS"))
			{
				options.sortnumbers = atoi(s1);
				continue;
			}
			if(!strcmp(line, "LEFT_WINDOW_SORT_TYPE"))
			{
				tui.lwin_sort = atoi(s1);
				continue;
			}
			if(!strcmp(line, "RIGHT_WINDOW_SORT_TYPE"))
			{
				tui.rwin_sort = atoi(s1);
				continue;
			}
			if(!strcmp(line, "LWIN_FILTER"))
			{
				state.lwin_filter = strdup(s1);
				continue;
			}
			if(!strcmp(line, "LWIN_INVERT"))
			{
				state.lwin_inverted = atoi(s1);
				continue;
			}
			if(!strcmp(line, "RWIN_FILTER"))
			{
				state.rwin_filter = strdup(s1);
				continue;
			}
			if(!strcmp(line, "RWIN_INVERT"))
			{
				state.rwin_inverted = atoi(s1);
				continue;
			}
			if(!strcmp(line, "USE_VIM_HELP"))
			{
				options.vimhelp = atoi(s1);
				continue;
			}
			if(!strcmp(line, "SAVE_LOCATION"))
			{
				if(!atoi(s1))
				{
					vifminfo &= ~VIFMINFO_DHISTORY;
					vifminfo &= ~VIFMINFO_SAVEDIRS;
				}
				else
				{
					vifminfo |= VIFMINFO_DHISTORY;
					vifminfo |= VIFMINFO_SAVEDIRS;
				}
				continue;
			}
			if(!strcmp(line, "LWIN_PATH"))
			{
				lwin_dir = strdup(s1);
				continue;
			}
			if(!strcmp(line, "RWIN_PATH"))
			{
				rwin_dir = strdup(s1);
				continue;
			}
			if(!strcmp(line, "FOLLOW_LINKS"))
			{
				options.followlinks = atoi(s1);
				continue;
			}
			if(!strcmp(line, "RUN_EXECUTABLE"))
			{
				options.runexec = atoi(s1);
				continue;
			}
			if(!strcmp(line, "USE_IEC_PREFIXES"))
			{
				options.iec = atoi(s1);
				continue;
			}
			if(!strcmp(line, "COLOR_SCHEME"))
			{
				color_scheme = strdup(s1);
				continue;
			}
			if(!strcmp(line, "TIME_STAMP_FORMAT"))
			{
				options.timefmt = strdup(s1);
				continue;
			}
			if(!strcmp(line, "FUSE_HOME"))
			{
				options.fusehome = strdup(s1);
				continue;
			}
		}
		if(args == 3)
		{
			if(!strcmp(line, "BOOKMARKS"))
			{
				if(isalnum(*s1))
					add_bookmark(*s1, s2, s3);
				continue;
			}
			if(!strcmp(line, "FILETYPE")) /* backward compatibility */
			{
				add_filetype(s1, s2, "", s3);
				continue;
			}
		}
		if(args == 4)
		{
			if(!strcmp(line, "FILETYPE"))
			{
				add_filetype(s1, s2, s3, s4);
				continue;
			}
		}
	}

	fclose(fp);

	return 1;
}

#ifdef _WIN32
static void
change_slashes(char *str)
{
	int i;
	for(i = 0; str[i] != '\0'; i++)
	{
		if(str[i] == '/')
			str[i] = '\\';
	}
}
#endif

int
main(int argc, char **argv)
{
	char *home_dir;
	char dir_name[6] = ".vifm";
	char config_dir[PATH_MAX];
	char config_file[PATH_MAX], config_file_bak[PATH_MAX];
	char startup_file[PATH_MAX];
	char vifminfo_file[PATH_MAX], vifminfo_file_bak[PATH_MAX];
	char colorschemes_file[PATH_MAX], colors_dir[PATH_MAX];
	int vifm_like;
	char *vifmrc_arg, *vifminfo_arg;
	int err;

	(void)setlocale(LC_ALL, "");

	if(argc != 2 && argc != 4)
	{
		puts("Usage: vifmrc-converter 0|1|2 [vifmrc_file vifminfo_file]\n\n"
				"1 means comment commands in vifmrc and put more things to vifminfo\n"
				"2 means convert colorscheme file only");
		return 1;
	}
	if(argv[1][1] != '\0' || (argv[1][0] != '0' && argv[1][0] != '1' &&
			argv[1][0] != '2'))
	{
		puts("Usage: vifmrc-converter 0|1|2 [vifmrc_file vifminfo_file]\n\n"
				"1 means comment commands in vifmrc and put more things to vifminfo\n"
				"2 means convert colorscheme file only");
		return 1;
	}

	vifm_like = atoi(argv[1]);

	home_dir = getenv("HOME");
#ifdef _WIN32
	snprintf(config_dir, sizeof(config_dir), "%s/Vifm", getenv("APPDATA"));
	if(home_dir == NULL || home_dir[0] == '\0' || is_dir(config_dir))
	{
		home_dir = getenv("APPDATA");
		strcpy(dir_name, "Vifm");
	}
#endif
	if(home_dir == NULL || home_dir[0] == '\0')
	{
		puts("Can't find configuration directory");
		return 1;
	}

	snprintf(config_dir, sizeof(config_dir), "%s/%s", home_dir, dir_name);
	snprintf(config_file, sizeof(config_file), "%s/vifmrc", config_dir);
	snprintf(config_file_bak, sizeof(config_file_bak), "%s/vifmrc.bak",
			config_dir);
	snprintf(startup_file, sizeof(startup_file), "%s/startup", config_dir);
	snprintf(vifminfo_file, sizeof(startup_file), "%s/vifminfo", config_dir);
	snprintf(vifminfo_file_bak, sizeof(vifminfo_file_bak), "%s/vifminfo.bak",
			config_dir);
	snprintf(colorschemes_file, sizeof(colorschemes_file), "%s/colorschemes",
			config_dir);
	snprintf(colors_dir, sizeof(colors_dir), "%s/colors", config_dir);
#ifdef _WIN32
	change_slashes(config_dir);
	change_slashes(config_file);
	change_slashes(config_file_bak);
	change_slashes(startup_file);
	change_slashes(vifminfo_file);
	change_slashes(vifminfo_file_bak);
	change_slashes(colorschemes_file);
	change_slashes(colors_dir);
#endif

	if(vifm_like == 2)
	{
		convert_color_schemes(colorschemes_file, colors_dir);
		return 0;
	}

	if(argc == 4)
	{
		vifmrc_arg = argv[2];
		vifminfo_arg = argv[3];
	}
	else
	{
		vifmrc_arg = config_file;
		vifminfo_arg = vifminfo_file;
	}

	if(!vifm_like)
		vifminfo = VIFMINFO_BOOKMARKS;

	read_config_file(config_file);
	read_config_file(vifminfo_file);

	(void)unlink(config_file_bak);
	err = rename(config_file, config_file_bak);
	if(err != 0 && err != ENOENT)
	{
		fprintf(stderr, "Can't move vifmrc file to make a backup copy "
				"(from \"%s\" to \"%s\")\n", config_file, config_file_bak);
		exit(1);
	}

	(void)unlink(vifminfo_file_bak);
	err = rename(vifminfo_file, vifminfo_file_bak);
	if(err != 0 && err != ENOENT)
	{
		fprintf(stderr, "Can't move vifminfo file to make a backup copy "
				"(from \"%s\" to \"%s\")\n", vifminfo_file, vifminfo_file_bak);
		exit(1);
	}

	write_vifmrc(vifmrc_arg, vifm_like);
	append_vifmrc(vifmrc_arg, vifm_like);
	append_vifminfo_option(vifmrc_arg, vifm_like);
	append_startup(vifmrc_arg, startup_file);

	write_vifminfo(vifminfo_arg, vifm_like);

	convert_color_schemes(colorschemes_file, colors_dir);

	return 0;
}

#ifdef _WIN32
static int
is_dir(const char *file)
{
	DWORD attr;

	attr = GetFileAttributesA(file);
	if(attr == INVALID_FILE_ATTRIBUTES)
		return 0;

	return (attr & FILE_ATTRIBUTE_DIRECTORY);
}
#endif

static void
add_command(const char *name, const char *cmd)
{
	commands = realloc(commands, sizeof(command)*(ncommands + 1));
	commands[ncommands].name = strdup(conv_udf_name(name));
	commands[ncommands].cmd = strdup(cmd);
	ncommands++;
}

static const char *
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
add_bookmark(char name, const char *dir, const char *file)
{
	bookmarks = realloc(bookmarks, sizeof(bookmark)*(nbookmarks + 1));
	bookmarks[nbookmarks].name = name;
	bookmarks[nbookmarks].dir = strdup(dir);
	bookmarks[nbookmarks].file = strdup(file);
	nbookmarks++;
}

static void
add_filetype(const char *desc, const char *exts, const char *viewer,
		const char *programs)
{
	char *p, *ex_copy, *free_this, *exptr2;
	size_t len;

	filetypes = realloc(filetypes, sizeof(filetype)*(nfiletypes + 1));
	filetypes[nfiletypes].description = strdup(desc);
	filetypes[nfiletypes].viewer = strdup(viewer);
	filetypes[nfiletypes].programs = strdup(programs);

	p = NULL;
	len = 0;
	ex_copy = strdup(exts);
	free_this = ex_copy;
	exptr2 = NULL;
	while(*ex_copy != '\0')
	{
		size_t new_len;
		if((exptr2 = strchr(ex_copy, ',')) == NULL)
			exptr2 = ex_copy + strlen(ex_copy);
		else
			*exptr2++ = '\0';

		new_len = len + 2 + strlen(ex_copy);
		if(len > 0)
			new_len += 1;
		p = realloc(p, new_len + 1);
		if(len > 0)
			strcpy(p + len, ",*.");
		else
			strcpy(p + len, "*.");
		strcat(p + len, ex_copy);
		len = new_len;

		ex_copy = exptr2;
	}
	free(free_this);
	filetypes[nfiletypes].extensions = p;

	nfiletypes++;
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

static void
write_vifmrc(const char *config_file, int comment_out)
{
	FILE *fp;
	const char *comment = comment_out ? "\" " : "";

	if((fp = fopen(config_file, "w")) == NULL)
	{
		fprintf(stderr, "Can't open configuration file \"%s\" for writing\n",
				config_file);
		exit(1);
	}

	fprintf(fp, "\" vim: filetype=vifm :\n");
	fprintf(fp, "\" You can edit this file by hand.\n");
	fprintf(fp, "\" The \" character at the beginning of a line comments out the line.\n");
	fprintf(fp, "\" Blank lines are ignored.\n");
	fprintf(fp, "\" The basic format for each item is shown with an example.\n");

	fprintf(fp, "\n\" This is the actual command used to start vi.  The default is vim.\n");
	fprintf(fp, "\" If you would like to use another vi clone such Elvis or Vile\n");
	fprintf(fp, "\" you will need to change this setting.\n");
	fprintf(fp, "\n%sset vicmd=%s", comment, escape_spaces(options.vicmd));
	fprintf(fp, "\n\" set vicmd=elvis\\ -G\\ termcap");
	fprintf(fp, "\n\" set vicmd=vile");
	fprintf(fp, "\n");
	fprintf(fp, "\n\" Trash Directory\n");
	fprintf(fp, "\" The default is to move files that are deleted with dd or :d to\n");
	fprintf(fp, "\" the trash directory.  If you change this you will not be able to move\n");
	fprintf(fp, "\" files by deleting them and then using p to put the file in the new location.\n");
	fprintf(fp, "\" I recommend not changing this until you are familiar with vifm.\n");
	fprintf(fp, "\" This probably shouldn't be an option.\n");
	fprintf(fp, "\n%sset %strash\n", comment, options.trash ? "" : "no");

	fprintf(fp, "\n\" This is how many directories to store in the directory history.\n");
	fprintf(fp, "\n%sset history=%d\n", comment, options.history);

	fprintf(fp, "\n\" Follow links on l or Enter.\n");
	fprintf(fp, "\n%sset %sfollowlinks\n", comment, options.followlinks ? "" : "no");

	fprintf(fp, "\n\" With this option turned on you can run partially entered commands with");
	fprintf(fp, "\n\" unambiguous beginning using :! (e.g. :!Te instead of :!Terminal or :!Te<tab>).\n");
	fprintf(fp, "\n%sset %sfastrun\n", comment, options.fastrun ? "" : "no");

	fprintf(fp, "\n\" Natural sort of (version) numbers within text.\n");
	fprintf(fp, "\n%sset %ssortnumbers\n", comment, options.sortnumbers ? "" : "no");

	fprintf(fp, "\n\" Maximum number of changes that can be undone.\n");
	fprintf(fp, "\n%sset undolevels=%d\n", comment, options.undolevels);

	fprintf(fp, "\n\" If you installed the vim.txt help file set vimhelp.\n");
	fprintf(fp, "\" If would rather use a plain text help file set novimhelp.\n");
	fprintf(fp, "\n%sset %svimhelp\n", comment, options.vimhelp ? "" : "no");

	fprintf(fp, "\n\" If you would like to run an executable file when you\n");
	fprintf(fp, "\" press return on the file name set this.\n");
	fprintf(fp, "\n%sset %srunexec\n", comment, options.runexec ? "" : "no");

	fprintf(fp, "\n\" Use KiB, MiB, ... instead of K, M, ...\n");
	fprintf(fp, "\n%sset %siec\n", comment, options.iec ? "" : "no");

	fprintf(fp, "\n\" Selected color scheme\n");
	fprintf(fp, "\n%scolorscheme %s\n", comment, escape_spaces(color_scheme));

	fprintf(fp, "\n");
	fprintf(fp, "\" The FUSE_HOME directory will be used as a root dir for all FUSE mounts.\n");
	fprintf(fp, "\" Unless it exists with write/exec permissions set, vifm will attempt to\n");
	fprintf(fp, "\" create it.\n");
	fprintf(fp, "\n%sset fusehome=%s\n", comment, escape_spaces(options.fusehome));

	fprintf(fp, "\n\" Format for displaying time in file list. For example:\n");
	fprintf(fp, "\" set timefmt=%%m/%%d-%%H:%%M\n");
	fprintf(fp, "\" See man date or man strftime for details.\n");
	fprintf(fp, "\n%sset timefmt=%s\n", comment, escape_spaces(options.timefmt));

	fclose(fp);
}

static void
append_vifmrc(const char *config_file, int comment_out)
{
	FILE *fp;
	int i;
	const char *comment = comment_out ? "\" " : "";

	if((fp = fopen(config_file, "a")) == NULL)
	{
		fprintf(stderr, "Can't open configuration file \"%s\" for appending\n",
				config_file);
		exit(1);
	}

	fprintf(fp, "\n\" :mark mark /full/directory/path [filename]\n\n");
	for(i = 0; i < nbookmarks; i++)
	{
		fprintf(fp, "\" mark %c %s", bookmarks[i].name,
				escape_spaces(bookmarks[i].dir));
		fprintf(fp, " %s\n", escape_spaces(bookmarks[i].file));
	}

	fprintf(fp, "\n\" :com[mand] command_name action\n");
	fprintf(fp, "\" The following macros can be used in a command\n");
	fprintf(fp, "\" %%a is replaced with the user arguments.\n");
	fprintf(fp, "\" %%c the current file under the cursor.\n");
	fprintf(fp, "\" %%C the current file under the cursor in the other directory.\n");
	fprintf(fp, "\" %%f the current selected file, or files.\n");
	fprintf(fp, "\" %%F the current selected file, or files in the other directory.\n");
  fprintf(fp, "\" %%b same as %%f %%F.\n");
	fprintf(fp, "\" %%d the current directory name.\n");
	fprintf(fp, "\" %%D the other window directory name.\n");
	fprintf(fp, "\" %%m run the command in a menu window\n\n");
	for(i = 0; i < ncommands; i++)
		fprintf(fp, "%scommand %s %s\n", comment, commands[i].name,
				commands[i].cmd);

	fprintf(fp, "\n\" The file type is for the default programs to be used with\n");
	fprintf(fp, "\" a file extension.\n");
	fprintf(fp, "\" :filetype pattern1,pattern2 defaultprogram,program2\n");
	fprintf(fp, "\" :fileviewer pattern1,pattern2 consoleviewer\n");
	fprintf(fp, "\" The other programs for the file type can be accessed with the :file command\n");
	fprintf(fp, "\" The command macros %%f, %%F, %%d, %%F may be used in the commands.\n");
	fprintf(fp, "\" The %%a macro is ignored.  To use a %% you must put %%%%.\n\n");
	for(i = 0; i < nfiletypes; i++)
	{
		fprintf(fp, "\" %s\n", filetypes[i].description);
		if(filetypes[i].programs[0] != '\0')
			fprintf(fp, "%sfiletype %s %s\n", comment, filetypes[i].extensions,
					filetypes[i].programs);
		if(filetypes[i].viewer[0] != '\0')
			fprintf(fp, "%sfileviewer %s %s\n", comment, filetypes[i].extensions,
					filetypes[i].viewer);
		fputs("\n", fp);
	}

	fprintf(fp, "\" For automated FUSE mounts, you must register an extension with FILETYPE=..\n");
	fprintf(fp, "\" in one of following formats:\n");
	fprintf(fp, "\"\n");
	fprintf(fp, "\" :filetype extensions FUSE_MOUNT|some_mount_command using %%SOURCE_FILE and %%DESTINATION_DIR variables\n");
	fprintf(fp, "\" %%SOURCE_FILE and %%DESTINATION_DIR are filled in by vifm at runtime.\n");
	fprintf(fp, "\" A sample line might look like this:\n");
	fprintf(fp, "\" :filetype *.zip,*.jar,*.war,*.ear FUSE_MOUNT|fuse-zip %%SOURCE_FILE %%DESTINATION_DIR\n");
	fprintf(fp, "\"\n");
	fprintf(fp, "\" :filetype extensions FUSE_MOUNT2|some_mount_command using %%PARAM and %%DESTINATION_DIR variables\n");
	fprintf(fp, "\" %%PARAM and %%DESTINATION_DIR are filled in by vifm at runtime.\n");
	fprintf(fp, "\" A sample line might look like this:\n");
	fprintf(fp, "\" :filetype *.ssh FUSE_MOUNT2|sshfs %%PARAM %%DESTINATION_DIR\n");
	fprintf(fp, "\" %%PARAM value is filled from the first line of file (whole line).\n");
	fprintf(fp, "\" Example first line for SshMount filetype: root@127.0.0.1:/\n");
	fprintf(fp, "\"\n");
	fprintf(fp, "\" You can also add %%CLEAR if you want to clear screen before running FUSE\n");
	fprintf(fp, "\" program.\n");

	if(!(vifminfo & VIFMINFO_TUI))
	{
		if(tui.windows != 2)
		{
			fprintf(fp, "\n\" One window view\n");
			fprintf(fp, "only\n");
		}
	}

	fclose(fp);
}

static void
append_startup(const char *config_file, const char *startup_file)
{
	FILE *cfp, *sfp;
	int c;

	if((cfp = fopen(config_file, "a")) == NULL)
	{
		fprintf(stderr, "Can't open configuration file \"%s\" for appending\n",
				config_file);
		exit(1);
	}

	if((sfp = fopen(startup_file, "r")) == NULL)
	{
		fclose(cfp);
		return;
	}

	fputs("\n\" Here is content of your startup file\n\n", cfp);

	while((c = fgetc(sfp)) != EOF)
		fputc(c, cfp);

	fclose(sfp);
	fclose(cfp);
}

static void
append_vifminfo_option(const char *config_file, int vifm_like)
{
	FILE *fp;

	if((fp = fopen(config_file, "a")) == NULL)
	{
		fprintf(stderr, "Can't open configuration file \"%s\" for appending\n",
				config_file);
		exit(1);
	}

	fputs("\n\" What should be saved automatically between vifm runs\n", fp);
	fputs("\" Like in previous versions of vifm\n", fp);

	if(vifminfo & VIFMINFO_DHISTORY)
		fprintf(fp, "%sset vifminfo=options,filetypes,commands,bookmarks,tui,"
				"dhistory,state,cs,savedirs,chistory,shistory,dirstack,registers,"
				"phistory\n", vifm_like ? "" : "\" ");
	else
		fprintf(fp, "%sset vifminfo=options,filetypes,commands,bookmarks,tui,"
				"state,cs,chistory,shistory,dirstack,registers,phistory\n",
				vifm_like ? "" : "\" ");
	fputs("\" Like in vi\n", fp);
	if(vifminfo & VIFMINFO_DHISTORY)
		fprintf(fp, "%sset vifminfo=bookmarks,dhistory,chistory,shistory,phistory,"
				"savedirs\n", !vifm_like ? "" : "\" ");
	else
		fprintf(fp, "%sset vifminfo=bookmarks,chistory,shistory,phistory\n",
				!vifm_like ? "" : "\" ");

	fclose(fp);
}

static void
write_vifminfo(const char *config_file, int vifm_like)
{
	FILE *fp;
	int i;

	if((fp = fopen(config_file, "w")) == NULL)
	{
		fprintf(stderr, "Can't open info file \"%s\" for writing\n", config_file);
		exit(1);
	}

	fprintf(fp, "# You can edit this file by hand, but it's recommended not to do that.\n");

	if(vifminfo & VIFMINFO_OPTIONS)
	{
		fputs("\n# Options:\n", fp);
		fprintf(fp, "=vicmd %s\n", escape_spaces(options.vicmd));
		fprintf(fp, "=%strash\n", options.trash ? "" : "no");
		fprintf(fp, "=history=%d\n", options.history);
		fprintf(fp, "=%sfollowlinks\n", options.followlinks ? "" : "no");
		fprintf(fp, "=%sfastrun\n", options.fastrun ? "" : "no");
		fprintf(fp, "=%ssortnumbers\n", options.sortnumbers ? "" : "no");
		fprintf(fp, "=undolevels=%d\n", options.undolevels);
		fprintf(fp, "=%svimhelp\n", options.vimhelp ? "" : "no");
		fprintf(fp, "=%srunexec\n", options.runexec ? "" : "no");
		fprintf(fp, "=%siec\n", options.iec ? "" : "no");
		fprintf(fp, "=fusehome=%s\n", escape_spaces(options.fusehome));
		fprintf(fp, "=timefmt=%s\n", escape_spaces(options.timefmt));
	}

	if(vifminfo & VIFMINFO_FILETYPES)
	{
		fputs("\n# Filetypes:\n", fp);
		for(i = 0; i < nfiletypes; i++)
		{
			if(filetypes[i].programs[0] != '\0')
				fprintf(fp, ".%s\n\t%s\n", filetypes[i].extensions,
						filetypes[i].programs);
		}

		fputs("\n# Fileviewers:\n", fp);
		for(i = 0; i < nfiletypes; i++)
		{
			if(filetypes[i].viewer[0] != '\0')
				fprintf(fp, ",%s\n\t%s\n", filetypes[i].extensions,
						filetypes[i].viewer);
		}
	}

	if(vifminfo & VIFMINFO_COMMANDS)
	{
		fputs("\n# Commands:\n", fp);
		for(i = 0; i < ncommands; i++)
			fprintf(fp, "!%s\n\t%s\n", commands[i].name, commands[i].cmd);
	}

	if(vifminfo & VIFMINFO_BOOKMARKS)
	{
		fputs("\n# Bookmarks:\n", fp);
		for(i = 0; i < nbookmarks; i++)
		{
			fprintf(fp, "'%c\n\t%s\n\t", bookmarks[i].name,
					escape_spaces(bookmarks[i].dir));
			fprintf(fp, "%s\n", escape_spaces(bookmarks[i].file));
		}
	}

	if(vifminfo & VIFMINFO_TUI)
	{
		fputs("\n# TUI:\n", fp);
		fprintf(fp, "v%d\n", tui.windows);
		fprintf(fp, "l%d\n", tui.lwin_sort);
		fprintf(fp, "r%d\n", tui.rwin_sort);
	}

	if(vifminfo & VIFMINFO_DHISTORY)
	{
		fputs("\n# Left window history (oldest to newest):\n", fp);
		fprintf(fp, "d%s\n", (lwin_dir == NULL) ? "" : lwin_dir);
		if(vifminfo & VIFMINFO_SAVEDIRS)
			fprintf(fp, "d\n");

		fputs("\n# Right window history (oldest to newest):\n", fp);
		fprintf(fp, "D%s\n", (rwin_dir == NULL) ? "" : rwin_dir);
		if(vifminfo & VIFMINFO_SAVEDIRS)
			fprintf(fp, "D\n");
	}

	if(vifminfo & VIFMINFO_STATE)
	{
		fputs("\n# State:\n", fp);
		fprintf(fp, "f%s\n", state.lwin_filter);
		fprintf(fp, "i%d\n", state.lwin_inverted);
		fprintf(fp, "F%s\n", state.rwin_filter);
		fprintf(fp, "I%d\n", state.rwin_inverted);
		fprintf(fp, "s%d\n", state.use_screen);
	}

	if(vifminfo & VIFMINFO_CS)
	{
		fputs("\n# Color scheme:\n", fp);
		fprintf(fp, "c%s\n", color_scheme);
	}

	fclose(fp);
}

static void
convert_color_schemes(const char *colorschemes_file, const char *colors_dir)
{
	read_color_scheme_file(colorschemes_file);
	write_color_schemes(colors_dir);
}

static void
read_color_scheme_file(const char *config_file)
{
	FILE *fp;
	char line[MAX_LEN];
	char *s1 = NULL;
	char *s2 = NULL;
	char *s3 = NULL;
	char *sx = NULL;

	if((fp = fopen(config_file, "r")) == NULL)
	{
		load_default_colors();
		cs.count = 1;
		return;
	}

	while(fgets(line, MAX_LEN, fp))
	{
		int args;

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
				cs.count++;

				if(cs.count > MAX_COLOR_SCHEMES)
					break;

				init_color_scheme(&cs.array[cs.count - 1]);

				snprintf(cs.array[cs.count - 1].name, NAME_MAX, "%s", s1);
				continue;
			}
			if(!strcmp(line, "DIRECTORY"))
			{
				col_scheme_t* c;

				c = &cs.array[cs.count - 1];
				snprintf(c->dir, PATH_MAX, "%s", s1);
				continue;
			}
		}
		if(!strcmp(line, "COLOR") && args == 3)
			add_color(s1, s2, s3);
	}

	fclose(fp);
}

static void
load_default_colors(void)
{
	init_color_scheme(&cs.array[0]);

	snprintf(cs.array[0].name, NAME_MAX, "Default");
	snprintf(cs.array[0].dir, PATH_MAX, "/");
}

static void
init_color_scheme(col_scheme_t *cs)
{
	int i;
	strcpy(cs->dir, "/");
	cs->defaulted = 0;

	for(i = 0; i < ARRAY_LEN(default_colors); i++)
	{
		cs->color[i].fg = default_colors[i][0];
		cs->color[i].bg = default_colors[i][1];
		cs->color[i].attr = default_colors[i][2];
	}
}

static void
add_color(char s1[], char s2[], char s3[])
{
	int fg, bg;
	const int x = cs.count - 1;
	int y;

	fg = colname2int(s2);
	bg = colname2int(s3);

	if(!strcmp(s1, "MENU"))
		y = MENU_COLOR;
	else if(!strcmp(s1, "BORDER"))
		y = BORDER_COLOR;
	else if(!strcmp(s1, "WIN"))
		y = WIN_COLOR;
	else if(!strcmp(s1, "STATUS_BAR"))
		y = CMD_LINE_COLOR;
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
	else if(!strcmp(s1, "BROKEN_LINK"))
		y = BROKEN_LINK_COLOR;
	else if(!strcmp(s1, "TOP_LINE"))
		y = TOP_LINE_COLOR;
	else if(!strcmp(s1, "STATUS_LINE"))
		y = STATUS_LINE_COLOR;
	else if(!strcmp(s1, "FIFO"))
		y = FIFO_COLOR;
	else if(!strcmp(s1, "ERROR_MSG"))
		y = ERROR_MSG_COLOR;
	else
		return;

	cs.array[x].color[y].fg = fg;
	cs.array[x].color[y].bg = bg;
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
	if(!strcasecmp(col, "black"))
		return 0;
	if(!strcasecmp(col, "red"))
		return 1;
	if(!strcasecmp(col, "green"))
		return 2;
	if(!strcasecmp(col, "yellow"))
		return 3;
	if(!strcasecmp(col, "blue"))
		return 4;
	if(!strcasecmp(col, "magenta"))
		return 5;
	if(!strcasecmp(col, "cyan"))
		return 6;
	if(!strcasecmp(col, "white"))
		return 7;
	/* return default color */
	return -1;
}

int make_dir(const char *dir_name, mode_t mode)
{
#ifndef _WIN32
	return mkdir(dir_name, mode);
#else
	return mkdir(dir_name);
#endif
}

static void
write_color_schemes(const char *colors_dir)
{
	int x;

	if(access(colors_dir, F_OK) != 0)
	{
		if(make_dir(colors_dir, 0777) != 0)
		{
			fprintf(stderr, "Can't create colors directory at \"%s\"\n", colors_dir);
			exit(1);
		}
	}

	for(x = 0; x < cs.count; x++)
	{
		FILE *fp;
		char buf[PATH_MAX];
		int y;

		snprintf(buf, sizeof(buf), "%s/%s", colors_dir, cs.array[x].name);
		if((fp = fopen(buf, "w")) == NULL)
		{
			fprintf(stderr, "Can't create color scheme file at \"%s\"\n", buf);
			continue;
		}

		fprintf(fp, "\" vim: filetype=vifm :\n");
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

		fprintf(fp, "\" Available style values (some of them can be combined):\n");
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

		fprintf(fp, "\" highlight group cterm=style ctermfg=foreground_color ctermbg=background_color\n\n");

		for(y = 0; y < MAXNUM_COLOR - 2; y++)
		{
			char fg_buf[16], bg_buf[16];
			int fg = cs.array[x].color[y].fg;
			int bg = cs.array[x].color[y].bg;
			if(fg == -1)
				strcpy(fg_buf, "none");
			else if(fg < ARRAY_LEN(COLOR_NAMES))
				snprintf(fg_buf, sizeof(fg_buf), "%s", COLOR_NAMES[fg]);
			else
				snprintf(fg_buf, sizeof(fg_buf), "%d", fg);

			if(bg == -1)
				strcpy(bg_buf, "none");
			else if(bg < ARRAY_LEN(COLOR_NAMES))
				snprintf(bg_buf, sizeof(bg_buf), "%s", COLOR_NAMES[bg]);
			else
				snprintf(bg_buf, sizeof(bg_buf), "%d", bg);

			fprintf(fp, "highlight %s cterm=%s ctermfg=%s ctermbg=%s\n", HI_GROUPS[y],
					conv_attrs_to_str(cs.array[x].color[y].attr), fg_buf, bg_buf);
		}

		fclose(fp);
	}
}

static const char *
conv_attrs_to_str(int attrs)
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
	result[strlen(result) - 1] = '\0';
	return result;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
