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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifdef _WIN32
#include <dirent.h> /* DIR */
#include <unistd.h> /* chdir() */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "background.h"
#include "config.h"
#include "ops.h"
#include "registers.h"
#include "undo.h"
#include "utils.h"

#include "trash.h"

static void empty_trash_dir(void);
static void empty_trash_list(void);

void
empty_trash(void)
{
	clean_regs_with_trash();
	empty_trash_dir();
	clean_cmds_with_trash();
	empty_trash_list();
}

static void
empty_trash_dir(void)
{
#ifndef _WIN32
	char cmd[24 + strlen(cfg.trash_dir)*2 + 1];
	char *escaped;

	escaped = escape_filename(cfg.trash_dir, 0);
	snprintf(cmd, sizeof(cmd), "sh -c 'rm -rf %s/* %s/.[!.]*'", escaped, escaped);
	free(escaped);

	start_background_job(cmd);
#else
	DIR *dir;
	struct dirent *d;

	dir = opendir(cfg.trash_dir);
	if(dir == NULL)
		return;
	while((d = readdir(dir)) != NULL)
	{
		char full[PATH_MAX];
		snprintf(full, sizeof(full), "%s/%s", cfg.trash_dir, d->d_name);
		perform_operation(OP_REMOVESL, NULL, full, NULL);
	}
	closedir(dir);
#endif
}

static void
empty_trash_list(void)
{
	int i;

	for(i = 0; i < nentries; i++)
	{
		free(trash_list[i].path);
		free(trash_list[i].trash_name);
	}
	free(trash_list);
	trash_list = NULL;
	nentries = 0;
}

int
add_to_trash(const char *path, const char *trash_name)
{
	void *p;

	if((p = realloc(trash_list, sizeof(*trash_list)*(nentries + 1))) == NULL)
		return -1;
	trash_list = p;

	trash_list[nentries].path = strdup(path);
	trash_list[nentries].trash_name = strdup(trash_name);
	if(trash_list[nentries].path == NULL ||
			trash_list[nentries].trash_name == NULL)
	{
		free(trash_list[nentries].path);
		free(trash_list[nentries].trash_name);
		return -1;
	}

	nentries++;
	return 0;
}

int
is_in_trash(const char *trash_name)
{
	int i;

	for(i = 0; i < nentries; i++)
	{
		if(strcmp(trash_list[i].trash_name, trash_name) == 0)
			return 1;
	}
	return 0;
}

int
restore_from_trash(const char *trash_name)
{
	int i;
	char full[PATH_MAX];
	char buf[PATH_MAX];

	for(i = 0; i < nentries; i++)
	{
		if(strcmp(trash_list[i].trash_name, trash_name) == 0)
			break;
	}
	if(i >= nentries)
		return -1;
	
	snprintf(buf, sizeof(buf), "%s", trash_list[i].path);
	snprintf(full, sizeof(full), "%s/%s", cfg.trash_dir,
			trash_list[i].trash_name);
	if(perform_operation(OP_MOVE, NULL, full, trash_list[i].path) == 0)
	{
		char *msg;
		size_t len;

		cmd_group_continue();

		msg = replace_group_msg(NULL);
		len = strlen(msg);
		msg = realloc(msg, COMMAND_GROUP_INFO_LEN);

		snprintf(msg + len, COMMAND_GROUP_INFO_LEN - len, "%s%s",
				(msg[len - 2] != ':') ? ", " : "", strchr(trash_name, '_') + 1);
		replace_group_msg(msg);

		add_operation(OP_MOVE, NULL, NULL, full, buf);
		cmd_group_end();
		remove_from_trash(trash_name);
		return 0;
	}
	return -1;
}

int
remove_from_trash(const char *trash_name)
{
	int i;
	for(i = 0; i < nentries; i++)
	{
		if(strcmp(trash_list[i].trash_name, trash_name) == 0)
			break;
	}
	if(i >= nentries)
		return -1;

	free(trash_list[i].path);
	free(trash_list[i].trash_name);
	memmove(trash_list + i, trash_list + i + 1,
			sizeof(*trash_list)*((nentries - 1) - i));

	nentries--;
	return 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
