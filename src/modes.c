#include <stdlib.h>

#include "keys_eng.h"

#include "modes.h"

static int mode = NORMAL_MODE;

static int mode_flags[] = {
    MF_USES_REGS | MF_USES_COUNT,
    0,
    MF_USES_COUNT
};

void
init_modes(void)
{
	init_keys(MODES_COUNT, &mode, (int*)&mode_flags, NULL);
	init_buildin_n_keys(&mode);
	init_buildin_v_keys(&mode);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
