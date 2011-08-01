#include <limits.h>
#include <unistd.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	VIFMINFO_OPTIONS   = 1 << 0,
	VIFMINFO_FILETYPES = 1 << 1,
	VIFMINFO_COMMANDS  = 1 << 2,
	VIFMINFO_BOOKMARKS = 1 << 3,
	VIFMINFO_TUI       = 1 << 4,
	VIFMINFO_DHISTORY  = 1 << 5,
	VIFMINFO_STATE     = 1 << 6,
	VIFMINFO_CS        = 1 << 7,
	VIFMINFO_WARN      = 1 << 8,
};

int vifminfo = 0x1ff;

struct{
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

char *color_scheme = "Default"; /* COLOR_SCHEME */

char config_dir[PATH_MAX];

typedef struct{
	char *description;
	char *extensions;
	char *programs;
	char *viewer;
}filetype;

filetype *filetypes;
int nfiletypes;

typedef struct{
	char *name;
	char *cmd;
}command;

command *commands;
int ncommands;

typedef struct{
	char name;
	char *dir;
	char *file;
}bookmark;

bookmark *bookmarks;
int nbookmarks;

struct{
	int windows;   /* vnumber of windows */
	int lwin_sort; /* lleft window sort */
	int rwin_sort; /* rright window sort */
}tui;

char *lwin_dir; /* dleft window directory */
char *rwin_dir; /* Dright window directory */

struct{
	char *lwin_filter; /* fleft window filter */
	int lwin_inverted; /* ileft window filter inverted */
	char *rwin_filter; /* Fright window filter */
	int rwin_inverted; /* Iright window filter inverted */
	int use_screen;    /* suse screen program */
}state;

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
int
read_config_file(const char *config_file)
{
	FILE *fp;
	char line[4096];
	char *s1 = NULL;
	char *s2 = NULL;
	char *s3 = NULL;
	char *s4 = NULL;
	char *sx = NULL;
	int args;

	if((fp = fopen(config_file, "r")) == NULL)
		return 0;

	while(fgets(line, sizeof(line), fp))
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
					vifminfo &= ~VIFMINFO_DHISTORY;
				else
					vifminfo |= VIFMINFO_DHISTORY;
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

int main(int argc, char** argv)
{
	char *home_dir;
	char config_dir[PATH_MAX];
	char config_file[PATH_MAX], config_file_bak[PATH_MAX];
	char startup_file[PATH_MAX];
	char vifminfo_file[PATH_MAX], vifminfo_file_bak[PATH_MAX];
	int vifm_like;
	char *vifmrc_arg, *vifminfo_arg;

	if(argc != 2 && argc != 4)
	{
		puts("Usage: vifmrc-converter 0|1 [vifmrc_file vifminfo_file]\n\n"
				"1 means comment commands in vifmrc and put more things to vifminfo");
		return 1;
	}
	if(argv[1][1] != '\0' || (argv[1][0] != '0' && argv[1][0] != '1'))
	{
		puts("Usage: vifmrc-converter 0|1 [vifmrc_file vifminfo_file]\n\n"
				"1 means comment commands in vifmrc and put more things to vifminfo");
		return 1;
	}

	vifm_like = atoi(argv[1]);

	home_dir = getenv("HOME");
	snprintf(config_dir, sizeof(config_dir), "%s/.vifm", home_dir);
	snprintf(config_file, sizeof(config_file), "%s/vifmrc", config_dir);
	snprintf(config_file_bak, sizeof(config_file_bak), "%s/vifmrc.bak",
			config_dir);
	snprintf(startup_file, sizeof(startup_file), "%s/startup", config_dir);
	snprintf(vifminfo_file, sizeof(startup_file), "%s/vifminfo", config_dir);
	snprintf(vifminfo_file_bak, sizeof(vifminfo_file_bak), "%s/vifminfo.bak",
			config_dir);

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

	if(argc != 4)
	{
		if(rename(config_file, config_file_bak) != 0)
		{
			fputs("Can't move vifmrc file to make a backup copy\n", stderr);
			exit(1);
		}
		if(rename(vifminfo_file, vifminfo_file_bak) != 0)
		{
			fputs("Can't move vifminfo file to make a backup copy\n", stderr);
			exit(1);
		}
	}

	write_vifmrc(vifmrc_arg, vifm_like);
	append_vifmrc(vifmrc_arg, vifm_like);
	append_vifminfo_option(vifmrc_arg, vifm_like);
	append_startup(vifmrc_arg, startup_file);

	write_vifminfo(vifminfo_arg, vifm_like);

	return 0;
}

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
		fputs("Can't open configuration file for writing", stderr);
		exit(1);
	}

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

	fprintf(fp, "\n\" This is how many files to show in the directory history menu.\n");
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

	fprintf(fp, "\n\" Use KiB, MiB, ... instead of KB, MB, ...\n");
	fprintf(fp, "\n%sset %siec\n", comment, options.iec ? "" : "no");

	fprintf(fp, "\n\" Selected color scheme\n");
	fprintf(fp, "\n%scolorscheme %s\n", comment, escape_spaces(color_scheme));

	fprintf(fp, "\n");
	fprintf(fp, "\" The FUSE_HOME directory will be used as a root dir for all FUSE mounts.\n");
	fprintf(fp, "\" Unless it exists with write/exec permissions set, vifm will attempt to\n");
	fprintf(fp, "\" create it.\n");
	fprintf(fp, "\n%sset fusehome=%s\n", comment, escape_spaces(options.fusehome));

	fprintf(fp, "\n\" Format for displaying time in file list. For example:\n");
	fprintf(fp, "\" TIME_STAMP_FORMAT=%%m/%%d-%%H:%%M\n");
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
		fputs("Can't open configuration file for appending", stderr);
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
		fputs("Can't open configuration file for appending", stderr);
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
		fputs("Can't open configuration file for appending", stderr);
		exit(1);
	}

	fputs("\n\" What should be saved automatically between vifm runs\n", fp);
	fputs("\" Like in previous versions of vifm\n", fp);
	if(vifminfo & VIFMINFO_DHISTORY)
		fprintf(fp, "%sset vifminfo=options,filetypes,commands,bookmarks,dhistory,state,cs,warn\n", vifm_like ? "" : "\" ");
	else
		fprintf(fp, "%sset vifminfo=options,filetypes,commands,bookmarks,state,cs,warn\n", vifm_like ? "" : "\" ");
	fputs("\" Like in vi\n", fp);
	fprintf(fp, "%sset vifminfo=bookmarks\n", !vifm_like ? "" : "\" ");

	fclose(fp);
}

static void
write_vifminfo(const char *config_file, int vifm_like)
{
	FILE *fp;
	int i;

	if((fp = fopen(config_file, "w")) == NULL)
	{
		fputs("Can't open info file for writing", stderr);
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

		fputs("\n# Right window history (oldest to newest):\n", fp);
		fprintf(fp, "D%s\n", (rwin_dir == NULL) ? "" : rwin_dir);
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
