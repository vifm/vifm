/*
 *  mntent
 *  mntent.h - compatibility header for FreeBSD
 *
 *  Copyright (c) 2001 David Rufino <daverufino@btinternet.com>
 *  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef VIFM__UTILS__MNTENT_H__
#define VIFM__UTILS__MNTENT_H__

#if defined(HAVE_MNTENT_H) && HAVE_MNTENT_H
/* We have a usable <mntent.h>. */

#include <mntent.h>

#elif defined(HAVE_GETMNTINFO) && HAVE_GETMNTINFO
/* We can emulate *mntent() using getmntinfo(). */

#include <stddef.h>
#include <stdio.h>

#define MOUNTED "dummy"

#define MNTTYPE_NFS "nfs"

struct mntent
{
	char *mnt_fsname;
	char *mnt_dir;
	char *mnt_type;
	char *mnt_opts;
	int mnt_freq;
	int mnt_passno;
};

#define setmntent(x,y) ((FILE *)0x1)
struct mntent * getmntent(FILE *fp);
char * hasmntopt(const struct mntent *mnt, const char option[]);
#define endmntent(x) ((int)1)

#else
/* The best we can do is provide stubs that do nothing. */

struct mntent
{
#ifndef _WIN32
	char *mnt_fsname;
	char *mnt_dir;
	char *mnt_type;
	char *mnt_opts;
	int mnt_freq;
	int mnt_passno;
#else
	const char *mnt_dir;
	const char *mnt_type;
#endif
};

#define setmntent(x,y) ((FILE *)0x1)
#define getmntent(x) ((struct mntent *)NULL)
#define hasmntopt(x,y) ((char *)NULL)
#define endmntent(x) ((int)1)

#endif

#endif /* VIFM__UTILS__MNTENT_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
