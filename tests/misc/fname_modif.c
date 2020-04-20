#include <stic.h>

#include <unistd.h> /* chdir() */

#include <stdlib.h>
#include <string.h>

#include "../../src/cfg/config.h"
#include "../../src/ui/ui.h"
#include "../../src/utils/dynarray.h"
#include "../../src/filelist.h"
#include "../../src/macros.h"
#include "../../src/registers.h"
#include "../../src/status.h"

#ifdef _WIN32
#define SL "\\\\"
#else
#define SL "/"
#endif

static void
setup_lwin(void)
{
	strcpy(lwin.curr_dir, "/lwin");

	lwin.list_rows = 5;
	lwin.list_pos = 2;
	lwin.dir_entry = dynarray_cextend(NULL,
			lwin.list_rows*sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("lfile0");
	lwin.dir_entry[0].origin = &lwin.curr_dir[0];
	lwin.dir_entry[1].name = strdup("lfile1");
	lwin.dir_entry[1].origin = &lwin.curr_dir[0];
	lwin.dir_entry[2].name = strdup("lfile2");
	lwin.dir_entry[2].origin = &lwin.curr_dir[0];
	lwin.dir_entry[3].name = strdup("lfile3");
	lwin.dir_entry[3].origin = &lwin.curr_dir[0];
	lwin.dir_entry[4].name = strdup(".lfile4");
	lwin.dir_entry[4].origin = &lwin.curr_dir[0];

	lwin.dir_entry[0].selected = 1;
	lwin.dir_entry[2].selected = 1;
	lwin.selected_files = 2;
}

static void
setup_rwin(void)
{
	strcpy(rwin.curr_dir, "/rwin");

	rwin.list_rows = 7;
	rwin.list_pos = 5;
	rwin.dir_entry = dynarray_cextend(NULL,
			rwin.list_rows*sizeof(*rwin.dir_entry));
	rwin.dir_entry[0].name = strdup("rfile0");
	rwin.dir_entry[0].origin = &rwin.curr_dir[0];
	rwin.dir_entry[1].name = strdup("rfile1");
	rwin.dir_entry[1].origin = &rwin.curr_dir[0];
	rwin.dir_entry[2].name = strdup("rfile2");
	rwin.dir_entry[2].origin = &rwin.curr_dir[0];
	rwin.dir_entry[3].name = strdup("rfile3");
	rwin.dir_entry[3].origin = &rwin.curr_dir[0];
	rwin.dir_entry[4].name = strdup("rfile4.tar.gz");
	rwin.dir_entry[4].origin = &rwin.curr_dir[0];
	rwin.dir_entry[5].name = strdup("rfile5");
	rwin.dir_entry[5].origin = &rwin.curr_dir[0];
	rwin.dir_entry[6].name = strdup("rdir6");
	rwin.dir_entry[6].origin = &rwin.curr_dir[0];

	rwin.dir_entry[1].selected = 1;
	rwin.dir_entry[3].selected = 1;
	rwin.dir_entry[5].selected = 1;
	rwin.selected_files = 3;
}

static void
setup_registers(void)
{
	regs_init();
	regs_append('z', "existing-files/a");
	regs_append('z', "existing-files/b");
	regs_append('z', "existing-files/c");
}

SETUP_ONCE()
{
	stats_update_shell_type("/bin/sh");
}

SETUP()
{
	assert_success(chdir(TEST_DATA_PATH));

	setup_lwin();
	setup_rwin();

	curr_view = &lwin;
	other_view = &rwin;

	strcpy(cfg.home_dir, "/rwin/");

	setup_registers();
}

TEARDOWN()
{
	int i;

	for(i = 0; i < lwin.list_rows; i++)
		free(lwin.dir_entry[i].name);
	dynarray_free(lwin.dir_entry);

	for(i = 0; i < rwin.list_rows; i++)
		free(rwin.dir_entry[i].name);
	dynarray_free(rwin.dir_entry);

	regs_reset();
}

TEST(colon_p)
{
	MacroFlags flags;
	char *expanded;

	expanded = ma_expand(" cp %f:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin" SL "lfile0 " SL "lwin" SL "lfile2 ",
			expanded);
	free(expanded);

	expanded = ma_expand(" cp %F:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5 ",
	                    expanded);
	free(expanded);

	expanded = ma_expand(" cp %b:p ", "", &flags, 1);
	assert_string_equal(
			" cp " SL "lwin" SL "lfile0 " SL "lwin" SL "lfile2 " SL "rwin" SL"rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = ma_expand(" cp %c:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin" SL "lfile2 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %C:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "rwin" SL "rfile5 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %d:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %D:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "rwin ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %rz:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin" SL "existing-files" SL "a " SL "lwin" SL "existing-files" SL "b " SL "lwin" SL "existing-files" SL "c ", expanded);
	free(expanded);
}

TEST(colon_p_in_root)
{
	MacroFlags flags;
	char *expanded;

	strcpy(lwin.curr_dir, "/");

	expanded = ma_expand(" cp %f:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "lfile0 " SL "lfile2 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %F:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5 ",
	                    expanded);
	free(expanded);

	expanded = ma_expand(" cp %b:p ", "", &flags, 1);
	assert_string_equal(
			" cp " SL "lfile0 " SL "lfile2 " SL "rwin" SL"rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = ma_expand(" cp %c:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "lfile2 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %C:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "rwin" SL "rfile5 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %d:p ", "", &flags, 1);
	assert_string_equal(" cp " SL " ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %D:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "rwin ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %rz:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "existing-files" SL "a " SL "existing-files" SL "b " SL "existing-files" SL "c ", expanded);
	free(expanded);
}

TEST(colon_tilde)
{
	MacroFlags flags;
	char *expanded;

	expanded = ma_expand(" cp %f:~ ", "", &flags, 1);
	assert_string_equal(" cp lfile0 lfile2 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %f:p:~ ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin" SL "lfile0 " SL "lwin" SL "lfile2 ",
			expanded);
	free(expanded);

	expanded = ma_expand(" cp %F:~ ", "", &flags, 1);
	assert_string_equal(" cp \\~" SL "rfile1 \\~" SL "rfile3 \\~" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = ma_expand(" cp %b:p:~ ", "", &flags, 1);
	assert_string_equal(
			" cp " SL "lwin" SL "lfile0 " SL "lwin" SL "lfile2 \\~" SL "rfile1 \\~" SL "rfile3 \\~" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = ma_expand(" cp %b:~ ", "", &flags, 1);
	assert_string_equal(" cp lfile0 lfile2 \\~" SL "rfile1 \\~" SL "rfile3 \\~" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = ma_expand(" cp %c:~ ", "", &flags, 1);
	assert_string_equal(" cp lfile2 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %c:p:~ ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin" SL "lfile2 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %C:~ ", "", &flags, 1);
	assert_string_equal(" cp \\~" SL "rfile5 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %d:~ ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %D:~ ", "", &flags, 1);
	assert_string_equal(" cp \\~ ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %rz:~ ", "", &flags, 1);
	assert_string_equal(" cp existing-files" SL "a existing-files" SL "b existing-files" SL "c ", expanded);
	free(expanded);
}

TEST(colon_dot)
{
	MacroFlags flags;
	char *expanded;

	expanded = ma_expand(" cp %f:. ", "", &flags, 1);
	assert_string_equal(" cp lfile0 lfile2 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %f:p:. ", "", &flags, 1);
	assert_string_equal(" cp lfile0 lfile2 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %F:~:. ", "", &flags, 1);
	assert_string_equal(" cp \\~" SL "rfile1 \\~" SL "rfile3 \\~" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = ma_expand(" cp %F:. ", "", &flags, 1);
	assert_string_equal(" cp " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = ma_expand(" cp %b:. ", "", &flags, 1);
	assert_string_equal(
			" cp lfile0 lfile2 " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = ma_expand(" cp %b:p:. ", "", &flags, 1);
	assert_string_equal(
			" cp lfile0 lfile2 " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = ma_expand(" cp %c:. ", "", &flags, 1);
	assert_string_equal(" cp lfile2 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %c:p:. ", "", &flags, 1);
	assert_string_equal(" cp lfile2 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %C:. ", "", &flags, 1);
	assert_string_equal(" cp " SL "rwin" SL "rfile5 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %d:. ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %D:. ", "", &flags, 1);
	assert_string_equal(" cp " SL "rwin ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %rz:. ", "", &flags, 1);
	assert_string_equal(" cp existing-files" SL "a existing-files" SL "b existing-files" SL "c ", expanded);
	free(expanded);
}

TEST(colon_h)
{
	MacroFlags flags;
	char *expanded;

	expanded = ma_expand(" cp %f:h ", "", &flags, 1);
	assert_string_equal(" cp . . ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %f:p:h ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin " SL "lwin ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %F:~:h ", "", &flags, 1);
	assert_string_equal(" cp \\~ \\~ \\~ ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %F:h ", "", &flags, 1);
	assert_string_equal(" cp " SL "rwin " SL "rwin " SL "rwin ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %b:h ", "", &flags, 1);
	assert_string_equal(" cp . . " SL "rwin " SL "rwin " SL "rwin ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %b:p:h ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin " SL "lwin " SL "rwin " SL "rwin " SL "rwin ",
			expanded);
	free(expanded);

	expanded = ma_expand(" cp %c:p:h ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %C:h ", "", &flags, 1);
	assert_string_equal(" cp " SL "rwin ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %d:h ", "", &flags, 1);
	assert_string_equal(" cp " SL " ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %D:h:h ", "", &flags, 1);
	assert_string_equal(" cp " SL " ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %rz:h ", "", &flags, 1);
	assert_string_equal(" cp existing-files existing-files existing-files ", expanded);
	free(expanded);
}

TEST(colon_t)
{
	MacroFlags flags;
	char *expanded;

	expanded = ma_expand(" cp %f:t ", "", &flags, 1);
	assert_string_equal(" cp lfile0 lfile2 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %f:p:t ", "", &flags, 1);
	assert_string_equal(" cp lfile0 lfile2 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %F:t ", "", &flags, 1);
	assert_string_equal(" cp rfile1 rfile3 rfile5 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %b:t ", "", &flags, 1);
	assert_string_equal(" cp lfile0 lfile2 rfile1 rfile3 rfile5 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %b:p:t ", "", &flags, 1);
	assert_string_equal(" cp lfile0 lfile2 rfile1 rfile3 rfile5 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %c:t ", "", &flags, 1);
	assert_string_equal(" cp lfile2 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %c:p:t ", "", &flags, 1);
	assert_string_equal(" cp lfile2 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %C:t ", "", &flags, 1);
	assert_string_equal(" cp rfile5 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %d:t ", "", &flags, 1);
	assert_string_equal(" cp lwin ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %D:t:t ", "", &flags, 1);
	assert_string_equal(" cp rwin ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %rz:t ", "", &flags, 1);
	assert_string_equal(" cp a b c ", expanded);
	free(expanded);
}

TEST(colon_r)
{
	MacroFlags flags;
	char *expanded;

	strcpy(rwin.curr_dir, "/rw.in");
	rwin.list_pos = 4;

	expanded = ma_expand(" cp %c:r ", "", &flags, 1);
	assert_string_equal(" cp lfile2 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %c:r:r ", "", &flags, 1);
	assert_string_equal(" cp lfile2 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %c:p:r ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin" SL "lfile2 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %C:r ", "", &flags, 1);
	assert_string_equal(" cp " SL "rw.in" SL "rfile4.tar ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %C:r:r ", "", &flags, 1);
	assert_string_equal(" cp " SL "rw.in" SL "rfile4 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %C:r:r:r ", "", &flags, 1);
	assert_string_equal(" cp " SL "rw.in" SL "rfile4 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %d:r ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %D:r ", "", &flags, 1);
	assert_string_equal(" cp " SL "rw ", expanded);
	free(expanded);

	lwin.list_pos = 4;

	expanded = ma_expand(" cp %c:r ", "", &flags, 1);
	assert_string_equal(" cp .lfile4 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %c:p:r ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin" SL ".lfile4 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %rz:t:r ", "", &flags, 1);
	assert_string_equal(" cp a b c ", expanded);
	free(expanded);
}

TEST(colon_e)
{
	MacroFlags flags;
	char *expanded;

	strcpy(rwin.curr_dir, "" SL "rw.in");
	rwin.list_pos = 4;

	expanded = ma_expand(" cp %c:e ", "", &flags, 1);
	assert_string_equal(" cp  ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %c:e:e ", "", &flags, 1);
	assert_string_equal(" cp  ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %c:p:e ", "", &flags, 1);
	assert_string_equal(" cp  ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %C:e ", "", &flags, 1);
	assert_string_equal(" cp gz ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %C:e:e ", "", &flags, 1);
	assert_string_equal(" cp  ", expanded);
	free(expanded);

	rwin.list_pos = 2;

	expanded = ma_expand(" cp %C:e ", "", &flags, 1);
	assert_string_equal(" cp  ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %d:e ", "", &flags, 1);
	assert_string_equal(" cp  ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %D:e ", "", &flags, 1);
	assert_string_equal(" cp in ", expanded);
	free(expanded);

	lwin.list_pos = 4;

	expanded = ma_expand(" cp %c:e ", "", &flags, 1);
	assert_string_equal(" cp  ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %c:p:e ", "", &flags, 1);
	assert_string_equal(" cp  ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %rz:t:. ", "", &flags, 1);
	assert_string_equal(" cp a b c ", expanded);
	free(expanded);
}

TEST(colon_s)
{
	MacroFlags flags;
	char *expanded;

	expanded = ma_expand(" cp %f:s?l?r ", "", &flags, 1);
	assert_string_equal(" cp lfile0 lfile2", expanded);
	free(expanded);

	expanded = ma_expand(" cp %f:s?l?r? ", "", &flags, 1);
	assert_string_equal(" cp rfile0 rfile2 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %f:salara ", "", &flags, 1);
	assert_string_equal(" cp rfile0 rfile2 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %f:s?l?r?k? ", "", &flags, 1);
	assert_string_equal(" cp rfile0 rfile2k? ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %f:s?l?r?:s!f!k!k? ", "", &flags, 1);
	assert_string_equal(" cp rkile0 rkile2k? ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %rz:t:s?a?y? ", "", &flags, 1);
	assert_string_equal(" cp y b c ", expanded);
	free(expanded);
}

TEST(colon_gs)
{
	MacroFlags flags;
	char *expanded;

	expanded = ma_expand(" cp %f:gs?l?r ", "", &flags, 1);
	assert_string_equal(" cp lfile0 lfile2", expanded);
	free(expanded);

	expanded = ma_expand(" cp %f:gs?l?r? ", "", &flags, 1);
	assert_string_equal(" cp rfire0 rfire2 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %f:gsalara ", "", &flags, 1);
	assert_string_equal(" cp rfire0 rfire2 ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %f:gs?l?r?k? ", "", &flags, 1);
	assert_string_equal(" cp rfire0 rfire2k? ", expanded);
	free(expanded);

	expanded = ma_expand(" cp %rz:t:gs?b?y? ", "", &flags, 1);
	assert_string_equal(" cp a y c ", expanded);
	free(expanded);

#ifdef _WIN32
	{
		char *old = cfg.shell;
		cfg.shell = strdup("bash");
		expanded = ma_expand(" cp %f:p:gs?\\?/? ", "", &flags, 1);
		assert_string_equal(" cp /lwin/lfile0 /lwin/lfile2 ", expanded);
		free(expanded);
		free(cfg.shell);
		cfg.shell = old;
	}
#endif
}

#ifdef _WIN32

TEST(colon_u)
{
	MacroFlags flags;
	char *expanded;

	expanded = ma_expand("%f:p:u", "", &flags, 1);
	assert_string_starts_with("\\\\", expanded);
	free(expanded);

	strcpy(lwin.curr_dir, "//server/share/directory");

	expanded = ma_expand(" cp %f:p:u ", "", &flags, 1);
	assert_string_equal(" cp " SL SL "server " SL SL "server ", expanded);
	free(expanded);
}

TEST(windows_specific)
{
	MacroFlags flags;
	char *expanded;

	strcpy(lwin.curr_dir, "h:/rwin");

	expanded = ma_expand(" %d:h ", "", &flags, 1);
	assert_string_equal(" h:\\\\ ", expanded);
	free(expanded);

	expanded = ma_expand(" %d:h:h ", "", &flags, 1);
	assert_string_equal(" h:\\\\ ", expanded);
	free(expanded);

	strcpy(lwin.curr_dir, "//ZX-Spectrum");

	expanded = ma_expand(" %d:h ", "", &flags, 1);
	assert_string_equal(" \\\\\\\\ZX-Spectrum ", expanded);
	free(expanded);

	expanded = ma_expand(" %d:h:h ", "", &flags, 1);
	assert_string_equal(" \\\\\\\\ZX-Spectrum ", expanded);
	free(expanded);
}

#endif

TEST(modif_without_macros)
{
	MacroFlags flags;
	char *expanded;

	expanded = ma_expand(" cp %f:t :p :~ :. :h :t :r :e :s :gs ", "", &flags, 0);
	assert_string_equal(" cp lfile0 lfile2 :p :~ :. :h :t :r :e :s :gs ",
			expanded);
	free(expanded);
}

TEST(with_quotes)
{
	MacroFlags flags;

	char *expanded = ma_expand(" cp %\"f:p ", "", &flags, 1);
	assert_string_equal(" cp \"" SL "lwin" SL "lfile0\" \"" SL "lwin" SL "lfile2\" ",
			expanded);
	free(expanded);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
