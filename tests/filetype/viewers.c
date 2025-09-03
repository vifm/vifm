#include <stic.h>

#include <stdlib.h>
#include <string.h>

#include <test-utils.h>

#include "../../src/int/file_magic.h"
#include "../../src/filetype.h"
#include "../../src/status.h"
#include "../../src/utils/str.h"
#include "../../src/utils/string_array.h"
#include "test.h"

static int name_based_exists(const char name[]);

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

	assoc_viewers("*.tar.bz2", "prog1");
	assoc_viewers("*.tar.bz2", "prog2");

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

	assoc_viewers("*.tar.bz2", "prog1,prog2");

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

TEST(description_is_allowed)
{
	assoc_viewers("*.tar.bz2", "{archives} prog1");

	ft_init(&prog1_available);
	assert_string_equal("prog1", ft_get_viewer("file.version.tar.bz2"));
}

TEST(several_patterns)
{
	assoc_viewers("*.tbz,*.tbz2,*.tar.bz2", "prog1");

	ft_init(&prog1_available);

	assert_true(ft_get_viewer("file.version.tbz") != NULL);
	assert_true(ft_get_viewer("file.version.tbz2") != NULL);
	assert_true(ft_get_viewer("file.version.tar.bz2") != NULL);
}

TEST(multiple_viewers)
{
	assoc_viewers("*.tbz", "prog1 a");
	assoc_viewers("*.tba", "prog2 a");
	assoc_viewers("*.tbz", "prog2 b");
	assoc_viewers("*.tbz", "prog1 b");

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
	assoc_viewers(cmd, "prog1");

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
	assert_int_equal(VK_GRAPHICAL, ft_viewer_kind("echo %px %py %ph %pw"));
	assert_int_equal(VK_TEXTUAL, ft_viewer_kind("echo %%px %%py"));
	assert_int_equal(VK_TEXTUAL, ft_viewer_kind("echo %%px %%py %%ph %%pw"));

	assert_int_equal(VK_PASS_THROUGH, ft_viewer_kind("echo %pd"));
	assert_int_equal(VK_PASS_THROUGH, ft_viewer_kind("echo %px %py %pw %ph %pd"));
	assert_int_equal(VK_TEXTUAL, ft_viewer_kind("echo %%pd"));
	assert_int_equal(VK_GRAPHICAL, ft_viewer_kind("echo %px %py %pw %ph %%pd"));
}

TEST(viewer_topping_1file_2separate_viewers)
{
	assoc_viewers("*.tar.bz2", "prog1");
	assoc_viewers("*.tar.bz2", "prog2");

	assert_string_equal("prog1", ft_get_viewer("archive.tar.bz2"));

	/* Already the top one. */
	ft_move_viewer_to_top("archive.tar.bz2", "prog1");
	assert_string_equal("prog1", ft_get_viewer("archive.tar.bz2"));

	/* Swap the order. */
	ft_move_viewer_to_top("archive.tar.bz2", "prog2");
	assert_string_equal("prog2", ft_get_viewer("archive.tar.bz2"));

	/* Swap the order again. */
	ft_move_viewer_to_top("archive.tar.bz2", "prog2");
	assert_string_equal("prog2", ft_get_viewer("archive.tar.bz2"));
}

TEST(viewer_topping_1file_2joined_viewers)
{
	assoc_viewers("*.tar.bz2", "prog1, prog2");

	assert_string_equal("prog1", ft_get_viewer("archive.tar.bz2"));

	/* Already the top one. */
	ft_move_viewer_to_top("archive.tar.bz2", "prog1");
	assert_string_equal("prog1", ft_get_viewer("archive.tar.bz2"));

	/* Swap the order. */
	ft_move_viewer_to_top("archive.tar.bz2", "prog2");
	assert_string_equal("prog2", ft_get_viewer("archive.tar.bz2"));

	/* Swap the order again. */
	ft_move_viewer_to_top("archive.tar.bz2", "prog2");
	assert_string_equal("prog2", ft_get_viewer("archive.tar.bz2"));
}

TEST(viewer_topping_2files_2joined_viewers)
{
	assoc_viewers("*.tar.bz2,*.zip", "prog1, prog2");

	assert_string_equal("prog1", ft_get_viewer("archive.tar.bz2"));
	assert_string_equal("prog1", ft_get_viewer("archive.zip"));

	/* Already the top one. */
	ft_move_viewer_to_top("archive.tar.bz2", "prog1");
	assert_string_equal("prog1", ft_get_viewer("archive.tar.bz2"));
	assert_string_equal("prog1", ft_get_viewer("archive.zip"));

	/* Swap the order. */
	ft_move_viewer_to_top("archive.tar.bz2", "prog2");
	assert_string_equal("prog2", ft_get_viewer("archive.tar.bz2"));
	assert_string_equal("prog2", ft_get_viewer("archive.zip"));

	/* Swap the order again. */
	ft_move_viewer_to_top("archive.tar.bz2", "prog2");
	assert_string_equal("prog2", ft_get_viewer("archive.tar.bz2"));
	assert_string_equal("prog2", ft_get_viewer("archive.zip"));
}

TEST(viewer_topping_2files_mixed_viewers)
{
	assoc_viewers("*.tar.bz2,*.zip", "prog1, prog2");
	assoc_viewers("*.zip", "prog4");
	assoc_viewers("*.tar.bz2", "prog3");
	assoc_viewers("*", "defprog");

	assert_string_equal("prog1", ft_get_viewer("archive.tar.bz2"));
	assert_string_equal("prog1", ft_get_viewer("archive.zip"));

	/* Already the top one. */
	ft_move_viewer_to_top("archive.tar.bz2", "prog1");
	assert_string_equal("prog1", ft_get_viewer("archive.tar.bz2"));
	assert_string_equal("prog1", ft_get_viewer("archive.zip"));

	/* Swap the order. */
	ft_move_viewer_to_top("archive.tar.bz2", "prog3");
	assert_string_equal("prog3", ft_get_viewer("archive.tar.bz2"));
	assert_string_equal("prog4", ft_get_viewer("archive.zip"));

	/* Swap the order again. */
	ft_move_viewer_to_top("archive.tar.bz2", "prog2");
	assert_string_equal("prog2", ft_get_viewer("archive.tar.bz2"));
	assert_string_equal("prog4", ft_get_viewer("archive.zip"));
}

TEST(viewer_cycling_1file_overlap_next)
{
	assoc_viewers("{*.png}", "unrelated");
	assoc_viewers("{*.tar.bz2},{*.tar.*},{*.bz2}", "prog,prog2");
	assoc_viewers("{*}", "defprog");

	assert_string_equal("unrelated", ft_get_viewer("file.png"));
	assert_string_equal("prog", ft_get_viewer("archive.tar.bz2"));

	ft_move_viewer_cycle_next("archive.tar.bz2");
	assert_string_equal("prog2", ft_get_viewer("archive.tar.bz2"));
	ft_move_viewer_cycle_next("archive.tar.bz2");
	assert_string_equal("defprog", ft_get_viewer("archive.tar.bz2"));
	ft_move_viewer_cycle_next("archive.tar.bz2");
	assert_string_equal("prog", ft_get_viewer("archive.tar.bz2"));
	ft_move_viewer_cycle_next("archive.tar.bz2");
	assert_string_equal("prog2", ft_get_viewer("archive.tar.bz2"));

	/* Verifying that nothing unexpected has happened to this file. */
	assert_string_equal("unrelated", ft_get_viewer("file.png"));
}

TEST(viewer_cycling_1file_overlap_prev)
{
	assoc_viewers("{*.png}", "unrelated");
	assoc_viewers("{*.tar.bz2},{*.tar.*},{*.bz2}", "prog");
	assoc_viewers("{*}", "defprog");

	assert_string_equal("unrelated", ft_get_viewer("file.png"));
	assert_string_equal("prog", ft_get_viewer("archive.tar.bz2"));

	ft_move_viewer_cycle_prev("archive.tar.bz2");
	assert_string_equal("defprog", ft_get_viewer("archive.tar.bz2"));
	ft_move_viewer_cycle_prev("archive.tar.bz2");
	assert_string_equal("prog", ft_get_viewer("archive.tar.bz2"));
	ft_move_viewer_cycle_prev("archive.tar.bz2");
	assert_string_equal("defprog", ft_get_viewer("archive.tar.bz2"));

	/* Verifying that nothing unexpected has happened to this file. */
	assert_string_equal("unrelated", ft_get_viewer("file.png"));
}

TEST(viewer_cycling_1file_overlap)
{
	assoc_viewers("{*.mp3}", "prog1");
	assoc_viewers("{*.mp3}", "prog2");
	assoc_viewers("{*}", "defprog");

	assert_string_equal("prog1", ft_get_viewer("file.mp3"));

	/* Forwards. */
	ft_move_viewer_cycle_next("file.mp3");
	assert_string_equal("prog2", ft_get_viewer("file.mp3"));
	ft_move_viewer_cycle_next("file.mp3");
	assert_string_equal("defprog", ft_get_viewer("file.mp3"));
	ft_move_viewer_cycle_next("file.mp3");
	assert_string_equal("prog1", ft_get_viewer("file.mp3"));

	/* Backwards. */
	ft_move_viewer_cycle_prev("file.mp3");
	assert_string_equal("defprog", ft_get_viewer("file.mp3"));
	ft_move_viewer_cycle_prev("file.mp3");
	assert_string_equal("prog2", ft_get_viewer("file.mp3"));
	ft_move_viewer_cycle_prev("file.mp3");
	assert_string_equal("prog1", ft_get_viewer("file.mp3"));
}

TEST(viewer_reordering_1file_missing_viewers)
{
	ft_init(&name_based_exists);

	assoc_viewers("{*.mp3}", "m1");
	assoc_viewers("{*.mp3}", "p2");
	assoc_viewers("{*.mp3}", "p3");

	assert_string_equal("p2", ft_get_viewer("file.mp3"));

	/* Forwards. */
	ft_move_viewer_cycle_next("file.mp3");
	assert_string_equal("p3", ft_get_viewer("file.mp3"));
	ft_move_viewer_cycle_next("file.mp3");
	assert_string_equal("p2", ft_get_viewer("file.mp3"));
	ft_move_viewer_cycle_next("file.mp3");
	assert_string_equal("p3", ft_get_viewer("file.mp3"));
	ft_init(NULL);
	ft_move_viewer_cycle_next("file.mp3");
	assert_string_equal("m1", ft_get_viewer("file.mp3"));
	ft_move_viewer_cycle_next("file.mp3");
	assert_string_equal("p2", ft_get_viewer("file.mp3"));

	ft_init(&name_based_exists);

	/* Backwards. */
	ft_move_viewer_cycle_prev("file.mp3");
	assert_string_equal("p3", ft_get_viewer("file.mp3"));
	ft_move_viewer_cycle_prev("file.mp3");
	assert_string_equal("p2", ft_get_viewer("file.mp3"));
	ft_init(NULL);
	ft_move_viewer_cycle_prev("file.mp3");
	assert_string_equal("m1", ft_get_viewer("file.mp3"));
	ft_move_viewer_cycle_prev("file.mp3");
	assert_string_equal("p3", ft_get_viewer("file.mp3"));
	ft_move_viewer_cycle_prev("file.mp3");
	assert_string_equal("p2", ft_get_viewer("file.mp3"));

	ft_init(&name_based_exists);

	/* To the top. */
	ft_move_viewer_to_top("file.mp3", "p3");
	assert_string_equal("p3", ft_get_viewer("file.mp3"));
	ft_init(NULL);
	ft_move_viewer_cycle_next("file.mp3");
	assert_string_equal("m1", ft_get_viewer("file.mp3"));
	ft_init(&name_based_exists);
	assert_string_equal("p2", ft_get_viewer("file.mp3"));
}

TEST(viewer_reordering_file_with_no_viewers)
{
	assoc_viewers("{*.png}", "unrelated");

	assert_string_equal("unrelated", ft_get_viewer("file.png"));

	ft_move_viewer_cycle_prev("archive.tar.bz2");
	assert_string_equal(NULL, ft_get_viewer("archive.tar.bz2"));
	ft_move_viewer_cycle_next("archive.tar.bz2");
	assert_string_equal(NULL, ft_get_viewer("archive.tar.bz2"));
	ft_move_viewer_to_top("archive.tar.bz2", "nope");
	assert_string_equal(NULL, ft_get_viewer("archive.tar.bz2"));

	/* Verifying that nothing unexpected has happened to this file. */
	assert_string_equal("unrelated", ft_get_viewer("file.png"));
}

static int
name_based_exists(const char name[])
{
	/* 'p' for "present". */
	return (name[0] == 'p');
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
