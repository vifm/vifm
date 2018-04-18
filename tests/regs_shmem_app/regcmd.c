#include <stdio.h> /* printf() puts() */
#include <string.h> /* strcmp() strtok() */
#include <stdlib.h> /* EXIT_FAILURE exit() */

#include "../../src/registers.h"

#define LINE_SIZE 32768

static int regcmd_get_reg_name(void);
static void regcmd_set(void);
static void regcmd_get(void);
static void regcmd_help(void);
static void regcmd_sync(const char action[]);

int
main(int argc, char *argv[])
{
	char lnbuf[LINE_SIZE];
	char *action;
	char prompt = 0;

	if(argc >= 2)
	{
		if(strcmp(argv[1], "--help") == 0)
		{
			printf("USAGE %s NAME\n", argv[0]);
			return EXIT_SUCCESS;
		}
		else
		{
			printf("error,Unknown parameter: %s\n", argv[1]);
			return EXIT_FAILURE;
		}
	}

	regs_sync_enable_test_mode();
	regs_init();

	while(fgets(lnbuf, sizeof(lnbuf), stdin) != NULL)
	{
		lnbuf[strlen(lnbuf) - 1] = 0; /* replace \n by end of string */
		action = strtok(lnbuf, ",");
		if(strcmp(action, "set") == 0)
		{
			regcmd_set();
		}
		else if(strcmp(action, "get") == 0)
		{
			regcmd_get();
		}
		else if(strcmp(action, "help") == 0)
		{
			regcmd_help();
		}
		else if(strcmp(action, "prompt") == 0)
		{
			prompt = !prompt;
		}
		else if(strlen(action) > 5 && action[0] == 's' &&
				action[1] == 'y' && action[2] == 'n' &&
				action[3] == 'c' && action[4] == '_')
		{
			regcmd_sync(action + 5);
		}
		else if(strcmp(action, "print_mem") == 0)
		{
			regs_sync_debug_print_memory();
		}
		else
		{
			printf("error,Unknown command: %s\n", action);
		}

		if(prompt)
		{
			printf("> ");
		}
		fflush(stdout);
	}

	/* Reply to CTRL-D with \n. */
	putchar('\n');
	return EXIT_SUCCESS;
}

static void
regcmd_set(void)
{
	int reg_name = regcmd_get_reg_name();
	char *a_value;
	regs_clear(reg_name);
	while((a_value = strtok(NULL, ",")) != NULL)
	{
		regs_append(reg_name, a_value);
	}
}

static int
regcmd_get_reg_name(void)
{
	const char *curtok = strtok(NULL, ",");
	if(curtok == NULL)
	{
		printf("error,Syntax error / token expected. Terminating.\n");
		exit(EXIT_FAILURE);
	}
	return *curtok;
}

static void
regcmd_get(void)
{
	int reg_name = regcmd_get_reg_name();
	reg_t *cnt = regs_find(reg_name);
	if(cnt == NULL)
	{
		printf("error,Regsiter name incorrect: %d (%c)\n", reg_name,
				(char)reg_name);
	}
	else
	{
		int i;
		printf("%c,%d,", (char)cnt->name, cnt->nfiles);
		for(i = 0; i < cnt->nfiles; ++i)
		{
			printf("%s,", cnt->files[i]);
		}
		putchar('\n');
	}
	fflush(stdout);
}

static void
regcmd_help(void)
{
	puts(
	"set,REGISTER,VAL1,VAL2,...  Set register value.\n"
	"get,REGISTER                Print register contents.\n"
	"help                        Display help.\n"
	"prompt                      Toggle prompt.\n"
	"print_mem                   Print memory contents.\n"
	"sync_enable,SHMNAME         Attach to shared memory SHMNAME.\n"
	"sync_disable                Detach from shared memory.\n"
	"sync_from                   Synchronize from shared memory.\n"
	"sync_to                     Synchronize to shared memory.\n"
	);
}

static void
regcmd_sync(const char action[])
{
	if(strcmp(action, "to") == 0)
	{
		regs_sync_to_shared_memory();
	}
	else if(strcmp(action, "from") == 0)
	{
		regs_sync_from_shared_memory();
	}
	else if(strcmp(action, "enable") == 0)
	{
		regs_sync_enable(strtok(NULL, ","));
	}
	else if(strcmp(action, "disable") == 0)
	{
		regs_sync_disable();
	}
	else
	{
		printf("error,Unknown sync command: %s\n", action);
		return;
	}

	printf("ack,sync_%s\n", action);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
