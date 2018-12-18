#ifndef VIFM_TESTS__UTILS__UTILS_H__
#define VIFM_TESTS__UTILS__UTILS_H__

/* Whether running on Windows.  Returns non-zero if so, otherwise zero is
 * returned. */
int windows(void);

/* Whether running on something other than Windows.  Returns non-zero if so,
 * otherwise zero is returned. */
int not_windows(void);

/* Attempts to switch to UTF-8 capable locale.  Use utf8_locale() to check if
 * successful. */
void try_enable_utf8_locale(void);

/* Whether locale is UTF-8.  Returns non-zero if so, otherwise zero is
 * returned. */
int utf8_locale(void);

#endif /* VIFM_TESTS__UTILS__UTILS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
