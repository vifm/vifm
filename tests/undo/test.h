#ifndef VIFM_TESTS__UNDO__TEST_H__
#define VIFM_TESTS__UNDO__TEST_H__

#include "../../src/undo.h"

void init_undo_list_for_tests(un_perform_func exec_func, const int *max_levels);

#endif /* VIFM_TESTS__UNDO__TEST_H__ */
