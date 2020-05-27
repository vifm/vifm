#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/modes/modes.h"
#include "../../src/modes/wk.h"
#include "../../src/ui/ui.h"
#include "../../src/cmd_core.h"
#include "../../src/status.h"

static void silent_key(key_info_t key_info, keys_info_t *keys_info);
static void silence_ui(int more);

static int silence;

SETUP()
{
	init_modes();
	init_commands();

	curr_view = &lwin;
	other_view = &rwin;
}

TEARDOWN()
{
	vle_cmds_reset();
	vle_keys_reset();
}

TEST(map_commands_count_arguments_correctly)
{
	/* Each map command below should receive two arguments: "\\" and "j". */
	/* Each unmap command below should receive single argument: "\\". */
	assert_success(exec_commands("cmap \\ j", &lwin, CIT_COMMAND));
	assert_success(exec_commands("cnoremap \\ j", &lwin, CIT_COMMAND));
	assert_success(exec_commands("cunmap \\", &lwin, CIT_COMMAND));
	assert_success(exec_commands("dmap \\ j", &lwin, CIT_COMMAND));
	assert_success(exec_commands("dnoremap \\ j", &lwin, CIT_COMMAND));
	assert_success(exec_commands("dunmap \\", &lwin, CIT_COMMAND));
	assert_success(exec_commands("mmap \\ j", &lwin, CIT_COMMAND));
	assert_success(exec_commands("mnoremap \\ j", &lwin, CIT_COMMAND));
	assert_success(exec_commands("munmap \\", &lwin, CIT_COMMAND));
	assert_success(exec_commands("nmap \\ j", &lwin, CIT_COMMAND));
	assert_success(exec_commands("nnoremap \\ j", &lwin, CIT_COMMAND));
	assert_success(exec_commands("nunmap \\", &lwin, CIT_COMMAND));
	assert_success(exec_commands("map \\ j", &lwin, CIT_COMMAND));
	assert_success(exec_commands("noremap \\ j", &lwin, CIT_COMMAND));
	assert_success(exec_commands("unmap \\", &lwin, CIT_COMMAND));
	assert_success(exec_commands("map! \\ j", &lwin, CIT_COMMAND));
	assert_success(exec_commands("noremap! \\ j", &lwin, CIT_COMMAND));
	assert_success(exec_commands("unmap! \\", &lwin, CIT_COMMAND));
	assert_success(exec_commands("qmap \\ j", &lwin, CIT_COMMAND));
	assert_success(exec_commands("qnoremap \\ j", &lwin, CIT_COMMAND));
	assert_success(exec_commands("qunmap \\", &lwin, CIT_COMMAND));
	assert_success(exec_commands("vmap \\ j", &lwin, CIT_COMMAND));
	assert_success(exec_commands("vnoremap \\ j", &lwin, CIT_COMMAND));
	assert_success(exec_commands("vunmap \\", &lwin, CIT_COMMAND));
}

TEST(map_parses_args)
{
	static int mode_flags[] = {
		MF_USES_REGS | MF_USES_COUNT,
		MF_USES_INPUT,
		MF_USES_COUNT
	};

	vle_keys_reset();
	vle_keys_init(MODES_COUNT, mode_flags, &silence_ui);

	keys_add_info_t keys = { WK_x, { {&silent_key} } };
	vle_keys_add(&keys, 1U, NORMAL_MODE);

	/* <silent> */
	assert_int_equal(0, silence);
	assert_success(exec_commands("map a x", &lwin, CIT_COMMAND));
	assert_success(exec_commands("map <silent>b x", &lwin, CIT_COMMAND));
	assert_success(exec_commands("map <silent> c x", &lwin, CIT_COMMAND));
	assert_success(exec_commands("map <silent><silent>d x", &lwin, CIT_COMMAND));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"a")));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"1b")));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"1c")));
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"1d")));
	assert_int_equal(0, silence);

	/* <wait> */
	assert_success(exec_commands("map <wait>xj j", &lwin, CIT_COMMAND));
	assert_int_equal(KEYS_WAIT, vle_keys_exec(L"x"));
}

TEST(dialogs_exit_silent_mode)
{
	const char *cmd = "map <silent> b :cd /no-such-dir<cr>";
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));

	curr_stats.load_stage = -1;
	stats_silence_ui(1);
	assert_false(IS_KEYS_RET_CODE(vle_keys_exec(L"b")));
	assert_false(stats_silenced_ui());
	curr_stats.load_stage = 0;
}

static void
silent_key(key_info_t key_info, keys_info_t *keys_info)
{
	assert_int_equal(key_info.count == NO_COUNT_GIVEN ? 0 : 1, silence);
}

static void
silence_ui(int more)
{
	silence += (more != 0 ? 1 : -1);
	assert_true(silence >= 0);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
