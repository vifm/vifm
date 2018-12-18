#include "utils.h"

#include <locale.h> /* LC_ALL setlocale() */

#include "../../src/utils/utils.h"

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

void
try_enable_utf8_locale(void)
{
	(void)setlocale(LC_ALL, "");
	if(!utf8_locale())
	{
		(void)setlocale(LC_ALL, "en_US.utf8");
	}
}

int
utf8_locale(void)
{
	return (vifm_wcwidth(L'丝') == 2);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
