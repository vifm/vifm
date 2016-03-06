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

#ifndef VIFM__UTILS__LOG_H__
#define VIFM__UTILS__LOG_H__

#include "macros.h"

#define LOG_FUNC_ENTER \
    { \
        log_prefix(__FILE__, __FUNCTION__, __LINE__); \
        log_msg("Entered into this function"); \
    }

#define LOG_INFO_MSG(msg, args...) \
    { \
        log_prefix(__FILE__, __FUNCTION__, __LINE__); \
        log_vifm_state(); \
        log_msg((msg), ## args); \
    }

#define LOG_ERROR_MSG(msg, args...) \
    { \
        log_prefix(__FILE__, __FUNCTION__, __LINE__); \
        log_msg((msg), ## args); \
    }

#define LOG_SERROR(no) log_serror(__FILE__, __FUNCTION__, __LINE__, (no))
#define LOG_SERROR_MSG(no, msg, args...) \
    { \
        log_serror(__FILE__, __FUNCTION__, __LINE__, (no)); \
        log_msg((msg), ## args); \
    }

#ifdef _WIN32
#define LOG_WERROR(no) log_werror(__FILE__, __FUNCTION__, __LINE__, (no))
void log_werror(const char *file, const char *func, int line, int no);
#endif

void init_logger(int verbosity_level, const char log_path[]);
void reinit_logger(const char log_path[]);
void log_prefix(const char *file, const char *func, int line);
void log_vifm_state(void);
void log_serror(const char *file, const char *func, int line, int no);
void log_msg(const char msg[], ...) _gnuc_printf(1, 2);
void log_cwd(void);

#endif /* VIFM__UTILS__LOG_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
