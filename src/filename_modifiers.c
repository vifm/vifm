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

#include "filename_modifiers.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <stddef.h> /* size_t */
#include <stdio.h> /* snprintf() */
#include <string.h> /* memmove() strchr() strlen() strrchr() */

#include "cfg/config.h"
#include "compat/fs_limits.h"
#include "ui/ui.h"
#include "utils/path.h"
#include "utils/regexp.h"
#include "utils/str.h"
#include "filelist.h"
#include "status.h"

static const char * apply_mod(const char *path, const char *parent,
		const char *mod, int *mod_len, int for_shell);
static int apply_p_mod(const char *path, const char *parent, char *buf,
		size_t buf_len);
static int apply_tilde_mod(const char *path, char *buf, size_t buf_len);
static int apply_dot_mod(const char *path, char *buf, size_t buf_len);
static int apply_h_mod(const char *path, char *buf, size_t buf_len);
#ifdef _WIN32
static int apply_u_mod(const char *path, char *buf, size_t buf_len);
#endif
static int apply_t_mod(const char *path, char *buf, size_t buf_len);
static int apply_r_mod(const char *path, char *buf, size_t buf_len);
static int apply_e_mod(const char *path, char *buf, size_t buf_len);
static int apply_s_gs_mod(const char *path, const char *mod,
		char *buf, size_t buf_len);
static size_t get_mod_len(const char str[]);
static const char * find_nth_chr(const char *str, char c, int n);

const char *
mods_apply(const char path[], const char parent[], const char mod[],
		int for_shell)
{
	static char buf[PATH_MAX + 1];
	int napplied = 0;

	copy_str(buf, sizeof(buf), path);
	while(*mod != '\0')
	{
		int mod_len;
		const char *const p = apply_mod(buf, parent, mod, &mod_len, for_shell);
		if(p == NULL)
		{
			break;
		}
		copy_str(buf, sizeof(buf), p);
		mod += mod_len;
		napplied++;
	}

	/* This is needed to run something like explorer.exe, which isn't smart enough
	 * to understand forward slashes. */
	if(for_shell && curr_stats.shell_type != ST_CMD && napplied == 0)
	{
		internal_to_system_slashes(buf);
	}

	return buf;
}

/* Applies one filename modifiers per call. */
static const char *
apply_mod(const char *path, const char *parent, const char *mod, int *mod_len,
		int for_shell)
{
	char path_buf[PATH_MAX + 1];
	static char buf[PATH_MAX + 16];

	copy_str(path_buf, sizeof(path_buf), path);
	system_to_internal_slashes(path_buf);

	*mod_len = 2;
	if(starts_with_lit(mod, ":p"))
		*mod_len += apply_p_mod(path_buf, parent, buf, sizeof(buf));
	else if(starts_with_lit(mod, ":~"))
		*mod_len += apply_tilde_mod(path_buf, buf, sizeof(buf));
	else if(starts_with_lit(mod, ":."))
		*mod_len += apply_dot_mod(path_buf, buf, sizeof(buf));
	else if(starts_with_lit(mod, ":h"))
		*mod_len += apply_h_mod(path_buf, buf, sizeof(buf));
#ifdef _WIN32
	else if(starts_with_lit(mod, ":u"))
		*mod_len += apply_u_mod(path_buf, buf, sizeof(buf));
#endif
	else if(starts_with_lit(mod, ":t"))
		*mod_len += apply_t_mod(path_buf, buf, sizeof(buf));
	else if(starts_with_lit(mod, ":r"))
		*mod_len += apply_r_mod(path_buf, buf, sizeof(buf));
	else if(starts_with_lit(mod, ":e"))
		*mod_len += apply_e_mod(path_buf, buf, sizeof(buf));
	else if(starts_with_lit(mod, ":s") || starts_with_lit(mod, ":gs"))
		*mod_len += apply_s_gs_mod(path_buf, mod, buf, sizeof(buf));
	else
		return NULL;

	/* This is needed to run something like explorer.exe, which isn't smart enough
	 * to understand forward slashes. */
	if(for_shell && curr_stats.shell_type != ST_CMD)
	{
		if(!starts_with_lit(mod, ":s") && !starts_with_lit(mod, ":gs"))
		{
			internal_to_system_slashes(buf);
		}
	}

	return buf;
}

/* Implementation of :p filename modifier. */
static int
apply_p_mod(const char *path, const char *parent, char *buf, size_t buf_len)
{
	size_t len;
	if(is_path_absolute(path))
	{
		copy_str(buf, buf_len, path);
		return 0;
	}

	copy_str(buf, buf_len, parent);
	chosp(buf);
	len = strlen(buf);
	snprintf(buf + len, buf_len - len, "/%s", path);
	return 0;
}

/* Implementation of :~ filename modifier. */
static int
apply_tilde_mod(const char *path, char *buf, size_t buf_len)
{
	size_t home_len = strlen(cfg.home_dir);
	if(strnoscmp(path, cfg.home_dir, home_len - 1) != 0)
	{
		copy_str(buf, buf_len, path);
		return 0;
	}

	snprintf(buf, buf_len, "~%s", path + home_len - 1);
	return 0;
}

/* Implementation of :. filename modifier. */
static int
apply_dot_mod(const char *path, char *buf, size_t buf_len)
{
	const char *curr_dir = flist_get_dir(curr_view);
	if(path_starts_with(path, curr_dir) && !paths_are_equal(path, curr_dir))
		copy_str(buf, buf_len, make_rel_path(path, curr_dir));
	else
		copy_str(buf, buf_len, path);
	return 0;
}

/* Implementation of :h filename modifier. */
static int
apply_h_mod(const char *path, char *buf, size_t buf_len)
{
	char *p = strrchr(path, '/');
	if(p == NULL)
	{
		snprintf(buf, buf_len, ".");
	}
	else
	{
		copy_str(buf, buf_len, path);
		if(!is_root_dir(path))
		{
			buf[p - path + 1] = '\0';
			if(!is_root_dir(buf))
				buf[p - path] = '\0';
		}
	}
	return 0;
}

#ifdef _WIN32
/* Implementation of :u filename modifier. */
static int
apply_u_mod(const char *path, char *buf, size_t buf_len)
{
	if(!is_unc_path(path))
	{
		DWORD size = buf_len - 2;
		snprintf(buf, buf_len, "//");
		GetComputerNameA(buf + 2, &size);
		return 0;
	}
	copy_str(buf, buf_len, path);
	break_at(buf + 2, '/');
	return 0;
}
#endif

/* Implementation of :t filename modifier. */
static int
apply_t_mod(const char *path, char *buf, size_t buf_len)
{
	char *p = strrchr(path, '/');
	copy_str(buf, buf_len, (p == NULL) ? path : (p + 1));
	return 0;
}

/* Implementation of :r filename modifier. */
static int
apply_r_mod(const char *path, char *buf, size_t buf_len)
{
	int root_len;
	const char *ext_pos;

	copy_str(buf, buf_len, path);
	split_ext(buf, &root_len, &ext_pos);

	return 0;
}

/* Implementation of :e filename modifier. */
static int
apply_e_mod(const char *path, char *buf, size_t buf_len)
{
	int root_len;
	const char *ext_pos;

	copy_str(buf, buf_len, path);
	split_ext(buf, &root_len, &ext_pos);
	memmove(buf, ext_pos, strlen(ext_pos) + 1);

	return 0;
}

/* Implementation of :s and :gs filename modifiers. */
static int
apply_s_gs_mod(const char *path, const char *mod, char *buf, size_t buf_len)
{
	char pattern[256], sub[256];
	int global;
	const char *start = mod;
	char c = (mod[1] == 'g') ? mod++[3] : mod[2];
	const char *t, *p = find_nth_chr(mod, c, 3);
	if(p == NULL)
	{
		copy_str(buf, buf_len, path);
		return 0;
	}
	t = find_nth_chr(mod, c, 2);
	copy_str(pattern, t - (mod + 3) + 1, mod + 3);
	copy_str(sub, p - (t + 1) + 1, t + 1);
	global = (mod[0] == 'g');
	copy_str(buf, buf_len, regexp_replace(path, pattern, sub, global, 0));
	return (p + 1) - start - 2;
}

size_t
mods_length(const char str[])
{
	size_t total = 0;
	while(str[total] != '\0')
	{
		size_t len = get_mod_len(&str[total]);
		if(len == 0)
		{
			break;
		}
		total += len;
	}
	return total;
}

/* Computes length of filename modifier at the beginning of the passed string.
 * Returns the length. */
static size_t
get_mod_len(const char str[])
{
	static const char FIXED_LENGTH_FILEMODS[] = "p~.htre";
	size_t result = 0;
	if(str[0] != ':')
	{
	}
	else if(char_is_one_of(FIXED_LENGTH_FILEMODS, str[1]))
	{
		result = 2;
	}
#ifdef _WIN32
	else if(str[1] == 'u')
	{
		result = 2;
	}
#endif
	else if(starts_with(str, ":s") || starts_with(str, ":gs"))
	{
		const char *p;
		result = (str[1] == 'g') ? 3 : 2;
		p = find_nth_chr(str, str[result], 3);
		if(p != NULL)
			result = (p - str) + 1;
		else
			result = strlen(str);
	}
	return result;
}

static const char *
find_nth_chr(const char *str, char c, int n)
{
	str--;
	while(n-- > 0 && (str = strchr(str + 1, c)) != NULL)
	{
	}
	return str;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
