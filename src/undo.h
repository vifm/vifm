#ifndef __UNDO_H__
#define __UNDO_H__

/* Won't call reset_undo_list, so this function could be called multiple
 * times */
void init_undo_list(void (*exec_func)(const char *), const int* max_levels);

/* Frees all allocated memory */
void reset_undo_list(void);

/* Only stores msg pointer, so it should be valid until cmd_group_end is
 * called */
void cmd_group_begin(const char *msg);

/* Reopens last command group */
void cmd_group_continue(void);

/* Returns 0 on success */
int add_operation(const char *do_cmd, const char *undo_cmd);

void cmd_group_end(void);

/* Returns 0 on success, otherwise no operation for undo is available and -1 is
 * returned. */
int undo_group(void);

/* Returns 0 on success, otherwise no operation for redo is available and -1 is
 * returned. */
int redo_group(void);

/* 
 * When detail is not 0 show detailed information for groups.
 * Last element of list returned is NULL.
 * Returns NULL on error.
 */
char ** undolist(int detail);

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
