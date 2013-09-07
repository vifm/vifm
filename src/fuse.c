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

#include <sys/stat.h> /* S_IRWXU */

#include <string.h> /* memmove() strcpy() strlen() strcmp() strcat() */

#include "cfg/config.h"
#include "menus/menus.h"
#include "utils/fs.h"
#include "utils/fs_limits.h"
#include "utils/log.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/test_helpers.h"
#include "utils/utils.h"
#include "background.h"
#include "filelist.h"
#include "status.h"

typedef struct fuse_mount_t
{
	char source_file_name[PATH_MAX]; /* full path to source file */
	char source_file_dir[PATH_MAX]; /* full path to directory of source file */
	char mount_point[PATH_MAX]; /* full path to mount point */
	int mount_point_id;
	struct fuse_mount_t *next;
}
fuse_mount_t;

static fuse_mount_t *fuse_mounts;

static int fuse_mount(FileView *view, char *file_full_path, const char *param,
		const char *program, char *mount_point);
TSTATIC int format_mount_command(const char mount_point[],
		const char file_name[], const char param[], const char format[],
		size_t buf_size, char buf[]);
static fuse_mount_t * get_mount_by_source(const char *source);
static fuse_mount_t * get_mount_by_mount_point(const char *dir);
static void updir_from_mount(FileView *view, fuse_mount_t *runner);

void
fuse_try_mount(FileView *view, const char *program)
{
	/* TODO: refactor this function fuse_try_mount() */

	fuse_mount_t *runner;
	char file_full_path[PATH_MAX];
	char mount_point[PATH_MAX];

	if(!path_exists(cfg.fuse_home))
	{
		if(make_dir(cfg.fuse_home, S_IRWXU) != 0)
		{
			show_error_msg("Unable to create FUSE mount home directory",
					cfg.fuse_home);
			return;
		}
	}

	snprintf(file_full_path, PATH_MAX, "%s/%s", view->curr_dir,
			get_current_file_name(view));

	/* check if already mounted */
	runner = get_mount_by_source(file_full_path);

	if(runner != NULL)
	{
		strcpy(mount_point, runner->mount_point);
	}
	else
	{
		char param[PATH_MAX] = "";
		/* new file to be mounted */
		if(starts_with(program, "FUSE_MOUNT2"))
		{
			FILE *f;
			if((f = fopen(file_full_path, "r")) == NULL)
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
			return;
	}

	navigate_to(view, mount_point);
}

/* Searchers for mount record by source file path. */
static fuse_mount_t *
get_mount_by_source(const char *source)
{
	fuse_mount_t *runner = fuse_mounts;
	while(runner != NULL)
	{
		if(paths_are_equal(runner->source_file_name, source))
			break;
		runner = runner->next;
	}
	return runner;
}

/*
 * mount_point should be an array of at least PATH_MAX characters
 * Returns non-zero on error.
 */
static int
fuse_mount(FileView *view, char *file_full_path, const char *param,
		const char *program, char *mount_point)
{
	/* TODO: refactor this function fuse_mount() */

	fuse_mount_t *runner = NULL;
	int mount_point_id = 0;
	fuse_mount_t *fuse_item = NULL;
	char buf[2*PATH_MAX];
	char *escaped_filename;
	int clear_before_mount = 0;
	char errors_file[PATH_MAX];
	int status;

	escaped_filename = escape_filename(get_current_file_name(view), 0);

	/* get mount_point_id + mount_point and set runner pointing to the list's
	 * tail */
	if(fuse_mounts != NULL)
	{
		runner = fuse_mounts;
		while(runner->next != NULL)
			runner = runner->next;
		mount_point_id = runner->mount_point_id;
	}
	do
	{
		snprintf(mount_point, PATH_MAX, "%s/%03d_%s", cfg.fuse_home,
				++mount_point_id, get_current_file_name(view));
	}
	while(path_exists(mount_point));
	if(make_dir(mount_point, S_IRWXU) != 0)
	{
		free(escaped_filename);
		show_error_msg("Unable to create FUSE mount directory", mount_point);
		return -1;
	}
	free(escaped_filename);

	/* Just before running the mount,
		 I need to chdir out temporarily from any FUSE mounted
		 paths, Otherwise the fuse-zip command fails with
		 "fusermount: failed to open current directory: permission denied"
		 (this happens when mounting JARs from mounted JARs) */
	if(my_chdir(cfg.fuse_home) != 0)
	{
		show_error_msg("FUSE MOUNT ERROR", "Can't chdir() to FUSE home");
		return -1;
	}

	clear_before_mount = format_mount_command(mount_point, file_full_path, param,
			program, sizeof(buf), buf);

	status_bar_message("FUSE mounting selected file, please stand by..");

	if(clear_before_mount)
	{
		def_prog_mode();
		endwin();
	}

	generate_tmp_file_name("vifm.errors", errors_file, sizeof(errors_file));

	strcat(buf, " 2> ");
	strcat(buf, errors_file);
	LOG_INFO_MSG("FUSE mount command: `%s`", buf);
	status = background_and_wait_for_status(buf);

	clean_status_bar();

	/* check child status */
	if(!WIFEXITED(status) || (WIFEXITED(status) && WEXITSTATUS(status)))
	{
		FILE *ef = fopen(errors_file, "r");
		print_errors(ef);
		unlink(errors_file);

		werase(status_bar);
		/* remove the directory we created for the mount */
		if(path_exists(mount_point))
			rmdir(mount_point);
		show_error_msg("FUSE MOUNT ERROR", file_full_path);
		(void)my_chdir(view->curr_dir);
		return -1;
	}
	unlink(errors_file);
	status_bar_message("FUSE mount success");

	fuse_item = malloc(sizeof(*fuse_item));
	copy_str(fuse_item->source_file_name, sizeof(fuse_item->source_file_name),
			file_full_path);
	strcpy(fuse_item->source_file_dir, view->curr_dir);
	canonicalize_path(mount_point, fuse_item->mount_point,
			sizeof(fuse_item->mount_point));
	fuse_item->mount_point_id = mount_point_id;
	fuse_item->next = NULL;
	if(fuse_mounts == NULL)
		fuse_mounts = fuse_item;
	else
		runner->next = fuse_item;

	return 0;
}

/* Builds the mount command based on the file type program.
 * Accepted formats are:
 *   FUSE_MOUNT|some_mount_command %SOURCE_FILE %DESTINATION_DIR [%CLEAR]
 * and
 *   FUSE_MOUNT2|some_mount_command %PARAM %DESTINATION_DIR [%CLEAR]
 * Returns non-zero if format contains %CLEAR.
 * */
TSTATIC int
format_mount_command(const char mount_point[], const char file_name[],
		const char param[], const char format[], size_t buf_size, char buf[])
{
	char *buf_pos;
	const char *prog_pos;
	char *escaped_path;
	char *escaped_mount_point;
	int result = 0;

	escaped_path = escape_filename(file_name, 0);
	escaped_mount_point = escape_filename(mount_point, 0);

	buf_pos = buf;
	prog_pos = format;
	strcpy(buf_pos, "");
	buf_pos += strlen(buf_pos);
	while(*prog_pos != '\0' && *prog_pos != '|')
		prog_pos++;
	prog_pos++;
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
				if(cmd_pos - cmd_buf < sizeof(cmd_buf))
					cmd_pos++;
				prog_pos++;
			}
			*cmd_pos = '\0';
			/* FIXME: possible buffer overflow */
			if(buf_pos + strlen(escaped_path) >= buf + buf_size + 2)
				continue;
			else if(!strcmp(cmd_buf, "%SOURCE_FILE"))
			{
				strcpy(buf_pos, escaped_path);
				buf_pos += strlen(escaped_path);
			}
			else if(!strcmp(cmd_buf, "%PARAM"))
			{
				strcpy(buf_pos, param);
				buf_pos += strlen(param);
			}
			else if(!strcmp(cmd_buf, "%DESTINATION_DIR"))
			{
				strcpy(buf_pos, escaped_mount_point);
				buf_pos += strlen(escaped_mount_point);
			}
			else if(!strcmp(cmd_buf, "%CLEAR"))
			{
				result = 1;
			}
		}
		else
		{
			*buf_pos = *prog_pos;
			if(buf_pos - buf < buf_size - 1)
				buf_pos++;
			prog_pos++;
		}
	}

	*buf_pos = '\0';
	free(escaped_mount_point);
	free(escaped_path);

	return result;
}

void
unmount_fuse(void)
{
	fuse_mount_t *runner;

	if(fuse_mounts == NULL)
		return;

	if(my_chdir("/") != 0)
		return;

	runner = fuse_mounts;
	while(runner)
	{
		char buf[14 + PATH_MAX + 1];
		char *escaped_filename;

		escaped_filename = escape_filename(runner->mount_point, 0);
		snprintf(buf, sizeof(buf), "fusermount -u %s", escaped_filename);
		free(escaped_filename);

		my_system(buf);
		if(path_exists(runner->mount_point))
			rmdir(runner->mount_point);

		runner = runner->next;
	}

	leave_invalid_dir(&lwin);
	leave_invalid_dir(&rwin);
}

int
try_updir_from_fuse_mount(const char *path, FileView *view)
{
	fuse_mount_t *runner;
	if((runner = get_mount_by_mount_point(path)) != NULL)
	{
		updir_from_mount(view, runner);
		return 1;
	}
	return 0;
}

int
in_mounted_dir(const char *path)
{
	return get_mount_by_mount_point(path) != NULL;
}

/* Searchers for mount record by path to mount point. */
static fuse_mount_t *
get_mount_by_mount_point(const char *dir)
{
	fuse_mount_t *runner = fuse_mounts;
	while(runner != NULL)
	{
		if(paths_are_equal(runner->mount_point, dir))
			break;
		runner = runner->next;
	}
	return runner;
}

int
try_unmount_fuse(FileView *view)
{
	char buf[14 + PATH_MAX + 1];
	fuse_mount_t *runner, *trailer;
	int status;
	fuse_mount_t *sniffer;
	char *escaped_mount_point;

	runner = fuse_mounts;
	trailer = NULL;
	while(runner)
	{
		if(paths_are_equal(runner->mount_point, view->curr_dir))
			break;

		trailer = runner;
		runner = runner->next;
	}

	if(runner == NULL)
		return 0;

	/* we are exiting a top level dir */
	escaped_mount_point = escape_filename(runner->mount_point, 0);
	snprintf(buf, sizeof(buf), "fusermount -u %s 2> /dev/null",
			escaped_mount_point);
	LOG_INFO_MSG("FUSE unmount command: `%s`", buf);
	free(escaped_mount_point);

	/* have to chdir to parent temporarily, so that this DIR can be unmounted */
	if(my_chdir(cfg.fuse_home) != 0)
	{
		show_error_msg("FUSE UMOUNT ERROR", "Can't chdir to FUSE home");
		return -1;
	}

	status_bar_message("FUSE unmounting selected file, please stand by..");
	status = background_and_wait_for_status(buf);
	clean_status_bar();
	/* check child status */
	if(!WIFEXITED(status) || (WIFEXITED(status) && WEXITSTATUS(status)))
	{
		werase(status_bar);
		show_error_msgf("FUSE UMOUNT ERROR", "Can't unmount %s.  It may be busy.",
				runner->source_file_name);
		(void)my_chdir(view->curr_dir);
		return -1;
	}

	/* remove the directory we created for the mount */
	if(path_exists(runner->mount_point))
		rmdir(runner->mount_point);

	/* remove mount point from fuse_mount_t */
	sniffer = runner->next;
	if(trailer)
		trailer->next = sniffer ? sniffer : NULL;
	else
		fuse_mounts = sniffer;

	updir_from_mount(view, runner);
	free(runner);
	return 1;
}

static void
updir_from_mount(FileView *view, fuse_mount_t *runner)
{
	char *file;
	int pos;

	if(change_directory(view, runner->source_file_dir) < 0)
		return;

	load_dir_list(view, 0);

	file = runner->source_file_name;
	file += strlen(runner->source_file_dir) + 1;
	pos = find_file_pos_in_list(view, file);
	move_to_list_pos(view, pos);
}

int
has_mount_prefixes(const char string[])
{
	return starts_with(string, "FUSE_MOUNT|") ||
		starts_with(string, "FUSE_MOUNT2|");
}

void
remove_mount_prefixes(char string[])
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
/* vim: set cinoptions+=t0 : */
