#ifndef VIFM_TESTS__UTILS_H__
#define VIFM_TESTS__UTILS_H__

void create_test_file(const char name[]);

void clone_test_file(const char src[], const char dst[]);

void delete_test_file(const char name[]);

int files_are_identical(const char a[], const char b[]);

#endif /* VIFM_TESTS__UTILS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
