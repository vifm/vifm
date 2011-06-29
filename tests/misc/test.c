#include "seatest.h"

void canonical(void);
void test_append_selected_files(void);
void test_expand_macros(void);
void path_starts_with_tests(void);
void check_dir_for_colorscheme_tests(void);
void comments_tests(void);
void compare_file_names_tests(void);

void all_tests(void)
{
	canonical();
	test_append_selected_files();
	test_expand_macros();
	path_starts_with_tests();
	check_dir_for_colorscheme_tests();
	comments_tests();
	compare_file_names_tests();
}

int main(int argc, char **argv)
{
	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
