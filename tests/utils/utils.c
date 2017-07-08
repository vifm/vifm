#include "utils.h"

int
windows(void)
{
#ifdef _WIN32
	return 1;
#else
	return 0;
#endif
}

int
not_windows(void)
{
	return !windows();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
