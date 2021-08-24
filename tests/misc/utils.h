#ifndef VIFM_TESTS__UTILS_H__
#define VIFM_TESTS__UTILS_H__

struct dir_entry_t;
struct view_t;

int load_tree(struct view_t *view, const char path[], const char cwd[]);

int load_limited_tree(struct view_t *view, const char path[], const char cwd[],
		int depth);

void load_view(struct view_t *view);

void validate_tree(const struct view_t *view);

void validate_parents(const struct dir_entry_t *entries, int nchildren);

#endif /* VIFM_TESTS__UTILS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
