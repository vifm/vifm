#include "test.h"

#include "../../src/int/file_magic.h"

int
has_mime_type_detection(void)
{
	return get_mimetype(TEST_DATA_PATH "/read/dos-line-endings", 0) != NULL;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
