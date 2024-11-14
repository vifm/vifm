#include <stic.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/lua/vlua.h"
#include "../../src/utils/fs.h"
#include "../../src/status.h"

#include <test-utils.h>

#include "asserts.h"

static void cpmv_prepare(int is_mv);
static void file_is_line(const char path[], const char line[]);

static vlua_t *vlua;
static void (*rm_src_file)(const char path[]);
static void (*rm_src_dir)(const char path[]);

SETUP()
{
	vlua = vlua_init();
	curr_stats.vlua = vlua;
}

TEARDOWN()
{
	vlua_finish(vlua);
	curr_stats.vlua = NULL;

	rm_src_file = NULL;
	rm_src_dir = NULL;
}

TEST(fs_cpmv_file, REPEAT(2))
{
	cpmv_prepare(/*is_mv=*/STIC_TEST_PARAM == 1);
	create_file("file");
	GLUA_EQ(vlua, "true", "print(op('file', 'target'))");
	rm_src_file("file");
	remove_file("target");
}

TEST(fs_cpmv_empty_dir, REPEAT(2))
{
	cpmv_prepare(/*is_mv=*/STIC_TEST_PARAM == 1);
	create_dir("dir");
	GLUA_EQ(vlua, "true", "print(op('dir', 'target'))");
	rm_src_dir("dir");
	remove_dir("target");
}

TEST(fs_cpmv_non_empty_dir, REPEAT(2))
{
	cpmv_prepare(/*is_mv=*/STIC_TEST_PARAM == 1);
	create_dir("dir");
	create_file("dir/file");
	GLUA_EQ(vlua, "true", "print(op('dir', 'target'))");
	rm_src_file("dir/file");
	rm_src_dir("dir");
	remove_file("target/file");
	remove_dir("target");
}

TEST(fs_cpmv_file_exists, REPEAT(2))
{
	cpmv_prepare(/*is_mv=*/STIC_TEST_PARAM == 1);
	create_file("file");
	create_file("target");
	GLUA_EQ(vlua, "false", "print(op('file', 'target'))");
	remove_file("file");
	remove_file("target");
}

TEST(fs_cpmv_dir_exists, REPEAT(2))
{
	cpmv_prepare(/*is_mv=*/STIC_TEST_PARAM == 1);
	create_dir("dir");
	create_dir("target");
	GLUA_EQ(vlua, "false", "print(op('dir', 'target'))");
	remove_dir("dir");
	remove_dir("target");
}

TEST(fs_cpmv_file_skip, REPEAT(2))
{
	cpmv_prepare(/*is_mv=*/STIC_TEST_PARAM == 1);
	make_file("file", "abc");
	make_file("target", "xyz");

	GLUA_EQ(vlua, "true", "print(op('file', 'target', 'skip'))");
	file_is_line("target", "xyz");

	remove_file("file");
	remove_file("target");
}

TEST(fs_cpmv_dir_skip, REPEAT(2))
{
	cpmv_prepare(/*is_mv=*/STIC_TEST_PARAM == 1);
	create_dir("dir");
	make_file("dir/file", "abc");
	create_dir("target");
	make_file("target/file", "xyz");

	GLUA_EQ(vlua, "true", "print(op('dir', 'target', 'skip'))");
	file_is_line("target/file", "xyz");

	remove_file("dir/file");
	remove_dir("dir");
	remove_file("target/file");
	remove_dir("target");
}

TEST(fs_cpmv_file_overwrite, REPEAT(2))
{
	cpmv_prepare(/*is_mv=*/STIC_TEST_PARAM == 1);
	make_file("file", "abc");
	make_file("target", "xyz");

	GLUA_EQ(vlua, "true", "print(op('file', 'target', 'overwrite'))");
	file_is_line("target", "abc");

	rm_src_file("file");
	remove_file("target");
}

TEST(fs_cpmv_dir_overwrite, REPEAT(2))
{
	cpmv_prepare(/*is_mv=*/STIC_TEST_PARAM == 1);
	create_dir("dir");
	create_file("dir/file");
	create_dir("target");
	create_file("target/tfile");

	GLUA_EQ(vlua, "true", "print(op('dir', 'target', 'overwrite'))");

	rm_src_file("dir/file");
	rm_src_dir("dir");
	remove_file("target/file");
	remove_file("target/tfile");
	remove_dir("target");
}

TEST(fs_cpmv_file_append_tail)
{
	cpmv_prepare(/*is_mv=*/STIC_TEST_PARAM == 1);
	make_file("file", "abc");
	make_file("target", "xy");

	GLUA_EQ(vlua, "true", "print(op('file', 'target', 'append-tail'))");
	file_is_line("target", "xyc");

	rm_src_file("file");
	remove_file("target");
}

TEST(fs_cpmv_dir_append_tail)
{
	cpmv_prepare(/*is_mv=*/STIC_TEST_PARAM == 1);
	create_dir("dir");
	create_file("dir/file");
	create_dir("target");
	create_file("target/tfile");

	GLUA_EQ(vlua, "false", "print(op('dir', 'target', 'append-tail'))");

	rm_src_file("dir/file");
	rm_src_dir("dir");
	remove_file("target/tfile");
	remove_dir("target");
}

TEST(fs_ln_create, IF(not_windows))
{
	GLUA_EQ(vlua, "true", "print(vifm.fs.ln('link', 'target'))");

	char target[PATH_MAX + 1];
	assert_success(get_link_target("link", target, sizeof(target)));
	assert_string_equal("target", target);

	remove_file("link");
}

TEST(fs_ln_change, IF(not_windows))
{
	GLUA_EQ(vlua, "true", "print(vifm.fs.ln('link', 'target'))");
	GLUA_EQ(vlua, "true", "print(vifm.fs.ln('link', 'newtarget'))");

	char target[PATH_MAX + 1];
	assert_success(get_link_target("link", target, sizeof(target)));
	assert_string_equal("newtarget", target);

	remove_file("link");
}

TEST(fs_ln_file_exists)
{
	create_file("link");
	GLUA_EQ(vlua, "false", "print(vifm.fs.ln('link', 'target'))");
	remove_file("link");
}

TEST(fs_mkdir_parent_exists)
{
	GLUA_EQ(vlua, "true", "print(vifm.fs.mkdir('dir'))");
	remove_dir("dir");
}

TEST(fs_mkdir_no_parent_exists)
{
	GLUA_EQ(vlua, "false", "print(vifm.fs.mkdir('sub/dir'))");
	no_remove_dir("sub");
}

TEST(fs_mkdir_parent_created)
{
	GLUA_EQ(vlua, "true", "print(vifm.fs.mkdir('sub/dir', 'create'))");
	remove_dir("sub/dir");
	remove_dir("sub");
}

TEST(fs_mkfile)
{
	GLUA_EQ(vlua, "true", "print(vifm.fs.mkfile('file'))");
	remove_file("file");
}

TEST(fs_mkfile_already_exists)
{
	create_file("file");
	GLUA_EQ(vlua, "false", "print(vifm.fs.mkdir('file'))");
	remove_file("file");
}

TEST(fs_rm_file)
{
	create_file("file");
	GLUA_EQ(vlua, "true", "print(vifm.fs.rm('file'))");
	no_remove_file("file");
}

TEST(fs_rm_empty_dir)
{
	create_dir("dir");
	GLUA_EQ(vlua, "true", "print(vifm.fs.rm('dir'))");
	no_remove_dir("dir");
}

TEST(fs_rm_non_empty_dir)
{
	create_dir("dir");
	create_file("dir/file");
	GLUA_EQ(vlua, "true", "print(vifm.fs.rm('dir'))");
	no_remove_file("dir/file");
	no_remove_dir("dir");
}

TEST(fs_rmdir_dir)
{
	create_dir("dir");
	GLUA_EQ(vlua, "true", "print(vifm.fs.rmdir('dir'))");
	no_remove_dir("dir");
}

TEST(fs_rmdir_file)
{
	create_file("file");
	GLUA_EQ(vlua, "false", "print(vifm.fs.rmdir('file'))");
	remove_file("file");
}

static void
cpmv_prepare(int is_mv)
{
	GLUA_EQ(vlua, "", is_mv ? "op = vifm.fs.mv" : "op = vifm.fs.cp");
	rm_src_file = (is_mv ? &no_remove_file : &remove_file);
	rm_src_dir = (is_mv ? &no_remove_dir : &remove_dir);
}

static void
file_is_line(const char path[], const char line[])
{
	const char *lines[] = { line };
	file_is(path, lines, 1);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
