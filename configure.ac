dnl Process this file with autoconf to produce a configure script.

AC_INIT(vifm, 0.12.1, xaizek@posteo.net, vifm, https://vifm.info)
AC_CONFIG_SRCDIR(src/vifm.c)
AC_CONFIG_AUX_DIR(build-aux)
AC_CONFIG_MACRO_DIR(build-aux/m4)
AM_INIT_AUTOMAKE([no-dist-gzip dist-bzip2])
AC_CONFIG_HEADERS(build-aux/config.h)

AC_DEFINE([PACKAGE_SYSCONF_DIR], [], [Configuration directory of the package.])
AC_DEFINE([PACKAGE_DATA_DIR], [], [Data directory of the package.])
AC_DEFINE([PACKAGE_SOURCE_DIR], [], [Source directory of the package.])

dnl Request some additional extensions.  The macro makes it possible to use
dnl features of Posix that are extensions to C, as well as platform extensions
dnl not defined by Posix.
AC_USE_SYSTEM_EXTENSIONS

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC
AM_PROG_CC_C_O
AM_SILENT_RULES([yes])

AC_LANG(C)

dnl Determine host machine type.
AC_CANONICAL_HOST
dnl Require $host variable presence.
AC_MSG_CHECKING([host])
AC_MSG_RESULT([$host])
dnl Define $OSX_HOST variable for OS X
case "$host_os" in
    macos* | darwin*)
        OSX_HOST=1
        ;;
    *)
        OSX_HOST=
        ;;
esac

dnl pass some CFLAGS to tests (TESTS_CFLAGS is also extended below)
TESTS_CFLAGS="$CFLAGS"
AC_SUBST([TESTS_CFLAGS])

AC_DEFINE([_XOPEN_SOURCE_EXTENDED], [1],
          [Define to 1 to enable wide functions of ncurses in some environments.])

dnl Check for system headers.
AC_CHECK_HEADERS([alloca.h], [AC_DEFINE([HAVE_ALLOCA_H], [1], [alloca.h header is available.])])
AC_CHECK_HEADERS([malloc.h], [AC_DEFINE([HAVE_MALLOC_H], [1], [malloc.h header is available.])])
AC_CHECK_HEADER([assert.h], [], [AC_MSG_ERROR([assert.h header not found.])])
AC_CHECK_HEADER([ctype.h], [], [AC_MSG_ERROR([ctype.h header not found.])])
AC_CHECK_HEADER([dirent.h], [], [AC_MSG_ERROR([dirent.h header not found.])])
AC_CHECK_HEADER([errno.h], [], [AC_MSG_ERROR([errno.h header not found.])])
AC_CHECK_HEADER([fcntl.h], [], [AC_MSG_ERROR([fcntl.h header not found.])])
AC_CHECK_HEADER([grp.h], [], [AC_MSG_ERROR([grp.h header not found.])])
AC_CHECK_HEADER([inttypes.h], [], [AC_MSG_ERROR([inttypes.h header not found.])])
AC_CHECK_HEADER([limits.h], [], [AC_MSG_ERROR([limits.h header not found.])])
AC_CHECK_HEADER([locale.h], [], [AC_MSG_ERROR([locale.h header not found.])])
AC_CHECK_HEADER([math.h], [], [AC_MSG_ERROR([math.h header not found.])])
AC_CHECK_HEADERS([mntent.h], [HAVE_MNTENT_H=1])
AC_CHECK_HEADER([pwd.h], [], [AC_MSG_ERROR([pwd.h header not found.])])
AC_CHECK_HEADER([signal.h], [], [AC_MSG_ERROR([signal.h header not found.])])
AC_CHECK_HEADER([stdarg.h], [], [AC_MSG_ERROR([stdarg.h header not found.])])
AC_CHECK_HEADER([stddef.h], [], [AC_MSG_ERROR([stddef.h header not found.])])
AC_CHECK_HEADER([stdint.h], [], [AC_MSG_ERROR([stdint.h header not found.])])
AC_CHECK_HEADER([stdio.h], [], [AC_MSG_ERROR([stdio.h header not found.])])
AC_CHECK_HEADER([stdlib.h], [], [AC_MSG_ERROR([stdlib.h header not found.])])
AC_CHECK_HEADER([string.h], [], [AC_MSG_ERROR([string.h header not found.])])
AC_CHECK_HEADER([sys/ioctl.h], [], [AC_MSG_ERROR([sys/ioctl.h header not found.])])
AC_CHECK_HEADER([sys/stat.h], [], [AC_MSG_ERROR([sys/stat.h header not found.])])
AC_CHECK_HEADER([sys/time.h], [], [AC_MSG_ERROR([sys/time.h header not found.])])
AC_CHECK_HEADER([sys/types.h], [], [AC_MSG_ERROR([sys/types.h header not found.])])
AC_CHECK_HEADER([sys/wait.h], [], [AC_MSG_ERROR([sys/wait.h header not found.])])
AC_CHECK_HEADER([termios.h], [], [AC_MSG_ERROR([termios.h header not found.])])
AC_CHECK_HEADER([time.h], [], [AC_MSG_ERROR([time.h header not found.])])
AC_CHECK_HEADER([unistd.h], [], [AC_MSG_ERROR([unistd.h header not found.])])
AC_CHECK_HEADER([wchar.h], [], [AC_MSG_ERROR([wchar.h header not found.])])
AC_CHECK_HEADER([wctype.h], [], [AC_MSG_ERROR([wctype.h header not found.])])
dnl Headers required for breaking too long command-lines.
AC_CHECK_HEADERS([sys/sysmacros.h sys/param.h sys/user.h linux/binfmts.h], [],
                 [],
                 [[#if HAVE_SYS_PARAM_H
                   # include <sys/param.h>
                   #endif
                 ]])
if test "$ac_cv_header_sys_user_h" = yes -a "$ac_cv_header_linux_binfmts_h" = yes; then
    AC_COMPILE_IFELSE([AC_LANG_SOURCE(
                      [[#include <linux/binfmts.h>
                        #include <sys/user.h>
                        #include <unistd.h>
                        #ifndef PAGE_SIZE
                        #define PAGE_SIZE sysconf(_SC_PAGESIZE)
                        #endif
                        int main() { (void)MAX_ARG_STRLEN; return 0; }
                      ]])],
                      [AC_DEFINE([HAVE_MAX_ARG_STRLEN], [1], [MAX_ARG_STRLEN is available.])])
fi

dnl Check for various system types.
AC_CHECK_TYPE([uid_t])
AC_CHECK_TYPE([gid_t])
AC_CHECK_TYPE([mode_t])
AC_CHECK_TYPE([off_t])
AC_CHECK_TYPE([time_t])
AC_CHECK_TYPE([uint64_t])
AC_CHECK_TYPE([direntry_t])
if test -n "$HAVE_MNTENT_H" ; then
    AC_CHECK_TYPE([mntent])
else
    AC_CHECK_TYPES([getmntinfo], [], [], [[
                     #include <sys/param.h>
                     #include <sys/ucred.h>
                     #include <sys/mount.h>
                   ]])
fi
AC_CHECK_TYPE([stat])
AC_CHECK_TYPE([FILE])
AC_CHECK_TYPE([wchar_t])

dnl Check for various system functions.
AC_CHECK_FUNC([access], [], [AC_MSG_ERROR([access() function not found.])])
AC_CHECK_FUNC([atof], [], [AC_MSG_ERROR([atof() function not found.])])
AC_CHECK_FUNC([atoi], [], [AC_MSG_ERROR([atoi() function not found.])])
AC_CHECK_FUNC([calloc], [], [AC_MSG_ERROR([calloc() function not found.])])
AC_CHECK_FUNC([chdir], [], [AC_MSG_ERROR([chdir() function not found.])])
AC_CHECK_FUNC([close], [], [AC_MSG_ERROR([close() function not found.])])
AC_CHECK_FUNC([closedir], [], [AC_MSG_ERROR([closedir() function not found.])])
AC_CHECK_FUNC([dup], [], [AC_MSG_ERROR([dup() function not found.])])
AC_CHECK_FUNC([dup2], [], [AC_MSG_ERROR([dup2() function not found.])])
if test -n "$HAVE_MNTENT_H" ; then
    AC_CHECK_FUNC([endmntent], [], [AC_MSG_ERROR([endmntent() function not found.])])
fi
AC_CHECK_FUNC([execve], [], [AC_MSG_ERROR([execve() function not found.])])
AC_CHECK_FUNC([execvp], [], [AC_MSG_ERROR([execvp() function not found.])])
AC_CHECK_FUNC([exit], [], [AC_MSG_ERROR([exit() function not found.])])
AC_CHECK_FUNC([fdatasync], [AC_DEFINE([HAVE_FDATASYNC], [1], [fdatasync() is available.])], [])
AC_CHECK_FUNC([fclose], [], [AC_MSG_ERROR([fclose() function not found.])])
AC_CHECK_FUNC([fdopen], [], [AC_MSG_ERROR([fdopen() function not found.])])
AC_CHECK_FUNC([feof], [], [AC_MSG_ERROR([feof() function not found.])])
AC_CHECK_FUNC([fgetc], [], [AC_MSG_ERROR([fgetc() function not found.])])
AC_CHECK_FUNC([fgets], [], [AC_MSG_ERROR([fgets() function not found.])])
AC_CHECK_FUNC([fork], [], [AC_MSG_ERROR([fork() function not found.])])
AC_CHECK_FUNC([fprintf], [], [AC_MSG_ERROR([fprintf() function not found.])])
AC_CHECK_FUNC([fputc], [], [AC_MSG_ERROR([fputc() function not found.])])
AC_CHECK_FUNC([fputs], [], [AC_MSG_ERROR([fputs() function not found.])])
AC_CHECK_FUNC([free], [], [AC_MSG_ERROR([free() function not found.])])
AC_CHECK_FUNCS([futimens])
AC_CHECK_FUNC([fwrite], [], [AC_MSG_ERROR([fwrite() function not found.])])
AC_CHECK_FUNC([getcwd], [], [AC_MSG_ERROR([getcwd() function not found.])])
AC_CHECK_FUNC([getenv], [], [AC_MSG_ERROR([getenv() function not found.])])
AC_CHECK_FUNC([geteuid], [], [AC_MSG_ERROR([geteuid() function not found.])])
AC_CHECK_FUNC([getgrent], [], [AC_MSG_ERROR([getgrent() function not found.])])
AC_CHECK_FUNC([getgrgid], [], [AC_MSG_ERROR([getgrgid() function not found.])])
AC_CHECK_FUNC([getgrgid_r], [], [AC_MSG_ERROR([getgrgid_r() function not found.])])
AC_CHECK_FUNC([getgrnam], [], [AC_MSG_ERROR([getgrnam() function not found.])])
if test -n "$HAVE_MNTENT_H" ; then
    AC_CHECK_FUNC([getmntent], [], [AC_MSG_ERROR([getmntent() function not found.])])
fi
AC_CHECK_FUNC([getpid], [], [AC_MSG_ERROR([getpid() function not found.])])
AC_CHECK_FUNC([getppid], [], [AC_MSG_ERROR([getppid() function not found.])])
AC_CHECK_FUNC([getpwent], [], [AC_MSG_ERROR([getpwent() function not found.])])
AC_CHECK_FUNC([getpwnam], [], [AC_MSG_ERROR([getpwnam() function not found.])])
AC_CHECK_FUNC([getpwuid], [], [AC_MSG_ERROR([getpwuid() function not found.])])
AC_CHECK_FUNC([getpwuid_r], [], [AC_MSG_ERROR([getpwuid_r() function not found.])])
AC_CHECK_FUNC([ioctl], [], [AC_MSG_ERROR([ioctl() function not found.])])
AC_CHECK_FUNC([iswalnum], [], [AC_MSG_ERROR([iswalnum() function not found.])])
AC_CHECK_FUNC([iswdigit], [], [AC_MSG_ERROR([iswdigit() function not found.])])
AC_CHECK_FUNC([iswprint], [], [AC_MSG_ERROR([iswprint() function not found.])])
AC_CHECK_FUNC([iswspace], [], [AC_MSG_ERROR([iswspace() function not found.])])
AC_CHECK_FUNC([iswupper], [], [AC_MSG_ERROR([iswupper() function not found.])])
AC_CHECK_FUNC([kill], [], [AC_MSG_ERROR([kill() function not found.])])
AC_CHECK_FUNC([localtime], [], [AC_MSG_ERROR([localtime() function not found.])])
AC_CHECK_FUNC([malloc], [], [AC_MSG_ERROR([malloc() function not found.])])
AC_CHECK_FUNC([mbstowcs], [], [AC_MSG_ERROR([mbstowcs() function not found.])])
AC_CHECK_FUNC([memcmp], [], [AC_MSG_ERROR([memcmp() function not found.])])
AC_CHECK_FUNC([memcpy], [], [AC_MSG_ERROR([memcpy() function not found.])])
AC_CHECK_FUNC([memccpy], [], [AC_MSG_ERROR([memccpy() function not found.])])
AC_CHECK_FUNC([memmove], [], [AC_MSG_ERROR([memmove() function not found.])])
AC_CHECK_FUNC([memset], [], [AC_MSG_ERROR([memset() function not found.])])
AC_CHECK_FUNC([mkdir], [], [AC_MSG_ERROR([mkdir() function not found.])])
AC_CHECK_FUNC([opendir], [], [AC_MSG_ERROR([opendir() function not found.])])
AC_CHECK_FUNC([pathconf], [], [AC_MSG_ERROR([pathconf() function not found.])])
AC_CHECK_FUNC([pause], [], [AC_MSG_ERROR([pause() function not found.])])
AC_CHECK_FUNC([pclose], [], [AC_MSG_ERROR([pclose() function not found.])])
AC_CHECK_FUNC([perror], [], [AC_MSG_ERROR([perror() function not found.])])
AC_CHECK_FUNC([pipe], [], [AC_MSG_ERROR([pipe() function not found.])])
AC_CHECK_FUNC([popen], [], [AC_MSG_ERROR([popen() function not found.])])
AC_CHECK_FUNC([printf], [], [AC_MSG_ERROR([printf() function not found.])])
AC_CHECK_FUNC([puts], [], [AC_MSG_ERROR([puts() function not found.])])
AC_CHECK_FUNC([qsort], [], [AC_MSG_ERROR([qsort() function not found.])])
AC_CHECK_FUNC([read], [], [AC_MSG_ERROR([read() function not found.])])
AC_CHECK_FUNC([readlink], [], [AC_MSG_ERROR([readlink() function not found.])])
AC_CHECK_FUNC([realloc], [], [AC_MSG_ERROR([realloc() function not found.])])
AC_CHECK_FUNCS([reallocarray])
AC_CHECK_FUNC([realpath], [], [AC_MSG_ERROR([realpath() function not found.])])
AC_CHECK_FUNC([rename], [], [AC_MSG_ERROR([rename() function not found.])])
AC_CHECK_FUNC([rmdir], [], [AC_MSG_ERROR([rmdir() function not found.])])
AC_CHECK_FUNC([select], [], [AC_MSG_ERROR([select() function not found.])])
AC_CHECK_FUNC([setenv], [], [AC_MSG_ERROR([setenv() function not found.])])
AC_CHECK_FUNC([setgrent], [], [AC_MSG_ERROR([setgrent() function not found.])])
AC_CHECK_FUNC([setlocale], [], [AC_MSG_ERROR([setlocale() function not found.])])
if test -n "$HAVE_MNTENT_H" ; then
    AC_CHECK_FUNC([setmntent], [], [AC_MSG_ERROR([setmntent() function not found.])])
fi
AC_CHECK_FUNC([setpgid], [], [AC_MSG_ERROR([setpgid() function not found.])])
AC_CHECK_FUNC([setpwent], [], [AC_MSG_ERROR([setpwent() function not found.])])
AC_CHECK_FUNC([setsid], [], [AC_MSG_ERROR([setsid() function not found.])])
AC_CHECK_FUNC([setvbuf], [], [AC_MSG_ERROR([setvbuf() function not found.])])
AC_CHECK_FUNC([sigaction], [], [AC_MSG_ERROR([sigaction() function not found.])])
AC_CHECK_FUNC([sigaddset], [], [AC_MSG_ERROR([sigaddset() function not found.])])
AC_CHECK_FUNC([sigemptyset], [], [AC_MSG_ERROR([sigemptyset() function not found.])])
AC_CHECK_FUNC([signal], [], [AC_MSG_ERROR([signal() function not found.])])
AC_CHECK_FUNC([snprintf], [], [AC_MSG_ERROR([snprintf() function not found.])])
AC_CHECK_FUNC([sprintf], [], [AC_MSG_ERROR([sprintf() function not found.])])
AC_CHECK_FUNC([strcasecmp], [], [AC_MSG_ERROR([strcasecmp() function not found.])])
AC_CHECK_FUNC([strcasestr], [AC_DEFINE([HAVE_STRCASESTR], [1], [strcasestr() is available.])], [])
AC_CHECK_FUNC([strcat], [], [AC_MSG_ERROR([strcat() function not found.])])
AC_CHECK_FUNC([strchr], [], [AC_MSG_ERROR([strchr() function not found.])])
AC_CHECK_FUNC([strcmp], [], [AC_MSG_ERROR([strcmp() function not found.])])
AC_CHECK_FUNC([strcpy], [], [AC_MSG_ERROR([strcpy() function not found.])])
AC_CHECK_FUNC([strdup], [], [AC_MSG_ERROR([strdup() function not found.])])
AC_CHECK_FUNC([strerror], [], [AC_MSG_ERROR([strerror() function not found.])])
AC_CHECK_FUNC([strftime], [], [AC_MSG_ERROR([strftime() function not found.])])
AC_CHECK_FUNC([strlen], [], [AC_MSG_ERROR([strlen() function not found.])])
AC_CHECK_FUNC([strncasecmp], [], [AC_MSG_ERROR([strncasecmp() function not found.])])
AC_CHECK_FUNC([strncat], [], [AC_MSG_ERROR([strncat() function not found.])])
AC_CHECK_FUNC([strncmp], [], [AC_MSG_ERROR([strncmp() function not found.])])
AC_CHECK_FUNC([strncpy], [], [AC_MSG_ERROR([strncpy() function not found.])])
AC_CHECK_FUNC([strpbrk], [], [AC_MSG_ERROR([strpbrk() function not found.])])
AC_CHECK_FUNC([strrchr], [], [AC_MSG_ERROR([strrchr() function not found.])])
AC_CHECK_FUNC([strspn], [], [AC_MSG_ERROR([strspn() function not found.])])
AC_CHECK_FUNC([strstr], [], [AC_MSG_ERROR([strstr() function not found.])])
AC_CHECK_FUNC([strtok_r], [], [AC_MSG_ERROR([strtok_r() function not found.])])
AC_CHECK_FUNC([strtol], [], [AC_MSG_ERROR([strtol() function not found.])])
AC_CHECK_FUNC([strtoll], [], [AC_MSG_ERROR([strtoll() function not found.])])
AC_CHECK_FUNC([strverscmp], [AC_DEFINE([HAVE_STRVERSCMP_FUNC], [1], [strverscmp() function is available.])])
AC_CHECK_FUNC([sysconf], [], [AC_MSG_ERROR([sysconf() function not found.])])
AC_CHECK_FUNC([time], [], [AC_MSG_ERROR([time() function not found.])])
AC_CHECK_FUNC([tolower], [], [AC_MSG_ERROR([tolower() function not found.])])
AC_CHECK_FUNC([toupper], [], [AC_MSG_ERROR([toupper() function not found.])])
AC_CHECK_FUNC([towupper], [], [AC_MSG_ERROR([towupper() function not found.])])
AC_CHECK_FUNC([ungetc], [], [AC_MSG_ERROR([ungetc() function not found.])])
AC_CHECK_FUNC([unlink], [], [AC_MSG_ERROR([unlink() function not found.])])
AC_CHECK_FUNC([unsetenv], [], [AC_MSG_ERROR([unsetenv() function not found.])])
AC_CHECK_FUNC([utimes], [], [AC_MSG_ERROR([utimes() function not found.])])
AC_CHECK_FUNC([vfprintf], [], [AC_MSG_ERROR([vfprintf() function not found.])])
AC_CHECK_FUNC([vsnprintf], [], [AC_MSG_ERROR([vsnprintf() function not found.])])
AC_CHECK_FUNC([vswprintf], [], [AC_MSG_ERROR([vswprintf() function not found.])])
AC_CHECK_FUNC([waitpid], [], [AC_MSG_ERROR([waitpid() function not found.])])
AC_CHECK_FUNC([wcscat], [], [AC_MSG_ERROR([wcscat() function not found.])])
AC_CHECK_FUNC([wcscmp], [], [AC_MSG_ERROR([wcscmp() function not found.])])
AC_CHECK_FUNC([wcscpy], [], [AC_MSG_ERROR([wcscpy() function not found.])])
AC_CHECK_FUNC([wcslen], [], [AC_MSG_ERROR([wcslen() function not found.])])
AC_CHECK_FUNC([wcsncmp], [], [AC_MSG_ERROR([wcsncmp() function not found.])])
AC_CHECK_FUNC([wcsncpy], [], [AC_MSG_ERROR([wcsncpy() function not found.])])
AC_CHECK_FUNC([wcstof], [], [AC_MSG_ERROR([wcstof() function not found.])])
AC_CHECK_FUNC([wcstol], [], [AC_MSG_ERROR([wcstol() function not found.])])
AC_CHECK_FUNC([wcstombs], [], [AC_MSG_ERROR([wcstombs() function not found.])])
AC_CHECK_FUNC([wcswidth], [], [AC_MSG_ERROR([wcswidth() function not found.])])
AC_CHECK_FUNC([wcwidth], [], [AC_MSG_ERROR([wcwidth() function not found.])])

AC_CHECK_DECLS([_PC_CASE_SENSITIVE], [], [], [[#include <unistd.h>]])

dnl Check for regex.h header, its functions, types and macros.
AC_CHECK_HEADER([regex.h], [], [AC_MSG_ERROR([regex.h header not found.])])
AC_CHECK_FUNC([regcomp], [], [AC_MSG_ERROR([regcomp() function not found.])])
AC_CHECK_FUNC([regerror], [], [AC_MSG_ERROR([regerror() function not found.])])
AC_CHECK_FUNC([regexec], [], [AC_MSG_ERROR([regexec() function not found.])])
AC_CHECK_FUNC([regfree], [], [AC_MSG_ERROR([regfree() function not found.])])
AC_CHECK_TYPE([regex_t], [], [AC_MSG_ERROR([regex_t type not found in regex.h])], [[#include <regex.h>]])
AC_CHECK_DECL([REG_EXTENDED], [], [AC_MSG_ERROR([REG_EXTENDED not found in regex.h])], [[#include <regex.h>]])
AC_CHECK_DECL([REG_ICASE], [], [AC_MSG_ERROR([REG_ICASE not found in regex.h])], [[#include <regex.h>]])
AC_CHECK_DECL([REG_NOMATCH], [], [AC_MSG_ERROR([REG_NOMATCH not found in regex.h])], [[#include <regex.h>]])

AC_CHECK_MEMBERS([struct stat.st_mtim], [], [], [[
                  #include <sys/types.h>
                  #include <sys/stat.h>
                  #include <unistd.h>
                ]])
AC_CHECK_MEMBERS([struct dirent.d_type], [], [], [[
                  #include <dirent.h>
                ]])
AC_SYS_LARGEFILE
AC_FUNC_FSEEKO
AC_TYPE_OFF_T

dnl At least on AIX `st_atim` and `st_mtim` might not match `struct timespec`.
if test "$ac_cv_member_struct_stat_st_mtim" = yes; then
    AC_COMPILE_IFELSE([AC_LANG_SOURCE(
                      [[#include <sys/types.h>
                        #include <sys/stat.h>
                        #include <unistd.h>
                        int main() {
                            struct stat st;
                            struct timespec ts[2] = { st.st_atim, st.st_mtim };
                            return 0;
                        }
                      ]])],
                      [AC_DEFINE([HAVE_CONSISTENT_TIMESPEC], [1], [timespec is good.])])
fi

dnl ----------------------------------------------------------------------------
dnl check for available tools/applications
dnl ----------------------------------------------------------------------------

AC_CHECK_PROG(HAVE_FILE_PROG, file, 1)
if test "x${HAVE_FILE_PROG}" = 'x1' ; then
    AC_DEFINE_UNQUOTED(HAVE_FILE_PROG, 1, [Define if file program present])
fi

dnl Use either groff or mandoc for generating plain text help from manual page.
AC_CHECK_PROGS(MANGEN_PROG, [groff mandoc], [])

dnl col might be used for filtering out terminal escape sequences
AC_CHECK_PROG(COL_PROG, col, col)

dnl awk is used to generate source with list of documentation tags
AC_CHECK_PROG(AWK_PROG, awk, awk)

dnl perl is used to generate tags from documentation
AC_CHECK_PROG(PERL_PROG, perl, perl)

dnl sed is used to make plain text documentation
AC_CHECK_PROG(SED_PROG, sed, sed)

dnl vim is a fallback for generating documentation tags when perl isn't there
AC_CHECK_PROG(VIM_PROG, vim, vim)

dnl check if we're building inside git repository
AC_CHECK_PROG(GIT_PROG, git, git)
if test "x${GIT_PROG}" != 'x' -a -d .git/; then
    IN_GIT_REPO=1
fi
AC_SUBST([IN_GIT_REPO])

dnl ----------------------------------------------------------------------------
dnl setup various package/compilation settings
dnl ----------------------------------------------------------------------------

dnl Set PACKAGE_SYSCONF_DIR in config.h.
if test "x${sysconfdir}" = 'x${prefix}/etc'; then
  if test "x${prefix}" = "xNONE"; then
    AC_DEFINE_UNQUOTED(PACKAGE_SYSCONF_DIR, "${ac_default_prefix}/etc/${PACKAGE}")
  else
    AC_DEFINE_UNQUOTED(PACKAGE_SYSCONF_DIR, "${prefix}/etc/${PACKAGE}")
  fi
else
  AC_DEFINE_UNQUOTED(PACKAGE_SYSCONF_DIR, "${sysconfdir}/${PACKAGE}")
fi

dnl Set PACKAGE_DATA_DIR in config.h.
if test "x${datadir}" = 'x${prefix}/share'; then
  if test "x${prefix}" = "xNONE"; then
    AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR, "${ac_default_prefix}/share/${PACKAGE}")
  else
    AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR, "${prefix}/share/${PACKAGE}")
  fi
else
  if test "x${datadir}" = 'x${datarootdir}'; then
    if test "x${datarootdir}" = 'x${prefix}/share'; then
      if test "x${prefix}" = "xNONE"; then
        AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR, "${ac_default_prefix}/share/${PACKAGE}")
      else
        AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR, "${prefix}/share/${PACKAGE}")
      fi
    fi
  else
    AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR, "${datadir}/${PACKAGE}")
  fi
fi

dnl Setup suffix for data files.
if test -n "$OSX_HOST" ; then
    DATA_SUFFIX=-osx
fi
AC_SUBST([DATA_SUFFIX])

dnl Set PACKAGE_SOURCE_DIR in config.h.
packagesrcdir=`cd $srcdir && pwd`
AC_DEFINE_UNQUOTED(PACKAGE_SOURCE_DIR, "${packagesrcdir}")

dnl Use -Wall if available.
AX_CHECK_COMPILE_FLAG([-Wall], [
    case " $CFLAGS " in
      *[\ \	]-Wall[\ \	]*) ;;
      *) CFLAGS="$CFLAGS -Wall" ;;
    esac
  ])

dnl Define _DARWIN_C_SOURCE macro while compiling on OS X.
if test -n "$OSX_HOST" ; then
  AC_DEFINE([_DARWIN_C_SOURCE], [1],
            [Enable extensions in header files on OS X.])
fi

dnl Use 64-bit off_t type
AC_DEFINE([_FILE_OFFSET_BITS], [64],
          [Enable large file processing on 32-bit systems.])

dnl Use math library if present (standard doesn't require it to be separated
dnl from libc)
AC_CHECK_LIB(m, pow, [LIBS="$LIBS -lm"])

dnl Enable POSIX shared memory
AC_CHECK_LIB(rt, shm_open, [LIBS="$LIBS -lrt"])

dnl Use pthread library
AX_CHECK_COMPILE_FLAG([-pthread], [
    TESTS_CFLAGS="$CFLAGS -pthread"
    CFLAGS="$CFLAGS -pthread"
    ], [
    AC_CHECK_LIB(pthread, pthread_create,
      [LIBS="$LIBS -lpthread"
      AC_CHECK_HEADER([pthread.h], [], [AC_MSG_ERROR([pthread.h header not found.])])
      ],
      [AC_MSG_ERROR([libpthread not found])])
  ])

dnl Check for all required elements in pthread.h.
AC_CHECK_FUNC([pthread_create], [], [AC_MSG_ERROR([pthread_create() function not found.])])
AC_CHECK_FUNC([pthread_getspecific], [], [AC_MSG_ERROR([pthread_getspecific() function not found.])])
AC_CHECK_FUNC([pthread_key_create], [], [AC_MSG_ERROR([pthread_key_create() function not found.])])
AC_CHECK_FUNC([pthread_mutex_lock], [], [AC_MSG_ERROR([pthread_mutex_lock() function not found.])])
AC_CHECK_FUNC([pthread_mutex_unlock], [], [AC_MSG_ERROR([pthread_mutex_unlock() function not found.])])
AC_CHECK_FUNC([pthread_once], [], [AC_MSG_ERROR([pthread_once() function not found.])])
AC_CHECK_FUNC([pthread_setspecific], [], [AC_MSG_ERROR([pthread_setspecific() function not found.])])
AC_CHECK_TYPE([pthread_t], [], [AC_MSG_ERROR([pthread_t type not found in pthread.h])], [[#include <pthread.h>]])
AC_CHECK_TYPE([pthread_once_t], [], [AC_MSG_ERROR([pthread_once_t type not found in pthread.h])], [[#include <pthread.h>]])
AC_CHECK_TYPE([pthread_key_t], [], [AC_MSG_ERROR([pthread_key_t type not found in pthread.h])], [[#include <pthread.h>]])
AC_CHECK_TYPE([pthread_mutex_t], [], [AC_MSG_ERROR([pthread_mutex_t type not found in pthread.h])], [[#include <pthread.h>]])
AC_CHECK_DECL([PTHREAD_ONCE_INIT], [], [AC_MSG_ERROR([PTHREAD_ONCE_INIT not found in pthread.h])], [[#include <pthread.h>]])
AC_CHECK_DECL([PTHREAD_MUTEX_INITIALIZER], [], [AC_MSG_ERROR([PTHREAD_MUTEX_INITIALIZER not found in pthread.h])], [[#include <pthread.h>]])

dnl Name of curses library file.
curses_lib_name=ncursesw
AC_ARG_WITH(curses-name,
    [  --with-curses-name=NAME name of curses library file @<:@default=ncursesw@:>@],
    [curses_lib_name=$withval])

dnl AC_CHECK_LIB(XCurses, initscr)
vifm_cv_curses=/usr
AC_ARG_WITH(curses, [  --with-curses=DIR       where ncurses is installed ],
    [if test $withval != yes; then
        vifm_cv_curses=$withval
    fi
    if test x$vifm_cv_curses != x/usr; then
        LDFLAGS="-L${vifm_cv_curses}/lib $LDFLAGS"
        CPPFLAGS="$CPPFLAGS -I${vifm_cv_curses}/include -I${vifm_cv_curses}/include/${curses_lib_name}"
    fi])

dnl Try to find ncurses library.
ncurses_found=no
dnl Firstly check for lib${curses_lib_name}.
AC_CHECK_LIB(${curses_lib_name}, initscr,
    [LIBS="$LIBS -l${curses_lib_name}"
    if test x$vifm_cv_curses = x/usr -a -d /usr/include/${curses_lib_name}; then
        CPPFLAGS="$CPPFLAGS -I/usr/include/${curses_lib_name}"
    fi
    ncurses_found=yes]
    AC_SEARCH_LIBS([curs_set], [tinfow],,
        AC_MSG_ERROR(could not find a library providing curs_set)))
dnl If libncursesw is not found on OS X, check for libncurses.  On OS X it has
dnl support of wide characters.
if test -n "$OSX_HOST" -a "x$ncurses_found" != "xyes"; then
    AC_CHECK_LIB(ncurses, initscr,
        [LIBS="$LIBS -lncurses"
        if test x$vifm_cv_curses = x/usr -a -d /usr/include/ncurses; then
            CPPFLAGS="$CPPFLAGS -I/usr/include/ncurses"
        fi
        ncurses_found=yes])
fi
dnl Quit with appropriate error message if ncurses library wasn't found.
if test "x$ncurses_found" != "xyes"; then
    if test -z "$OSX_HOST"; then
        AC_MSG_ERROR([libncursesw not found])
    else
        AC_MSG_ERROR([libncursesw and libncurses not found])
    fi
fi
LIBS=$LIBS $GUI_LINK_OPTS_TERM

dnl Check for curses.h header and all its required elements.
AC_CHECK_HEADER([curses.h], [], [AC_MSG_ERROR([curses.h header not found.])])
AC_CHECK_DECL([A_ITALIC], [AC_DEFINE([HAVE_A_ITALIC_DECL], [1], [A_ITALIC attribute is available.])], [], [[#include <curses.h>]])
AC_CHECK_DECL([COLORS], [], [AC_MSG_ERROR([COLORS not found in curses.h])], [[#include <curses.h>]])
AC_CHECK_DECL([TABSIZE], [], [AC_MSG_ERROR([TABSIZE not found in curses.h])], [[#include <curses.h>]])
AC_CHECK_FUNC([curs_set], [], [AC_MSG_ERROR([curs_set() function not found.])])
AC_CHECK_FUNC([def_prog_mode], [], [AC_MSG_ERROR([def_prog_mode() function not found.])])
AC_CHECK_FUNC([doupdate], [], [AC_MSG_ERROR([doupdate() function not found.])])
AC_CHECK_FUNC([endwin], [], [AC_MSG_ERROR([endwin() function not found.])])
AC_CHECK_FUNC([flushinp], [], [AC_MSG_ERROR([flushinp() function not found.])])
AC_CHECK_FUNC([getcchar], [], [AC_MSG_ERROR([getcchar() function not found.])])
AC_CHECK_FUNC([has_colors], [], [AC_MSG_ERROR([has_colors() function not found.])])
AC_CHECK_FUNC([init_pair], [], [AC_MSG_ERROR([init_pair() function not found.])])
AC_CHECK_FUNC([initscr], [], [AC_MSG_ERROR([initscr() function not found.])])
AC_CHECK_FUNC([isendwin], [], [AC_MSG_ERROR([isendwin() function not found.])])
AC_CHECK_FUNC([keypad], [], [AC_MSG_ERROR([keypad() function not found.])])
AC_CHECK_FUNC([mvwin], [], [AC_MSG_ERROR([mvwin() function not found.])])
AC_CHECK_FUNC([mvwprintw], [], [AC_MSG_ERROR([mvwprintw() function not found.])])
AC_CHECK_FUNC([newwin], [], [AC_MSG_ERROR([newwin() function not found.])])
AC_CHECK_FUNC([noecho], [], [AC_MSG_ERROR([noecho() function not found.])])
AC_CHECK_FUNC([nonl], [], [AC_MSG_ERROR([nonl() function not found.])])
AC_CHECK_FUNC([raw], [], [AC_MSG_ERROR([raw() function not found.])])
AC_CHECK_FUNC([reset_prog_mode], [], [AC_MSG_ERROR([reset_prog_mode() function not found.])])
AC_CHECK_FUNC([resize_term], [], [AC_MSG_ERROR([resize_term() function not found.])])
AC_CHECK_FUNC([resizeterm], [], [AC_MSG_ERROR([resizeterm() function not found.])])
AC_CHECK_FUNC([scrollok], [], [AC_MSG_ERROR([scrollok() function not found.])])
AC_CHECK_FUNC([set_escdelay], [AC_DEFINE([HAVE_SET_ESCDELAY_FUNC], [1], [set_escdelay() function is available.])])
AC_CHECK_FUNC([setcchar], [], [AC_MSG_ERROR([setcchar() function not found.])])
AC_CHECK_FUNC([start_color], [], [AC_MSG_ERROR([start_color() function not found.])])
AC_CHECK_FUNC([use_default_colors], [], [AC_MSG_ERROR([use_default_colors() function not found.])])
AC_CHECK_FUNC([waddch], [], [AC_MSG_ERROR([waddch() function not found.])])
AC_CHECK_FUNC([waddnstr], [], [AC_MSG_ERROR([waddnstr() function not found.])])
AC_CHECK_FUNC([waddnwstr], [], [AC_MSG_ERROR([waddnwstr() function not found.])])
AC_CHECK_FUNC([wattr_set], [], [AC_MSG_ERROR([wattr_set() function not found.])])
AC_CHECK_FUNC([wbkgrndset], [], [AC_MSG_ERROR([wbkgrndset() function not found.])])
AC_CHECK_FUNC([wborder], [], [AC_MSG_ERROR([wborder() function not found.])])
AC_CHECK_FUNC([wclrtoeol], [], [AC_MSG_ERROR([wclrtoeol() function not found.])])
AC_CHECK_FUNC([werase], [], [AC_MSG_ERROR([werase() function not found.])])
AC_CHECK_FUNC([wget_wch], [], [AC_MSG_ERROR([wget_wch() function not found.])])
AC_CHECK_FUNC([wgetch], [], [AC_MSG_ERROR([wgetch() function not found.])])
AC_CHECK_FUNC([wmove], [], [AC_MSG_ERROR([wmove() function not found.])])
AC_CHECK_FUNC([wnoutrefresh], [], [AC_MSG_ERROR([wnoutrefresh() function not found.])])
AC_CHECK_FUNC([wprintw], [], [AC_MSG_ERROR([wprintw() function not found.])])
AC_CHECK_FUNC([wredrawln], [], [AC_MSG_ERROR([wredrawln() function not found.])])
AC_CHECK_FUNC([wrefresh], [], [AC_MSG_ERROR([wrefresh() function not found.])])
AC_CHECK_FUNC([wresize], [], [AC_MSG_ERROR([wresize() function not found.])])
AC_CHECK_FUNC([wtimeout], [], [AC_MSG_ERROR([wtimeout() function not found.])])
AC_CHECK_FUNC([wtouchln], [], [AC_MSG_ERROR([wtouchln() function not found.])])

dnl ----------------------------------------------------------------------------
dnl check for extended colors/pairs
dnl ----------------------------------------------------------------------------

use_extended_colors=yes

AC_CHECK_FUNC([extended_pair_content], [], [use_extended_colors=no], [[#include <curses.h>]])
AC_CHECK_FUNC([pair_content], [], [use_extended_colors=no], [[#include <curses.h>]])

if test "$use_extended_colors" = "yes"; then
    AC_DEFINE([HAVE_EXTENDED_COLORS], [1], [extended curses colors are present])
fi

dnl ----------------------------------------------------------------------------
dnl check for inotify
dnl ----------------------------------------------------------------------------

AC_CHECK_HEADER([sys/inotify.h], [use_inotify=yes], [use_inotify=no])

if test "$use_inotify" = "yes"; then
    AC_CHECK_DECL([IN_NONBLOCK], [], [use_inotify=no], [[#include <sys/inotify.h>]])
    AC_CHECK_DECL([IN_CLOEXEC], [], [use_inotify=no], [[#include <sys/inotify.h>]])
    AC_CHECK_DECL([IN_ATTRIB], [], [use_inotify=no], [[#include <sys/inotify.h>]])
    AC_CHECK_DECL([IN_MODIFY], [], [use_inotify=no], [[#include <sys/inotify.h>]])
    AC_CHECK_DECL([IN_CREATE], [], [use_inotify=no], [[#include <sys/inotify.h>]])
    AC_CHECK_DECL([IN_DELETE], [], [use_inotify=no], [[#include <sys/inotify.h>]])
    AC_CHECK_DECL([IN_MOVED_FROM], [], [use_inotify=no], [[#include <sys/inotify.h>]])
    AC_CHECK_DECL([IN_MOVED_TO], [], [use_inotify=no], [[#include <sys/inotify.h>]])
    AC_CHECK_DECL([IN_EXCL_UNLINK], [], [use_inotify=no], [[#include <sys/inotify.h>]])
    AC_CHECK_DECL([IN_CLOSE_WRITE], [], [use_inotify=no], [[#include <sys/inotify.h>]])
    AC_CHECK_DECL([IN_Q_OVERFLOW], [], [use_inotify=no], [[#include <sys/inotify.h>]])
    AC_CHECK_FUNC([inotify_init1], [], [use_inotify=no])
    AC_CHECK_FUNC([inotify_add_watch], [], [use_inotify=no])
    AC_CHECK_TYPE([struct inotify_event], [], [use_inotify=no], [[#include <sys/inotify.h>]])

    if test "$use_inotify" = "yes"; then
        AC_DEFINE([HAVE_INOTIFY], [1], [inotify is available])
    fi
fi

dnl ----------------------------------------------------------------------------
dnl check for extended file attributes
dnl ----------------------------------------------------------------------------

AC_CHECK_HEADER([sys/xattr.h], [use_xattrs=yes], [use_xattrs=no])

if test "$use_xattrs" = "yes"; then
    AC_CHECK_FUNC([lgetxattr], [], [use_xattrs=no], [[
                    #include <sys/types.h>
                    #include <sys/xattr.h>
                  ]])
    AC_CHECK_FUNC([llistxattr], [], [use_xattrs=no], [[
                    #include <sys/types.h>
                    #include <sys/xattr.h>
                  ]])
    AC_CHECK_FUNC([lsetxattr], [], [use_xattrs=no], [[
                    #include <sys/types.h>
                    #include <sys/xattr.h>
                  ]])
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
    #include <sys/types.h>
    #include <sys/xattr.h>
    int main() {
        return XATTR_CREATE;
    }
    ]])], [], [use_xattrs="no"])

    if test "$use_xattrs" = "yes"; then
        AC_DEFINE([HAVE_XATTRS], [1], [extended file attributes are available])
    fi
fi

dnl ----------------------------------------------------------------------------
dnl check for gnu coreutils version
dnl ----------------------------------------------------------------------------

version="$(mv --version 2> /dev/null | sed -ne 's/^.*(GNU coreutils) //p')"

major="${version%.*}"
minor="${version#*.}"

if test "x$major" != "x" -a "x$minor" != "x" ; then
    if test $major -eq 7 -a $minor -ge 1 -o $major -gt 7; then
        AC_DEFINE([SUPPORT_NO_CLOBBER], [1], [-n option is available for cp and mv])
    fi
    if test $major -eq 7 -a $minor -ge 6 -o $major -gt 7; then
        AC_DEFINE([SUPPORT_REFLINK_AUTO], [1], [--reflink=auto option is available for cp])
    fi
fi

dnl ----------------------------------------------------------------------------
dnl checks for external libraries
dnl ----------------------------------------------------------------------------

AC_ARG_WITH(gtk,
    AS_HELP_STRING([--with-gtk],
        [use GTK+ to determine mimetypes if available @<:@default=yes@:>@]),
    [use_gtk=$withval],
    [use_gtk=yes])

AC_ARG_WITH(libmagic,
    AS_HELP_STRING([--with-libmagic],
        [use libmagic to determine mimetypes if available @<:@default=yes@:>@]),
    [use_libmagic=$withval],
    [use_libmagic=yes])

AC_ARG_WITH(X11,
    AS_HELP_STRING([--with-X11],
        [use libX11 to determine terminal emulator title  @<:@default=yes@:>@]),
    [use_libX11=$withval],
    [use_libX11=yes])

AC_ARG_WITH(dyn-X11,
    AS_HELP_STRING([--with-dyn-X11],
        [load libX11 dynamically @<:@default=yes@:>@]),
    [use_dyn_libX11=$withval],
    [use_dyn_libX11=yes])

dnl ----------------------------------------------------------------------------
dnl checks for mimetype detection
dnl ----------------------------------------------------------------------------

if test "$use_gtk" = "yes"; then
    if test "x$(which pkg-config)" != "x"; then
        if pkg-config --exists glib-2.0 gtk+-2.0; then
            CFLAGS="$CFLAGS $(pkg-config --cflags glib-2.0 gtk+-2.0)"
            LIBS="$LIBS $(pkg-config --libs glib-2.0 gtk+-2.0)"
            AC_CHECK_HEADER([gio/gio.h], [], [AC_MSG_ERROR([gio/gio.h header not found.])], [[#include <gio/gio.h>]])
            AC_CHECK_HEADER([glib.h], [], [AC_MSG_ERROR([glib.h header not found.])], [[#include <glib.h>]])
            AC_CHECK_HEADER([gtk/gtk.h], [], [AC_MSG_ERROR([gtk/gtk.h header not found.])], [[#include <gtk/gtk.h>]])
            AC_CHECK_FUNC([gtk_init_check], [], [AC_MSG_ERROR([gtk_init_check() function not found.])])
            AC_CHECK_FUNC([g_file_info_get_content_type], [], [AC_MSG_ERROR([g_file_info_get_content_type() function not found.])])
            AC_CHECK_FUNC([g_file_new_for_path], [], [AC_MSG_ERROR([g_file_new_for_path() function not found.])])
            AC_CHECK_FUNC([g_file_query_info], [], [AC_MSG_ERROR([g_file_query_info() function not found.])])
            AC_CHECK_FUNC([g_object_unref], [], [AC_MSG_ERROR([g_object_unref() function not found.])])
            AC_DEFINE([HAVE_LIBGTK], [1], [use gtk to determine mime type])
        else
            AC_MSG_WARN("Could not find glib-2.0 gtk+-2.0 via pkg-config.  No GTK+ support.")
        fi
    else
        AC_MSG_WARN("Could not find pkg-config.  No GTK+ support.")
    fi
fi

if test "$use_libmagic" = "yes"; then
    AC_CHECK_LIB(magic, magic_open,
        [LIBS="$LIBS -lmagic"
         AC_CHECK_FUNC([magic_close], [], [AC_MSG_ERROR([magic_close() function not found.])])
         AC_CHECK_FUNC([magic_file], [], [AC_MSG_ERROR([magic_file() function not found.])])
         AC_CHECK_FUNC([magic_load], [], [AC_MSG_ERROR([magic_load() function not found.])])
         AC_CHECK_FUNC([magic_open], [], [AC_MSG_ERROR([magic_open() function not found.])])
         AC_CHECK_HEADER([magic.h], [], [AC_MSG_ERROR([magic.h header not found.])])
         AC_CHECK_DECLS([MAGIC_MIME_TYPE], [], [], [[#include <magic.h>]])
         AC_DEFINE([HAVE_LIBMAGIC], [1], [Define to 1 if you have the `magic' library (-lmagic).])])
fi

dnl ----------------------------------------------------------------------------
dnl handle options
dnl ----------------------------------------------------------------------------

AC_ARG_ENABLE(extended_keys,
    AS_HELP_STRING(
        [--disable-extended-keys],
        [disable extended keys (like arrows, home etc.). Without it will be no
         delay after pressing escape @<:@default=enabled@:>@     ]),
    [extended_keys=$enableval],
    [extended_keys=yes])

AC_ARG_ENABLE(desktop_files,
    AS_HELP_STRING(
        [--disable-desktop-files],
        [disable parsing of *.desktop files found on your system to get a list
         of programs associated with filetypes @<:@default=enabled@:>@     ]),
    [desktop_files=$enableval],
    [desktop_files=yes])

AC_ARG_ENABLE(remote_cmds,
    AS_HELP_STRING(
        [--enable-remote-cmds],
        [enable remote command sending.
         @<:@default=enabled@:>@     ]),
    [remote_cmds=$enableval],
    [remote_cmds=yes])

AC_ARG_ENABLE(developer,
    AS_HELP_STRING(
        [--enable-developer],
        [enables features of interest to developers
         @<:@default=disabled@:>@     ]),
    [developer=$enableval],
    [developer=no])

AC_ARG_ENABLE(werror,
    AS_HELP_STRING(
        [--enable-werror],
        [pass -Werror flag to compiler
         @<:@default=disabled@:>@     ]),
    [werror=$enableval],
    [werror=no])

AC_ARG_ENABLE(coverage,
    AS_HELP_STRING(
        [--enable-coverage],
        [enables coverage information generation
         @<:@default=disabled@:>@     ]),
    [coverage=$enableval],
    [coverage=no])

AC_ARG_ENABLE(build-timestamp,
    AS_HELP_STRING(
        [--disable-build-timestamp],
        [disables embedding build-timestamp information into executable
         @<:@default=enabled@:>@     ]),
    [build_timestamp=$enableval],
    [build_timestamp=yes])

AC_ARG_WITH(sanitize,
    AS_HELP_STRING(
            [--with-sanitize=basic|thread|leak],
            [enables sanitizers, "basic" means address and undefined sanitizers
             @<:@default=@:>@     ]),
    [sanitize=$withval])

if test "$extended_keys" = "yes"; then
    AC_DEFINE([ENABLE_EXTENDED_KEYS], [1], [enables extended keys (arrows etc.)])
fi

if test "$desktop_files" = "yes"; then
    AC_DEFINE([ENABLE_DESKTOP_FILES], [1], [parsing of .desktop files])
fi

if test "$remote_cmds" = "yes"; then
    AC_DEFINE([ENABLE_REMOTE_CMDS], [1], [executing commands remotely])
fi

if test "$use_dyn_libX11" = "yes"; then
    ORIG_LIBS="$LIBS"
    AC_CHECK_LIB(dl, dlopen,
                 [
                 use_dyn_libX11=yes
                 LIBS="$LIBS -ldl"
                 ], [use_dyn_libX11=no])

    AC_CHECK_HEADER([dlfcn.h], [], [use_dyn_libX11=no])
    AC_CHECK_FUNC([dlsym], [], [use_dyn_libX11=no])
    AC_CHECK_FUNC([dlclose], [], [use_dyn_libX11=no])

    if test "$use_dyn_libX11" != "yes"; then
        LIBS="$ORIG_LIBS"
        AC_MSG_WARN(["libdl seems to don't work as expected.  No dynamic X11."])
    fi
fi

if test "$use_libX11" = "yes"; then
    AC_CHECK_LIB(X11, XOpenDisplay,
        [
        AC_CHECK_HEADER([X11/Xlib.h], [], [AC_MSG_ERROR([X11/Xlib.h header not found.])])
        AC_CHECK_HEADER([X11/Xutil.h], [], [AC_MSG_ERROR([X11/Xutil.h header not found.])])
        AC_DEFINE([HAVE_X11], [1], [use X11 to determine terminal emulator title])
        if test "$use_dyn_libX11" = "yes"; then
            AC_DEFINE([DYN_X11], [1], [load libX11 dynamically])
        else
            LIBS="$LIBS -lX11"
        fi
        ])
fi

if test "$developer" = "yes"; then
    CFLAGS="$CFLAGS -g -O0 -Werror -Werror=sign-compare -Wno-unused-parameter"
    TESTS_CFLAGS="$TESTS_CFLAGS -Werror -Werror=sign-compare -Wno-unused-parameter"
fi

if test "$werror" = "yes"; then
    CFLAGS="$CFLAGS -Werror"
    TESTS_CFLAGS="$TESTS_CFLAGS -Werror"
fi

if test "$coverage" = "yes"; then
    CFLAGS="$CFLAGS -O0 --coverage"
    LDFLAGS="$LDFLAGS --coverage"
fi

if test "$build_timestamp" = "yes"; then
    AC_DEFINE([WITH_BUILD_TIMESTAMP], [1],
              [Define to 1 to embed compilation date into executable.])
fi

dnl Handle sanitizers options
if test -n "$sanitize" -a "$sanitize" != "basic" -a "$sanitize" != "thread" -a \
    "$sanitize" != "leak"; then
    AC_MSG_ERROR([Unknown type of sanitizing: $sanitize])
fi
if test "$sanitize" = "basic"; then
    SANITIZERS_CFLAGS="-fsanitize=address -fsanitize=undefined"
fi
if test "$sanitize" = "thread"; then
    SANITIZERS_CFLAGS="-fsanitize=thread -fPIC"
    LDFLAGS="$LDFLAGS -pie"
fi
if test "$sanitize" = "leak"; then
    LDFLAGS="$LDFLAGS -fsanitize=leak"
fi
dnl The flags should be available separately for fetching on building tests.
AC_SUBST([SANITIZERS_CFLAGS])
CFLAGS="$CFLAGS $SANITIZERS_CFLAGS"
LDFLAGS="$LDFLAGS $SANITIZERS_CFLAGS"

dnl Include config.h in all translation units implicitly
CFLAGS="$CFLAGS -include ../build-aux/config.h"

AC_CONFIG_FILES([
Makefile
src/Makefile
])
AC_OUTPUT
