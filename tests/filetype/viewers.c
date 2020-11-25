#include <stic.h>

#include <stdlib.h>
#include <string.h>

#include "../../src/int/file_magic.h"
#include "../../src/filetype.h"
#include "../../src/status.h"
#include "../../src/utils/str.h"
#include "../../src/utils/string_array.h"
#include "test.h"

static int
prog1_available(const char name[])
{
	return stroscmp(name, "prog1") == 0;
}

static int
prog2_available(const char name[])
{
	return stroscmp(name, "prog2") == 0;
}

static int
nothing_available(const char name[])
{
	return 0;
}

TEST(null_if_nothing_set)
{
	assert_true(ft_get_viewer("file.version.tar.bz2") == NULL);
}

TEST(multiple_choice_separated)
{
	const char *viewer;

	set_viewers("*.tar.bz2", "prog1");
	set_viewers("*.tar.bz2", "prog2");

	ft_init(&prog1_available);
	viewer = ft_get_viewer("file.version.tar.bz2");
	assert_string_equal("prog1", viewer);

	ft_init(&prog2_available);
	viewer = ft_get_viewer("file.version.tar.bz2");
	assert_string_equal("prog2", viewer);

	ft_init(&nothing_available);
	viewer = ft_get_viewer("file.version.tar.bz2");
	assert_true(viewer == NULL);
}

TEST(multiple_choice_joined)
{
	const char *viewer;

	set_viewers("*.tar.bz2", "prog1,prog2");

	ft_init(&prog1_available);
	viewer = ft_get_viewer("file.version.tar.bz2");
	assert_string_equal("prog1", viewer);

	ft_init(&prog2_available);
	viewer = ft_get_viewer("file.version.tar.bz2");
	assert_string_equal("prog2", viewer);

	ft_init(&nothing_available);
	viewer = ft_get_viewer("file.version.tar.bz2");
	assert_true(viewer == NULL);
}

TEST(description_is_not_allowed)
{
	const char *viewer;

	set_viewers("*.tar.bz2", "{archives} prog1");

	ft_init(&prog1_available);
	viewer = ft_get_viewer("file.version.tar.bz2");
	assert_true(viewer == NULL);
}

TEST(several_patterns)
{
	set_viewers("*.tbz,*.tbz2,*.tar.bz2", "prog1");

	ft_init(&prog1_available);

	assert_true(ft_get_viewer("file.version.tbz") != NULL);
	assert_true(ft_get_viewer("file.version.tbz2") != NULL);
	assert_true(ft_get_viewer("file.version.tar.bz2") != NULL);
}

TEST(multiple_viewers)
{
	set_viewers("*.tbz", "prog1 a");
	set_viewers("*.tba", "prog2 a");
	set_viewers("*.tbz", "prog2 b");
	set_viewers("*.tbz", "prog1 b");

	ft_init(&prog1_available);

	strlist_t viewers = ft_get_viewers("a.tbz");
	assert_int_equal(2, viewers.nitems);
	assert_string_equal("prog1 a", viewers.items[0]);
	assert_string_equal("prog1 b", viewers.items[1]);
	free_string_array(viewers.items, viewers.nitems);
}

TEST(pattern_list, IF(has_mime_type_detection))
{
	char cmd[1024];

	snprintf(cmd, sizeof(cmd), "<%s>{binary-data}",
			get_mimetype(TEST_DATA_PATH "/read/binary-data", 0));
	set_viewers(cmd, "prog1");

	ft_init(&prog1_available);

	assert_string_equal("prog1",
			ft_get_viewer(TEST_DATA_PATH "/read/binary-data"));
}

TEST(viewer_kind)
{
	assert_int_equal(VK_TEXTUAL, ft_viewer_kind(NULL));
	assert_int_equal(VK_TEXTUAL, ft_viewer_kind(""));

	assert_int_equal(VK_TEXTUAL, ft_viewer_kind("cat"));
	assert_int_equal(VK_TEXTUAL, ft_viewer_kind("cat %c"));

	assert_int_equal(VK_TEXTUAL, ft_viewer_kind("echo %px"));
	assert_int_equal(VK_TEXTUAL, ft_viewer_kind("echo %py"));

	assert_int_equal(VK_GRAPHICAL, ft_viewer_kind("echo %px %py"));

	assert_int_equal(VK_PASS_THROUGH, ft_viewer_kind("echo %pd"));
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
