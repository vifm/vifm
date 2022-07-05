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

#include "fops_rename.h"

#include <assert.h> /* assert() */
#include <ctype.h> /* isdigit() */
#include <string.h> /* memset() strcmp() strdup() strlen() */

#include "compat/os.h"
#include "modes/dialogs/msg_dialog.h"
#include "ui/fileview.h"
#include "ui/statusbar.h"
#include "utils/fs.h"
#include "utils/path.h"
#include "utils/regexp.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/utils.h"
#include "cmd_completion.h"
#include "filelist.h"
#include "flist_sel.h"
#include "fops_common.h"
#include "undo.h"

/* What to do with rename candidate name (old name and new name). */
typedef enum
{
	RA_SKIP,   /* Skip rename (when new name matches the old one). */
	RA_FAIL,   /* Abort renaming (status bar error was printed). */
	RA_RENAME, /* Rename this file. */
}
RenameAction;

static void rename_file_cb(const char new_name[]);
static int complete_filename_only(const char str[], void *arg);
static char ** list_files_to_rename(view_t *view, int recursive, int *len);
static int verify_list(char *files[], int nfiles, char *names[], int nnames,
		char **error, void *data);
static char ** add_files_to_list(const char base[], const char path[],
		char *files[], int *len);
static int perform_renaming(view_t *view, char *files[], char is_dup[], int len,
		char *dst[]);
TSTATIC const char * incdec_name(const char fname[], int k);
static int count_digits(int number);
static const char * substitute_tr(const char name[], const char pattern[],
		const char sub[]);
static RenameAction check_rename(const char old_fname[], const char new_fname[],
		char **dest, int ndest);
static int rename_marked(view_t *view, const char desc[], const char lhs[],
		const char rhs[], char **dest);

/* Temporary storage for extension of file being renamed in name-only mode. */
static char rename_file_ext[NAME_MAX + 1];

void
fops_rename_current(view_t *view, int name_only)
{
	const dir_entry_t *const curr = get_current_entry(view);
	char filename[strlen(curr->name) + 1];

	if(!fops_view_can_be_changed(view))
	{
		return;
	}
	if(fentry_is_fake(curr))
	{
		return;
	}
	if(is_parent_dir(curr->name))
	{
		show_error_msg("Rename error",
				"You can't rename parent directory this way");
		return;
	}

	copy_str(filename, sizeof(filename), curr->name);
	if(name_only)
	{
		copy_str(rename_file_ext, sizeof(rename_file_ext), cut_extension(filename));
	}
	else
	{
		rename_file_ext[0] = '\0';
	}

	flist_sel_stash(view);
	fops_line_prompt(name_only ? "New name root: " : "New full name: ", filename,
			&rename_file_cb, &complete_filename_only, 1);
}

/* Callback for processing file rename query. */
static void
rename_file_cb(const char new_name[])
{
	char buf[MAX(COMMAND_GROUP_INFO_LEN, 10 + NAME_MAX + 1)];
	char new[strlen(new_name) + 1 + strlen(rename_file_ext) + 1 + 1];
	int mv_res;
	dir_entry_t *const curr = get_current_entry(curr_view);
	const char *const fname = curr->name;
	const char *const forigin = curr->origin;

	if(is_null_or_empty(new_name))
	{
		return;
	}

	if(contains_slash(new_name))
	{
		ui_sb_err("Name can not contain slash");
		curr_stats.save_msg = 1;
		return;
	}

	snprintf(new, sizeof(new), "%s%s%s", new_name,
			(rename_file_ext[0] == '\0') ? "" : ".", rename_file_ext);

	char *error = NULL;
	if(fops_check_file_rename(forigin, fname, new, &error) <= 0)
	{
		if(error != NULL)
		{
			show_error_msg("Rename error", error);
			free(error);
		}
		return;
	}

	snprintf(buf, sizeof(buf), "rename in %s: %s to %s",
			replace_home_part(forigin), fname, new);
	un_group_open(buf);
	mv_res = fops_mv_file(fname, forigin, new, forigin, OP_MOVE, 1, NULL);
	un_group_close();
	if(mv_res != 0)
	{
		show_error_msg("Rename Error", "Rename operation failed");
		return;
	}

	/* Rename file in internal structures for correct positioning of cursor after
	 * reloading, as cursor will be positioned on the file with the same name. */
	fentry_rename(curr_view, curr, new);

	ui_view_schedule_reload(curr_view);
}

/* Command-line file name completion callback.  Returns completion start
 * offset. */
static int
complete_filename_only(const char str[], void *arg)
{
	return filename_completion(str, CT_FILE_WOE, 0);
}

int
fops_rename(view_t *view, char *list[], int nlines, int recursive)
{
	/* Allow list of names in tests. */
	if(curr_stats.load_stage != 0 && recursive && nlines != 0)
	{
		ui_sb_err("Recursive rename doesn't accept list of new names");
		return 1;
	}
	if(!fops_view_can_be_changed(view))
	{
		return 0;
	}

	int nfiles;
	char **files = list_files_to_rename(view, recursive, &nfiles);
	/* No files to process. */
	if(nfiles == 0)
	{
		ui_sb_msg("0 files renamed");
		return 1;
	}

	char *is_dup = calloc(nfiles, 1);
	if(is_dup == NULL)
	{
		free_string_array(files, nfiles);
		show_error_msg("Memory Error", "Unable to allocate enough memory");
		return 0;
	}

	/* If we weren't given list of new file names, try to obtain it from the
	 * user. */
	const int from_file = (nlines == 0);
	if(from_file)
	{
		list = fops_query_list(nfiles, files, &nlines, 0, &verify_list, NULL);
	}

	if(nlines == 0)
	{
		ui_sb_msg("0 files renamed");
	}
	else
	{
		char *error_str = NULL;
		if(verify_list(files, nfiles, list, nlines, &error_str, is_dup))
		{
			const int renamed = perform_renaming(view, files, is_dup, nfiles, list);
			if(renamed >= 0)
			{
				ui_sb_msgf("%d file%s renamed", renamed, (renamed == 1) ? "" : "s");
			}

			flist_sel_stash(view);
			redraw_view(view);
		}
		else if(error_str != NULL)
		{
			ui_sb_err(error_str);
		}
	}

	if(from_file)
	{
		free_string_array(list, nlines);
	}
	free_string_array(files, nfiles);
	free(is_dup);

	return 1;
}

/* Makes list of files to be renamed.  Always sets *len.  Returns list of files,
 * which might be empty (NULL). */
static char **
list_files_to_rename(view_t *view, int recursive, int *len)
{
	*len = 0;

	char **files = NULL;
	dir_entry_t *entry = NULL;
	while(iter_marked_entries(view, &entry))
	{
		char path[PATH_MAX + 1];
		get_short_path_of(view, entry, NF_NONE, 0, sizeof(path), path);

		if(recursive)
		{
			files = add_files_to_list(flist_get_dir(view), path, files, len);
		}
		else
		{
			*len = add_to_string_array(&files, *len, path);
		}
	}

	return files;
}

/* Checks that renaming can be performed.  Returns non-zero if so, otherwise
 * zero is returned along with setting *error. */
static int
verify_list(char *files[], int nfiles, char *names[], int nnames, char **error,
		void *data)
{
	char *is_dup = data;
	if(is_dup == NULL)
	{
		is_dup = calloc(nfiles, 1);
	}

	int ok = fops_is_name_list_ok(nfiles, nnames, names, files, error)
	      && fops_is_rename_list_ok(files, is_dup, nfiles, names, error);

	if(data == NULL)
	{
		free(is_dup);
	}
	return ok;
}

/* Appends files inside of the specified path to the list of the length *len.
 * Returns new list pointer. */
static char **
add_files_to_list(const char base[], const char path[], char *files[], int *len)
{
	DIR *dir;
	struct dirent *dentry;
	const char *slash = "";

	char full_path[PATH_MAX + 1];
	to_canonic_path(path, base, full_path, sizeof(full_path));

	if(!is_dir(full_path))
	{
		*len = add_to_string_array(&files, *len, path);
		return files;
	}

	dir = os_opendir(full_path);
	if(dir == NULL)
		return files;

	if(path[strlen(path) - 1] != '/')
		slash = "/";

	while((dentry = os_readdir(dir)) != NULL)
	{
		if(!is_builtin_dir(dentry->d_name))
		{
			char buf[PATH_MAX + 1];
			snprintf(buf, sizeof(buf), "%s%s%s", path, slash, dentry->d_name);
			files = add_files_to_list(base, buf, files, len);
		}
	}

	os_closedir(dir);
	return files;
}

/* Renames files named files in current directory of the view to dst.  is_dup
 * marks elements that are in both lists.  Lengths of all lists must be equal to
 * len.  Returns number of renamed files. */
static int
perform_renaming(view_t *view, char *files[], char is_dup[], int len,
		char *dst[])
{
	char undo_msg[MAX(10 + NAME_MAX, COMMAND_GROUP_INFO_LEN) + 1];
	size_t undo_msg_len;
	int i;
	int renamed = 0;
	char **const orig_names = calloc(len, sizeof(*orig_names));
	const char *const curr_dir = flist_get_dir(view);

	snprintf(undo_msg, sizeof(undo_msg), "rename in %s: ",
			replace_home_part(curr_dir));
	undo_msg_len = strlen(undo_msg);

	for(i = 0; i < len && undo_msg_len < COMMAND_GROUP_INFO_LEN; ++i)
	{
		if(undo_msg[undo_msg_len - 2U] != ':')
		{
			strncat(undo_msg, ", ", sizeof(undo_msg) - undo_msg_len - 1U);
			undo_msg_len = strlen(undo_msg);
		}
		snprintf(undo_msg + undo_msg_len, sizeof(undo_msg) - undo_msg_len,
				"%s to %s", files[i], dst[i]);
		undo_msg_len += strlen(undo_msg + undo_msg_len);
	}

	un_group_open(undo_msg);

	/* Stage 1: give files that are in both source and destination lists temporary
	 *          names. */
	for(i = 0; i < len; ++i)
	{
		const char *unique_name;

		if(dst[i][0] == '\0')
			continue;
		if(strcmp(dst[i], files[i]) == 0)
			continue;
		if(!is_dup[i])
			continue;

		unique_name = make_name_unique(files[i]);
		if(fops_mv_file(files[i], curr_dir, unique_name, curr_dir, OP_MOVETMP2, 1,
					NULL) != 0)
		{
			un_group_close();
			if(!un_last_group_empty())
			{
				un_group_undo();
			}
			show_error_msg("Rename", "Failed to perform temporary rename");
			curr_stats.save_msg = 1;
			free_string_array(orig_names, len);
			return 0;
		}
		orig_names[i] = files[i];
		files[i] = strdup(unique_name);
	}

	/* Stage 2: rename all files (including those renamed at Stage 1) to their
	 *          final names. */
	for(i = 0; i < len; ++i)
	{
		if(dst[i][0] == '\0')
			continue;
		if(strcmp(dst[i], files[i]) == 0)
			continue;

		if(fops_mv_file(files[i], curr_dir, dst[i], curr_dir,
				is_dup[i] ? OP_MOVETMP1 : OP_MOVE, 1, NULL) == 0)
		{
			char path[PATH_MAX + 1];
			dir_entry_t *entry;
			const char *const old_name = is_dup[i] ? orig_names[i] : files[i];
			const char *new_name;

			++renamed;

			to_canonic_path(old_name, curr_dir, path, sizeof(path));
			entry = entry_from_path(view, view->dir_entry, view->list_rows, path);
			if(entry == NULL)
			{
				continue;
			}

			new_name = get_last_path_component(dst[i]);

			/* For regular views rename file in internal structures for correct
			 * positioning of cursor after reloading.  For custom views rename to
			 * prevent files from disappearing. */
			fentry_rename(view, entry, new_name);

			if(flist_custom_active(view))
			{
				entry = entry_from_path(view, view->custom.entries,
						view->custom.entry_count, path);
				if(entry != NULL)
				{
					fentry_rename(view, entry, new_name);
				}
			}
		}
	}

	un_group_close();

	free_string_array(orig_names, len);
	return renamed;
}

int
fops_incdec(view_t *view, int k)
{
	size_t names_len = 0;
	char **names = NULL;
	size_t tmp_len = 0;
	char **tmp_names = NULL;
	char undo_msg[COMMAND_GROUP_INFO_LEN];
	dir_entry_t *entry;
	int i;
	int err, nrenames, nrenamed;

	snprintf(undo_msg, sizeof(undo_msg), "<c-a> in %s: ",
			replace_home_part(flist_get_dir(view)));
	fops_append_marked_files(view, undo_msg, NULL);

	entry = NULL;
	while(iter_marked_entries(view, &entry))
	{
		char full_path[PATH_MAX + 1];

		if(strpbrk(entry->name, "0123456789") == NULL)
		{
			entry->marked = 0;
			continue;
		}

		get_full_path_of(entry, sizeof(full_path), full_path);

		names_len = add_to_string_array(&names, names_len, full_path);
		tmp_len = add_to_string_array(&tmp_names, tmp_len,
				make_name_unique(entry->name));
	}

	err = 0;

	entry = NULL;
	while(iter_marked_entries(view, &entry))
	{
		char new_path[PATH_MAX + 1];
		const char *const new_fname = incdec_name(entry->name, k);

		snprintf(new_path, sizeof(new_path), "%s/%s", entry->origin, new_fname);

		/* Skip fops_check_file_rename() for final name that matches one of the
		 * original names. */
		if(is_in_string_array_os(names, names_len, new_path))
		{
			continue;
		}

		char *error = NULL;
		if(fops_check_file_rename(entry->origin, entry->name, new_fname,
					&error) != 0)
		{
			continue;
		}

		ui_sb_err(error);
		free(error);
		err = -1;
		break;
	}

	free_string_array(names, names_len);

	nrenames = 0;
	nrenamed = 0;

	/* Two-step renaming. */
	un_group_open(undo_msg);

	entry = NULL;
	i = 0;
	while(!err && iter_marked_entries(view, &entry))
	{
		const char *const path = entry->origin;
		/* Rename: <original name> -> <temporary name>. */
		if(fops_mv_file(entry->name, path, tmp_names[i++], path, OP_MOVETMP4, 1,
					NULL) != 0)
		{
			err = 1;
			break;
		}
		++nrenames;
	}

	entry = NULL;
	i = 0;
	while(!err && iter_marked_entries(view, &entry))
	{
		const char *const path = entry->origin;
		const char *const new_fname = incdec_name(entry->name, k);
		/* Rename: <temporary name> -> <final name>. */
		if(fops_mv_file(tmp_names[i++], path, new_fname, path, OP_MOVETMP3, 1,
					NULL) != 0)
		{
			err = 1;
			break;
		}
		fentry_rename(view, entry, new_fname);
		++nrenames;
		++nrenamed;
	}

	un_group_close();

	free_string_array(tmp_names, tmp_len);

	if(err)
	{
		if(err > 0 && !un_last_group_empty())
		{
			un_group_undo();
		}
	}

	if(nrenames > 0)
	{
		ui_view_schedule_reload(view);
	}

	if(err > 0)
	{
		ui_sb_err("Rename error");
	}
	else if(err == 0)
	{
		ui_sb_msgf("%d file%s renamed", nrenamed, (nrenamed == 1) ? "" : "s");
	}

	return 1;
}

/* Increments/decrements first number in fname k time, if any. Returns pointer
 * to statically allocated buffer. */
TSTATIC const char *
incdec_name(const char fname[], int k)
{
	static char result[NAME_MAX + 1];
	char format[16];
	char *b, *e;
	int i, n;

	b = strpbrk(fname, "0123456789");
	if(b == NULL)
	{
		copy_str(result, sizeof(result), fname);
		return result;
	}

	n = 0;
	while(b[n] == '0' && isdigit(b[n + 1]))
	{
		++n;
	}

	if(b != fname && b[-1] == '-')
	{
		--b;
	}

	i = strtol(b, &e, 10);

	if(i + k < 0)
	{
		++n;
	}

	copy_str(result, b - fname + 1, fname);
	snprintf(format, sizeof(format), "%%0%dd%%s", n + count_digits(i));
	snprintf(result + (b - fname), sizeof(result) - (b - fname), format, i + k,
			e);

	return result;
}

/* Counts number of digits in passed number.  Returns the count. */
static int
count_digits(int number)
{
	int result = 0;
	while(number != 0)
	{
		number /= 10;
		result++;
	}
	return MAX(1, result);
}

int
fops_case(view_t *view, int to_upper)
{
	char **dest;
	int ndest;
	dir_entry_t *entry;
	int save_msg;
	int err;

	if(!fops_view_can_be_changed(view))
	{
		return 0;
	}

	entry = NULL;
	ndest = 0;
	dest = NULL;
	err = 0;
	while(iter_marked_entries(view, &entry))
	{
		const char *const old_fname = entry->name;
		char new_fname[NAME_MAX + 1];

		/* Ignore too small buffer errors by not caring about part that didn't
		 * fit. */
		if(to_upper)
		{
			(void)str_to_upper(old_fname, new_fname, sizeof(new_fname));
		}
		else
		{
			(void)str_to_lower(old_fname, new_fname, sizeof(new_fname));
		}

		if(strcmp(new_fname, old_fname) == 0)
		{
			entry->marked = 0;
			continue;
		}

		if(is_in_string_array(dest, ndest, new_fname))
		{
			ui_sb_errf("Name \"%s\" duplicates", new_fname);
			err = 1;
			break;
		}
		if(path_exists(new_fname, NODEREF) && !is_case_change(new_fname, old_fname))
		{
			ui_sb_errf("File \"%s\" already exists", new_fname);
			err = 1;
			break;
		}

		ndest = add_to_string_array(&dest, ndest, new_fname);
	}

	if(err)
	{
		save_msg = 1;
	}
	else
	{
		save_msg = rename_marked(view, to_upper ? "gU" : "gu", NULL, NULL, dest);
	}

	free_string_array(dest, ndest);

	return save_msg;
}

int
fops_subst(view_t *view, const char pattern[], const char sub[], int ic,
		int glob)
{
	regex_t re;
	char **dest;
	int ndest;
	int cflags;
	dir_entry_t *entry;
	int err, save_msg;

	if(!fops_view_can_be_changed(view))
	{
		return 0;
	}

	if(ic == 0)
	{
		cflags = get_regexp_cflags(pattern);
	}
	else if(ic > 0)
	{
		cflags = REG_EXTENDED | REG_ICASE;
	}
	else
	{
		cflags = REG_EXTENDED;
	}

	if((err = regexp_compile(&re, pattern, cflags)) != 0)
	{
		ui_sb_errf("Regexp error: %s", get_regexp_error(err, &re));
		regfree(&re);
		return 1;
	}

	entry = NULL;
	ndest = 0;
	dest = NULL;
	err = 0;
	while(iter_marked_entries(view, &entry) && !err)
	{
		const char *new_fname;
		regmatch_t matches[10];
		RenameAction action;

		if(regexec(&re, entry->name, ARRAY_LEN(matches), matches, 0) != 0)
		{
			entry->marked = 0;
			continue;
		}

		if(glob && pattern[0] != '^')
		{
			new_fname = regexp_gsubst(&re, entry->name, sub, matches);
		}
		else
		{
			new_fname = regexp_subst(entry->name, sub, matches, NULL);
		}

		action = check_rename(entry->name, new_fname, dest, ndest);
		switch(action)
		{
			case RA_SKIP:
				entry->marked = 0;
				continue;
			case RA_FAIL:
				err = 1;
				break;
			case RA_RENAME:
				ndest = add_to_string_array(&dest, ndest, new_fname);
				break;

			default:
				assert(0 && "Unhandled rename action.");
				break;
		}
	}

	regfree(&re);

	if(err)
	{
		save_msg = 1;
	}
	else
	{
		save_msg = rename_marked(view, "s", pattern, sub, dest);
	}

	free_string_array(dest, ndest);

	return save_msg;
}

int
fops_tr(view_t *view, const char from[], const char to[])
{
	char **dest;
	int ndest;
	dir_entry_t *entry;
	int err, save_msg;

	assert(strlen(from) == strlen(to) && "Lengths don't match.");

	if(!fops_view_can_be_changed(view))
	{
		return 0;
	}

	entry = NULL;
	ndest = 0;
	dest = NULL;
	err = 0;
	while(iter_marked_entries(view, &entry) && !err)
	{
		const char *new_fname;
		RenameAction action;

		new_fname = substitute_tr(entry->name, from, to);

		action = check_rename(entry->name, new_fname, dest, ndest);
		switch(action)
		{
			case RA_SKIP:
				entry->marked = 0;
				continue;
			case RA_FAIL:
				err = 1;
				break;
			case RA_RENAME:
				ndest = add_to_string_array(&dest, ndest, new_fname);
				break;

			default:
				assert(0 && "Unhandled rename action.");
				break;
		}
	}

	if(err)
	{
		save_msg = 1;
	}
	else
	{
		save_msg = rename_marked(view, "t", from, to, dest);
	}

	free_string_array(dest, ndest);

	return save_msg;
}

/* Performs tr-like substitution in the string.  Returns pointer to a statically
 * allocated buffer. */
static const char *
substitute_tr(const char name[], const char pattern[], const char sub[])
{
	static char buf[NAME_MAX + 1];
	char *p = buf;
	while(*name != '\0')
	{
		const char *const t = strchr(pattern, *name);
		if(t != NULL)
			*p++ = sub[t - pattern];
		else
			*p++ = *name;
		++name;
	}
	*p = '\0';
	return buf;
}

/* Evaluates possibility of renaming old_fname to new_fname.  Returns
 * resolution. */
static RenameAction
check_rename(const char old_fname[], const char new_fname[], char **dest,
		int ndest)
{
	/* Compare case sensitive strings even on Windows to let user rename file
	 * changing only case of some characters. */
	if(strcmp(old_fname, new_fname) == 0)
	{
		return RA_SKIP;
	}

	if(is_in_string_array(dest, ndest, new_fname))
	{
		ui_sb_errf("Name \"%s\" duplicates", new_fname);
		return RA_FAIL;
	}
	if(new_fname[0] == '\0')
	{
		ui_sb_errf("Destination name of \"%s\" is empty", old_fname);
		return RA_FAIL;
	}
	if(contains_slash(new_fname))
	{
		ui_sb_errf("Destination name \"%s\" contains slash", new_fname);
		return RA_FAIL;
	}
	if(path_exists(new_fname, NODEREF))
	{
		ui_sb_errf("File \"%s\" already exists", new_fname);
		return RA_FAIL;
	}

	return RA_RENAME;
}

/* Renames marked files using corresponding entries of the dest array.  lhs and
 * rhs can be NULL to omit their printing (both at the same time).  Returns new
 * value for save_msg flag. */
static int
rename_marked(view_t *view, const char desc[], const char lhs[],
		const char rhs[], char **dest)
{
	int i;
	int nrenamed;
	char undo_msg[COMMAND_GROUP_INFO_LEN + 1];
	dir_entry_t *entry;

	if(lhs == NULL && rhs == NULL)
	{
		snprintf(undo_msg, sizeof(undo_msg), "%s in %s: ", desc,
				replace_home_part(flist_get_dir(view)));
	}
	else
	{
		snprintf(undo_msg, sizeof(undo_msg), "%s/%s/%s/ in %s: ", desc, lhs, rhs,
				replace_home_part(flist_get_dir(view)));
	}
	fops_append_marked_files(view, undo_msg, NULL);
	un_group_open(undo_msg);

	nrenamed = 0;
	i = 0;
	entry = NULL;
	while(iter_marked_entries(view, &entry))
	{
		const char *const new_fname = dest[i++];
		if(fops_mv_file(entry->name, entry->origin, new_fname, entry->origin,
					OP_MOVE, 1, NULL) == 0)
		{
			fentry_rename(view, entry, new_fname);
			++nrenamed;
		}
	}

	un_group_close();
	ui_sb_msgf("%d file%s renamed", nrenamed, (nrenamed == 1) ? "" : "s");

	return 1;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
