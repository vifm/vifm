#include <stdlib.h>
#include <string.h>

#include "seatest.h"

#include "../../src/cfg/config.h"
#include "../../src/filelist.h"
#include "../../src/macros.h"
#include "../../src/registers.h"
#include "../../src/ui.h"

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
	lwin.dir_entry = calloc(lwin.list_rows, sizeof(*lwin.dir_entry));
	lwin.dir_entry[0].name = strdup("lfile0");
	lwin.dir_entry[1].name = strdup("lfile1");
	lwin.dir_entry[2].name = strdup("lfile2");
	lwin.dir_entry[3].name = strdup("lfile3");
	lwin.dir_entry[4].name = strdup(".lfile4");

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
	rwin.dir_entry = calloc(rwin.list_rows, sizeof(*rwin.dir_entry));
	rwin.dir_entry[0].name = strdup("rfile0");
	rwin.dir_entry[1].name = strdup("rfile1");
	rwin.dir_entry[2].name = strdup("rfile2");
	rwin.dir_entry[3].name = strdup("rfile3");
	rwin.dir_entry[4].name = strdup("rfile4.tar.gz");
	rwin.dir_entry[5].name = strdup("rfile5");
	rwin.dir_entry[6].name = strdup("rdir6");

	rwin.dir_entry[1].selected = 1;
	rwin.dir_entry[3].selected = 1;
	rwin.dir_entry[5].selected = 1;
	rwin.selected_files = 3;
}

static void
setup_registers(void)
{
	init_registers();
	append_to_register('z', "test-data/existing-files/a");
	append_to_register('z', "test-data/existing-files/b");
	append_to_register('z', "test-data/existing-files/c");
}

static void
setup(void)
{
	setup_lwin();
	setup_rwin();

	curr_view = &lwin;
	other_view = &rwin;

	cfg.max_args = 8192;
	strcpy(cfg.home_dir, "/rwin/");

	setup_registers();
}

static void
teardown(void)
{
	int i;

	for(i = 0; i < lwin.list_rows; i++)
		free(lwin.dir_entry[i].name);
	free(lwin.dir_entry);

	for(i = 0; i < rwin.list_rows; i++)
		free(rwin.dir_entry[i].name);
	free(rwin.dir_entry);

	clear_registers();
}

static void
test_colon_p(void)
{
	MacroFlags flags;
	char *expanded;

	expanded = expand_macros(" cp %f:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin" SL "lfile0 " SL "lwin" SL "lfile2 ",
			expanded);
	free(expanded);

	expanded = expand_macros(" cp %F:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5 ",
	                    expanded);
	free(expanded);

	expanded = expand_macros(" cp %b:p ", "", &flags, 1);
	assert_string_equal(
			" cp " SL "lwin" SL "lfile0 " SL "lwin" SL "lfile2 " SL "rwin" SL"rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = expand_macros(" cp %c:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin" SL "lfile2 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %C:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "rwin" SL "rfile5 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %d:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %D:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "rwin ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %rz:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin" SL "test-data" SL "existing-files" SL "a " SL "lwin" SL "test-data" SL "existing-files" SL "b " SL "lwin" SL "test-data" SL "existing-files" SL "c ", expanded);
	free(expanded);
}

static void
test_colon_p_in_root(void)
{
	MacroFlags flags;
	char *expanded;

	strcpy(lwin.curr_dir, "/");

	expanded = expand_macros(" cp %f:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "lfile0 " SL "lfile2 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %F:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5 ",
	                    expanded);
	free(expanded);

	expanded = expand_macros(" cp %b:p ", "", &flags, 1);
	assert_string_equal(
			" cp " SL "lfile0 " SL "lfile2 " SL "rwin" SL"rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = expand_macros(" cp %c:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "lfile2 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %C:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "rwin" SL "rfile5 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %d:p ", "", &flags, 1);
	assert_string_equal(" cp " SL " ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %D:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "rwin ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %rz:p ", "", &flags, 1);
	assert_string_equal(" cp " SL "test-data" SL "existing-files" SL "a " SL "test-data" SL "existing-files" SL "b " SL "test-data" SL "existing-files" SL "c ", expanded);
	free(expanded);
}

static void
test_colon_tilde(void)
{
	MacroFlags flags;
	char *expanded;

	expanded = expand_macros(" cp %f:~ ", "", &flags, 1);
	assert_string_equal(" cp lfile0 lfile2 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %f:p:~ ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin" SL "lfile0 " SL "lwin" SL "lfile2 ",
			expanded);
	free(expanded);

	expanded = expand_macros(" cp %F:~ ", "", &flags, 1);
	assert_string_equal(" cp ~" SL "rfile1 ~" SL "rfile3 ~" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = expand_macros(" cp %b:p:~ ", "", &flags, 1);
	assert_string_equal(
			" cp " SL "lwin" SL "lfile0 " SL "lwin" SL "lfile2 ~" SL "rfile1 ~" SL "rfile3 ~" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = expand_macros(" cp %b:~ ", "", &flags, 1);
	assert_string_equal(" cp lfile0 lfile2 ~" SL "rfile1 ~" SL "rfile3 ~" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = expand_macros(" cp %c:~ ", "", &flags, 1);
	assert_string_equal(" cp lfile2 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %c:p:~ ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin" SL "lfile2 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %C:~ ", "", &flags, 1);
	assert_string_equal(" cp ~" SL "rfile5 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %d:~ ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %D:~ ", "", &flags, 1);
	assert_string_equal(" cp ~ ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %rz:~ ", "", &flags, 1);
	assert_string_equal(" cp test-data" SL "existing-files" SL "a test-data" SL "existing-files" SL "b test-data" SL "existing-files" SL "c ", expanded);
	free(expanded);
}

static void
test_colon_dot(void)
{
	MacroFlags flags;
	char *expanded;

	expanded = expand_macros(" cp %f:. ", "", &flags, 1);
	assert_string_equal(" cp lfile0 lfile2 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %f:p:. ", "", &flags, 1);
	assert_string_equal(" cp lfile0 lfile2 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %F:~:. ", "", &flags, 1);
	assert_string_equal(" cp ~" SL "rfile1 ~" SL "rfile3 ~" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = expand_macros(" cp %F:. ", "", &flags, 1);
	assert_string_equal(" cp " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = expand_macros(" cp %b:. ", "", &flags, 1);
	assert_string_equal(
			" cp lfile0 lfile2 " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = expand_macros(" cp %b:p:. ", "", &flags, 1);
	assert_string_equal(
			" cp lfile0 lfile2 " SL "rwin" SL "rfile1 " SL "rwin" SL "rfile3 " SL "rwin" SL "rfile5 ",
			expanded);
	free(expanded);

	expanded = expand_macros(" cp %c:. ", "", &flags, 1);
	assert_string_equal(" cp lfile2 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %c:p:. ", "", &flags, 1);
	assert_string_equal(" cp lfile2 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %C:. ", "", &flags, 1);
	assert_string_equal(" cp " SL "rwin" SL "rfile5 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %d:. ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %D:. ", "", &flags, 1);
	assert_string_equal(" cp " SL "rwin ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %rz:. ", "", &flags, 1);
	assert_string_equal(" cp test-data" SL "existing-files" SL "a test-data" SL "existing-files" SL "b test-data" SL "existing-files" SL "c ", expanded);
	free(expanded);
}

static void
test_colon_h(void)
{
	MacroFlags flags;
	char *expanded;

	expanded = expand_macros(" cp %f:h ", "", &flags, 1);
	assert_string_equal(" cp . . ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %f:p:h ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin " SL "lwin ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %F:~:h ", "", &flags, 1);
	assert_string_equal(" cp ~ ~ ~ ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %F:h ", "", &flags, 1);
	assert_string_equal(" cp " SL "rwin " SL "rwin " SL "rwin ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %b:h ", "", &flags, 1);
	assert_string_equal(" cp . . " SL "rwin " SL "rwin " SL "rwin ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %b:p:h ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin " SL "lwin " SL "rwin " SL "rwin " SL "rwin ",
			expanded);
	free(expanded);

	expanded = expand_macros(" cp %c:p:h ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %C:h ", "", &flags, 1);
	assert_string_equal(" cp " SL "rwin ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %d:h ", "", &flags, 1);
	assert_string_equal(" cp " SL " ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %D:h:h ", "", &flags, 1);
	assert_string_equal(" cp " SL " ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %rz:h ", "", &flags, 1);
	assert_string_equal(" cp test-data" SL "existing-files test-data" SL "existing-files test-data" SL "existing-files ", expanded);
	free(expanded);
}

static void
test_colon_t(void)
{
	MacroFlags flags;
	char *expanded;

	expanded = expand_macros(" cp %f:t ", "", &flags, 1);
	assert_string_equal(" cp lfile0 lfile2 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %f:p:t ", "", &flags, 1);
	assert_string_equal(" cp lfile0 lfile2 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %F:t ", "", &flags, 1);
	assert_string_equal(" cp rfile1 rfile3 rfile5 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %b:t ", "", &flags, 1);
	assert_string_equal(" cp lfile0 lfile2 rfile1 rfile3 rfile5 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %b:p:t ", "", &flags, 1);
	assert_string_equal(" cp lfile0 lfile2 rfile1 rfile3 rfile5 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %c:t ", "", &flags, 1);
	assert_string_equal(" cp lfile2 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %c:p:t ", "", &flags, 1);
	assert_string_equal(" cp lfile2 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %C:t ", "", &flags, 1);
	assert_string_equal(" cp rfile5 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %d:t ", "", &flags, 1);
	assert_string_equal(" cp lwin ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %D:t:t ", "", &flags, 1);
	assert_string_equal(" cp rwin ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %rz:t ", "", &flags, 1);
	assert_string_equal(" cp a b c ", expanded);
	free(expanded);
}

static void
test_colon_r(void)
{
	MacroFlags flags;
	char *expanded;

	strcpy(rwin.curr_dir, "/rw.in");
	rwin.list_pos = 4;

	expanded = expand_macros(" cp %c:r ", "", &flags, 1);
	assert_string_equal(" cp lfile2 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %c:r:r ", "", &flags, 1);
	assert_string_equal(" cp lfile2 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %c:p:r ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin" SL "lfile2 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %C:r ", "", &flags, 1);
	assert_string_equal(" cp " SL "rw.in" SL "rfile4.tar ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %C:r:r ", "", &flags, 1);
	assert_string_equal(" cp " SL "rw.in" SL "rfile4 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %C:r:r:r ", "", &flags, 1);
	assert_string_equal(" cp " SL "rw.in" SL "rfile4 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %d:r ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %D:r ", "", &flags, 1);
	assert_string_equal(" cp " SL "rw ", expanded);
	free(expanded);

	lwin.list_pos = 4;

	expanded = expand_macros(" cp %c:r ", "", &flags, 1);
	assert_string_equal(" cp .lfile4 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %c:p:r ", "", &flags, 1);
	assert_string_equal(" cp " SL "lwin" SL ".lfile4 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %rz:t:r ", "", &flags, 1);
	assert_string_equal(" cp a b c ", expanded);
	free(expanded);
}

static void
test_colon_e(void)
{
	MacroFlags flags;
	char *expanded;

	strcpy(rwin.curr_dir, "" SL "rw.in");
	rwin.list_pos = 4;

	expanded = expand_macros(" cp %c:e ", "", &flags, 1);
	assert_string_equal(" cp  ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %c:e:e ", "", &flags, 1);
	assert_string_equal(" cp  ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %c:p:e ", "", &flags, 1);
	assert_string_equal(" cp  ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %C:e ", "", &flags, 1);
	assert_string_equal(" cp gz ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %C:e:e ", "", &flags, 1);
	assert_string_equal(" cp  ", expanded);
	free(expanded);

	rwin.list_pos = 2;

	expanded = expand_macros(" cp %C:e ", "", &flags, 1);
	assert_string_equal(" cp  ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %d:e ", "", &flags, 1);
	assert_string_equal(" cp  ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %D:e ", "", &flags, 1);
	assert_string_equal(" cp in ", expanded);
	free(expanded);

	lwin.list_pos = 4;

	expanded = expand_macros(" cp %c:e ", "", &flags, 1);
	assert_string_equal(" cp  ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %c:p:e ", "", &flags, 1);
	assert_string_equal(" cp  ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %rz:t:. ", "", &flags, 1);
	assert_string_equal(" cp a b c ", expanded);
	free(expanded);
}

static void
test_colon_s(void)
{
	MacroFlags flags;
	char *expanded;

	expanded = expand_macros(" cp %f:s?l?r ", "", &flags, 1);
	assert_string_equal(" cp lfile0 lfile2", expanded);
	free(expanded);

	expanded = expand_macros(" cp %f:s?l?r? ", "", &flags, 1);
	assert_string_equal(" cp rfile0 rfile2 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %f:salara ", "", &flags, 1);
	assert_string_equal(" cp rfile0 rfile2 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %f:s?l?r?k? ", "", &flags, 1);
	assert_string_equal(" cp rfile0 rfile2k? ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %f:s?l?r?:s!f!k!k? ", "", &flags, 1);
	assert_string_equal(" cp rkile0 rkile2k? ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %rz:t:s?a?y? ", "", &flags, 1);
	assert_string_equal(" cp y b c ", expanded);
	free(expanded);
}

static void
test_colon_gs(void)
{
	MacroFlags flags;
	char *expanded;

	expanded = expand_macros(" cp %f:gs?l?r ", "", &flags, 1);
	assert_string_equal(" cp lfile0 lfile2", expanded);
	free(expanded);

	expanded = expand_macros(" cp %f:gs?l?r? ", "", &flags, 1);
	assert_string_equal(" cp rfire0 rfire2 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %f:gsalara ", "", &flags, 1);
	assert_string_equal(" cp rfire0 rfire2 ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %f:gs?l?r?k? ", "", &flags, 1);
	assert_string_equal(" cp rfire0 rfire2k? ", expanded);
	free(expanded);

	expanded = expand_macros(" cp %rz:t:gs?b?y? ", "", &flags, 1);
	assert_string_equal(" cp a y c ", expanded);
	free(expanded);

#ifdef _WIN32
	{
		char *old = cfg.shell;
		cfg.shell = strdup("bash");
		expanded = expand_macros(" cp %f:p:gs?\\?/? ", "", &flags, 1);
		assert_string_equal(" cp /lwin/lfile0 /lwin/lfile2 ", expanded);
		free(expanded);
		free(cfg.shell);
		cfg.shell = old;
	}
#endif
}

#ifdef _WIN32
static void
test_colon_u(void)
{
	MacroFlags flags;
	char *expanded;

	expanded = expand_macros("%f:p:u", "", &flags, 1);
	assert_string_starts_with("\\\\", expanded);
	free(expanded);

	strcpy(lwin.curr_dir, "//server/share/directory");

	expanded = expand_macros(" cp %f:p:u ", "", &flags, 1);
	assert_string_equal(" cp " SL SL "server " SL SL "server ", expanded);
	free(expanded);
}

static void
test_windows_specific(void)
{
	MacroFlags flags;
	char *expanded;

	strcpy(lwin.curr_dir, "h:/rwin");

	expanded = expand_macros(" %d:h ", "", &flags, 1);
	assert_string_equal(" h:\\\\ ", expanded);
	free(expanded);

	expanded = expand_macros(" %d:h:h ", "", &flags, 1);
	assert_string_equal(" h:\\\\ ", expanded);
	free(expanded);

	strcpy(lwin.curr_dir, "//ZX-Spectrum");

	expanded = expand_macros(" %d:h ", "", &flags, 1);
	assert_string_equal(" \\\\\\\\ZX-Spectrum ", expanded);
	free(expanded);

	expanded = expand_macros(" %d:h:h ", "", &flags, 1);
	assert_string_equal(" \\\\\\\\ZX-Spectrum ", expanded);
	free(expanded);
}
#endif

static void
test_modif_without_macros(void)
{
	MacroFlags flags;
	char *expanded;

	expanded = expand_macros(" cp %f:t :p :~ :. :h :t :r :e :s :gs ", "", &flags,
			0);
	assert_string_equal(" cp lfile0 lfile2 :p :~ :. :h :t :r :e :s :gs ",
			expanded);
	free(expanded);
}

static void
test_with_quotes(void)
{
	MacroFlags flags;
	char *expanded;

	expanded = expand_macros(" cp %\"f:p ", "", &flags, 1);
	assert_string_equal(" cp \"" SL "lwin" SL "lfile0\" \"" SL "lwin" SL "lfile2\" ",
			expanded);
	free(expanded);
}

void
fname_modif_tests(void)
{
	test_fixture_start();

	fixture_setup(setup);
	fixture_teardown(teardown);

	run_test(test_colon_p);
	run_test(test_colon_p_in_root);
	run_test(test_colon_tilde);
	run_test(test_colon_dot);
	run_test(test_colon_h);
	run_test(test_colon_t);
	run_test(test_colon_r);
	run_test(test_colon_e);
	run_test(test_colon_s);
	run_test(test_colon_gs);

#ifdef _WIN32
	run_test(test_colon_u);
	run_test(test_windows_specific);
#endif

	run_test(test_modif_without_macros);
	run_test(test_with_quotes);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
