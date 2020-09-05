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

#include <windows.h>

#include <stdio.h>
#include <string.h>

static int create_symlink(const char *link, const char *target);
static int is_dir(const char *file);

int
main(int argc, char **argv)
{
	if(argc > 1 && strcmp(argv[1], "-s") == 0)
	{
		if(argc != 4)
		{
			puts("Invalid number of arguments, should be 3");
			return 1;
		}
		return create_symlink(argv[3], argv[2]);
	}
	else
	{
		puts("Invalid first argument, should be -s");
	}
	return 1;
}

static int
create_symlink(const char *link, const char *target)
{
	DWORD flag;

	flag = is_dir(target) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;

	if(!CreateSymbolicLink((char *)link, (char *)target, flag))
	{
		return -1;
	}

	return 0;
}

static int
is_dir(const char *file)
{
	DWORD attr;

	attr = GetFileAttributesA(file);
	if(attr == INVALID_FILE_ATTRIBUTES)
		return 0;

	return (attr & FILE_ATTRIBUTE_DIRECTORY);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
