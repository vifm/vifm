#include <stic.h>

#include "../../src/compat/fs_limits.h"
#include "../../src/utils/fs.h"
#include "../../src/utils/path.h"
#include "../../src/utils/utils.h"

#ifdef _WIN32

TEST(shortcut_target_is_read)
{
    char target[PATH_MAX + 1];
    assert_success(win_shortcut_read(TEST_DATA_PATH "/various-files/link.lnk",
                                     target, sizeof(target)));
    assert_true(paths_are_equal(target, "c:/home/some.file"));
}

TEST(shortcut_target_is_read_by_get_link_target)
{
    char target[PATH_MAX + 1];
    assert_success(get_link_target(TEST_DATA_PATH "/various-files/link.lnk",
                                   target, sizeof(target)));
    assert_true(paths_are_equal(target, "c:/home/some.file"));
}

#endif
