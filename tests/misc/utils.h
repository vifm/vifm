#ifndef VIFM_TESTS__UTILS_H__
#define VIFM_TESTS__UTILS_H__

#include "../../src/ui/ui.h"

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

/* Either puts base/sub or cwd/base/sub into the buf. */
void make_abs_path(char buf[], size_t buf_len, const char base[],
		const char sub[], const char cwd[]);

#endif /* VIFM_TESTS__UTILS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
