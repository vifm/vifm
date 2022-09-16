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

#include "args.h"

#include <stdio.h> /* stderr fprintf() puts() snprintf() */
#include <stdlib.h> /* EXIT_FAILURE EXIT_SUCCESS exit() */
#include <string.h> /* strcmp() */

#include "compat/fs_limits.h"
#include "compat/getopt.h"
#include "compat/reallocarray.h"
#include "modes/dialogs/msg_dialog.h"
#include "utils/fs.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "int/vim.h"
#include "ipc.h"
#include "status.h"
#include "version.h"
#include "vifm.h"

static void list_servers(void);
static void get_path_or_std(const char dir[], const char arg[], char output[]);
static void handle_arg_or_fail(const char arg[], int select, const char dir[],
		args_t *args);
static int handle_path_spec(const char arg[], int select, const char dir[],
		args_t *args);
static int is_path_arg(const char arg[]);
static void parse_path(const char dir[], const char path[], char buf[]);
static void process_general_args(args_t *args);
static void show_help_msg(const char wrong_arg[]);
static void show_version_msg(void);
static void process_ipc_args(args_t *args, ipc_t *ipc);
static void process_other_args(args_t *args);
static void quit_on_arg_parsing(int code);

/* Command line arguments definition for getopt_long(). */
static struct option long_opts[] = {
	{ "logging",      optional_argument, .flag = NULL, .val = 'l' },
	{ "no-configs",   no_argument,       .flag = NULL, .val = 'n' },
	{ "select",       required_argument, .flag = NULL, .val = 's' },
	{ "choose-files", required_argument, .flag = NULL, .val = 'F' },
	{ "choose-dir",   required_argument, .flag = NULL, .val = 'D' },
	{ "delimiter",    required_argument, .flag = NULL, .val = 'd' },
	{ "on-choose",    required_argument, .flag = NULL, .val = 'o' },

#ifdef ENABLE_REMOTE_CMDS
	{ "server-list",  no_argument,       .flag = NULL, .val = 'L' },
	{ "server-name",  required_argument, .flag = NULL, .val = 'N' },
	{ "remote",       no_argument,       .flag = NULL, .val = 'r' },
	{ "remote-expr",  required_argument, .flag = NULL, .val = 'R' },
#endif

	{ "help",         no_argument,       .flag = NULL, .val = 'h' },
	{ "version",      no_argument,       .flag = NULL, .val = 'v' },

	{ }
};

/* Parses command-line arguments into fields of the *args structure. */
void
args_parse(args_t *args, int argc, char *argv[], const char dir[])
{
	int done = 0;

	/* Request getopt() reinitialization. */
	optind = 0;

	while(!done)
	{
		switch(getopt_long(argc, argv, "-c:fhv", long_opts, NULL))
		{
			case 'f': /* -f */
				args->file_picker = 1;
				break;
			case 'F': /* --choose-files <path>|- */
				get_path_or_std(dir, optarg, args->chosen_files_out);
				args->file_picker = 0;
				break;
			case 'D': /* --choose-dir <path>|- */
				get_path_or_std(dir, optarg, args->chosen_dir_out);
				break;
			case 'd': /* --delimiter <delimiter> */
				args->delimiter = optarg;
				break;
			case 'o': /* --on-choose <cmd> */
				args->on_choose = optarg;
				break;

			case 'L': /* --server-list */
				list_servers();
				break;
			case 'N': /* --server-name <name> */
				args->server_name = optarg;
				break;
			case 'r': /* --remote <args>... */
				args->remote_cmds = argv + optind;
				done = 1;
				break;
			case 'R': /* --remote-expr <expr> */
				args->remote_expr = optarg;
				break;

			case 'h': /* -h, --help */
				/* Only first one of -v and -h should take effect. */
				if(!args->version)
				{
					args->help = 1;
				}
				break;
			case 'v': /* -v, --version */
				/* Only first one of -v and -h should take effect. */
				if(!args->help)
				{
					args->version = 1;
				}
				break;

			case 'c': /* -c <cmd> */
				args->ncmds = add_to_string_array(&args->cmds, args->ncmds, optarg);
				break;
			case 'l': /* --logging */
				args->logging = 1;
				if(!is_null_or_empty(optarg))
				{
					replace_string(&args->startup_log_path, optarg);
				}
				break;
			case 'n': /* --no-configs */
				args->no_configs = 1;
				break;

			case 's': /* --select <path> */
				handle_arg_or_fail(optarg, 1, dir, args);
				break;
			case 1: /* Positional argument. */
				if(argv[optind - 1][0] == '+')
				{
					const char *cmd = argv[optind - 1] + 1;
					if(*cmd == '\0')
					{
						cmd = "$";
					}
					args->ncmds = add_to_string_array(&args->cmds, args->ncmds, cmd);
				}
				else
				{
					handle_arg_or_fail(argv[optind - 1], 0, dir, args);
				}
				break;

			case '?': /* Parsing error. */
#ifndef ENABLE_REMOTE_CMDS
				if(starts_with("--remote", argv[optind - 1]) ||
						starts_with("--remote-expr", argv[optind - 1]) ||
						starts_with("--server-list", argv[optind - 1]) ||
						starts_with("--server-name", argv[optind - 1]))
				{
					fprintf(stderr,
							"Warning: remote commands were disabled at build-time!\n");
				}
#endif
				/* getopt_long() already printed error message. */
				quit_on_arg_parsing(EXIT_FAILURE);
				break;

			case -1: /* No more arguments. */
				done = 1;
				break;
		}
	}

	if(args->remote_cmds != NULL || args->remote_expr != NULL)
	{
		args->target_name = args->server_name;
		args->server_name = NULL;
	}
}

/* Lists names of servers on stdout. */
static void
list_servers(void)
{
	int i;
	int len;
	char **lst = ipc_list(&len);

	for(i = 0; i < len; ++i)
	{
		puts(lst[i]);
	}

	free_string_array(lst, len);
	quit_on_arg_parsing(EXIT_SUCCESS);
}

/* Parses the arg as absolute or relative path (to the dir), unless it's equal
 * to "-".  output should be at least PATH_MAX characters length. */
static void
get_path_or_std(const char dir[], const char arg[], char output[])
{
	if(arg[0] == '\0')
	{
		output[0] = '\0';
	}
	else if(strcmp(arg, "-") == 0)
	{
		strcpy(output, "-");
	}
	else
	{
		parse_path(dir, arg, output);
	}
}

/* Handles path command-line argument or fails with appropriate message.
 * Returns zero on successful handling, otherwise non-zero is returned. */
static void
handle_arg_or_fail(const char arg[], int select, const char dir[], args_t *args)
{
	if(handle_path_spec(arg, select, dir, args) == 0)
	{
		if(strcmp(args->lwin_path, "-") == 0 && strcmp(args->rwin_path, "-") == 0)
		{
			show_help_msg("\"-\" can be specified at most once");
			quit_on_arg_parsing(EXIT_FAILURE);
		}

		return;
	}

	if(curr_stats.load_stage == 0)
	{
		show_help_msg(arg);
		quit_on_arg_parsing(EXIT_FAILURE);
	}
#ifdef ENABLE_REMOTE_CMDS
	/* IPC won't happen before full initialization.  Yet this check will allow
	 * testing this unit. */
	else if(curr_stats.load_stage > 2)
	{
		show_error_msgf("--remote error", "Invalid argument: %s", arg);
	}
#endif
}

/* Handles path command-line argument.  Returns zero on successful handling,
 * otherwise non-zero is returned. */
static int
handle_path_spec(const char arg[], int select, const char dir[], args_t *args)
{
	const int cv_marker = (!select && strcmp(arg, "-") == 0);
	/* XXX: should we use path_exists_at() here and in is_path_arg()? */
	const int dash_to_select = select && strcmp(arg, "-") == 0
	                        && path_exists(arg, NODEREF);
	if(dash_to_select)
	{
		arg = "./-";
	}
	else if(!cv_marker && !is_path_arg(arg))
	{
		return 1;
	}

	if(args->lwin_path[0] != '\0')
	{
		parse_path(dir, arg, args->rwin_path);
		args->rwin_handle = !select;
	}
	else
	{
		parse_path(dir, arg, args->lwin_path);
		args->lwin_handle = !select;
	}

	return 0;
}

/* Checks whether argument mentions a valid path.  Returns non-zero if so,
 * otherwise zero is returned. */
static int
is_path_arg(const char arg[])
{
	/* FIXME: why allow nonexistent absolute or root paths? */
	return path_exists(arg, NODEREF) || is_path_absolute(arg) || is_root_dir(arg);
}

/* Ensures that path is in suitable form for processing.  buf should be at least
 * PATH_MAX characters length. */
static void
parse_path(const char dir[], const char path[], char buf[])
{
	if(copy_str(buf, PATH_MAX, path) == PATH_MAX && path[PATH_MAX - 1] != '\0')
	{
		fprintf(stderr, "An argument is too long: %s\n", path);
		quit_on_arg_parsing(EXIT_FAILURE);
	}

	system_to_internal_slashes(buf);

	if(is_path_absolute(buf) || strcmp(path, "-") == 0)
	{
		/* Do nothing. */
	}
#ifdef _WIN32
	else if(buf[0] == '/')
	{
		snprintf(buf, PATH_MAX, "%c:%s", dir[0], path);
	}
#endif
	else
	{
		char new_path[PATH_MAX + 1];
		build_path(new_path, sizeof(new_path), dir, path);
		canonicalize_path(new_path, buf, PATH_MAX);
	}
	if(!is_root_dir(buf))
	{
		chosp(buf);
	}

	system_to_internal_slashes(buf);
}

void
args_process(args_t *args, ArgsSubset subset, ipc_t *ipc)
{
	switch(subset)
	{
		case AS_GENERAL: /* --help, --version */
			process_general_args(args);
			break;
		case AS_IPC:     /* --remote, --remote-expr */
			process_ipc_args(args, ipc);
			break;
		case AS_OTHER:   /* All other options. */
			process_other_args(args);
			break;
	}
}

/* Processes general command-line arguments (--help and --version). */
static void
process_general_args(args_t *args)
{
	if(args->help)
	{
		show_help_msg(NULL);
		quit_on_arg_parsing(EXIT_SUCCESS);
		return;
	}

	if(args->version)
	{
		show_version_msg();
		quit_on_arg_parsing(EXIT_SUCCESS);
		return;
	}
}

/* Prints brief help to the screen.  If wrong_arg is not NULL, it's reported as
 * wrong. */
static void
show_help_msg(const char wrong_arg[])
{
	if(wrong_arg != NULL)
	{
		fprintf(stderr, "Wrong argument: %s\n\n", wrong_arg);
	}

	puts("vifm usage:\n");
	puts("  To read list of files from stdin use\n");
	puts("    vifm -\n");
	puts("  To start in a specific directory give the directory path.\n");
	puts("    vifm /path/to/start/dir/one");
	puts("    or");
	puts("    vifm /path/to/start/dir/one  /path/to/start/dir/two\n");
	puts("  To open file using associated program pass its path to vifm.\n");
	puts("  If no path is given vifm will start in the current working directory.\n");
	puts("  vifm --select <path>");
	puts("    open parent directory of the given path and select specified file");
	puts("    in it.\n");
	puts("  vifm -f");
	puts("    makes vifm instead of opening files write selection to");
	puts("    $VIFM/vimfiles and quit.\n");
	puts("  vifm --choose-files <path>|-");
	puts("    sets output file to write selection into on exit instead of");
	puts("    opening files.  \"-\" means standard output.\n");
	puts("  vifm --choose-dir <path>|-");
	puts("    sets output file to write last visited directory into on exit.");
	puts("    \"-\" means standard output.\n");
	puts("  vifm --delimiter <delimiter>");
	puts("    sets separator for list of file paths written out by vifm.\n");
	puts("  vifm --on-choose <command>");
	puts("    sets command to be executed on selected files instead of opening");
	puts("    them.  Command can use any of command macros.\n");
	puts("  vifm --logging[=<startup log path>]");
	puts("    log some operational details to $XDG_DATA_HOME/vifm/log or");
	puts("    $VIFM/log.  If the optional startup log path is specified and");
	puts("    permissions allow to open it for writing, then logging of early");
	puts("    initialization (before configuration directories are determined)");
	puts("    is put there.\n");

#ifdef ENABLE_REMOTE_CMDS
	puts("  vifm --server-list");
	puts("    list available server names and exit.\n");
	puts("  vifm --server-name <name>");
	puts("    name of target or this instance.\n");
	puts("  vifm --remote");
	puts("    passes all arguments that left in command line to vifm server.\n");
	puts("  vifm --remote-expr <expr>");
	puts("    passes expression to vifm server and prints result.\n");
#endif
	puts("  vifm -c <command> | +<command>");
	puts("    run command-line mode <command> on startup.\n");
	puts("  vifm --help | -h");
	puts("    show this help message and quit.\n");
	puts("  vifm --version | -v");
	puts("    show version number and quit.\n");
	puts("  vifm --no-configs");
	puts("    don't read vifmrc and vifminfo.");
}

/* Prints detailed version information to the screen. */
static void
show_version_msg(void)
{
	int i, len;
	char **list;

	list = reallocarray(NULL, fill_version_info(NULL, /*include_stats=*/0),
			sizeof(char *));
	len = fill_version_info(list, /*include_stats=*/0);

	for(i = 0; i < len; ++i)
	{
		puts(list[i]);
	}

	free_string_array(list, len);
}

/* Processes options related to IPC client functionality. */
static void
process_ipc_args(args_t *args, ipc_t *ipc)
{
	if(args->remote_cmds != NULL && args->remote_expr != NULL)
	{
		fprintf(stderr, "%s\n", "--remote and --remote-expr can't be combined.");
		quit_on_arg_parsing(EXIT_FAILURE);
	}
	else if(args->remote_cmds != NULL)
	{
		if(ipc_send(ipc, args->target_name, args->remote_cmds) != 0)
		{
			fprintf(stderr, "%s\n", "Sending remote commands failed.");
			quit_on_arg_parsing(EXIT_FAILURE);
		}
		else
		{
			quit_on_arg_parsing(EXIT_SUCCESS);
		}
	}
	else if(args->remote_expr != NULL)
	{
		char *const result = ipc_eval(ipc, args->target_name, args->remote_expr);
		if(result == NULL)
		{
			fprintf(stderr, "%s\n", "Evaluating expression remotely failed.");
			quit_on_arg_parsing(EXIT_FAILURE);
		}
		else
		{
			fprintf(stdout, "%s\n", result);
			quit_on_arg_parsing(EXIT_SUCCESS);
		}
	}
}

/* Processes all non-general command-line arguments except for IPC ones. */
static void
process_other_args(args_t *args)
{
	if(args->file_picker)
	{
		vim_get_list_file_path(args->chosen_files_out,
				sizeof(args->chosen_files_out));
	}
	if(args->chosen_files_out[0] != '\0')
	{
		stats_set_chosen_files_out(args->chosen_files_out);
	}

	if(args->chosen_dir_out[0] != '\0')
	{
		stats_set_chosen_dir_out(args->chosen_dir_out);
	}

	if(args->delimiter != NULL)
	{
		stats_set_output_delimiter(args->delimiter);
	}

	if(args->on_choose)
	{
		stats_set_on_choose(args->on_choose);
	}
}

/* Quits during argument parsing when it's allowed (e.g. not for remote
 * commands). */
static void
quit_on_arg_parsing(int code)
{
	if(curr_stats.load_stage == 0)
	{
		vifm_exit(code);
	}
}

void
args_free(args_t *args)
{
	if(args != NULL)
	{
		free_string_array(args->cmds, args->ncmds);
		args->cmds = NULL;
		args->ncmds = 0;

		update_string(&args->startup_log_path, NULL);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
