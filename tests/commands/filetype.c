#include <stic.h>

#include <test-utils.h>

#include "../../src/engine/keys.h"
#include "../../src/int/file_magic.h"
#include "../../src/modes/modes.h"
#include "../../src/ui/ui.h"
#include "../../src/cmd_core.h"
#include "../../src/filelist.h"
#include "../../src/filetype.h"
#include "../../src/status.h"

static void check_filetype(void);
static int prog_exists(const char name[]);
static int has_mime_type_detection(void);

SETUP()
{
	init_commands();
	conf_setup();

	view_setup(&lwin);
	view_setup(&rwin);

	curr_view = &lwin;
	other_view = &rwin;
}

TEARDOWN()
{
	view_teardown(&lwin);
	view_teardown(&rwin);

	conf_teardown();
	vle_cmds_reset();
}

TEST(filetype_accepts_negated_patterns)
{
	ft_init(&prog_exists);

	assert_success(exec_commands("filetype !{*.tar} prog", &lwin, CIT_COMMAND));

	check_filetype();

	ft_reset(0);
}

TEST(filextype_accepts_negated_patterns)
{
	ft_init(&prog_exists);
	curr_stats.exec_env_type = EET_EMULATOR_WITH_X;

	assert_success(exec_commands("filextype !{*.tar} prog", &lwin, CIT_COMMAND));

	check_filetype();

	curr_stats.exec_env_type = EET_LINUX_NATIVE;
	ft_reset(1);

	curr_stats.exec_env_type = EET_EMULATOR_WITH_X;
}

TEST(fileviewer_accepts_negated_patterns)
{
	ft_init(&prog_exists);

	assert_success(exec_commands("fileviewer !{*.tar} view", &lwin, CIT_COMMAND));
	assert_string_equal("view", ft_get_viewer("file.version.tar.bz2"));

	ft_reset(0);
}

TEST(pattern_anding_and_orring_failures)
{
	/* No matching is performed, so we can use application/octet-stream. */
	assert_failure(exec_commands("filetype /*/,"
				"<application/octet-stream>{binary-data} app", &lwin, CIT_COMMAND));
	assert_failure(exec_commands("fileviewer /*/,"
				"<application/octet-stream>{binary-data} viewer", &lwin, CIT_COMMAND));
}

TEST(pattern_anding_and_orring, IF(has_mime_type_detection))
{
	char cmd[1024];
	assoc_records_t ft;

	ft_init(&prog_exists);

	snprintf(cmd, sizeof(cmd),
			"filetype {two-lines}<text/plain>,<%s>{binary-data} app",
			get_mimetype(TEST_DATA_PATH "/read/binary-data", 0));
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));
	snprintf(cmd, sizeof(cmd),
			"fileviewer {two-lines}<text/plain>,<%s>{binary-data} viewer",
			get_mimetype(TEST_DATA_PATH "/read/binary-data", 0));
	assert_success(exec_commands(cmd, &lwin, CIT_COMMAND));

	ft = ft_get_all_programs(TEST_DATA_PATH "/read/two-lines");
	assert_int_equal(1, ft.count);
	if(ft.count == 1)
	{
		assert_string_equal("app", ft.list[0].command);
	}
	ft_assoc_records_free(&ft);

	ft = ft_get_all_programs(TEST_DATA_PATH "/read/binary-data");
	assert_int_equal(1, ft.count);
	if(ft.count == 1)
	{
		assert_string_equal("app", ft.list[0].command);
	}
	ft_assoc_records_free(&ft);

	ft = ft_get_all_programs(TEST_DATA_PATH "/read/utf8-bom");
	assert_int_equal(0, ft.count);
	ft_assoc_records_free(&ft);

	assert_string_equal("viewer",
			ft_get_viewer(TEST_DATA_PATH "/read/two-lines"));
	assert_string_equal("viewer",
			ft_get_viewer(TEST_DATA_PATH "/read/binary-data"));
	assert_string_equal(NULL, ft_get_viewer(TEST_DATA_PATH "/read/utf8-bom"));

	ft_reset(0);
}

TEST(cv_is_built_by_handler)
{
	init_modes();

	make_abs_path(lwin.curr_dir, sizeof(lwin.curr_dir), TEST_DATA_PATH, "", NULL);

	flist_custom_start(&lwin, "test");
	assert_non_null(flist_custom_add(&lwin, "existing-files/a"));
	assert_success(flist_custom_finish(&lwin, CV_REGULAR, 0));

	assert_success(exec_commands("filetype a echo %c %u", &lwin, CIT_COMMAND));
	assert_success(exec_commands("normal l", &lwin, CIT_COMMAND));
	assert_true(flist_custom_active(&lwin));

	assert_string_equal("echo %c %u", lwin.custom.title);

	vle_keys_reset();
}

static void
check_filetype(void)
{
	assoc_records_t ft;

	ft = ft_get_all_programs("file.version.tar");
	assert_int_equal(0, ft.count);
	ft_assoc_records_free(&ft);

	ft = ft_get_all_programs("file.version.tar.bz");
	assert_int_equal(1, ft.count);
	assert_string_equal("prog", ft.list[0].command);
	ft_assoc_records_free(&ft);
}

static int
prog_exists(const char name[])
{
	return 1;
}

static int
has_mime_type_detection(void)
{
	return get_mimetype(TEST_DATA_PATH "/read/dos-line-endings", 0) != NULL;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
