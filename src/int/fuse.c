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

#include "fuse.h"

#include <curses.h> /* werase() */

#include <sys/types.h> /* pid_t ssize_t */
#include <sys/stat.h> /* S_IRWXU */
#ifndef _WIN32
#include <sys/wait.h> /* WEXITSTATUS() WIFEXITED() waitpid() */
#endif
#include <unistd.h> /* execve() fork() unlink() */

#include <errno.h> /* errno ENOTDIR */
#include <stddef.h> /* NULL size_t */
#include <stdio.h> /* snprintf() fclose() */
#include <stdlib.h> /* EXIT_SUCCESS free() malloc() */
#include <string.h> /* memmove() strcpy() strlen() strcmp() strcat() */

#include "../cfg/config.h"
#include "../compat/fs_limits.h"
#include "../compat/os.h"
#include "../menus/menus.h"
#include "../modes/dialogs/msg_dialog.h"
#include "../ui/cancellation.h"
#include "../ui/statusbar.h"
#include "../ui/ui.h"
#include "../utils/cancellation.h"
#include "../utils/fs.h"
#include "../utils/log.h"
#include "../utils/macros.h"
#include "../utils/path.h"
#include "../utils/str.h"
#include "../utils/test_helpers.h"
#include "../utils/utils.h"
#include "../background.h"
#include "../filelist.h"
#include "../flist_pos.h"
#include "../status.h"

/* Description of existing FUSE mounts. */
typedef struct fuse_mount_t
{
	char source_file_path[PATH_MAX + 1]; /* Full path to source file. */
	char source_file_dir[PATH_MAX + 1];  /* Full path to dir of source file. */
	char mount_point[PATH_MAX + 1];      /* Full path to mount point. */
	int mount_point_id;                  /* ID of mounts for unique dirs. */
	int needs_unmounting;                /* Whether unmount call is required. */
	struct fuse_mount_t *next;           /* Pointer to the next mount in chain. */
}
fuse_mount_t;

static int fuse_mount(view_t *view, char file_full_path[], const char param[],
		const char program[], char mount_point[]);
static int get_last_mount_point_id(const fuse_mount_t *mounts);
static void register_mount(fuse_mount_t **mounts, const char file_full_path[],
		const char mount_point[], int id, int needs_unmounting);
TSTATIC void format_mount_command(const char mount_point[],
		const char file_name[], const char param[], const char format[],
		size_t buf_size, char buf[], int *foreground);
static fuse_mount_t * get_mount_by_source(const char source[]);
static fuse_mount_t * get_mount_by_mount_point(const char dir[]);
static fuse_mount_t * get_mount_by_path(const char path[]);
static int run_fuse_command(char cmd[], const cancellation_t *cancellation,
		int *cancelled);
static void kill_mount_point(const char mount_point[]);
static void updir_from_mount(view_t *view, fuse_mount_t *runner);

/* List of active mounts. */
static fuse_mount_t *fuse_mounts;

void
fuse_try_mount(view_t *view, const char program[])
{
	/* TODO: refactor this function fuse_try_mount() */

	fuse_mount_t *runner;
	char file_full_path[PATH_MAX + 1];
	char mount_point[PATH_MAX + 1];

	if(make_path(cfg.fuse_home, S_IRWXU) != 0)
	{
		show_error_msg("Unable to create FUSE mount home directory",
				cfg.fuse_home);
		return;
	}

	get_current_full_path(view, sizeof(file_full_path), file_full_path);

	/* Check if already mounted. */
	runner = get_mount_by_source(file_full_path);

	if(runner != NULL)
	{
		strcpy(mount_point, runner->mount_point);
	}
	else
	{
		char param[PATH_MAX + 1];
		param[0] = '\0';

		/* New file to be mounted. */
		if(starts_with(program, "FUSE_MOUNT2|"))
		{
			FILE *f;
			if((f = os_fopen(file_full_path, "r")) == NULL)
			{
				show_error_msg("SSH mount failed", "Can't open file for reading");
				curr_stats.save_msg = 1;
				return;
			}

			if(fgets(param, sizeof(param), f) == NULL)
			{
				show_error_msg("SSH mount failed", "Can't read file content");
				curr_stats.save_msg = 1;
				fclose(f);
				return;
			}
			fclose(f);

			chomp(param);
			if(param[0] == '\0')
			{
				show_error_msg("SSH mount failed", "File is empty");
				curr_stats.save_msg = 1;
				return;
			}
		}

		if(fuse_mount(view, file_full_path, param, program, mount_point) != 0)
		{
			return;
		}
	}

	navigate_to(view, mount_point);
}

/* Searchers for mount record by source file path. */
static fuse_mount_t *
get_mount_by_source(const char source[])
{
	fuse_mount_t *runner = fuse_mounts;
	while(runner != NULL)
	{
		if(paths_are_equal(runner->source_file_path, source))
			break;
		runner = runner->next;
	}
	return runner;
}

/* mount_point should be an array of at least PATH_MAX characters
 * Returns non-zero on error. */
static int
fuse_mount(view_t *view, char file_full_path[], const char param[],
		const char program[], char mount_point[])
{
	/* TODO: refactor this function fuse_mount(). */

	int id;
	int mount_point_id;
	char buf[2*PATH_MAX];
	int foreground;
	char errors_file[PATH_MAX + 1];
	int status;
	int cancelled;

	id = get_last_mount_point_id(fuse_mounts);
	mount_point_id = id;
	do
	{
		snprintf(mount_point, PATH_MAX, "%s/%03d_%s", cfg.fuse_home,
				++mount_point_id, get_current_file_name(view));

		/* Make sure this is not an infinite loop, although practically this
		 * condition will always be false. */
		if(mount_point_id == id)
		{
			show_error_msg("Unable to create FUSE mount directory", mount_point);
			return -1;
		}

		errno = 0;
	}
	while(os_mkdir(mount_point, S_IRWXU) != 0 && errno == EEXIST);

	if(errno != 0)
	{
		show_error_msg("Unable to create FUSE mount directory", mount_point);
		return -1;
	}

	/* Just before running the mount,
		 I need to chdir out temporarily from any FUSE mounted
		 paths, Otherwise the fuse-zip command fails with
		 "fusermount: failed to open current directory: permission denied"
		 (this happens when mounting JARs from mounted JARs) */
	if(vifm_chdir(cfg.fuse_home) != 0)
	{
		show_error_msg("FUSE MOUNT ERROR", "Can't chdir() to FUSE home");
		return -1;
	}

	format_mount_command(mount_point, file_full_path, param, program, sizeof(buf),
			buf, &foreground);

	ui_sb_msg("FUSE mounting selected file, please stand by..");

	if(foreground)
	{
		ui_shutdown();
	}

	generate_tmp_file_name("vifm.errors", errors_file, sizeof(errors_file));

	strcat(buf, " 2> ");
	strcat(buf, errors_file);
	LOG_INFO_MSG("FUSE mount command: `%s`", buf);
	if(foreground)
	{
		cancelled = 0;
		status = run_fuse_command(buf, &no_cancellation, NULL);
	}
	else
	{
		ui_cancellation_push_on();
		status = run_fuse_command(buf, &ui_cancellation_info, &cancelled);
		ui_cancellation_pop();
	}

	ui_sb_clear();

	/* Check child process exit status. */
	if(!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS)
	{
		FILE *ef;

		if(!WIFEXITED(status))
		{
			LOG_ERROR_MSG("FUSE mounter didn't exit!");
		}
		else
		{
			LOG_ERROR_MSG("FUSE mount command exit status: %d", WEXITSTATUS(status));
		}

		ef = os_fopen(errors_file, "r");
		if(ef == NULL)
		{
			LOG_SERROR_MSG(errno, "Failed to open temporary stderr file: %s",
					errors_file);
		}
		show_errors_from_file(ef, "FUSE mounter error");

		werase(status_bar);

		if(cancelled)
		{
			ui_sb_msg("FUSE mount cancelled");
			curr_stats.save_msg = 1;
		}
		else
		{
			show_error_msg("FUSE MOUNT ERROR", file_full_path);
		}

		if(unlink(errors_file) != 0)
		{
			LOG_SERROR_MSG(errno, "Error file deletion failure: %s", errors_file);
		}

		/* Remove the directory we created for the mount. */
		kill_mount_point(mount_point);

		(void)vifm_chdir(flist_get_dir(view));
		return -1;
	}
	unlink(errors_file);
	ui_sb_msg("FUSE mount success");

	register_mount(&fuse_mounts, file_full_path, mount_point, mount_point_id,
			!starts_with(program, "FUSE_MOUNT3|"));

	return 0;
}

/* Gets last mount point id used.  Returns the id or 0 if list of mounts is
 * empty. */
static int
get_last_mount_point_id(const fuse_mount_t *mounts)
{
	/* As new entries are added at the front, first entry must have the largest
	 * value of the id. */
	return (mounts == NULL) ? 0 : mounts->mount_point_id;
}

/* Adds new entry to the list of *mounts. */
static void
register_mount(fuse_mount_t **mounts, const char file_full_path[],
		const char mount_point[], int id, int needs_unmounting)
{
	fuse_mount_t *fuse_mount = malloc(sizeof(*fuse_mount));
	if(fuse_mount == NULL)
	{
		return;
	}

	copy_str(fuse_mount->source_file_path, sizeof(fuse_mount->source_file_path),
			file_full_path);

	copy_str(fuse_mount->source_file_dir, sizeof(fuse_mount->source_file_dir),
			file_full_path);
	remove_last_path_component(fuse_mount->source_file_dir);

	canonicalize_path(mount_point, fuse_mount->mount_point,
			sizeof(fuse_mount->mount_point));

	fuse_mount->mount_point_id = id;
	fuse_mount->needs_unmounting = needs_unmounting;

	fuse_mount->next = *mounts;
	*mounts = fuse_mount;
}

/* Builds the mount command based on the file type program.
 * Accepted formats are:
 *   FUSE_MOUNT|some_mount_command %SOURCE_FILE %DESTINATION_DIR [%FOREGROUND]
 *   FUSE_MOUNT2|some_mount_command %PARAM %DESTINATION_DIR [%FOREGROUND]
 *   FUSE_MOUNT3|some_mount_command %SOURCE_FILE %DESTINATION_DIR [%FOREGROUND]
 * %CLEAR is an obsolete name of %FOREGROUND.
 * Always sets value of *foreground. */
TSTATIC void
format_mount_command(const char mount_point[], const char file_name[],
		const char param[], const char format[], size_t buf_size, char buf[],
		int *foreground)
{
	char *buf_pos;
	const char *prog_pos;
	char *escaped_path;
	char *escaped_mount_point;

	*foreground = 0;

	escaped_path = shell_like_escape(file_name, 0);
	escaped_mount_point = shell_like_escape(mount_point, 0);

	buf_pos = buf;
	buf_pos[0] = '\0';

	prog_pos = after_first(format, '|');
	while(*prog_pos != '\0')
	{
		if(*prog_pos == '%')
		{
			char cmd_buf[96];
			char *cmd_pos;

			cmd_pos = cmd_buf;
			while(*prog_pos != '\0' && *prog_pos != ' ')
			{
				*cmd_pos = *prog_pos;
				if((size_t)(cmd_pos - cmd_buf) < sizeof(cmd_buf))
				{
					++cmd_pos;
				}
				++prog_pos;
			}
			*cmd_pos = '\0';

			if(!strcmp(cmd_buf, "%SOURCE_FILE"))
			{
				copy_str(buf_pos, buf_size - (buf_pos - buf), escaped_path);
				buf_pos += strlen(buf_pos);
			}
			else if(!strcmp(cmd_buf, "%PARAM"))
			{
				copy_str(buf_pos, buf_size - (buf_pos - buf), param);
				buf_pos += strlen(buf_pos);
			}
			else if(!strcmp(cmd_buf, "%DESTINATION_DIR"))
			{
				copy_str(buf_pos, buf_size - (buf_pos - buf), escaped_mount_point);
				buf_pos += strlen(buf_pos);
			}
			else if(!strcmp(cmd_buf, "%FOREGROUND") || !strcmp(cmd_buf, "%CLEAR"))
			{
				*foreground = 1;
			}
		}
		else
		{
			*buf_pos = *prog_pos;
			if((size_t)(buf_pos - buf) < buf_size - 1)
			{
				++buf_pos;
			}
			++prog_pos;
		}
	}

	*buf_pos = '\0';
	free(escaped_mount_point);
	free(escaped_path);
}

void
fuse_unmount_all(void)
{
	if(fuse_mounts == NULL)
	{
		return;
	}

	if(vifm_chdir("/") != 0)
	{
		return;
	}

	fuse_mount_t *runner = fuse_mounts;
	fuse_mounts = NULL;

	while(runner != NULL)
	{
		if(runner->needs_unmounting)
		{
			char *escaped_filename = shell_like_escape(runner->mount_point, 0);
			char buf[14 + PATH_MAX + 1];
			snprintf(buf, sizeof(buf), "%s %s", curr_stats.fuse_umount_cmd,
					escaped_filename);
			free(escaped_filename);

			(void)vifm_system(buf, SHELL_BY_APP);
		}

		kill_mount_point(runner->mount_point);

		fuse_mount_t *next = runner->next;
		free(runner);
		runner = next;
	}

	leave_invalid_dir(&lwin);
	leave_invalid_dir(&rwin);
}

int
fuse_try_updir_from_a_mount(const char path[], view_t *view)
{
	fuse_mount_t *const mount = get_mount_by_mount_point(path);
	if(mount == NULL)
	{
		return 0;
	}

	updir_from_mount(view, mount);
	return 1;
}

int
fuse_is_mount_point(const char path[])
{
	return get_mount_by_mount_point(path) != NULL;
}

/* Searches for mount record by path to mount point.  Returns mount point or
 * NULL on failure. */
static fuse_mount_t *
get_mount_by_mount_point(const char dir[])
{
	fuse_mount_t *runner = fuse_mounts;
	while(runner != NULL)
	{
		if(paths_are_equal(runner->mount_point, dir))
		{
			return runner;
		}
		runner = runner->next;
	}
	return NULL;
}

const char *
fuse_get_mount_file(const char path[])
{
	const fuse_mount_t *const mount = get_mount_by_path(path);
	return (mount == NULL) ? NULL : mount->source_file_path;
}

/* Searches for mount record by path inside one of mount points.  Picks the
 * longest match so that even nested mount points work.  Returns mount point or
 * NULL on failure. */
static fuse_mount_t *
get_mount_by_path(const char path[])
{
	size_t max_len = 0U;
	fuse_mount_t *mount = NULL;
	fuse_mount_t *runner = fuse_mounts;
	while(runner != NULL)
	{
		if(path_starts_with(path, runner->mount_point))
		{
			const size_t len = strlen(runner->mount_point);
			if(len > max_len)
			{
				max_len = len;
				mount = runner;
			}
		}
		runner = runner->next;
	}
	return mount;
}

int
fuse_try_unmount(view_t *view)
{
	fuse_mount_t *runner = fuse_mounts;
	fuse_mount_t *trailer = NULL;
	while(runner)
	{
		if(paths_are_equal(runner->mount_point, view->curr_dir))
		{
			break;
		}

		trailer = runner;
		runner = runner->next;
	}

	if(runner == NULL)
	{
		return 0;
	}

	/* We are exiting a top level dir. */

	if(runner->needs_unmounting)
	{
		char *escaped_mount_point = shell_like_escape(runner->mount_point, 0);

		char buf[14 + PATH_MAX + 1];
		snprintf(buf, sizeof(buf), "%s %s 2> /dev/null", curr_stats.fuse_umount_cmd,
				escaped_mount_point);
		LOG_INFO_MSG("FUSE unmount command: `%s`", buf);
		free(escaped_mount_point);

		/* Have to chdir to parent temporarily, so that this DIR can be
		 * unmounted. */
		if(vifm_chdir(cfg.fuse_home) != 0)
		{
			show_error_msg("FUSE UMOUNT ERROR", "Can't chdir to FUSE home");
			return -1;
		}

		ui_sb_msg("FUSE unmounting selected file, please stand by..");
		int status = run_fuse_command(buf, &no_cancellation, NULL);
		ui_sb_clear();
		/* Check child status. */
		if(!WIFEXITED(status) || WEXITSTATUS(status))
		{
			werase(status_bar);
			show_error_msgf("FUSE UMOUNT ERROR", "Can't unmount %s.  It may be busy.",
					runner->source_file_path);
			(void)vifm_chdir(flist_get_dir(view));
			return -1;
		}
	}

	/* Remove the directory we created for the mount. */
	kill_mount_point(runner->mount_point);

	/* Remove mount point from fuse_mount_t. */
	fuse_mount_t *sniffer = runner->next;
	if(trailer)
		trailer->next = sniffer ? sniffer : NULL;
	else
		fuse_mounts = sniffer;

	updir_from_mount(view, runner);
	free(runner);
	return 1;
}

/* Runs command in background not redirecting its streams.  To determine an
 * error uses exit status only.  cancelled can be NULL when operations is not
 * cancellable.  Returns status on success, otherwise -1 is returned.  Sets
 * correct value of *cancelled even on error. */
static int
run_fuse_command(char cmd[], const cancellation_t *cancellation, int *cancelled)
{
#ifndef _WIN32
	pid_t pid;
	int status;

	if(cancellation_possible(cancellation))
	{
		*cancelled = 0;
	}

	if(cmd == NULL)
	{
		return 1;
	}

	pid = fork();
	if(pid == (pid_t)-1)
	{
		LOG_SERROR_MSG(errno, "Forking has failed.");
		return -1;
	}

	if(pid == (pid_t)0)
	{
		extern char **environ;

		prepare_for_exec();
		(void)execve(get_execv_path(cfg.shell),
				make_execv_array(cfg.shell, "-c", cmd), environ);
		_Exit(127);
	}

	while(waitpid(pid, &status, 0) == -1)
	{
		if(errno != EINTR)
		{
			LOG_SERROR_MSG(errno, "Failed waiting for process: %" PRINTF_ULL,
					(unsigned long long)pid);
			status = -1;
			break;
		}
		process_cancel_request(pid, cancellation);
	}

	if(cancellation_requested(cancellation))
	{
		*cancelled = 1;
	}

	return status;
#else
	return -1;
#endif
}

/* Deletes mount point by its path. */
static void
kill_mount_point(const char mount_point[])
{
	/* rmdir() on some systems (FreeBSD at least) can resolve symbolic links if
	 * path ends with a slash and unlink() will also fail if there is a trailing
	 * slash. */
	char no_slash[strlen(mount_point) + 1];
	strcpy(no_slash, mount_point);
	chosp(no_slash);

	if(os_rmdir(no_slash) != 0 && errno == ENOTDIR)
	{
		/* FUSE mounter might replace directory with a symbolic link, account for
		 * this possibility. */
		(void)unlink(no_slash);
	}
}

static void
updir_from_mount(view_t *view, fuse_mount_t *runner)
{
	char *file;
	int pos;

	if(change_directory(view, runner->source_file_dir) < 0)
		return;

	load_dir_list(view, 0);

	file = runner->source_file_path;
	file += strlen(runner->source_file_dir) + 1;
	pos = fpos_find_by_name(view, file);
	fpos_set_pos(view, pos);
}

int
fuse_is_mount_string(const char string[])
{
	return starts_with(string, "FUSE_MOUNT|")
	    || starts_with(string, "FUSE_MOUNT2|")
	    || starts_with(string, "FUSE_MOUNT3|");
}

void
fuse_strip_mount_metadata(char string[])
{
	size_t prefix_len;
	if(starts_with(string, "FUSE_MOUNT|"))
	{
		prefix_len = ARRAY_LEN("FUSE_MOUNT|") - 1;
	}
	else if(starts_with(string, "FUSE_MOUNT2|"))
	{
		prefix_len = ARRAY_LEN("FUSE_MOUNT2|") - 1;
	}
	else if(starts_with(string, "FUSE_MOUNT3|"))
	{
		prefix_len = ARRAY_LEN("FUSE_MOUNT3|") - 1;
	}
	else
	{
		prefix_len = 0;
	}

	if(prefix_len != 0)
	{
		size_t new_len = strlen(string) - prefix_len;
		memmove(string, string + prefix_len, new_len + 1);
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
