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

#ifndef VIFM__UTILS__MACROS_H__
#define VIFM__UTILS__MACROS_H__

#include <stddef.h> /* size_t */

/* some useful macros */

/* for portable use of GNUC extensions (see Robert Love's book about system
 * programming in Linux) */
#if __GNUC__ > 3
#undef inline
#define inline inline     __attribute__ ((always_inline))
#define _gnuc_noinline    __attribute__ ((noinline))
#define _gnuc_pure        __attribute__ ((pure))
#define _gnuc_const       __attribute__ ((const))
#define _gnuc_noreturn    __attribute__ ((noreturn))
#define _gnuc_malloc      __attribute__ ((malloc))
#define _gnuc_must_check  __attribute__ ((must_check))
#define _gnuc_deprecated  __attribute__ ((deprecated))
#define _gnuc_used        __attribute__ ((used))
#define _gnuc_unused      __attribute__ ((unused))
#define _gnuc_packed      __attribute__ ((packed))
#define _gnuc_align(x)    __attribute__ ((aligned(x)))
#define _gnuc_align_max   __attribute__ ((aligned))
#define _gnuc_likely(x)   __builtin_expect (!!(x), 1)
#define _gnuc_unlikely(x) __builtin_expect (!!(x), 0)
#else
#define _gnuc_noinline
#define _gnuc_pure
#define _gnuc_const
#define _gnuc_noreturn
#define _gnuc_malloc
#define _gnuc_must_check
#define _gnuc_deprecated
#define _gnuc_used
#define _gnuc_unused
#define _gnuc_packed
#define _gnuc_align
#define _gnuc_align_max
#define _gnuc_likely(x)   (x)
#define _gnuc_unlikely(x) (x)
#endif

#define ARRAY_LEN(x) (sizeof(x)/sizeof((x)[0]))
#define ARRAY_GUARD(x, len) \
    typedef int x##_array_guard[(ARRAY_LEN(x) == (len)) ? 1 : -1]; \
    /* Fake use to suppress "Unused local variable" warning. */ \
    enum { x##_array_guard_fake_use = (size_t)(x##_array_guard*)0 }

#define MIN(a,b) ({ \
										typeof(a) _a = (a); \
										typeof(b) _b = (b); \
										(_a < _b) ? _a : _b; \
									})

#define MAX(a,b) ({ \
										typeof(a) _a = (a); \
										typeof(b) _b = (b); \
										(_a > _b) ? _a : _b; \
									})

#define DIV_ROUND_UP(a,b)  ({ \
															typeof(a) _a = (a); \
															typeof(b) _b = (b); \
															(_a + (_b - 1))/_b; \
														})

#define ROUND_DOWN(a,b)  ({ \
														typeof(a) _a = (a); \
														typeof(b) _b = (b); \
														_a - _a%_b; \
													})

#endif /* VIFM__UTILS__MACROS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
