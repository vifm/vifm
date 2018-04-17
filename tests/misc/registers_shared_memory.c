#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>     /* SIGTERM */
#include <unistd.h>
#include <stic.h>

#include "../../src/utils/gmux.h"
#include "../../src/utils/shmem.h"

static void spawn_regcmd(size_t number);
static void send_query(size_t instance, char* query);
static void receive_ack(size_t instance);
static void check_is_initial(size_t instance, char* reglist);
static void receive_answer(size_t instance, char* lnbuf);
static int read_safely(int fd, char* buf, size_t sz);
static void sync_to_from(size_t instance);
static void sync_from(size_t instance);
static void check_register_contents(size_t instance, char register_name,
	char* expected_content);
static void test_pat(char* result, size_t patsz, char register_name,
	size_t pat_id_every, char p0, char p1);
static void sync_disable(size_t instance);

#define LINE_SIZE 32768
#define NUM_INSTANCES 3

#define TEST_REGISTERS            "abcdefghijklmnopqrstuvwxyz"
#define TEST_REGISTERS_MINUS_D    "abcefghijklmnopqrstuvwxyz"
#define TEST_REGISTERS_MINUS_DE   "abcfghijklmnopqrstuvwxyz"
#define TEST_REGISTERS_MINUS_DEF  "abcghijklmnopqrstuvwxyz"
#define TEST_REGISTERS_MINUS_DEFG "abchijklmnopqrstuvwxyz"

#define TEST_EXPECT_FOR_D "d,3,newd,nd1,nd2,"
#define TEST_EXPECT_FOR_E "e,4,longerthanbeforee,le1,le2,le3,"
#define TEST_EXPECT_FOR_G "g,1,G,"

static int pid_instance[NUM_INSTANCES];
static int fd_instance_write_to[NUM_INSTANCES];
static int fd_instance_receive_from[NUM_INSTANCES];

static char pat4kib[4096 + 8];

/* All of the tests need to run in exactly the order given for correct
 * results. */

/* Tests basic transfer of values from 0 to 1 and spawns processes. */
SETUP_ONCE()
{
	/* Make sure nothing retained from previous tests. */
	gmux_destroy(gmux_create("regs-test-shmem"));
	shmem_destroy(shmem_create("regs-test-shmem", 10, 10));

	/* spawn processes */
	spawn_regcmd(0);
	spawn_regcmd(1);

	/* initial register values */
	char* curletr = TEST_REGISTERS;
	for(; *curletr != 0; curletr++) {
		dprintf(fd_instance_write_to[0],
			"set,%c,initial%c,i%c1,i%c2,i%c3\n",
			*curletr, *curletr, *curletr, *curletr, *curletr);
		dprintf(fd_instance_write_to[1],
			"set,%c,lossval%c,l%c1,l%c2,l%c3\n",
			*curletr, *curletr, *curletr, *curletr, *curletr);
	}

	/* enable shared memory synchronization */
	send_query(0, "sync_enable,test-shmem\n");
	receive_ack(0);
	send_query(1, "sync_enable,test-shmem\n");
	receive_ack(1);
}

/* https://stackoverflow.com/questions/6171552 */
static void spawn_regcmd(size_t number)
{
	int inpipefd[2];
	int outpipefd[2];
	if(pipe(inpipefd) || pipe(outpipefd)) {
		perror("Failed to establish pipe file descriptors");
		exit(1);
	}

	int pid = fork();
	switch(pid) {
	case -1:
		perror("Failed to call fork");
		exit(1);
		return;
	case 0:
		/* in child */
		dup2(outpipefd[0], STDIN_FILENO);
		dup2(inpipefd[1], STDOUT_FILENO);
		dup2(inpipefd[1], STDERR_FILENO);
		close(outpipefd[0]);
		close(outpipefd[1]);
		close(inpipefd[0]);
		close(inpipefd[1]);
		if(getenv("DEBUG") == NULL)
			execl("./bin/regs_shmem_app", "regs_shmem_app", (char*)0);
		else
			execl("./bin/debug/regs_shmem_app", "regs_shmem_app", (char*)0);
		perror("Exec failed");
		exit(1);
		return;
	default:
		/* in parent process */
		pid_instance[number] = pid;
		/* close unused file descriptors */
		close(outpipefd[0]);
		close(inpipefd[1]);
		/* save useful file descriptors */
		fd_instance_write_to[number]     = outpipefd[1];
		fd_instance_receive_from[number] = inpipefd[0];
		return;
	}
}

static void send_query(size_t instance, char* query)
{
	if(write(fd_instance_write_to[instance], query, strlen(query)) == -1) {
		fprintf(stderr, "Failed to write query to FD %d "
			"(instance %d): %s; query was %s",
			fd_instance_write_to[instance],
			(int)instance, strerror(errno), query);
		exit(1);
	}
}

static void receive_ack(size_t instance)
{
	char bakval;
	char lnbuf[LINE_SIZE];
	receive_answer(instance, lnbuf);
	bakval = lnbuf[3];
	lnbuf[3] = 0; /* cut off ,... after ack */
	if(strcmp(lnbuf, "ack") != 0) {
		lnbuf[3] = bakval;
		fprintf(stderr, "Error: Did not receive ACK but the following "
			"response from instance %d: %s\n",
			(int)instance, lnbuf);
		exit(1);
	}
}

static void check_is_initial(size_t instance, char* reglist)
{
	char cmp[26];
	char* curletr = reglist;
	for(; *curletr != 0; curletr++) {
		snprintf(cmp, sizeof(cmp), "%c,4,initial%c,i%c1,i%c2,i%c3,",
			*curletr, *curletr, *curletr, *curletr, *curletr);
		check_register_contents(instance, *curletr, cmp);
	}
}

static void receive_answer(size_t instance, char* lnbuf)
{
	int fd = fd_instance_receive_from[instance];
	int num = 0;

	/* Read until answer ends which is when we encounter EOL */
	do {
		num += read_safely(fd, lnbuf + num, LINE_SIZE - num);
	} while(lnbuf[num - 1] != '\n');

	lnbuf[num - 1] = 0; /* terminate string by replacing \n with 0 */
}

static int read_safely(int fd, char* buf, size_t sz)
{
	int rv = read(fd, buf, sz);
	switch(rv) {
	case -1:
		perror("Error reading from child");
		exit(1);
	case 0:
		printf("Child unexpectedly sent eof (fd %d)\n", fd);
		exit(1);
	default:
		return rv;
	}
}

TEST(make_sure_instance_0_does_not_lose_data)
{
	sync_from(0);
	check_is_initial(0, TEST_REGISTERS);
}

static void sync_to_from(size_t instance)
{
	send_query(instance, "sync_to\n");
	receive_ack(instance);

	sync_from(!instance);
}

static void sync_from(size_t instance)
{
	send_query(instance, "sync_from\n");
	receive_ack(instance);
}

TEST(make_sure_sync_to_instance_1_works)
{
	sync_from(1);
	check_is_initial(1, TEST_REGISTERS);
}

TEST(chg_update_in_place)
{
	send_query(1, "set,d,newd,nd1,nd2\n");
	sync_to_from(1);

	check_register_contents(0, 'd', TEST_EXPECT_FOR_D);

	/* check others */
	check_is_initial(0, TEST_REGISTERS_MINUS_D);
}

static void check_register_contents(size_t instance, char register_name,
		char* expected_content)
{
	char lnbuf[LINE_SIZE];
	char query[7];
	strcpy(query, "get,X\n");
	query[4] = register_name;

	send_query(instance, query);
	receive_answer(instance, lnbuf);
	assert_string_equal(expected_content, lnbuf);
}

TEST(chg_append_to_end)
{
	send_query(1, "set,e,longerthanbeforee,le1,le2,le3\n");
	sync_to_from(1);

	/* ignore D */

	/* check E */
	check_register_contents(0, 'e', TEST_EXPECT_FOR_E);

	/* check others */
	check_is_initial(0, TEST_REGISTERS_MINUS_DE);
}

TEST(chg_double_allocation_size_till_fit)
{
	test_pat(pat4kib, 4096, 'f', 128, '4', 'f');
	/* check initial */
	check_is_initial(0, TEST_REGISTERS_MINUS_DEF);
}

/* result needs to have patsz + 8 entries (set,X, + \n + 0-byte) */
static void test_pat(char* result, size_t patsz, char register_name,
	size_t pat_id_every, char p0, char p1)
{
	size_t i;

	/* prepare pattern */
	char pre[] = "set,X,";
	strcpy(result, pre);
	result[4] = register_name;
	for(i = 0; i < patsz; ++i)
		result[sizeof(pre) - 1 + i] = ((i % pat_id_every) == 0)? p0: p1;
	strcpy(result + patsz + 6, "\n"); /* add \n and 0 terminator */

	send_query(1, result);
	sync_to_from(1);

	/* change set,f, to [se]f,1, */
	result[2] = register_name;
	char constpartcmp[] = ",1,";
	memcpy(result + 3, constpartcmp, sizeof(constpartcmp) - 1);
	/* for comparison change \n to , */
	result[patsz + 6] = ',';

	/* +2 to skip `se` */
	check_register_contents(0, register_name, result + 2);
}

TEST(chg_double_allocation_to_max)
{
	char result[16384 + 8];
	test_pat(result, 16384, 'g', 512, '6', 'g');
	check_is_initial(0, TEST_REGISTERS_MINUS_DEFG);
}

TEST(chg_halve_allocation)
{
	send_query(0, "set,g,G\n");
	sync_to_from(0);

	check_register_contents(1, 'g', TEST_EXPECT_FOR_G);
	check_is_initial(1, TEST_REGISTERS_MINUS_DEFG);
}

TEST(handover)
{
	/* open third instance */
	spawn_regcmd(2);
	send_query(2, "sync_enable,test-shmem\n");
	receive_ack(2);

	/* close previous instances */
	sync_disable(0);
	sync_disable(1),

	sync_from(2);

	check_is_initial(2, TEST_REGISTERS_MINUS_DEFG);
	check_register_contents(2, 'd', TEST_EXPECT_FOR_D);
	check_register_contents(2, 'e', TEST_EXPECT_FOR_E);
	check_register_contents(2, 'f', pat4kib + 2);
	check_register_contents(2, 'g', TEST_EXPECT_FOR_G);
}

static void sync_disable(size_t instance)
{
	send_query(instance, "sync_disable\n");
	receive_ack(instance);
	close(fd_instance_write_to[instance]);
	close(fd_instance_receive_from[instance]);
}

TEST(teardown_once)
{
	sync_disable(2);
}
