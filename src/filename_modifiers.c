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

#include <stddef.h> /* size_t */
#include <stdio.h> /* snprintf() */
#include <string.h> /* strchr() strlen() strrchr() */

#include "cfg/config.h"
#include "utils/fs_limits.h"
#include "utils/path.h"
#include "utils/str.h"
#include "fileops.h"
#include "ui.h"

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
static const char * find_nth_chr(const char *str, char c, int n);

const char *
apply_mods(const char path[], const char parent[], const char mod[],
		int for_shell)
{
	static char buf[PATH_MAX];
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

#ifdef _WIN32
	/* this is needed to run something like explorer.exe, which isn't smart enough
	 * to understand forward slashes */
	if(napplied == 0 && for_shell && stroscmp(cfg.shell, "cmd") != 0)
		to_back_slash(buf);
#endif

	return buf;
}

/* Applies one filename modifiers per call. */
static const char *
apply_mod(const char *path, const char *parent, const char *mod, int *mod_len,
		int for_shell)
{
	char path_buf[PATH_MAX];
	static char buf[PATH_MAX];

	snprintf(path_buf, sizeof(path_buf), "%s", path);
#ifdef _WIN32
	to_forward_slash(path_buf);
#endif

	*mod_len = 2;
	if(strncmp(mod, ":p", 2) == 0)
		*mod_len += apply_p_mod(path_buf, parent, buf, sizeof(buf));
	else if(strncmp(mod, ":~", 2) == 0)
		*mod_len += apply_tilde_mod(path_buf, buf, sizeof(buf));
	else if(strncmp(mod, ":.", 2) == 0)
		*mod_len += apply_dot_mod(path_buf, buf, sizeof(buf));
	else if(strncmp(mod, ":h", 2) == 0)
		*mod_len += apply_h_mod(path_buf, buf, sizeof(buf));
#ifdef _WIN32
	else if(strncmp(mod, ":u", 2) == 0)
		*mod_len += apply_u_mod(path_buf, buf, sizeof(buf));
#endif
	else if(strncmp(mod, ":t", 2) == 0)
		*mod_len += apply_t_mod(path_buf, buf, sizeof(buf));
	else if(strncmp(mod, ":r", 2) == 0)
		*mod_len += apply_r_mod(path_buf, buf, sizeof(buf));
	else if(strncmp(mod, ":e", 2) == 0)
		*mod_len += apply_e_mod(path_buf, buf, sizeof(buf));
	else if(strncmp(mod, ":s", 2) == 0 || strncmp(mod, ":gs", 3) == 0)
		*mod_len += apply_s_gs_mod(path_buf, mod, buf, sizeof(buf));
	else
		return NULL;

#ifdef _WIN32
	/* this is needed to run something like explorer.exe, which isn't smart enough
	 * to understand forward slashes */
	if(for_shell && strncmp(mod, ":s", 2) != 0 && strncmp(mod, ":gs", 3) != 0)
	{
		if(stroscmp(cfg.shell, "cmd") != 0)
			to_back_slash(buf);
	}
#endif

	return buf;
}

/* Implementation of :p filename modifier. */
static int
apply_p_mod(const char *path, const char *parent, char *buf, size_t buf_len)
{
	size_t len;
	if(is_path_absolute(path))
	{
		snprintf(buf, buf_len, "%s", path);
		return 0;
	}

	snprintf(buf, buf_len, "%s", parent);
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
		snprintf(buf, buf_len, "%s", path);
		return 0;
	}

	snprintf(buf, buf_len, "~%s", path + home_len - 1);
	return 0;
}

/* Implementation of :. filename modifier. */
static int
apply_dot_mod(const char *path, char *buf, size_t buf_len)
{
	size_t len = strlen(curr_view->curr_dir);
	if(strnoscmp(path, curr_view->curr_dir, len) != 0 || path[len] == '\0')
		snprintf(buf, buf_len, "%s", path);
	else
		snprintf(buf, buf_len, "%s", path + len + 1);
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
		snprintf(buf, buf_len, "%s", path);
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
	snprintf(buf, buf_len, "%s", path);
	break_at(buf + 2, '/');
	return 0;
}
#endif

/* Implementation of :t filename modifier. */
static int
apply_t_mod(const char *path, char *buf, size_t buf_len)
{
	char *p = strrchr(path, '/');
	snprintf(buf, buf_len, "%s", (p == NULL) ? path : (p + 1));
	return 0;
}

/* Implementation of :r filename modifier. */
static int
apply_r_mod(const char *path, char *buf, size_t buf_len)
{
	char *slash = strrchr(path, '/');
	char *dot = strrchr(path, '.');
	snprintf(buf, buf_len, "%s", path);
	if(dot == NULL || (slash != NULL && dot < slash) || dot == path ||
			dot == slash + 1)
		return 0;
	buf[dot - path] = '\0';
	return 0;
}

/* Implementation of :e filename modifier. */
static int
apply_e_mod(const char *path, char *buf, size_t buf_len)
{
	char *slash = strrchr(path, '/');
	char *dot = strrchr(path, '.');
	if(dot == NULL || (slash != NULL && dot < slash) || dot == path ||
			dot == slash + 1)
		snprintf(buf, buf_len, "%s", "");
	else
		snprintf(buf, buf_len, "%s", dot + 1);
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
		snprintf(buf, buf_len, "%s", path);
		return 0;
	}
	t = find_nth_chr(mod, c, 2);
	snprintf(pattern, t - (mod + 3) + 1, "%s", mod + 3);
	snprintf(sub, p - (t + 1) + 1, "%s", t + 1);
	global = (mod[0] == 'g');
	snprintf(buf, buf_len, "%s", substitute_in_name(path, pattern, sub, global));
	return (p + 1) - start - 2;
}

size_t
get_mods_len(const char *str)
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
/* vim: set cinoptions+=t0 : */
