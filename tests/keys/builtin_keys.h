#ifndef VIFM_TESTS__KEYS__BUILTIN_KEYS_H__
#define VIFM_TESTS__KEYS__BUILTIN_KEYS_H__

extern int last;                  /* 1 = k, 2 = j, 3 = yank, 4 = delete */
extern int last_command_count;    /* for ctrl+w <, dd and d + selector */
extern int last_command_register; /* for dd */
extern int last_selector_count;   /* for k */
extern int last_indexes_count;    /* for d + selector */
extern int key_is_mapped;         /* for : and m */
extern int mapping_state;         /* for : and m */

void init_builtin_keys(void);

#endif /* VIFM_TESTS__KEYS__BUILTIN_KEYS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
