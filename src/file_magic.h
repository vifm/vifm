#if defined(HAVE_LIBGTK) || defined(HAVE_LIBMAGIC)

#ifndef __MAGIC_H__
#define __MAGIC_H__

const char * get_mimetype(const char *file);
char * get_magic_handlers(const char *file);

#endif

#endif /* #if defined(HAVE_LIBGTK) || defined(HAVE_LIBMAGIC) */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
