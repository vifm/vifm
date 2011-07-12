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

#ifndef __LOG_H__
#define __LOG_H__

#define LOG_ERROR_MSG(msg, args...) \
    { \
        log_error(__FILE__, __FUNCTION__, __LINE__); \
        log_msg((msg), ## args); \
    }
#define LOG_SERROR(no) log_error(__FILE__, __FUNCTION__, __LINE__, (no))
#define LOG_SERROR_MSG(no, msg, args...) \
    { \
        log_serror(__FILE__, __FUNCTION__, __LINE__, (no)); \
        log_msg((msg), ## args); \
    }

void init_logger(int verbosity_level);
void log_error(const char *file, const char *func, int line);
void log_serror(const char *file, const char *func, int line, int no);
void log_msg(const char *msg, ...);
void log_cwd(void);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
