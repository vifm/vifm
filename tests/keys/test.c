#include "seatest.h"

#include "buildin_keys.h"
#include "../../src/keys.h"
#include "../../src/modes.h"

void buildin_and_custom(void);
void diff_motions(void);
void discard_not_full_cmds(void);
void dont_exec_motions_only(void);
void dont_remap_buildin(void);
void long_motions_wait(void);
void longest(void);
void motions(void);
void multi_are_not_motions(void);
void multi_keys(void);
void no_regs_long_key(void);
void put_wait_points(void);
void remap_users(void);
void same_multi_and_motion(void);
void users_key_to_key(void);

void all_tests(void)
{
	buildin_and_custom();
	diff_motions();
	discard_not_full_cmds();
	dont_exec_motions_only();
	dont_remap_buildin();
	long_motions_wait();
	longest();
	motions();
	multi_are_not_motions();
	multi_keys();
	no_regs_long_key();
	put_wait_points();
	remap_users();
	same_multi_and_motion();
	users_key_to_key();
}

void my_suite_setup(void)
{
	static int mode = NORMAL_MODE;
	static int mode_flags[]= {
		MF_USES_REGS | MF_USES_COUNT,
		0,
		MF_USES_COUNT
	};

	init_keys(MODES_COUNT, &mode, mode_flags);
	init_buildin_keys(&mode);
}

void my_suite_teardown(void)
{
	clear_keys();
}

int main(int argc, char **argv)
{
	suite_setup(my_suite_setup);
	suite_teardown(my_suite_teardown);

	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
