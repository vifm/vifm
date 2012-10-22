/* vifm
 * Copyright (C) 2001 Ken Steen.
 * Copyright (C) 2012 xaizek.
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
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <lm.h>
#endif

#include <sys/stat.h> /* stat */
#include <dirent.h> /* DIR dirent */

#ifndef _WIN32
#include <grp.h> /* getgrent setgrent */
#include <pwd.h> /* getpwent setpwent */
#endif

#include <limits.h> /* PATH_MAX */
#include <stdlib.h>
#include <string.h>

#include "cfg/config.h"
#include "engine/completion.h"
#include "engine/options.h"
#include "engine/variables.h"
#ifdef _WIN32
#include "menus/menus.h"
#endif
#include "utils/fs.h"
#include "utils/fs_limits.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/utils.h"
#include "color_scheme.h"
#include "commands.h"
#include "file_magic.h"
#include "filelist.h"
#include "filetype.h"
#include "path_env.h"
#include "tags.h"

#include "commands_completion.h"

static int cmd_ends_with_space(const char *cmd);
static void complete_colorscheme(const char *str, size_t arg_num);
static void complete_help(const char *str);
static void complete_history(const char *str);
static int complete_chown(const char *str);
static void complete_filetype(const char *str);
static void complete_progs(const char *str, assoc_records_t records);
static void complete_highlight_groups(const char *str);
static int complete_highlight_arg(const char *str);
static void complete_envvar(const char *str);
static void complete_winrun(const char *str);
static void exec_completion(const char str[]);
static void filename_completion_in_dir(const char *path, const char *str,
		CompletionType type);
static void filename_completion_internal(DIR * dir, const char * dirname,
		const char * filename, CompletionType type);
static void add_filename_completion(const char * filename, CompletionType type);
static int is_entry_dir(const struct dirent *d);
static int is_entry_exec(const struct dirent *d);
#ifdef _WIN32
static const char * escape_for_cd(const char *str);
static void complete_with_shared(const char *server, const char *file);
#endif
static int executable_exists_in_path(const char command_name[]);
static int executable_exists(const char full_path[]);

int
complete_args(int id, const char args[], int argc, char *argv[], int arg_pos)
{
	/* TODO: Refactor this function complete_args() */

	const char *arg;
	const char *start;
	const char *slash;
	const char *dollar;

	arg = after_last(args, ' ');
	start = arg;
	dollar = strrchr(arg, '$');
	slash = strrchr(args + arg_pos, '/');

	if(id == COM_SET)
		complete_options(args, &start);
	else if(id == COM_LET || id == COM_ECHO)
		complete_variables((dollar > arg) ? dollar : arg, &start);
	else if(id == COM_UNLET)
		complete_variables(arg, &start);
	else if(id == COM_HELP)
		complete_help(args);
	else if(id == COM_HISTORY)
		complete_history(args);
	else if(id == COM_CHOWN)
		start += complete_chown(args);
	else if(id == COM_FILE)
		complete_filetype(args);
	else if(id == COM_HIGHLIGHT)
	{
		if(argc == 0 || (argc == 1 && !cmd_ends_with_space(args)))
			complete_highlight_groups(args);
		else
			start += complete_highlight_arg(arg);
	}
	else if((id == COM_CD || id == COM_PUSHD || id == COM_EXECUTE ||
			id == COM_SOURCE) && dollar != NULL && dollar > slash)
	{
		start = dollar + 1;
		complete_envvar(start);
	}
	else if(id == COM_WINDO)
		;
	else if(id == COM_WINRUN)
	{
		if(argc == 0)
			complete_winrun(args);
	}
	else
	{
		size_t arg_num = argc;
		start = slash;
		if(start == NULL)
			start = args + arg_pos;
		else
			start++;

		if(argc > 0 && !cmd_ends_with_space(args))
		{
			arg_num = argc - 1;
			arg = argv[arg_num];
		}

		if(id == COM_COLORSCHEME)
		{
			complete_colorscheme(arg, arg_num);
		}
		else if(id == COM_CD || id == COM_PUSHD || id == COM_SYNC ||
				id == COM_MKDIR)
		{
			filename_completion(arg, CT_DIRONLY);
		}
		else if(id == COM_COPY || id == COM_MOVE || id == COM_ALINK ||
				id == COM_RLINK)
		{
			filename_completion_in_dir(other_view->curr_dir, arg, CT_ALL);
		}
		else if(id == COM_SPLIT || id == COM_VSPLIT)
		{
			filename_completion_in_dir(curr_view->curr_dir, arg, CT_DIRONLY);
		}
		else if(id == COM_FIND)
		{
			if(argc == 1 && !cmd_ends_with_space(args))
				filename_completion(arg, CT_DIRONLY);
		}
		else if(id == COM_EXECUTE)
		{
			if(argc == 0 || (argc == 1 && !cmd_ends_with_space(args)))
			{
				if(*arg == '.')
					filename_completion(arg, CT_DIREXEC);
				else
					exec_completion(arg);
			}
			else
				filename_completion(arg, CT_ALL);
		}
		else if(id == COM_TOUCH || id == COM_RENAME)
		{
			filename_completion(arg, CT_ALL_WOS);
		}
		else
		{
			filename_completion(arg, CT_ALL);
		}
	}

	return start - args;
}

static int
cmd_ends_with_space(const char *cmd)
{
	while(cmd[0] != '\0' && cmd[1] != '\0')
	{
		if(cmd[0] == '\\')
			cmd++;
		cmd++;
	}
	return cmd[0] == ' ';
}

static void
complete_colorscheme(const char *str, size_t arg_num)
{
	if(arg_num == 0)
	{
		complete_colorschemes(str);
	}
	else if(arg_num == 1)
	{
		filename_completion(str, CT_DIRONLY);
	}
}

static void
complete_help(const char *str)
{
	int i;

	if(!cfg.use_vim_help)
		return;

	for(i = 0; tags[i] != NULL; i++)
	{
		if(strstr(tags[i], str) != NULL)
			add_completion(tags[i]);
	}
	completion_group_end();
	add_completion(str);
}

static void
complete_history(const char *str)
{
	static const char *lines[] = {
		".",
		"dir",
		"@",
		"input",
		"/",
		"search",
		"fsearch",
		"?",
		"bsearch",
		":",
		"cmd",
	};
	int i;
	size_t len = strlen(str);
	for(i = 0; i < ARRAY_LEN(lines); i++)
	{
		if(strncmp(str, lines[i], len) == 0)
			add_completion(lines[i]);
	}
	completion_group_end();
	add_completion(str);
}

static int
complete_chown(const char *str)
{
#ifndef _WIN32
	char *colon = strchr(str, ':');
	if(colon == NULL)
	{
		complete_user_name(str);
		return 0;
	}
	else
	{
		complete_user_name(colon + 1);
		return colon - str + 1;
	}
#else
	add_completion(str);
	return 0;
#endif
}

static void
complete_filetype(const char *str)
{
	const char *filename = get_current_file_name(curr_view);
	assoc_records_t ft = get_all_programs_for_file(filename);

	complete_progs(str, ft);
	free(ft.list);

	complete_progs(str, get_magic_handlers(filename));

	completion_group_end();
	add_completion(str);
}

static void
complete_progs(const char *str, assoc_records_t records)
{
	int i;
	const size_t len = strlen(str);

	for(i = 0; i < records.count; i++)
	{
		char command[NAME_MAX];

		(void)get_command_name(records.list[i].command, 1, sizeof(command),
				command);

		if(strnoscmp(command, str, len) == 0)
		{
			char *escaped = escape_chars(command, "|");
			add_completion(escaped);
			free(escaped);
		}
	}
}

static void
complete_highlight_groups(const char *str)
{
	int i;
	size_t len = strlen(str);
	for(i = 0; i < MAXNUM_COLOR - 2; i++)
	{
		if(strncasecmp(str, HI_GROUPS[i], len) == 0)
			add_completion(HI_GROUPS[i]);
	}
	completion_group_end();
	add_completion(str);
}

static int
complete_highlight_arg(const char *str)
{
	/* TODO: Refactor this function complete_highlight_arg() */
	int i;
	char *equal = strchr(str, '=');
	int result = (equal == NULL) ? 0 : (equal - str + 1);
	size_t len = strlen((equal == NULL) ? str : ++equal);
	if(equal == NULL)
	{
		static const char *args[] = {
			"cterm",
			"ctermfg",
			"ctermbg",
		};
		for(i = 0; i < ARRAY_LEN(args); i++)
		{
			if(strncmp(str, args[i], len) == 0)
				add_completion(args[i]);
		}
	}
	else
	{
		if(strncmp(str, "cterm", equal - str - 1) == 0)
		{
			static const char *STYLES[] = {
				"bold",
				"underline",
				"reverse",
				"inverse",
				"standout",
				"none",
			};
			char *comma = strrchr(equal, ',');
			if(comma != NULL)
			{
				result += comma - equal + 1;
				equal = comma + 1;
				len = strlen(equal);
			}

			for(i = 0; i < ARRAY_LEN(STYLES); i++)
			{
				if(strncasecmp(equal, STYLES[i], len) == 0)
					add_completion(STYLES[i]);
			}
		}
		else
		{
			if(strncasecmp(equal, "default", len) == 0)
				add_completion("default");
			if(strncasecmp(equal, "none", len) == 0)
				add_completion("none");
			for(i = 0; i < ARRAY_LEN(COLOR_NAMES); i++)
			{
				if(strncasecmp(equal, COLOR_NAMES[i], len) == 0)
					add_completion(COLOR_NAMES[i]);
			}
			for(i = 0; i < ARRAY_LEN(LIGHT_COLOR_NAMES); i++)
			{
				if(strncasecmp(equal, LIGHT_COLOR_NAMES[i], len) == 0)
					add_completion(LIGHT_COLOR_NAMES[i]);
			}
		}
	}
	completion_group_end();
	add_completion((equal == NULL) ? str : equal);
	return result;
}

static void
complete_envvar(const char *str)
{
	extern char **environ;
	char **p = environ;
	size_t len = strlen(str);

	while(*p != NULL)
	{
		if(strncmp(*p, str, len) == 0)
		{
			char *equal = strchr(*p, '=');
			*equal = '\0';
			add_completion(*p);
			*equal = '=';
		}
		p++;
	}

	completion_group_end();
	add_completion(str);
}

static void
complete_winrun(const char *str)
{
	static const char *VARIANTS[] = { "^", "$", "%", ".", "," };
	size_t len = strlen(str);
	int i;

	for(i = 0; i < ARRAY_LEN(VARIANTS); i++)
	{
		if(strncmp(str, VARIANTS[i], len) == 0)
			add_completion(VARIANTS[i]);
	}
	completion_group_end();
	add_completion(str);
}

char *
fast_run_complete(const char *cmd)
{
	char *result = NULL;
	const char *args;
	char command[NAME_MAX];
	char *completed;

	args = get_command_name(cmd, 0, sizeof(command), command);

	reset_completion();
	exec_completion(command);
	completed = next_completion();

	if(get_completion_count() > 2)
	{
		int c = get_completion_count() - 1;
		while(c-- > 0)
		{
			if(stroscmp(command, completed) == 0)
			{
				result = strdup(cmd);
				break;
			}
			else
			{
				free(completed);
				completed = next_completion();
			}
		}

		if(result == NULL)
		{
			status_bar_error("Command beginning is ambiguous");
		}
	}
	else
	{
		free(completed);
		completed = next_completion();
		result = malloc(strlen(completed) + 1 + strlen(args) + 1);
		sprintf(result, "%s %s", completed, args);
	}
	free(completed);

	return result;
}

/* Fills list of complitions with executables in $PATH. */
static void
exec_completion(const char str[])
{
	int i;
	char ** paths;
	size_t paths_count;

	paths = get_paths(&paths_count);
	for(i = 0; i < paths_count; i++)
	{
		if(my_chdir(paths[i]) != 0)
			continue;
		filename_completion(str, CT_EXECONLY);
	}
	add_completion(str);
}

static void
filename_completion_in_dir(const char *path, const char *str,
		CompletionType type)
{
	char buf[PATH_MAX];
	if(is_root_dir(str))
	{
		snprintf(buf, sizeof(buf), "%s", str);
	}
	else
	{
		snprintf(buf, sizeof(buf), "%s/%s", path, str);
	}
	filename_completion(buf, type);
}

/*
 * type: CT_*
 */
void
filename_completion(const char *str, CompletionType type)
{
	/* TODO refactor filename_completion(...) function */
	DIR * dir;
	char * dirname;
	char * filename;
	char * temp;

	if(str[0] == '~' && strchr(str, '/') == NULL)
	{
		char *s = expand_tilde(strdup(str));
		add_completion(s);
		free(s);
		return;
	}

	dirname = expand_tilde(strdup(str));
	filename = strdup(dirname);

	temp = cmds_expand_envvars(dirname);
	free(dirname);
	dirname = temp;

	temp = strrchr(dirname, '/');
	if(temp != NULL && type != CT_FILE && type != CT_FILE_WOE)
	{
		strcpy(filename, ++temp);
		*temp = '\0';
	}
	else
	{
		dirname = realloc(dirname, 2);
		strcpy(dirname, ".");
	}

#ifdef _WIN32
	if(is_unc_root(dirname) ||
			(stroscmp(dirname, ".") == 0 && is_unc_root(curr_view->curr_dir)) ||
			(stroscmp(dirname, "/") == 0 && is_unc_path(curr_view->curr_dir)))
	{
		char buf[PATH_MAX];
		if(!is_unc_root(dirname))
			snprintf(buf,
					strchr(curr_view->curr_dir + 2, '/') - curr_view->curr_dir + 1, "%s",
					curr_view->curr_dir);
		else
			snprintf(buf, sizeof(buf), "%s", dirname);

		complete_with_shared(buf, filename);
		free(filename);
		free(dirname);
		return;
	}
	if(is_unc_path(curr_view->curr_dir))
	{
		char buf[PATH_MAX];
		if(is_path_absolute(dirname) && !is_unc_root(curr_view->curr_dir))
			snprintf(buf,
					strchr(curr_view->curr_dir + 2, '/') - curr_view->curr_dir + 2, "%s",
					curr_view->curr_dir);
		else
			snprintf(buf, sizeof(buf), "%s", curr_view->curr_dir);
		strcat(buf, dirname);
		chosp(buf);
		(void)replace_string(&dirname, buf);
	}
#endif

	dir = opendir(dirname);

	if(dir == NULL || my_chdir(dirname) != 0)
	{
		add_completion(filename);
	}
	else
	{
		filename_completion_internal(dir, dirname, filename, type);
		closedir(dir);
		(void)my_chdir(curr_view->curr_dir);
	}

	free(filename);
	free(dirname);
}

static void
filename_completion_internal(DIR * dir, const char * dirname,
		const char * filename, CompletionType type)
{
	struct dirent *d;

	size_t filename_len = strlen(filename);
	while((d = readdir(dir)) != NULL)
	{
		if(filename[0] == '\0' && d->d_name[0] == '.')
			continue;
		if(strnoscmp(d->d_name, filename, filename_len) != 0)
			continue;

		if(type == CT_DIRONLY && !is_entry_dir(d))
			continue;
		else if(type == CT_EXECONLY && !is_entry_exec(d))
			continue;
		else if(type == CT_DIREXEC && !is_entry_dir(d) && !is_entry_exec(d))
			continue;

		if(is_entry_dir(d) && type != CT_ALL_WOS)
		{
			char buf[NAME_MAX + 1];
			snprintf(buf, sizeof(buf), "%s/", d->d_name);
			add_filename_completion(buf, type);
		}
		else
		{
			add_filename_completion(d->d_name, type);
		}
	}

	completion_group_end();
	if(type != CT_EXECONLY)
	{
		if(get_completion_count() == 0)
		{
			add_completion(filename);
		}
		else
		{
			add_filename_completion(filename, type);
		}
	}
}

static void
add_filename_completion(const char * filename, CompletionType type)
{
#ifndef _WIN32
	int woe = (type == CT_ALL_WOE || type == CT_FILE_WOE);
	char * temp = woe ? strdup(filename) : escape_filename(filename, 1);
	add_completion(temp);
	free(temp);
#else
	add_completion(escape_for_cd(filename));
#endif
}

static int
is_entry_dir(const struct dirent *d)
{
#ifdef _WIN32
	struct stat st;
	if(stat(d->d_name, &st) != 0)
		return 0;
	return S_ISDIR(st.st_mode);
#else
	if(d->d_type == DT_UNKNOWN)
	{
		struct stat st;
		if(stat(d->d_name, &st) != 0)
			return 0;
		return S_ISDIR(st.st_mode);
	}

	if(d->d_type != DT_DIR && d->d_type != DT_LNK)
		return 0;
	if(d->d_type == DT_LNK && !check_link_is_dir(d->d_name))
		return 0;
	return 1;
#endif
}

static int
is_entry_exec(const struct dirent *d)
{
#ifndef _WIN32
	if(d->d_type == DT_DIR)
		return 0;
	if(d->d_type == DT_LNK && check_link_is_dir(d->d_name))
		return 0;
	if(access(d->d_name, X_OK) != 0)
		return 0;
	return 1;
#else
	return is_win_executable(d->d_name);
#endif
}

#ifndef _WIN32

void
complete_user_name(const char *str)
{
	struct passwd* pw;
	size_t len;

	len = strlen(str);
	setpwent();
	while((pw = getpwent()) != NULL)
	{
		if(strncmp(pw->pw_name, str, len) == 0)
			add_completion(pw->pw_name);
	}
	completion_group_end();
	add_completion(str);
}

void
complete_group_name(const char *str)
{
	struct group* gr;
	size_t len = strlen(str);

	setgrent();
	while((gr = getgrent()) != NULL)
	{
		if(strncmp(gr->gr_name, str, len) == 0)
			add_completion(gr->gr_name);
	}
	completion_group_end();
	add_completion(str);
}

#else

/* Returns pointer to a statically allocated buffer */
static const char *
escape_for_cd(const char *str)
{
	static char buf[PATH_MAX*2];
	char *p;

	p = buf;
	while(*str != '\0')
	{
		if(char_is_one_of("\\ $", *str))
			*p++ = '\\';
		else if(*str == '%')
			*p++ = '%';
		*p++ = *str;

		str++;
	}
	*p = '\0';
	return buf;
}

static void
complete_with_shared(const char *server, const char *file)
{
	NET_API_STATUS res;
	size_t len = strlen(file);

	do
	{
		PSHARE_INFO_502 buf_ptr;
		DWORD er = 0, tr = 0, resume = 0;
		wchar_t *wserver = to_wide(server + 2);

		if(wserver == NULL)
		{
			show_error_msg("Memory Error", "Unable to allocate enough memory");
			return;
		}

		res = NetShareEnum(wserver, 502, (LPBYTE *)&buf_ptr, -1, &er, &tr, &resume);
		free(wserver);
		if(res == ERROR_SUCCESS || res == ERROR_MORE_DATA)
		{
			PSHARE_INFO_502 p;
			DWORD i;

			p = buf_ptr;
			for(i = 1; i <= er; i++)
			{
				char buf[512];
				WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)p->shi502_netname, -1, buf,
						sizeof(buf), NULL, NULL);
				strcat(buf, "/");
				if(strnoscmp(buf, file, len) == 0)
				{
					char *escaped = escape_filename(buf, 1);
					add_completion(escaped);
					free(escaped);
				}
				p++;
			}
			NetApiBufferFree(buf_ptr);
		}
	}
	while(res == ERROR_MORE_DATA);
}

#endif

int
external_command_exists(const char command[])
{
	if(starts_with(command, "!!"))
	{
		command += 2;
	}

	if(strchr(command, '/') == NULL)
	{
		return executable_exists_in_path(command);
	}
	else
	{
		return executable_exists(command);
	}
}

/* Checks for executable in all directories in PATH environment variables. */
static int
executable_exists_in_path(const char command_name[])
{
	size_t i;
	size_t paths_count;
	char **paths;

	paths = get_paths(&paths_count);
	for(i = 0; i < paths_count; i++)
	{
		char full_path[PATH_MAX];
		snprintf(full_path, sizeof(full_path), "%s/%s", paths[i], command_name);

		if(executable_exists(full_path))
		{
			return 1;
		}
	}
	return 0;
}

/* Checks for executable by its full path. */
static int
executable_exists(const char full_path[])
{
#ifndef _WIN32
	if(!path_exists(full_path))
	{
		return 0;
	}
	return access(full_path, X_OK) == 0;
#else
	return win_executable_exists(full_path);
#endif
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
