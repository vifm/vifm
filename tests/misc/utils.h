#ifndef VIFM_TESTS__UTILS_H__
#define VIFM_TESTS__UTILS_H__

#include "../../src/ui/ui.h"

/* Executable suffixes. */
#if defined(__CYGWIN__) || defined(_WIN32)
#define EXE_SUFFIX ".exe"
#define EXE_SUFFIXW L".exe"
#else
#define EXE_SUFFIX ""
#define EXE_SUFFIXW L""
#endif

/* Prepares option handler for use in tests. */
void opt_handlers_setup(void);

/* Cleans up option handlers. */
void opt_handlers_teardown(void);

/* Initializes view with safe defaults. */
void view_setup(FileView *view);

/* Frees resources of the view. */
void view_teardown(FileView *view);

/* Creates file at the path. */
void create_file(const char path[]);

/* Creates executable file at the path. */
void create_executable(const char path[]);

/* Either puts base/sub or cwd/base/sub into the buf. */
void make_abs_path(char buf[], size_t buf_len, const char base[],
		const char sub[], const char cwd[]);

/* Copies file at src to dst. */
void copy_file(const char src[], const char dst[]);

/* Whether running on non-Windowsla.  Returns non-zero if so, otherwise zero is
 * returned. */
int not_windows(void);

#endif /* VIFM_TESTS__UTILS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
