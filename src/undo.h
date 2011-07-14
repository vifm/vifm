#ifndef __UNDO_H__
#define __UNDO_H__

/*
 * Won't call reset_undo_list, so this function could be called multiple
 * times.
 * exec_func can't be NULL and should return non-zero on error.
 */
void init_undo_list(int (*exec_func)(const char *), const int* max_levels);

/*
 * Frees all allocated memory
 */
void reset_undo_list(void);

/*
 * Only stores msg pointer, so it should be valid until cmd_group_end is
 * called
 */
void cmd_group_begin(const char *msg);

/*
 * Reopens last command group
 */
void cmd_group_continue(void);

/*
 * Returns 0 on success
 */
int add_operation(const char *do_cmd, const char *undo_cmd);

void cmd_group_end(void);

/*
 * Return value:
 *   0 - on success
 *  -1 - no operation for undo is available
 *  -2 - there were errors
 *   1 - operation was skipped due to previous errors (no command run)
 */
int undo_group(void);

/*
 * Return value:
 *   0 - on success
 *  -1 - no operation for undo is available
 *  -2 - there were errors
 *   1 - operation was skipped due to previous errors (no command run)
 */
int redo_group(void);

/*
 * When detail is not 0 show detailed information for groups.
 * Last element of list returned is NULL.
 * Returns NULL on error.
 */
char ** undolist(int detail);

/*
 * Returns number of the group.
 */
int get_undolist_pos(int detail);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
