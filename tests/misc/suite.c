#include <stic.h>

#include <string.h> /* strcpy() */

#include "../../src/ui/color_manager.h"
#include "../../src/ui/tabs.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/fs.h"
#include "../../src/background.h"

static int init_pair_stub(short pair, short f, short b);
static int pair_content_stub(short pair, short *f, short *b);
static int pair_in_use_stub(short int pair);
static void move_pair_stub(short int from, short int to);

static char *saved_cwd;

DEFINE_SUITE();

SETUP_ONCE()
{
#ifdef _WIN32
	extern int _CRT_glob;
	extern void __wgetmainargs(int *, wchar_t ***, wchar_t ***, int, int *);

	wchar_t **envp, **argv;
	int argc, si = 0;
	__wgetmainargs(&argc, &argv, &envp, _CRT_glob, &si);
#endif

	const colmgr_conf_t colmgr_conf = {
		.max_color_pairs = 256,
		.max_colors = 16,
		.init_pair = &init_pair_stub,
		.pair_content = &pair_content_stub,
		.pair_in_use = &pair_in_use_stub,
		.move_pair = &move_pair_stub,
	};
	colmgr_init(&colmgr_conf);

	/* Remember original path in global SETUP_ONCE instead of SETUP to make sure
	 * nothing will change the path before we try to save it. */
	saved_cwd = save_cwd();

	bg_init();

	tabs_init();
}

SETUP()
{
	strcpy(lwin.curr_dir, "/non-existing-dir");
	strcpy(rwin.curr_dir, "/non-existing-dir");
}

TEARDOWN()
{
	restore_cwd(saved_cwd);
	saved_cwd = save_cwd();
}

static int
init_pair_stub(short pair, short f, short b)
{
	return 0;
}

static int
pair_content_stub(short pair, short *f, short *b)
{
	*f = 0;
	*b = 0;
	return 0;
}

static int
pair_in_use_stub(short int pair)
{
	return 0;
}

static void
move_pair_stub(short int from, short int to)
{
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
