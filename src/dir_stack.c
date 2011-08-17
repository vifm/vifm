#include <stdlib.h>
#include <string.h>

#include "filelist.h"
#include "ui.h"

#include "dir_stack.h"

struct stack_entry * stack;
unsigned int stack_top;
static unsigned int stack_size;

static void free_entry(const struct stack_entry * entry);

int
pushd(void)
{
	return push_to_dirstack(lwin.curr_dir, lwin.dir_entry[lwin.list_pos].name,
			rwin.curr_dir, rwin.dir_entry[rwin.list_pos].name);
}

int
push_to_dirstack(const char *ld, const char *lf, const char *rd, const char *rf)
{
	if(stack_top == stack_size)
	{
		struct stack_entry* s = realloc(stack, (stack_size + 1)*sizeof(*stack));
		if(s == NULL)
			return -1;

		stack = s;
		stack_size++;
	}

	stack[stack_top].lpane_dir = strdup(ld);
	stack[stack_top].lpane_file = strdup(lf);
	stack[stack_top].rpane_dir = strdup(rd);
	stack[stack_top].rpane_file = strdup(rf);

	if(stack[stack_top].lpane_dir == NULL ||
			stack[stack_top].lpane_file == NULL ||
			stack[stack_top].rpane_dir == NULL || stack[stack_top].rpane_file == NULL)
	{
		free_entry(&stack[stack_top]);
		return -1;
	}

	stack_top++;

	return 0;
}

int
popd(void)
{
	if(stack_top == 0)
		return -1;

	stack_top -= 1;

	change_directory(&lwin, stack[stack_top].lpane_dir);
	load_dir_list(&lwin, 0);

	change_directory(&rwin, stack[stack_top].rpane_dir);
	load_dir_list(&rwin, 0);

	moveto_list_pos(curr_view, curr_view->list_pos);

	free_entry(&stack[stack_top]);

	return 0;
}

void
clean_stack(void)
{
	while(stack_top > 0)
		free_entry(&stack[--stack_top]);
}

static void
free_entry(const struct stack_entry * entry)
{
	free(entry->lpane_dir);
	free(entry->lpane_file);
	free(entry->rpane_dir);
	free(entry->rpane_file);
}

char **
dir_stack_list(void)
{
	int i;
	int len;
	char **list, **p;

	if(stack_top == 0)
		len = 2 + 1 + 1;
	else
		len = 2 + 1 + stack_top*2 + stack_top - 1 + 1;
	list = malloc(sizeof(char *)*len);

	if(list == NULL)
		return NULL;

	p = list;
	if((*p++ = strdup(lwin.curr_dir)) == NULL)
		return list;
	if((*p++ = strdup(rwin.curr_dir)) == NULL)
		return list;
	if(stack_top != 0)
	{
		if((*p++ = strdup("-----")) == NULL)
			return list;
	}

	for(i = 0; i < stack_top; i++)
	{
		if((*p++ = strdup(stack[stack_top - 1 - i].lpane_dir)) == NULL)
			return list;

		if((*p++ = strdup(stack[stack_top - 1 - i].rpane_dir)) == NULL)
			return list;

		if(i == stack_top - 1)
			continue;

		if((*p++ = strdup("-----")) == NULL)
			return list;
	}
	*p = NULL;

	return list;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
