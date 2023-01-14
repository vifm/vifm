#include <stic.h>

#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/modes/modes.h"

#include "builtin_keys.h"

static void silence(int more);

DEFINE_SUITE();

SETUP()
{
	static int mode_flags[MODES_COUNT] = {
		MF_USES_REGS | MF_USES_COUNT, /* NORMAL_MODE */
		MF_USES_INPUT,                /* CMDLINE_MODE */
		MF_USES_INPUT,                /* NAV_MODE */
		MF_USES_COUNT                 /* VISUAL_MODE */
	};

	vle_keys_init(MODES_COUNT, mode_flags, &silence);
	vle_mode_set(NORMAL_MODE, VMT_PRIMARY);
	init_builtin_keys();
}

static void
silence(int more)
{
	/* Do nothing. */
}

TEARDOWN()
{
	vle_keys_reset();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0: */
/* vim: set cinoptions+=t0 filetype=c : */
