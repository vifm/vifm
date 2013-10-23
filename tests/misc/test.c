#include "seatest.h"

void canonical(void);
void test_append_selected_files(void);
void test_expand_macros(void);
void path_starts_with_tests(void);
void comments_tests(void);
void format_edit_selection_cmd_tests(void);
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
void bookmarks_tests(void);
void leave_invalid_dir_tests(void);
void remove_last_path_component_tests(void);
void substitute_tests(void);
void extract_extension_tests(void);
void get_command_name_tests(void);
void format_mount_command_tests(void);
void trim_right_tests(void);
void echo_tests(void);
void format_str_tests(void);
void utf8_tests(void);
void read_line_tests(void);
void history_tests(void);
void extract_part_tests(void);
void strchar2str_tests(void);
void get_last_path_component_tests(void);
void get_line_tests(void);
void expand_custom_macros_tests(void);
void expand_envvars_tests(void);
void external_command_exists_tests(void);
void read_file_lines_tests(void);

void
all_tests(void)
{
	canonical();
	test_append_selected_files();
	test_expand_macros();
	path_starts_with_tests();
	comments_tests();
	format_edit_selection_cmd_tests();
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
	bookmarks_tests();
	leave_invalid_dir_tests();
	remove_last_path_component_tests();
	substitute_tests();
	extract_extension_tests();
	get_command_name_tests();
	format_mount_command_tests();
	trim_right_tests();
	echo_tests();
	format_str_tests();
	utf8_tests();
	read_line_tests();
	history_tests();
	extract_part_tests();
	strchar2str_tests();
	get_last_path_component_tests();
	get_line_tests();
	expand_custom_macros_tests();
	expand_envvars_tests();
	external_command_exists_tests();
	read_file_lines_tests();
}

int
main(void)
{
	return run_tests(all_tests) == 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
