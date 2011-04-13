/* vifm
 * Copyright (C) 2001 Ken Steen.
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

#ifndef __UTILS_H__
#define __UTILS_H__

#include "ui.h"

/*_SZ_BEGIN_*/
typedef struct Fuse_List {
	char source_file_name[PATH_MAX];
	char source_file_dir[PATH_MAX];
	char mount_point[PATH_MAX];
	int mount_point_id;
	struct Fuse_List *next;
} Fuse_List;

extern struct Fuse_List *fuse_mounts;
/*_SZ_END_*/

void chomp(char *text);
void * duplicate(void *stuff, int size);
void my_free(void *);
int is_dir(char *file);
char * escape_filename(const char *string, size_t len, int quote_percent);
int write_string_to_file(char *filename, char *string);
size_t guess_char_width(char c);
size_t get_char_width(char* string);
size_t get_real_string_width(char *string, size_t max_len);
size_t get_utf8_string_length(char *string);
size_t get_utf8_prev_width(char *string, size_t cut_length);
#endif
