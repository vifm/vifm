#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "undo.h"

struct cmd_t {
	char *msg;
	char *do_cmd;
	char *undo_cmd;
	long long group;

	struct cmd_t *prev;
	struct cmd_t *next;
};

static void (*do_func)(const char*);
static const int *undo_levels;

static struct cmd_t cmds = {
	.prev = &cmds,
};
static struct cmd_t *current = &cmds;

static int group_opened;
static long long next_group;
static const char *group_msg;

static int command_count;

static void remove_cmd(struct cmd_t *cmd);
static char **fill_undolist_detail(char **list);
static char **fill_undolist_nondetail(char **list);

void
init_undo_list(void (*exec_func)(const char *), const int* max_levels)
{
	do_func = exec_func;
	undo_levels = max_levels;
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

	group_msg = msg;
}

void
cmd_group_continue(void)
{
	assert(!group_opened);
	assert(next_group != 0);

	group_opened = 1;
	next_group--;
}

int
add_operation(const char *do_cmd, const char *undo_cmd)
{
	struct cmd_t *cmd;

	assert(group_opened);

	/* free list tail */
	while(current->next != NULL)
		remove_cmd(current->next);

	while(command_count > 0 && command_count >= *undo_levels)
	{
		current = current->prev;
		remove_cmd(current->next);
	}

	if(*undo_levels <= 0)
		return 0;

	command_count++;

	/* add operation to the list */
	cmd = calloc(1, sizeof(*cmd));
	if(cmd == NULL)
		return -1;

	cmd->msg = strdup(group_msg);
	cmd->do_cmd = strdup(do_cmd);
	cmd->undo_cmd = strdup(undo_cmd);
	if(cmd->msg == NULL || cmd->do_cmd == NULL || cmd->undo_cmd == NULL)
	{
		remove_cmd(cmd);
		return -1;
	}
	cmd->group = next_group;

	cmd->prev = current;
	current->next = cmd;
	current = cmd;
	cmds.prev = cmd;

	return 0;
}

static void
remove_cmd(struct cmd_t *cmd)
{
	if(cmd->prev != NULL)
		cmd->prev->next = cmd->next;
	if(cmd->next != NULL)
		cmd->next->prev = cmd->prev;

	free(cmd->msg);
	free(cmd->do_cmd);
	free(cmd->undo_cmd);

	free(cmd);

	command_count--;
}

void
cmd_group_end(void)
{
	assert(group_opened);

	group_opened = 0;
	next_group++;
}

int
undo_group(void)
{
	assert(!group_opened);

	if(current == &cmds)
		return -1;

	do
	{
		do_func(current->undo_cmd);
		current = current->prev;
	}
	while(current != &cmds && current->group == current->next->group);

	return 0;
}

int
redo_group(void)
{
	assert(!group_opened);

	if(current->next == NULL)
		return -1;

	do
	{
		current = current->next;
		do_func(current->do_cmd);
	}
	while(current->next != NULL && current->group == current->next->group);

	return 0;
}

char **
undolist(int detail)
{
	char **list, **p;
	int group_count;

	assert(!group_opened);

	group_count = next_group - cmds.group + 1;
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
		if((*list = strdup(cmd->msg)) == NULL)
			break;

		list++;
		do
		{
			if((*list = malloc(4 + strlen(cmd->do_cmd) + 1)) == NULL)
				return list;
			sprintf(*list, "do: %s", cmd->do_cmd);
			list++;

			if((*list = malloc(6 + strlen(cmd->undo_cmd) + 1)) == NULL)
				return list;
			sprintf(*list, "undo: %s", cmd->undo_cmd);
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
		if((*list = strdup(cmd->msg)) == NULL)
			break;

		do
			cmd = cmd->prev;
		while(cmd != &cmds && cmd->group == cmd->next->group);
		list++;
	}

	return list;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
