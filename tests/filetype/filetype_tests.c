#include <stdlib.h>

#include "seatest.h"

#include "../../src/filetype.h"
#include "../../src/status.h"

static void
test_one_ext(void)
{
	assoc_prog_t program;

	set_programs("*.tar", "tar prog", "description", 0);

	assert_true(get_default_program_for_file("file.version.tar", &program));
	if(program.com != NULL)
		assert_string_equal("tar prog", program.com);
	free_assoc_prog(&program);
}

static void
test_many_ext(void)
{
	assoc_prog_t program;

	set_programs("*.tar", "tar prog", "description", 0);
	set_programs("*.tar.gz", "tar.gz prog", "description", 0);

	assert_true(get_default_program_for_file("file.version.tar.gz", &program));
	if(program.com != NULL)
		assert_string_equal("tar.gz prog", program.com);
	free_assoc_prog(&program);
}

static void
test_many_fileext(void)
{
	assoc_prog_t program;

	set_programs("*.tgz,*.tar.gz", "tar.gz prog", "description", 0);

	assert_true(get_default_program_for_file("file.version.tar.gz", &program));
	if(program.com != NULL)
		assert_string_equal("tar.gz prog", program.com);
	free_assoc_prog(&program);
}

static void
test_dont_match_hidden(void)
{
	assoc_prog_t program;

	set_programs("*.tgz,*.tar.gz", "tar.gz prog", "description", 0);

	assert_false(get_default_program_for_file(".file.version.tar.gz", &program));
}

static void
test_match_empty(void)
{
	assoc_prog_t program;

	set_programs("a*bc", "empty prog", "description", 0);

	assert_true(get_default_program_for_file("abc", &program));
	if(program.com != NULL)
		assert_string_equal("empty prog", program.com);
	free_assoc_prog(&program);
}

static void
test_match_full_line(void)
{
	assoc_prog_t program;

	set_programs("abc", "full prog", "description", 0);

	assert_false(get_default_program_for_file("abcd", &program));
	assert_false(get_default_program_for_file("0abc", &program));
	assert_false(get_default_program_for_file("0abcd", &program));

	assert_true(get_default_program_for_file("abc", &program));
	if(program.com != NULL)
		assert_string_equal("full prog", program.com);
	free_assoc_prog(&program);
}

static void
test_match_qmark(void)
{
	assoc_prog_t program;

	set_programs("a?c", "full prog", "description", 0);

	assert_false(get_default_program_for_file("ac", &program));

	assert_true(get_default_program_for_file("abc", &program));
	if(program.com != NULL)
		assert_string_equal("full prog", program.com);
	free_assoc_prog(&program);
}

static void
test_qmark_escaping(void)
{
	assoc_prog_t program;

	set_programs("a\\?c", "qmark prog", "description", 0);

	assert_false(get_default_program_for_file("abc", &program));

	assert_true(get_default_program_for_file("a?c", &program));
	if(program.com != NULL)
		assert_string_equal("qmark prog", program.com);
	free_assoc_prog(&program);
}

static void
test_star_escaping(void)
{
	assoc_prog_t program;

	set_programs("a\\*c", "star prog", "description", 0);

	assert_false(get_default_program_for_file("abc", &program));

	assert_true(get_default_program_for_file("a*c", &program));
	if(program.com != NULL)
		assert_string_equal("star prog", program.com);
	free_assoc_prog(&program);
}

static void
test_classes(void)
{
	assoc_prog_t program;

	set_programs("*.[ch]", "c file", "description", 0);

	assert_false(get_default_program_for_file("main.cpp", &program));
	assert_false(get_default_program_for_file("main.hpp", &program));

	assert_true(get_default_program_for_file("main.c", &program));
	if(program.com != NULL)
		assert_string_equal("c file", program.com);
	free_assoc_prog(&program);

	assert_true(get_default_program_for_file("main.h", &program));
	if(program.com != NULL)
		assert_string_equal("c file", program.com);
	free_assoc_prog(&program);
}

static void
test_classes_negotiation_with_emark(void)
{
	assoc_prog_t program;

	set_programs("*.[!ch]", "not c file", "description", 0);

	assert_false(get_default_program_for_file("main.c", &program));
	assert_false(get_default_program_for_file("main.h", &program));

	assert_true(get_default_program_for_file("main.o", &program));
	if(program.com != NULL)
		assert_string_equal("not c file", program.com);
	free_assoc_prog(&program);
}

static void
test_classes_negotiation_with_hat(void)
{
	assoc_prog_t program;

	set_programs("*.[^ch]", "not c file", "description", 0);

	assert_false(get_default_program_for_file("main.c", &program));
	assert_false(get_default_program_for_file("main.h", &program));

	assert_true(get_default_program_for_file("main.o", &program));
	if(program.com != NULL)
		assert_string_equal("not c file", program.com);
	free_assoc_prog(&program);
}

static void
test_classes_ranges(void)
{
	assoc_prog_t program;

	set_programs("*.[0-9]", "part file", "description", 0);

	assert_false(get_default_program_for_file("main.c", &program));
	assert_false(get_default_program_for_file("main.A", &program));

	assert_true(get_default_program_for_file("main.0", &program));
	if(program.com != NULL)
		assert_string_equal("part file", program.com);
	free_assoc_prog(&program);

	assert_true(get_default_program_for_file("main.8", &program));
	if(program.com != NULL)
		assert_string_equal("part file", program.com);
	free_assoc_prog(&program);
}

static void
test_xfiletypes1_c(void)
{
	assoc_prog_t program;

	curr_stats.is_console = 1;
	set_programs("*.tar", "x prog", "description", 1);
	set_programs("*.tar", "console prog", "description", 0);

	assert_true(get_default_program_for_file("file.version.tar", &program));
	if(program.com != NULL)
		assert_string_equal("console prog", program.com);
	free_assoc_prog(&program);
}

static void
test_xfiletypes1_x(void)
{
	assoc_prog_t program;

	curr_stats.is_console = 0;
	set_programs("*.tar", "x prog", "description", 1);
	set_programs("*.tar", "console prog", "description", 0);

	assert_true(get_default_program_for_file("file.version.tar", &program));
	if(program.com != NULL)
		assert_string_equal("x prog", program.com);
	free_assoc_prog(&program);
}

static void
test_xfiletypes2_c(void)
{
	assoc_prog_t program;

	curr_stats.is_console = 1;
	set_programs("*.tgz", "2 x prog", "description", 1);

	assert_false(get_default_program_for_file("file.version.tgz", &program));
}

static void
test_xfiletypes2_x(void)
{
	assoc_prog_t program;

	curr_stats.is_console = 0;
	set_programs("*.tgz", "2 x prog", "description", 1);

	assert_true(get_default_program_for_file("file.version.tgz", &program));
	if(program.com != NULL)
		assert_string_equal("2 x prog", program.com);
	free_assoc_prog(&program);
}

static void
test_xfiletypes3_c(void)
{
	assoc_prog_t program;

	curr_stats.is_console = 0;
	set_programs("*.tar.bz2", "3 console prog", "description", 0);

	assert_true(get_default_program_for_file("file.version.tar.bz2", &program));
	if(program.com != NULL)
		assert_string_equal("3 console prog", program.com);
	free_assoc_prog(&program);
}

static void
test_xfiletypes3_x(void)
{
	assoc_prog_t program;

	curr_stats.is_console = 1;
	set_programs("*.tar.bz2", "3 console prog", "description", 0);

	assert_true(get_default_program_for_file("file.version.tar.bz2", &program));
	if(program.com != NULL)
		assert_string_equal("3 console prog", program.com);
	free_assoc_prog(&program);
}

static void
test_removing(void)
{
	assoc_prog_t program;

	curr_stats.is_console = 1;
	set_programs("*.tar.bz2", "3 console prog", "description", 0);
	set_programs("*.tgz", "3 console prog", "description", 0);

	assert_true(get_default_program_for_file("file.version.tar.bz2", &program));
	if(program.com != NULL)
		assert_string_equal("3 console prog", program.com);
	free_assoc_prog(&program);

	assert_true(get_default_program_for_file("file.version.tgz", &program));
	if(program.com != NULL)
		assert_string_equal("3 console prog", program.com);
	free_assoc_prog(&program);

	remove_filetypes("*.tar.bz2");

	assert_false(get_default_program_for_file("file.version.tar.bz2", &program));

	assert_true(get_default_program_for_file("file.version.tgz", &program));
	if(program.com != NULL)
		assert_string_equal("3 console prog", program.com);
	free_assoc_prog(&program);
}

static void
test_star_and_dot(void)
{
	assoc_prog_t program;

	curr_stats.is_console = 1;
	set_programs("*.doc", "libreoffice", "description", 0);

	assert_true(get_default_program_for_file("a.doc", &program));
	if(program.com != NULL)
		assert_string_equal("libreoffice", program.com);
	free_assoc_prog(&program);

	assert_false(get_default_program_for_file(".a.doc", &program));
	assert_false(get_default_program_for_file(".doc", &program));

	set_programs(".*.doc", "hlibreoffice", "description", 0);

	assert_true(get_default_program_for_file(".a.doc", &program));
	if(program.com != NULL)
		assert_string_equal("hlibreoffice", program.com);
	free_assoc_prog(&program);
}

void
filetype_tests(void)
{
	test_fixture_start();

	run_test(test_one_ext);
	run_test(test_many_ext);
	run_test(test_many_fileext);
	run_test(test_dont_match_hidden);
	run_test(test_match_empty);
	run_test(test_match_full_line);
	run_test(test_match_qmark);
	run_test(test_qmark_escaping);
	run_test(test_star_escaping);
	run_test(test_classes);
	run_test(test_classes_negotiation_with_emark);
	run_test(test_classes_negotiation_with_hat);
	run_test(test_classes_ranges);

	run_test(test_xfiletypes1_c);
	run_test(test_xfiletypes1_x);
	run_test(test_xfiletypes2_c);
	run_test(test_xfiletypes2_x);
	run_test(test_xfiletypes3_c);
	run_test(test_xfiletypes3_x);

	run_test(test_removing);

	run_test(test_star_and_dot);

	test_fixture_end();
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
