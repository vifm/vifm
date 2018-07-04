#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"

static void key_test(key_info_t key_info, keys_info_t *keys_info);
static void silence_ui(int more);

static int silence;

SETUP()
{
	static int mode_flags[] = {
		MF_USES_REGS | MF_USES_COUNT,
		MF_USES_INPUT,
		MF_USES_COUNT
	};

	vle_keys_reset();
	vle_keys_init(MODES_COUNT, mode_flags, &silence_ui);
}

TEST(removing_user_mapping_from_a_mapping_is_fine)
{
	keys_add_info_t keys = { WK_x, { {&key_test} } };
	vle_keys_add(&keys, 1U, NORMAL_MODE);

	assert_int_equal(0, silence);
	vle_keys_user_add(L"a", L"x", NORMAL_MODE, KEYS_FLAG_SILENT);
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"a")));
	assert_int_equal(0, silence);
}

static void
key_test(key_info_t key_info, keys_info_t *keys_info)
{
	assert_int_equal(1, silence);
}

static void
silence_ui(int more)
{
	silence += (more != 0 ? 1 : -1);
	assert_true(silence >= 0);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
