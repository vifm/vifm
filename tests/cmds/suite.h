#ifndef VIFM_TESTS__SUITE_H__
#define VIFM_TESTS__SUITE_H__

struct cmd_info_t;

extern int (*user_cmd_handler)(const struct cmd_info_t *cmd_info);

extern int swap_range;

#endif /* VIFM_TESTS__SUITE_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
