#ifndef VIFM_TESTS__UTILS_H__
#define VIFM_TESTS__UTILS_H__

#include "../../src/ui/ui.h"

/* Initializes view with safe defaults. */
void view_setup(FileView *view);

/* Frees resources of the view. */
void view_teardown(FileView *view);

/* Creates empty file at specified path. */
void create_empty_file(const char path[]);

#endif /* VIFM_TESTS__UTILS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
