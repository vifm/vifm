#ifndef __TEST_H__
#define __TEST_H__

#include "../../src/undo.h"

void init_undo_list_for_tests(perform_func exec_func, const int *max_levels);

#endif /* __TEST_H__ */
