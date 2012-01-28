#include "seatest.h"

void canonical(void);
void test_append_selected_files(void);
void test_expand_macros(void);
void path_starts_with_tests(void);
void comments_tests(void);
void edit_cmd_selection_tests(void);
void test_reserved_commands(void);
void test_user_commands(void);
void test_cmdline_completion(void);
void test_dispatch_line(void);
void test_command_separation(void);
void friendly_size(void);
void rel_symlinks_tests(void);
void fname_modif_tests(void);
void gen_clone_name_tests(void);
void rename_tests(void);
void sort_tests(void);
void filtering_tests(void);

void all_tests(void)
{
	canonical();
	test_append_selected_files();
	test_expand_macros();
	path_starts_with_tests();
	comments_tests();
	edit_cmd_selection_tests();
	test_reserved_commands();
	test_user_commands();
	test_cmdline_completion();
	test_dispatch_line();
	test_command_separation();
	friendly_size();
	rel_symlinks_tests();
	fname_modif_tests();
	gen_clone_name_tests();
	rename_tests();
	sort_tests();
	filtering_tests();
}

int main(int argc, char **argv)
{
	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
