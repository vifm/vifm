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

#ifndef __MACROS_H__
#define __MACROS_H__

/* some useful macros */

/* for portable use of GNUC extensions (see Robert Love's book about system
 * programming in Linux) */
#if __GNUC__ >= 3
#undef inline
#define inline inline   __attribute__ ((always_inline))
#define __noinline__    __attribute__ ((noinline))
#define __pure__        __attribute__ ((pure))
#define __const__       __attribute__ ((const))
#define __noreturn__    __attribute__ ((noreturn))
#define __malloc__      __attribute__ ((malloc))
#define __must_check__  __attribute__ ((must_check))
#define __deprecated__  __attribute__ ((deprecated))
#define __used__        __attribute__ ((used))
#define __unused__      __attribute__ ((unused))
#define __packed__      __attribute__ ((packed))
#define __align__(x)    __attribute__ ((aligned(x)))
#define __align_max__   __attribute__ ((aligned))
#define __likely__      __builtin_expect (!!(x), 1)
#define __unlikely__    __builtin_expect (!!(x), 0)
#else
#define __noinline__
#define __pure__
#define __const__
#define __noreturn__
#define __malloc__
#define __must_check__
#define __deprecated__
#define __used__
#define __unused__
#define __packed__
#define __align__
#define __align_max__
#define __likely__      (x)
#define __unlikely__    (x)
#endif

#define ARRAY_LEN(x) (sizeof(x)/sizeof((x)[0]))

#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))

#endif
