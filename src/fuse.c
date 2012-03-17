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

#include <sys/stat.h> /* S_IRWXU */

#include <string.h> /* strcpy() strlen() strcmp() strcat() */

#include "menus/menus.h"
#include "utils/utils.h"
#include "background.h"
#include "config.h"
#include "filelist.h"
#include "status.h"

#include "fuse.h"

typedef struct fuse_mount_t
{
	char source_file_name[PATH_MAX];
	char source_file_dir[PATH_MAX];
	char mount_point[PATH_MAX];
	int mount_point_id;
	struct fuse_mount_t *next;
}
fuse_mount_t;

static fuse_mount_t *fuse_mounts;

static fuse_mount_t * fuse_mount(FileView *view, char *filename,
		const char *program, char *mount_point);
static fuse_mount_t * find_fuse_mount(const char *dir);
static void updir_from_mount(FileView *view, fuse_mount_t *runner);

/*
 * mount_point should be an array of at least PATH_MAX characters
 * Returns 0 on success.
 */
static fuse_mount_t *
fuse_mount(FileView *view, char *filename, const char *program,
		char *mount_point)
{
	/* TODO: refactor this function fuse_mount() */

	fuse_mount_t *runner = NULL;
	int mount_point_id = 0;
	fuse_mount_t *fuse_item = NULL;
	char buf[2*PATH_MAX];
	char cmd_buf[96];
	char *cmd_pos;
	char *buf_pos;
	const char *prog_pos;
	char *escaped_path;
	char *escaped_filename;
	char *escaped_mount_point;
	int clear_before_mount = 0;
	const char *tmp_file;
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
	while(access(mount_point, F_OK) == 0);
	if(make_dir(mount_point, S_IRWXU) != 0)
	{
		free(escaped_filename);
		(void)show_error_msg("Unable to create FUSE mount directory", mount_point);
		return NULL;
	}
	free(escaped_filename);

	escaped_path = escape_filename(filename, 0);
	escaped_mount_point = escape_filename(mount_point, 0);

	/* I need the return code of the mount command */
	status_bar_message("FUSE mounting selected file, please stand by..");
	buf_pos = buf;
	prog_pos = program;
	/* Build the mount command based on the FUSE program config line in vifmrc.
		 Accepted FORMAT: FUSE_MOUNT|some_mount_command %SOURCE_FILE %DESTINATION_DIR
		 or
		 FUSE_MOUNT2|some_mount_command %PARAM %DESTINATION_DIR */
	strcpy(buf_pos, "");
	buf_pos += strlen(buf_pos);
	while(*prog_pos != '\0' && *prog_pos != '|')
		prog_pos++;
	prog_pos++;
	while(*prog_pos != '\0')
	{
		if(*prog_pos == '%')
		{
			cmd_pos = cmd_buf;
			while(*prog_pos != '\0' && *prog_pos != ' ')
			{
				*cmd_pos = *prog_pos;
				if(cmd_pos < cmd_pos+sizeof(cmd_buf))
					cmd_pos++;
				prog_pos++;
			}
			*cmd_pos = '\0';
			if(buf_pos + strlen(escaped_path) >= buf + sizeof(buf) + 2)
				continue;
			else if(!strcmp(cmd_buf, "%SOURCE_FILE") || !strcmp(cmd_buf, "%PARAM"))
			{
				strcpy(buf_pos, escaped_path);
				buf_pos += strlen(escaped_path);
			}
			else if(!strcmp(cmd_buf, "%DESTINATION_DIR"))
			{
				strcpy(buf_pos, escaped_mount_point);
				buf_pos += strlen(escaped_mount_point);
			}
			else if(!strcmp(cmd_buf, "%CLEAR"))
			{
				clear_before_mount = 1;
			}
		}
		else
		{
			*buf_pos = *prog_pos;
			if(buf_pos < buf_pos + sizeof(buf) - 1)
				buf_pos++;
			prog_pos++;
		}
	}

	*buf_pos = '\0';
	free(escaped_mount_point);
	free(escaped_path);
	/* CMD built */
	/* Just before running the mount,
		 I need to chdir out temporarily from any FUSE mounted
		 paths, Otherwise the fuse-zip command fails with
		 "fusermount: failed to open current directory: permission denied"
		 (this happens when mounting JARs from mounted JARs) */
	if(my_chdir(cfg.fuse_home) != 0)
	{
		(void)show_error_msg("FUSE MOUNT ERROR", "Can't chdir() to FUSE home");
		return NULL;
	}

	if(clear_before_mount)
	{
		def_prog_mode();
		endwin();
	}

	tmp_file = make_name_unique("/tmp/vifm.errors");
	strcat(buf, " 2> ");
	strcat(buf, tmp_file);
	status = background_and_wait_for_status(buf);
	/* check child status */
	if(!WIFEXITED(status) || (WIFEXITED(status) && WEXITSTATUS(status)))
	{
		FILE *ef = fopen(tmp_file, "r");
		print_errors(ef);
		unlink(tmp_file);

		werase(status_bar);
		/* remove the DIR we created for the mount */
		if(!access(mount_point, F_OK))
			rmdir(mount_point);
		(void)show_error_msg("FUSE MOUNT ERROR", filename);
		(void)my_chdir(view->curr_dir);
		return NULL;
	}
	unlink(tmp_file);
	status_bar_message("FUSE mount success");

	fuse_item = (fuse_mount_t *)malloc(sizeof(fuse_mount_t));
	strcpy(fuse_item->source_file_name, filename);
	strcpy(fuse_item->source_file_dir, view->curr_dir);
	strcpy(fuse_item->mount_point, mount_point);
	fuse_item->mount_point_id = mount_point_id;
	fuse_item->next = NULL;
	if(fuse_mounts == NULL)
		fuse_mounts = fuse_item;
	else
		runner->next = fuse_item;

	return fuse_item;
}

void
fuse_try_mount(FileView *view, const char *program)
{
	fuse_mount_t *runner;
	char filename[PATH_MAX];
	char mount_point[PATH_MAX];
	int mount_found;

	if(access(cfg.fuse_home, F_OK|W_OK|X_OK) != 0)
	{
		if(make_dir(cfg.fuse_home, S_IRWXU) != 0)
		{
			(void)show_error_msg("Unable to create FUSE mount home directory",
					cfg.fuse_home);
			return;
		}
	}

	runner = fuse_mounts;
	mount_found = 0;

	snprintf(filename, PATH_MAX, "%s/%s", view->curr_dir,
			get_current_file_name(view));
	/* check if already mounted */
	while(runner)
	{
		if(!pathcmp(filename, runner->source_file_name))
		{
			strcpy(mount_point, runner->mount_point);
			mount_found = 1;
			break;
		}
		runner = runner->next;
	}

	if(!mount_found)
	{
		fuse_mount_t *item;
		/* new file to be mounted */
		if(starts_with(program, "FUSE_MOUNT2"))
		{
			FILE *f;
			size_t len;
			if((f = fopen(filename, "r")) == NULL)
			{
				(void)show_error_msg("SSH mount failed", "Can't open file for reading");
				curr_stats.save_msg = 1;
				return;
			}

			if(fgets(filename, sizeof(filename), f) == NULL)
			{
				(void)show_error_msg("SSH mount failed", "Can't read file content");
				curr_stats.save_msg = 1;
				fclose(f);
				return;
			}
			len = strlen(filename);

			if(len == 0 || (len == 1 && filename[0] == '\n'))
			{
				(void)show_error_msg("SSH mount failed", "File is empty");
				curr_stats.save_msg = 1;
				fclose(f);
				return;
			}

			if(filename[len - 1] == '\n')
				filename[len - 1] = '\0';

			fclose(f);
		}
		if((item = fuse_mount(view, filename, program, mount_point)) != NULL)
			snprintf(item->source_file_name, PATH_MAX, "%s/%s", view->curr_dir,
					get_current_file_name(view));
		else
			return;
	}

	if(change_directory(view, mount_point) >= 0)
	{
		load_dir_list(view, 0);
		move_to_list_pos(view, view->curr_line);
	}
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
		char *tmp;

		tmp = escape_filename(runner->mount_point, 0);
		snprintf(buf, sizeof(buf), "fusermount -u %s", tmp);
		free(tmp);

		my_system(buf);
		if(access(runner->mount_point, F_OK) == 0)
			rmdir(runner->mount_point);

		runner = runner->next;
	}

	leave_invalid_dir(&lwin, lwin.curr_dir);
	leave_invalid_dir(&rwin, rwin.curr_dir);
}

int
try_updir_from_mount(const char *path, FileView *view)
{
	fuse_mount_t *runner;
	if((runner = find_fuse_mount(path)) != NULL)
	{
		updir_from_mount(view, runner);
		return 1;
	}
	return 0;
}

static fuse_mount_t *
find_fuse_mount(const char *dir)
{
	fuse_mount_t *runner = fuse_mounts;
	while(runner)
	{
		if(pathcmp(runner->mount_point, dir) == 0)
			break;
		runner = runner->next;
	}
	return runner;
}

int
in_mounted_dir(const char *path)
{
	return find_fuse_mount(path) != NULL;
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
		if(!pathcmp(runner->mount_point, view->curr_dir))
			break;

		trailer = runner;
		runner = runner->next;
	}

	if(runner == NULL)
		return 0;

	/* we are exiting a top level dir */
	status_bar_message("FUSE unmounting selected file, please stand by..");
	escaped_mount_point = escape_filename(runner->mount_point, 0);
	snprintf(buf, sizeof(buf), "fusermount -u %s 2> /dev/null",
			escaped_mount_point);
	free(escaped_mount_point);

	/* have to chdir to parent temporarily, so that this DIR can be unmounted */
	if(my_chdir(cfg.fuse_home) != 0)
	{
		(void)show_error_msg("FUSE UMOUNT ERROR", "Can't chdir to FUSE home");
		return -1;
	}

	status = background_and_wait_for_status(buf);
	/* check child status */
	if(!WIFEXITED(status) || (WIFEXITED(status) && WEXITSTATUS(status)))
	{
		werase(status_bar);
		(void)show_error_msgf("FUSE UMOUNT ERROR",
				"Can't unmount %s.  It may be busy.", runner->source_file_name);
		(void)my_chdir(view->curr_dir);
		return -1;
	}

	/* remove the DIR we created for the mount */
	if(access(runner->mount_point, F_OK) == 0)
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

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
