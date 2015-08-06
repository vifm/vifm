#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/modes/modes.h"

#include "builtin_keys.h"

DEFINE_SUITE();

SETUP()
{
	static int mode_flags[] = {
		MF_USES_REGS | MF_USES_COUNT,
		MF_USES_INPUT,
		MF_USES_COUNT
	};

	init_keys(MODES_COUNT, mode_flags);
	vle_mode_set(NORMAL_MODE, VMT_PRIMARY);
	init_builtin_keys();
}

TEARDOWN()
{
	clear_keys();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
