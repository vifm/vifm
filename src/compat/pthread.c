/* vifm
 * Copyright (C) 2016 xaizek.
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

#include "pthread.h"

#ifdef __APPLE__

#include <assert.h> /* assert() */
#include <errno.h> /* EBUSY */

/* This shim is a slightly modified version of code from here:
 * https://idea.popcount.org/2012-09-12-reinventing-spinlocks/ by Marek */

int
pthread_spin_init(pthread_spinlock_t *lock, int pshared)
{
	assert(pshared == PTHREAD_PROCESS_PRIVATE &&
			"Only private spinlocks are shimmed!");
	__asm__ __volatile__("" ::: "memory");
	*lock = 0;
	return 0;
}

int
pthread_spin_destroy(pthread_spinlock_t *lock)
{
	return 0;
}

int
pthread_spin_lock(pthread_spinlock_t *lock)
{
	enum { NTRIES = 10000 };

	while(1)
	{
		int i;
		for(i = 0; i < NTRIES; ++i)
		{
			if(__sync_bool_compare_and_swap(lock, 0, 1))
			{
				return 0;
			}
		}
		sched_yield();
	}
	return EBUSY;
}

int
pthread_spin_trylock(pthread_spinlock_t *lock)
{
	if(__sync_bool_compare_and_swap(lock, 0, 1))
	{
		return 0;
	}
	return EBUSY;
}

int
pthread_spin_unlock(pthread_spinlock_t *lock)
{
	__asm__ __volatile__("" ::: "memory");
	*lock = 0;
	return 0;
}

#endif /* __APPLE__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
