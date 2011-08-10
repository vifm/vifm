#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "registers.h"
#include "utils.h"

#include "undo.h"

struct group_t {
	char *msg;
	int error;
	int balance;
	int can_undone;
	int incomplete;
};

struct op_t {
	char *cmd;
	char *src;
	char *dst;
};

struct cmd_t {
	struct op_t do_op;
	struct op_t undo_op;
	struct group_t *group;

	struct cmd_t *prev;
	struct cmd_t *next;
};

static int (*do_func)(const char*);
static const int *undo_levels;

static struct cmd_t cmds = {
	.prev = &cmds,
};
static struct cmd_t *current = &cmds;

static size_t trash_dir_len;

static int group_opened;
static long long next_group;
static struct group_t *last_group;
static char *group_msg;

static int command_count;

static void remove_cmd(struct cmd_t *cmd);
static int is_undo_group_possible(void);
static int is_redo_group_possible(void);
static int is_op_possible(const struct op_t *op);
static void change_filename_in_trash(struct cmd_t *cmd, const char *filename);
static char **fill_undolist_detail(char **list);
static char **fill_undolist_nondetail(char **list);

void
init_undo_list(int (*exec_func)(const char *), const int* max_levels)
{
	assert(exec_func != NULL);

	do_func = exec_func;
	undo_levels = max_levels;

	trash_dir_len = strlen(cfg.trash_dir);
}

void
reset_undo_list(void)
{
	assert(!group_opened);

	while(cmds.next != NULL)
		remove_cmd(cmds.next);
	cmds.prev = &cmds;

	current = &cmds;
	next_group = 0;
}

void
cmd_group_begin(const char *msg)
{
	assert(!group_opened);

	group_opened = 1;

	free(group_msg);
	group_msg = strdup(msg);
	last_group = NULL;
}

void
cmd_group_continue(void)
{
	assert(!group_opened);
	assert(next_group != 0);

	group_opened = 1;
	next_group--;
}

char *
replace_group_msg(const char *msg)
{
	char *result;

	result = group_msg;
	group_msg = (msg != NULL) ? strdup(msg) : NULL;
	if(last_group != NULL && group_msg != NULL)
	{
		free(last_group->msg);
		last_group->msg = strdup(group_msg);
	}
	return result;
}

int
add_operation(const char *do_cmd, const char *do_src, const char *do_dst,
		const char *undo_cmd, const char *undo_src, const char *undo_dst)
{
	int mem_error;
	struct cmd_t *cmd;

	assert(group_opened);

	/* free list tail */
	while(current->next != NULL)
		remove_cmd(current->next);

	while(command_count > 0 && command_count >= *undo_levels)
		remove_cmd(cmds.next);

	if(*undo_levels <= 0)
		return 0;

	command_count++;

	/* add operation to the list */
	cmd = calloc(1, sizeof(*cmd));
	if(cmd == NULL)
		return -1;

	cmd->do_op.cmd = strdup(do_cmd);
	cmd->undo_op.cmd = strdup(undo_cmd);
	if(last_group != NULL)
	{
		cmd->group = last_group;
	}
	else if((cmd->group = malloc(sizeof(struct group_t))) != NULL)
	{
		cmd->group->msg = strdup(group_msg);
		cmd->group->error = 0;
		cmd->group->balance = 0;
		cmd->group->can_undone = 1;
		cmd->group->incomplete = 0;
	}
	mem_error = cmd->do_op.cmd == NULL || cmd->undo_op.cmd == NULL ||
			cmd->group == NULL || cmd->group->msg == NULL;
	if(do_src != NULL && (cmd->do_op.src = strdup(do_src)) == NULL)
		mem_error = 1;
	if(do_dst != NULL && (cmd->do_op.dst = strdup(do_dst)) == NULL)
		mem_error = 1;
	if(undo_src != NULL && (cmd->undo_op.src = strdup(undo_src)) == NULL)
		mem_error = 1;
	if(undo_dst != NULL && (cmd->undo_op.dst = strdup(undo_dst)) == NULL)
		mem_error = 1;
	if(mem_error)
	{
		remove_cmd(cmd);
		return -1;
	}
	last_group = cmd->group;

	if(undo_cmd[0] == '\0')
		cmd->group->can_undone = 0;

	cmd->prev = current;
	current->next = cmd;
	current = cmd;
	cmds.prev = cmd;

	return 0;
}

static void
remove_cmd(struct cmd_t *cmd)
{
	int last_cmd_in_group = 1;

	if(cmd == current)
		current = cmd->prev;

	if(cmd->prev != NULL)
	{
		cmd->prev->next = cmd->next;
		if(cmd->group == cmd->prev->group)
			last_cmd_in_group = 0;
	}
	if(cmd->next != NULL)
	{
		cmd->next->prev = cmd->prev;
		if(cmd->group == cmd->next->group)
			last_cmd_in_group = 0;
	}
	else /* this is the last command in the list */
	{
		cmds.prev = cmd->prev;
	}

	if(last_cmd_in_group)
	{
		free(cmd->group->msg);
		free(cmd->group);
		if(last_group == cmd->group)
			last_group = NULL;
	}
	else
	{
		cmd->group->incomplete = 1;
	}
	free(cmd->do_op.cmd);
	free(cmd->do_op.src);
	free(cmd->do_op.dst);
	free(cmd->undo_op.cmd);
	free(cmd->undo_op.src);
	free(cmd->undo_op.dst);

	free(cmd);

	command_count--;
}

void
cmd_group_end(void)
{
	assert(group_opened);

	group_opened = 0;
	next_group++;

	while(cmds.next != NULL && cmds.next->group->incomplete)
		remove_cmd(cmds.next);
}

int
undo_group(void)
{
	int errors, disbalance, cant_undone;
	int skip;
	assert(!group_opened);

	if(current == &cmds)
		return -1;

	errors = current->group->error != 0;
	disbalance = current->group->balance != 0;
	cant_undone = !current->group->can_undone;
	if(errors || disbalance || cant_undone || !is_undo_group_possible())
	{
		do
			current = current->prev;
		while(current != &cmds && current->group == current->next->group);
		if(errors)
			return 1;
		else if(disbalance)
			return -4;
		else if(cant_undone)
			return -5;
		else
			return -3;
	}

	current->group->balance--;

	skip = 0;
	do
	{
		if(!skip)
		{
			int err = do_func(current->undo_op.cmd);
			if(err == SKIP_UNDO_REDO_OPERATION)
			{
				skip = 1;
				current->group->balance++;
			}
			else if(err != 0)
			{
				current->group->error = 1;
				errors = 1;
			}
		}
		current = current->prev;
	}
	while(current != &cmds && current->group == current->next->group);

	if(skip)
		return -6;

	return errors ? -2 : 0;
}

static int
is_undo_group_possible(void)
{
	struct cmd_t *cmd = current;
	do
	{
		int ret;
		ret = is_op_possible(&cmd->undo_op);
		if(ret == 0)
			return 0;
		else if(ret < 0)
			change_filename_in_trash(cmd, cmd->undo_op.dst);
		cmd = cmd->prev;
	}
	while(cmd != &cmds && cmd->group == cmd->next->group);
	return 1;
}

int
redo_group(void)
{
	int errors, disbalance;
	int skip;
	assert(!group_opened);

	if(current->next == NULL)
		return -1;

	errors = current->next->group->error != 0;
	disbalance = current->next->group->balance == 0;
	if(errors || disbalance || !is_redo_group_possible())
	{
		do
			current = current->next;
		while(current->next != NULL && current->group == current->next->group);
		if(errors)
			return 1;
		else if(disbalance)
			return -4;
		else
			return -3;
	}

	current->next->group->balance++;

	skip = 0;
	do
	{
		current = current->next;
		if(!skip)
		{
			int err = do_func(current->do_op.cmd);
			if(err == SKIP_UNDO_REDO_OPERATION)
			{
				current->next->group->balance--;
				skip = 1;
			}
			else if(err != 0)
			{
				current->group->error = 1;
				errors = 1;
			}
		}
	}
	while(current->next != NULL && current->group == current->next->group);

	if(skip)
		return -6;

	return errors ? -2 : 0;
}

static int
is_redo_group_possible(void)
{
	struct cmd_t *cmd = current;
	do
	{
		int ret;
		cmd = cmd->next;
		ret = is_op_possible(&cmd->do_op);
		if(ret == 0)
			return 0;
		else if(ret < 0)
			change_filename_in_trash(cmd, cmd->do_op.dst);
	}
	while(cmd->next != NULL && cmd->group == cmd->next->group);
	return 1;
}

/*
 * Return value:
 *   0 - impossible
 * < 0 - possible with renaming file in trash
 * > 0 -possible
 */
static int
is_op_possible(const struct op_t *op)
{
	struct stat st;

	if(op->src != NULL && lstat(op->src, &st) != 0)
		return 0;
	if(op->dst != NULL && lstat(op->dst, &st) == 0)
	{
		if(strncmp(op->dst, cfg.trash_dir, trash_dir_len) == 0)
			return -1;
		return 0;
	}
	return 1;
}

static void
change_filename_in_trash(struct cmd_t *cmd, const char *filename)
{
	char *escaped;
	char *p;
	char buf[PATH_MAX];
	int i = -1;

	p = strchr(filename + trash_dir_len, '_') + 1;

	do
		snprintf(buf, sizeof(buf), "%s/%03i_%s", cfg.trash_dir, ++i, p);
	while(access(buf, F_OK) == 0);
	rename_in_registers(filename, buf);
	escaped = escape_filename(buf, 0, 0);

	p = strstr(cmd->do_op.cmd, cfg.escaped_trash_dir);
	if(p != NULL)
	{
		*p = '\0';
		snprintf(buf, sizeof(buf), "%s%s%s", cmd->do_op.cmd, escaped,
				p + strlen(filename));
		free(cmd->do_op.cmd);
		cmd->do_op.cmd = strdup(buf);
	}

	p = strstr(cmd->undo_op.cmd, cfg.escaped_trash_dir);
	if(p != NULL)
	{
		*p = '\0';
		snprintf(buf, sizeof(buf), "%s%s%s", cmd->undo_op.cmd, escaped,
				p + strlen(filename));
		free(cmd->undo_op.cmd);
		cmd->undo_op.cmd = strdup(buf);
	}

	snprintf(buf, sizeof(buf), "%s/%03i%s", cfg.trash_dir, ++i, filename);
	if(strncmp(cmd->do_op.dst, cfg.trash_dir, trash_dir_len) == 0)
	{
		free(cmd->do_op.dst);
		cmd->do_op.dst = strdup(buf);
	}
	if(strncmp(cmd->undo_op.dst, cfg.trash_dir, trash_dir_len) == 0)
	{
		free(cmd->undo_op.dst);
		cmd->undo_op.dst = strdup(buf);
	}

	free(escaped);
}

char **
undolist(int detail)
{
	char **list, **p;
	int group_count;
	struct cmd_t *cmd;

	assert(!group_opened);

	group_count = 1;
	cmd = cmds.prev;
	while(cmd != &cmds)
	{
		if(cmd->group != cmd->prev->group)
			group_count++;
		cmd = cmd->prev;
	}

	if(detail)
		list = malloc(sizeof(char *)*(group_count + command_count*2 + 1));
	else
		list = malloc(sizeof(char *)*group_count);

	if(list == NULL)
		return NULL;

	if(detail)
		p = fill_undolist_detail(list);
	else
		p = fill_undolist_nondetail(list);
	*p = NULL;

	return list;
}

static char **
fill_undolist_detail(char **list)
{
	int left;
	struct cmd_t *cmd;

	left = *undo_levels;
	cmd = cmds.prev;
	while(cmd != &cmds && left > 0)
	{
		if((*list = strdup(cmd->group->msg)) == NULL)
			break;

		list++;
		do
		{
			if((*list = malloc(4 + strlen(cmd->do_op.cmd) + 1)) == NULL)
				return list;
			sprintf(*list, "do: %s", cmd->do_op.cmd);
			list++;

			if((*list = malloc(6 + strlen(cmd->undo_op.cmd) + 1)) == NULL)
				return list;
			sprintf(*list, "undo: %s", cmd->undo_op.cmd);
			list++;

			cmd = cmd->prev;
			--left;
		}
		while(cmd != &cmds && cmd->group == cmd->next->group && left > 0);
	}

	return list;
}

static char **
fill_undolist_nondetail(char **list)
{
	int left;
	struct cmd_t *cmd;

	left = *undo_levels;
	cmd = cmds.prev;
	while(cmd != &cmds && left-- > 0)
	{
		if((*list = strdup(cmd->group->msg)) == NULL)
			break;

		do
			cmd = cmd->prev;
		while(cmd != &cmds && cmd->group == cmd->next->group);
		list++;
	}

	return list;
}

int
get_undolist_pos(int detail)
{
	struct cmd_t *cur = cmds.prev;
	int result_group = 0;
	int result_cmd = 0;

	assert(!group_opened);

	if(cur == &cmds)
		result_group++;
	while(cur != current)
	{
		if(cur->group != cur->prev->group)
			result_group++;
		result_cmd += 2;
		cur = cur->prev;
	}
	return detail ? (result_group + result_cmd) : result_group;
}

void
clean_cmds_with_trash(void)
{
	struct cmd_t *cur = cmds.prev;

	assert(!group_opened);

	while(cur != &cmds)
	{
		struct cmd_t *prev = cur->prev;

		if(cur->group->balance < 0)
		{
			if(cur->do_op.src != NULL &&
					strncmp(cur->do_op.src, cfg.trash_dir, trash_dir_len) == 0)
				remove_cmd(cur);
		}
		else
		{
			if(cur->undo_op.src != NULL &&
					strncmp(cur->undo_op.src, cfg.trash_dir, trash_dir_len) == 0)
				remove_cmd(cur);
		}
		cur = prev;
	}
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
