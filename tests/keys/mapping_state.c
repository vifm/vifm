#include "seatest.h"

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"

extern int mode;
extern int key_is_mapped;
extern int is_in_maping_state;

static int mapping_states;

static int
handler(wchar_t key)
{
	mapping_states += is_inside_mapping() != 0;
	return 0;
}

static void
add_custom_keys(void)
{
	set_def_handler(CMDLINE_MODE, &handler);
	add_user_keys(L"s", L":shell", NORMAL_MODE, 0);
}

static void
teardown(void)
{
	set_def_handler(CMDLINE_MODE, NULL);
	mode = NORMAL_MODE;
}

static void
test_none_mapping_as_initial_state(void)
{
	assert_false(is_inside_mapping());
}

static void
test_none_mapping_after_calling_a_mapping(void)
{
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"s")));
	assert_false(is_inside_mapping());
}

static void
test_none_mapping_in_defhandler(void)
{
	mapping_states = 0;
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"s")));
	assert_int_equal(0, mapping_states);
}

static void
test_mapping_inside_mapping(void)
{
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"s")));
	assert_true(key_is_mapped);
	assert_true(is_in_maping_state);
}

static void
test_mapping_outside_mapping(void)
{
	assert_false(IS_KEYS_RET_CODE(execute_keys(L"ma")));
	assert_false(key_is_mapped);
	assert_false(is_in_maping_state);
}

void
mapping_state_tests(void)
{
	test_fixture_start();

	fixture_setup(add_custom_keys);
	fixture_teardown(teardown);

	run_test(test_none_mapping_as_initial_state);
	run_test(test_none_mapping_after_calling_a_mapping);
	run_test(test_none_mapping_in_defhandler);
	run_test(test_mapping_inside_mapping);
	run_test(test_mapping_outside_mapping);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
