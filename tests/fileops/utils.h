#ifndef VIFM_TESTS__UTILS_H__
#define VIFM_TESTS__UTILS_H__

#include <stddef.h> /* size_t */

#include "../../src/ui/ui.h"

/* Initializes view with safe defaults. */
void view_setup(FileView *view);

/* Frees resources of the view. */
void view_teardown(FileView *view);

/* Creates empty file at specified path. */
void create_empty_file(const char path[]);

/* Creates empty directory at specified path. */
void create_empty_dir(const char path[]);

/* Waits termination of all background tasks. */
void wait_for_bg(void);

/* Writes absolute path to the sandbox into the buffer. */
void set_to_sandbox_path(char buf[], size_t buf_len);

#endif /* VIFM_TESTS__UTILS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
