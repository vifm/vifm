#include "seatest.h"

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "builtin_keys.h"

int mode = NORMAL_MODE;

void builtin_and_custom(void);
void diff_motions(void);
void discard_not_full_cmds(void);
void dont_exec_motions_only(void);
void remap_builtin(void);
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
void pick_longest(void);
void unmap_tests(void);
void noremap_tests(void);
void num_in_the_middle_tests(void);
void selectors_tests(void);

void
all_tests(void)
{
	builtin_and_custom();
	diff_motions();
	discard_not_full_cmds();
	dont_exec_motions_only();
	remap_builtin();
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
	def_keys_and_user_mappings();
	pick_longest();
	unmap_tests();
	noremap_tests();
	num_in_the_middle_tests();
	selectors_tests();
}

void
my_suite_setup(void)
{
	static int mode_flags[] = {
		MF_USES_REGS | MF_USES_COUNT,
		MF_USES_INPUT,
		MF_USES_COUNT
	};

	init_keys(MODES_COUNT, &mode, mode_flags);
	init_builtin_keys(&mode);
}

void
my_suite_teardown(void)
{
	clear_keys();
}

int
main(void)
{
	suite_setup(my_suite_setup);
	suite_teardown(my_suite_teardown);

	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 : */
