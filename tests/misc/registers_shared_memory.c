#include <stic.h>

#include <fcntl.h>
#include <unistd.h>

#include <errno.h> /* errno */
#include <signal.h> /* SIGTERM */
#include <stdio.h> /* fclose() fdopen() fgets() fprintf() fputs() snprintf() */
#include <stdlib.h> /* exit() */
#include <string.h> /* strerror() */

#include <test-utils.h>

#include "../../src/utils/gmux.h"
#include "../../src/utils/shmem.h"
#include "../../src/utils/utils.h"

static void spawn_regcmd(int number);
static void send_query(int instance, const char query[]);
static void receive_ack(int instance);
static void check_is_initial(int instance, const char reglist[]);
static void receive_answer(int instance, char lnbuf[]);
static void sync_to_from(int instance);
static void sync_from(int instance);
static void check_register_contents(int instance, char register_name,
		const char expected_content[]);
static void test_pat(char result[], size_t patsz, char register_name,
		size_t pat_id_every, char p0, char p1);
static void sync_disable(int instance);
static pid_t popen2(const char cmd[], FILE **in, FILE **out);

#define LINE_SIZE 32768
#define NUM_INSTANCES 3

#define TEST_REGISTERS            "abcdefghijklmnopqrstuvwxyz"
#define TEST_REGISTERS_MINUS_D    "abcefghijklmnopqrstuvwxyz"
#define TEST_REGISTERS_MINUS_DE   "abcfghijklmnopqrstuvwxyz"
#define TEST_REGISTERS_MINUS_DEF  "abcghijklmnopqrstuvwxyz"
#define TEST_REGISTERS_MINUS_DEFG "abchijklmnopqrstuvwxyz"

#define TEST_EXPECT_FOR_D "d,3,nd1,nd2,newd,"
#define TEST_EXPECT_FOR_E "e,4,le1,le2,le3,longerthanbeforee,"
#define TEST_EXPECT_FOR_G "g,1,G,"

static FILE *instance_stdin[NUM_INSTANCES];
static FILE *instance_stdout[NUM_INSTANCES];

static char pat4kib[4096 + 8];

static char *tmpdir_value;

/* All of the tests need to run in exactly the order given for correct
 * results. */

/* An update of Wine broke something, specifically second instance of helper
 * application doesn't seem to be started in SETUP_ONCE. */

/* Tests basic transfer of values from 0 to 1 and spawns processes. */
SETUP_ONCE()
{
	if(!not_wine())
	{
		return;
	}

	/* Put all the temporary files into sandbox to avoid issues with running tests
	 * from multiple accounts. */
	char sandbox[PATH_MAX + 1];
	make_abs_path(sandbox, sizeof(sandbox), SANDBOX_PATH, "", NULL);
	tmpdir_value = mock_env("TMPDIR", sandbox);

	/* Make sure nothing is retained from previous tests. */
	gmux_destroy(gmux_create("regs-test-shmem"));
	shmem_destroy(shmem_create("regs-test-shmem", 10, 10));

	/* Spawn processes. */
	spawn_regcmd(0);
	spawn_regcmd(1);

	/* Initial register values. */
	const char *curletr;
	for(curletr = TEST_REGISTERS; *curletr != '\0'; ++curletr)
	{
		fprintf(instance_stdin[0], "set,%c,_initial%c,i%c1,i%c2,i%c3\n",
			*curletr, *curletr, *curletr, *curletr, *curletr);
		fprintf(instance_stdin[1], "set,%c,lossval%c,l%c1,l%c2,l%c3\n",
			*curletr, *curletr, *curletr, *curletr, *curletr);
	}

	/* Enable shared memory synchronization. */
	send_query(0, "sync_enable,test-shmem\n");
	receive_ack(0);
	send_query(1, "sync_enable,test-shmem\n");
	receive_ack(1);
}

TEARDOWN_ONCE()
{
	if(!not_wine())
	{
		return;
	}

	/* Make sure nothing is retained from the tests. */
	gmux_destroy(gmux_create("regs-test-shmem"));
	shmem_destroy(shmem_create("regs-test-shmem", 10, 10));

	unmock_env("TMPDIR", tmpdir_value);
}

static void
spawn_regcmd(int number)
{
	const int debug = (getenv("DEBUG") != NULL);
	const char *cmd = debug ? "bin" SL "debug" SL "regs_shmem_app"
	                        : "bin" SL "regs_shmem_app";

	if(popen2(cmd, &instance_stdin[number],
				&instance_stdout[number]) == (pid_t)-1)
	{
		perror("Failed to call fork");
		exit(1);
	}

	setvbuf(instance_stdin[number], NULL, _IONBF, 0);
}

static void
send_query(int instance, const char query[])
{
	if(fputs(query, instance_stdin[instance]) == EOF)
	{
		fprintf(stderr, "Failed to write query to FD %p "
			"(instance %d): %s; query was %s",
			instance_stdin[instance], instance, strerror(errno), query);
		exit(1);
	}
}

static void
receive_ack(int instance)
{
	char bakval;
	char lnbuf[LINE_SIZE];
	receive_answer(instance, lnbuf);
	bakval = lnbuf[3];
	/* Cut off ,... after ack */
	lnbuf[3] = '\0';
	if(strcmp(lnbuf, "ack") != 0)
	{
		lnbuf[3] = bakval;
		fprintf(stderr, "Error: Did not receive ACK but the following "
			"response from instance %d: %s\n", instance, lnbuf);
		exit(1);
	}
}

static void
check_is_initial(int instance, const char reglist[])
{
	const char *curletr;
	for(curletr = reglist; *curletr != '\0'; ++curletr)
	{
		char cmp[27];
		snprintf(cmp, sizeof(cmp), "%c,4,_initial%c,i%c1,i%c2,i%c3,",
			*curletr, *curletr, *curletr, *curletr, *curletr);
		check_register_contents(instance, *curletr, cmp);
	}
}

static void
receive_answer(int instance, char lnbuf[])
{
	if(fgets(lnbuf, LINE_SIZE, instance_stdout[instance]) == NULL)
	{
		printf("Child (instance %d) unexpectedly sent eof\n", instance);
		exit(1);
	}

	/* Replace \n with '\0'. */
	lnbuf[strlen(lnbuf) - 1] = '\0';
}

TEST(make_sure_instance_0_does_not_lose_data, IF(not_wine))
{
	sync_from(0);
	check_is_initial(0, TEST_REGISTERS);
}

static void
sync_to_from(int instance)
{
	send_query(instance, "sync_to\n");
	receive_ack(instance);

	sync_from(!instance);
}

static void
sync_from(int instance)
{
	send_query(instance, "sync_from\n");
	receive_ack(instance);
}

TEST(make_sure_sync_to_instance_1_works, IF(not_wine))
{
	sync_from(1);
	check_is_initial(1, TEST_REGISTERS);
}

TEST(chg_update_in_place, IF(not_wine))
{
	send_query(1, "set,d,newd,nd1,nd2\n");
	sync_to_from(1);

	check_register_contents(0, 'd', TEST_EXPECT_FOR_D);

	/* check others */
	check_is_initial(0, TEST_REGISTERS_MINUS_D);
}

static void
check_register_contents(int instance, char register_name,
		const char expected_content[])
{
	char lnbuf[LINE_SIZE];
	char query[7];
	strcpy(query, "get,X\n");
	query[4] = register_name;

	send_query(instance, query);
	receive_answer(instance, lnbuf);
	assert_string_equal(expected_content, lnbuf);
}

TEST(chg_append_to_end, IF(not_wine))
{
	send_query(1, "set,e,longerthanbeforee,le1,le2,le3\n");
	sync_to_from(1);

	/* Ignore D. */

	/* Check E. */
	check_register_contents(0, 'e', TEST_EXPECT_FOR_E);

	/* Check others. */
	check_is_initial(0, TEST_REGISTERS_MINUS_DE);
}

TEST(chg_double_allocation_size_till_fit, IF(not_wine))
{
	test_pat(pat4kib, 4096, 'f', 128, '4', 'f');
	/* Check initial. */
	check_is_initial(0, TEST_REGISTERS_MINUS_DEF);
}

/* Result needs to have patsz + 8 entries (set,X, + \n + 0-byte). */
static void
test_pat(char result[], size_t patsz, char register_name, size_t pat_id_every,
		char p0, char p1)
{
	/* Prepare pattern. */
	size_t i;
	char pre[] = "set,X,";
	strcpy(result, pre);
	result[4] = register_name;
	for(i = 0; i < patsz; ++i)
	{
		result[sizeof(pre) - 1 + i] = ((i % pat_id_every) == 0)? p0: p1;
	}
	strcpy(result + patsz + 6, "\n"); /* add \n and 0 terminator */

	send_query(1, result);
	sync_to_from(1);

	/* Change set,f, to [se]f,1, */
	result[2] = register_name;
	char constpartcmp[] = ",1,";
	memcpy(result + 3, constpartcmp, sizeof(constpartcmp) - 1);
	/* For comparison change \n to , */
	result[patsz + 6] = ',';

	/* +2 to skip `se` */
	check_register_contents(0, register_name, result + 2);
}

TEST(chg_double_allocation_to_max, IF(not_wine))
{
	char result[16384 + 8];
	test_pat(result, 16384, 'g', 512, '6', 'g');
	check_is_initial(0, TEST_REGISTERS_MINUS_DEFG);
}

TEST(chg_halve_allocation, IF(not_wine))
{
	send_query(0, "set,g,G\n");
	sync_to_from(0);

	check_register_contents(1, 'g', TEST_EXPECT_FOR_G);
	check_is_initial(1, TEST_REGISTERS_MINUS_DEFG);
}

TEST(handover, IF(not_wine))
{
	/* Open third instance. */
	spawn_regcmd(2);
	send_query(2, "sync_enable,test-shmem\n");
	receive_ack(2);

	/* Close previous instances. */
	sync_disable(0);
	sync_disable(1);

	sync_from(2);

	check_is_initial(2, TEST_REGISTERS_MINUS_DEFG);
	check_register_contents(2, 'd', TEST_EXPECT_FOR_D);
	check_register_contents(2, 'e', TEST_EXPECT_FOR_E);
	check_register_contents(2, 'f', pat4kib + 2);
	check_register_contents(2, 'g', TEST_EXPECT_FOR_G);
}

static void
sync_disable(int instance)
{
	send_query(instance, "sync_disable\n");
	receive_ack(instance);
	fclose(instance_stdin[instance]);
	fclose(instance_stdout[instance]);
}

TEST(teardown_once, IF(not_wine))
{
	sync_disable(2);
}

#ifndef _WIN32

static pid_t
popen2(const char cmd[], FILE **in, FILE **out)
{
	pid_t pid;
	int in_pipe[2];
	int out_pipe[2];

	if(pipe(out_pipe) != 0)
	{
		return (pid_t)-1;
	}

	if(pipe(in_pipe) != 0)
	{
		close(out_pipe[0]);
		close(out_pipe[1]);
		return (pid_t)-1;
	}

	if((pid = fork()) == -1)
	{
		close(in_pipe[0]);
		close(in_pipe[1]);
		close(out_pipe[0]);
		close(out_pipe[1]);
		return (pid_t)-1;
	}

	if(pid == 0)
	{
		close(in_pipe[1]);
		close(out_pipe[0]);
		if(dup2(in_pipe[0], STDIN_FILENO) == -1)
		{
			_Exit(EXIT_FAILURE);
		}
		if(dup2(out_pipe[1], STDOUT_FILENO) == -1)
		{
			_Exit(EXIT_FAILURE);
		}
		if(dup2(out_pipe[1], STDERR_FILENO) == -1)
		{
			_Exit(EXIT_FAILURE);
		}

		close(in_pipe[0]);
		close(out_pipe[1]);

		execvp("/bin/sh", make_execv_array("/bin/sh", "-c", (char *)cmd));
		_Exit(127);
	}

	close(in_pipe[0]);
	close(out_pipe[1]);
	*in = fdopen(in_pipe[1], "w");
	if(*in == NULL)
	{
		close(in_pipe[1]);
		close(out_pipe[0]);
		kill(pid, SIGTERM);
		return (pid_t)-1;
	}
	*out = fdopen(out_pipe[0], "r");
	if(*out == NULL)
	{
		fclose(*in);
		close(out_pipe[0]);
		kill(pid, SIGTERM);
		return (pid_t)-1;
	}

	return pid;
}

#else

/* Runs command in background and redirects all its streams to file streams
 * which are set.  Returns (pid_t)0 or (pid_t)-1 on error. */
static pid_t
popen2_int(const char cmd[], FILE **in, FILE **out,
		int in_pipe[2], int out_pipe[2])
{
	const char *args[4];
	int code;

	if(_dup2(in_pipe[0], _fileno(stdin)) != 0)
		return (pid_t)-1;
	if(_dup2(out_pipe[1], _fileno(stdout)) != 0)
		return (pid_t)-1;
	if(_dup2(out_pipe[1], _fileno(stderr)) != 0)
		return (pid_t)-1;

	args[0] = "cmd";
	args[1] = "/C";
	args[2] = cmd;
	args[3] = NULL;

	code = _spawnvp(P_NOWAIT, "cmd", args);

	if(code == 0)
	{
		return (pid_t)-1;
	}

	if((*in = _fdopen(in_pipe[1], "w")) == NULL)
		return (pid_t)-1;
	if((*out = _fdopen(out_pipe[0], "r")) == NULL)
	{
		fclose(*in);
		return (pid_t)-1;
	}

	return 0;
}

static pid_t
popen2(const char cmd[], FILE **in, FILE **out)
{
	int in_fd, in_pipe[2];
	int out_fd, out_pipe[2];
	int err_fd;
	pid_t pid;

	if(_pipe(in_pipe, 32*1024, O_NOINHERIT) != 0)
	{
		return (pid_t)-1;
	}

	if(_pipe(out_pipe, 32*1024, O_NOINHERIT) != 0)
	{
		close(in_pipe[0]);
		close(in_pipe[1]);
		return (pid_t)-1;
	}

	in_fd = dup(_fileno(stdin));
	out_fd = dup(_fileno(stdout));
	err_fd = dup(_fileno(stderr));

	pid = popen2_int(cmd, in, out, in_pipe, out_pipe);

	_close(in_pipe[0]);
	_close(out_pipe[1]);

	_dup2(in_fd, _fileno(stdin));
	_dup2(out_fd, _fileno(stdout));
	_dup2(err_fd, _fileno(stderr));

	_close(in_fd);
	_close(out_fd);
	_close(err_fd);

	if(pid == (pid_t)-1)
	{
		_close(in_pipe[1]);
		_close(out_pipe[0]);
	}

	return pid;
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
