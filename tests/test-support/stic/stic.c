/* stic
 * Copyright (C) 2010 Keith Nicholas (as seatest project).
 * Copyright (C) 2015 xaizek.
 *
 * See LICENSE.txt for license text.
 */

#include "stic.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef WIN32

#include <windows.h>

int stic_is_string_equal_i(const char* s1, const char* s2)
{
#ifdef _MSC_VER
	#pragma warning(disable: 4996)
#endif
	return stricmp(s1, s2) == 0;
}

#else

#include <sys/time.h>
#include <strings.h>

static unsigned int GetTickCount()
{
    enum {
        MS_PER_SEC = 1000,
        NS_PER_MS  = 1000,
    };

    struct timeval t;
    gettimeofday(&t, 0);
    return t.tv_sec*MS_PER_SEC + t.tv_usec/NS_PER_MS;
}

int stic_is_string_equal_i(const char* s1, const char* s2)
{
	return strcasecmp(s1, s2) == 0;
}

#endif

#ifdef STIC_INTERNAL_TESTS
static int stic_test_last_passed = 0;
#endif

typedef enum
{
	STIC_DISPLAY_TESTS,
	STIC_RUN_TESTS,
	STIC_DO_NOTHING,
	STIC_DO_ABORT
} stic_action_t;

typedef struct
{
	int argc;
	char** argv;
	stic_action_t action;
} stic_testrunner_t;

static int stic_screen_width = 70;
static int stic_tests_run = 0;
static int stic_tests_skipped = 0;
static int stic_tests_failed = 0;
static int stic_checks_passed = 0;
static int stic_checks_failed = 0;
static int stic_display_only = 0;
static int stic_verbose = 0;
static int stic_random_failures = 0;
static int stic_silent = 0;
static int stic_machine_readable = 0;
static const char *stic_current_fixture;
static const char *stic_current_fixture_path;
static char stic_magic_marker[20] = "";

static stic_void_void stic_suite_setup_func = 0;
static stic_void_void stic_suite_teardown_func = 0;
static stic_void_void stic_fixture_setup = 0;
static stic_void_void stic_fixture_teardown = 0;

const char *stic_current_test_name;
stic_void_void stic_current_test;
const char *stic_suite_name;

void (*stic_simple_test_result)(int passed, char* reason, const char* function, const char file[], unsigned int line) = stic_simple_test_result_log;

void suite_setup(stic_void_void setup)
{
	stic_suite_setup_func = setup;
}
void suite_teardown(stic_void_void teardown)
{
	stic_suite_teardown_func = teardown;
}

int stic_is_display_only()
{
	return stic_display_only;
}

static void stic_header_printer(const char s[], int length, char f)
{
	int l = strlen(s);
	int d = (length- (l + 2)) / 2;
	int i;
	if(stic_is_display_only() || stic_machine_readable) return;
	for(i = 0; i<d; i++) printf("%c",f);
	if(l==0) printf("%c%c", f, f);
	else printf(" %s ", s);
	for(i = (d+l+2); i<length; i++) printf("%c",f);
	printf("\n");
}

void stic_suite_setup( void )
{
	if(stic_suite_setup_func != 0) stic_suite_setup_func();
}

void stic_skip_test(const char fixture[], const char test[])
{
	stic_tests_skipped++;
}

int stic_positive_predicate( void )
{
    return 1;
}

void stic_printf(char buf[], const char format[], ...)
{
    va_list ap;
    va_start(ap, format);
    vsprintf(buf, format, ap);
    va_end(ap);
}

void stic_suite_teardown( void )
{
	if(stic_suite_teardown_func != 0) stic_suite_teardown_func();
}

void fixture_setup(void (*setup)( void ))
{
	stic_current_test_name = "<setup>";
	stic_fixture_setup = setup;
}
void fixture_teardown(void (*teardown)( void ))
{
	stic_current_test_name = "<teardown>";
	stic_fixture_teardown = teardown;
}

void stic_setup( void )
{
	if(stic_fixture_setup != 0) stic_fixture_setup();
}

void stic_teardown( void )
{
	if(stic_fixture_teardown != 0) stic_fixture_teardown();
}

static const char * test_file_name(const char path[])
{
	const char * file = path + strlen(path);
	while(file != path && *file!= '\\' ) file--;
	if(*file == '\\') file++;
	return file;
}

static int stic_fixture_tests_run;
static int stic_fixture_checks_failed;
static int stic_fixture_checks_passed;

static int test_had_output(void)
{
	const int nfailed = stic_checks_failed - stic_fixture_checks_failed;
	const int npassed = stic_checks_passed - stic_fixture_checks_passed;
	return (nfailed != 0 || (npassed != 0 && stic_verbose));
}

void stic_simple_test_result_log(int passed, char* reason, const char* function, const char file[], unsigned int line)
{
	static stic_void_void last_test;
	static stic_void_void last_failed_test;

	const char *test_name = (stic_current_test == last_test) ? "" : stic_current_test_name;

	if (stic_current_test_name != NULL && strcmp(function, stic_current_test_name) == 0)
	{
		function = "test body";
	}

	if (stic_random_failures && rand() % 8 == 0)
	{
		passed = !passed;
	}

	if (stic_silent && !test_had_output() && (!passed || stic_verbose))
	{
		stic_header_printer(stic_current_fixture, stic_screen_width, '-');
	}

	if (!passed)
	{
		if(stic_machine_readable)
		{
			printf("%s%s,%s,%u,%s\n", stic_magic_marker, stic_current_fixture_path, stic_current_test_name, line, reason);
		}
		else
		{
			if(stic_current_test != last_test)
			{
				printf("\n%s:\n", test_name);
			}
			printf("   (-) FAILED\n"
			       "       %s:%u: error: check failed\n"
			       "       in %s\n"
			       "       %s\n",
			       file, line, function, reason );
		}
		stic_checks_failed++;

		if (last_failed_test != stic_current_test)
		{
			++stic_tests_failed;
		}
		last_failed_test = stic_current_test;

		last_test = stic_current_test;
	}
	else
	{
		if(stic_verbose)
		{
			if(stic_machine_readable)
			{
				printf("%s%s,%s,%u,Passed\n", stic_magic_marker, stic_current_fixture_path, stic_current_test_name, line);
			}
			else
			{
				if(stic_current_test != last_test)
				{
					printf("\n%s\n", test_name);
				}
				printf("   (+) passed\n"
				       "       %s:%u\n"
				       "       in %s\n", file, line, function);
			}
			last_test = stic_current_test;
		}
		stic_checks_passed++;
	}
}

void stic_assert_true(int test, const char* function, const char file[], unsigned int line)
{
	stic_simple_test_result(test, "Should have been true", function, file, line);
}

void stic_assert_false(int test, const char* function, const char file[], unsigned int line)
{
	stic_simple_test_result(!test, "Should have been false", function, file, line);
}

void stic_assert_success(int test, const char function[], const char file[], unsigned int line)
{
	stic_simple_test_result(test == 0, "Should have been success (zero)", function, file, line);
}

void stic_assert_failure(int test, const char function[], const char file[], unsigned int line)
{
	stic_simple_test_result(test != 0, "Should have been failure (non-zero)", function, file, line);
}

void stic_assert_null(const void *value, const char function[], const char file[], unsigned int line)
{
	stic_simple_test_result(value == NULL, "Should have been NULL", function, file, line);
}

void stic_assert_non_null(const void *value, const char function[], const char file[], unsigned int line)
{
	stic_simple_test_result(value != NULL, "Should have been non-NULL", function, file, line);
}

void stic_assert_int_equal(int expected, int actual, const char* function, const char file[], unsigned int line)
{
	char s[STIC_PRINT_BUFFER_SIZE];
	sprintf(s, "Expected %d but was %d", expected, actual);
	stic_simple_test_result(expected==actual, s, function, file, line);
}

void stic_assert_ulong_equal(unsigned long expected, unsigned long actual, const char* function, const char file[], unsigned int line)
{
	char s[STIC_PRINT_BUFFER_SIZE];
	sprintf(s, "Expected %lu but was %lu", expected, actual);
	stic_simple_test_result(expected==actual, s, function, file, line);
}

void stic_assert_float_equal( float expected, float actual, float delta, const char* function, const char file[], unsigned int line )
{
	char s[STIC_PRINT_BUFFER_SIZE];
	float result = expected-actual;
	sprintf(s, "Expected %f but was %f", expected, actual);
	if(result < 0.0) result = 0.0f - result;
	stic_simple_test_result( result <= delta, s, function, file, line);
}

void stic_assert_double_equal( double expected, double actual, double delta, const char* function, const char file[], unsigned int line )
{
	char s[STIC_PRINT_BUFFER_SIZE];
	double result = expected-actual;
	sprintf(s, "Expected %f but was %f", expected, actual);
	if(result < 0.0) result = 0.0 - result;
	stic_simple_test_result( result <= delta, s, function, file, line);
}

void stic_assert_string_equal(const char* expected, const char* actual, const char* function, const char file[], unsigned int line)
{
	int comparison;
	char s[STIC_PRINT_BUFFER_SIZE];

	if ((expected == (char *)0) && (actual == (char *)0))
	{
          sprintf(s, "Expected <NULL> but was <NULL>");
	  comparison = 1;
	}
        else if (expected == (char *)0)
	{
	  sprintf(s, "Expected <NULL> but was \"%s\"", actual);
	  comparison = 0;
	}
        else if (actual == (char *)0)
	{
	  sprintf(s, "Expected \"%s\" but was <NULL>", expected);
	  comparison = 0;
	}
	else
	{
	  comparison = strcmp(expected, actual) == 0;
	  sprintf(s, "Expected \"%s\" but was \"%s\"", expected, actual);
	}

	stic_simple_test_result(comparison, s, function, file, line);
}

void stic_assert_wstring_equal(const wchar_t expected[], const wchar_t actual[], const char function[], const char file[], unsigned int line)
{
	int comparison;
	char s[STIC_PRINT_BUFFER_SIZE];

	if ((expected == NULL) && (actual == NULL))
	{
		sprintf(s, "Expected <NULL> but was <NULL>");
		comparison = 1;
	}
	else if (expected == NULL)
	{
#ifdef STIC_C99
		sprintf(s, "Expected <NULL> but was \"%ls\"", actual);
#else
		sprintf(s, "Expected <NULL> but was wide string");
#endif
		comparison = 0;
	}
	else if (actual == NULL)
	{
#ifdef STIC_C99
		sprintf(s, "Expected \"%ls\" but was <NULL>", expected);
#else
		sprintf(s, "Expected wide string but was <NULL>");
#endif
		comparison = 0;
	}
	else
	{
		comparison = wcscmp(expected, actual) == 0;
#ifdef STIC_C99
		sprintf(s, "Expected \"%ls\" but was \"%ls\"", expected, actual);
#else
		sprintf(s, "Expected wide string doesn't match");
#endif
	}

	stic_simple_test_result(comparison, s, function, file, line);
}

void stic_assert_string_ends_with(const char* expected, const char* actual, const char* function, const char file[], unsigned int line)
{
	char s[STIC_PRINT_BUFFER_SIZE];
	sprintf(s, "Expected \"%s\" to end with \"%s\"", actual, expected);
	stic_simple_test_result(strcmp(expected, actual+(strlen(actual)-strlen(expected)))==0, s, function, file, line);
}

void stic_assert_string_starts_with(const char* expected, const char* actual, const char* function, const char file[], unsigned int line)
{
	char s[STIC_PRINT_BUFFER_SIZE];
	sprintf(s, "Expected \"%s\" to start with \"%s\"", actual, expected);
	stic_simple_test_result(strncmp(expected, actual, strlen(expected))==0, s, function, file, line);
}

void stic_assert_string_contains(const char* expected, const char* actual, const char* function, const char file[], unsigned int line)
{
	char s[STIC_PRINT_BUFFER_SIZE];
	sprintf(s, "Expected \"%s\" to be in \"%s\"", expected, actual);
	stic_simple_test_result(strstr(actual, expected)!=0, s, function, file, line);
}

void stic_assert_string_doesnt_contain(const char* expected, const char* actual, const char* function, const char file[], unsigned int line)
{
	char s[STIC_PRINT_BUFFER_SIZE];
	sprintf(s, "Expected \"%s\" not to have \"%s\" in it", actual, expected);
	stic_simple_test_result(strstr(actual, expected)==0, s, function, file, line);
}

void stic_run_test(const char fixture[], const char test[])
{
	stic_tests_run++;
}

void stic_test_fixture_start(const char filepath[])
{
	stic_current_fixture_path = filepath;
	stic_current_fixture = test_file_name(filepath);
	stic_fixture_checks_failed = stic_checks_failed;
	stic_fixture_checks_passed = stic_checks_passed;
	stic_fixture_tests_run = stic_tests_run;
	stic_fixture_teardown = 0;
	stic_fixture_setup = 0;

	if (!stic_silent)
	{
		stic_header_printer(stic_current_fixture, stic_screen_width, '-');
	}
}

void stic_test_fixture_end()
{
	char s[STIC_PRINT_BUFFER_SIZE];
	const int nrun = stic_tests_run - stic_fixture_tests_run;
	const int nfailed = stic_checks_failed - stic_fixture_checks_failed;

	if (stic_silent)
	{
		if (stic_verbose)
		{
			if(!test_had_output())
			{
				stic_header_printer(stic_current_fixture, stic_screen_width, '-');
			}
		}
		else if(!test_had_output())
		{
			return;
		}
		printf("\n");
	}
	else if(test_had_output())
	{
		printf("\n");
	}

	sprintf(s, "%d test%s run  %d check%s failed", nrun, nrun == 1 ? "" : "s",
			nfailed, nfailed == 1 ? "" : "s");

	stic_header_printer(s, stic_screen_width, ' ');
	printf("\n");
}

static char* stic_fixture_filter = 0;
static char* stic_test_filter = 0;

void fixture_filter(char* filter)
{
	stic_fixture_filter = filter;
}

void test_filter(char* filter)
{
	stic_test_filter = filter;
}

void set_magic_marker(char* marker)
{
	if(marker == NULL) return;
	strcpy(stic_magic_marker, marker);
}

static void stic_display_test(const char fixture_name[], const char test_name[])
{
	if(test_name == NULL) return;
	printf("%s,%s\n", fixture_name, test_name);
}

int stic_should_run(const char fixture[], const char test[])
{
	int run = 1;
	if(stic_fixture_filter)
	{
		if(strncmp(stic_fixture_filter, fixture, strlen(stic_fixture_filter)) != 0) run = 0;
	}
	if(stic_test_filter && test != NULL)
	{
		if(strncmp(stic_test_filter, test, strlen(stic_test_filter)) != 0) run = 0;
	}

	if(run && stic_display_only)
	{
		stic_display_test(fixture, test);
		run = 0;
	}
	return run;
}

int run_tests(stic_void_void tests)
{
	unsigned long end;
	unsigned long start = GetTickCount();
	char version[40];
	char s[100];
	char time[40];
	tests();
	end = GetTickCount();

	if(stic_suite_name == NULL)
	{
		stic_suite_name = "";
	}

	if(stic_is_display_only() || stic_machine_readable) return 1;
	snprintf(version, sizeof(version), "stic v%s%s%s", STIC_VERSION,
			(stic_suite_name[0] == '\0' ? "" : " :: "), stic_suite_name);
	printf("\n");
	stic_header_printer(version, stic_screen_width, '=');
	printf("\n");
	if (stic_checks_failed > 0) {
		snprintf(s, sizeof(s), "%d CHECK%s IN %d TEST%s FAILED",
				 stic_checks_failed, stic_checks_failed == 1 ? "" : "S",
				 stic_tests_failed, stic_tests_failed == 1 ? "" : "S");
		stic_header_printer(s, stic_screen_width, ' ');
	}
	else
	{
		snprintf(s, sizeof(s), "ALL TESTS PASSED");
		stic_header_printer(s, stic_screen_width, ' ');
	}

	memset(s, '-', strlen(s));
	stic_header_printer(s, stic_screen_width, ' ');

	if (end - start == 0)
	{
		sprintf(time,"< 1 ms");
	}
	else
	{
		sprintf(time,"%lu ms",end - start);
	}

	sprintf(s,"%d check%s :: %d run test%s :: %d skipped test%s :: %s",
			stic_checks_passed + stic_checks_failed,
			stic_checks_passed + stic_checks_failed == 1 ? "" : "s",
			stic_tests_run, stic_tests_run == 1 ? "" : "s",
			stic_tests_skipped, stic_tests_skipped == 1 ? "" : "s",
			time);
	stic_header_printer(s, stic_screen_width, ' ');

	printf("\n");
	stic_header_printer("", stic_screen_width, '=');

	return stic_checks_failed == 0;
}

void stic_show_help( void )
{
	printf("Usage: [-t <testname>] [-f <fixturename>] [-d] [help] [-v] [-m] [-k <marker>\n");
	printf("Flags:\n");
	printf("\thelp:\twill display this help\n");
	printf("\t-t:\twill only run tests that match <testname>\n");
	printf("\t-f:\twill only run fixtures that match <fixturename>\n");
	printf("\t-d:\twill just display test names and fixtures without\n");
	printf("\t-d:\trunning the test\n");
	printf("\t-r:\tproduce random failures\n");
	printf("\t-s:\tdo not display fixtures unless they contain failures\n");
	printf("\t-v:\twill print a more verbose version of the test run\n");
	printf("\t-m:\twill print a machine readable format of the test run, ie :- \n");
	printf("\t   \t<textfixture>,<testname>,<linenumber>,<testresult><EOL>\n");
	printf("\t-k:\twill prepend <marker> before machine readable output \n");
	printf("\t   \t<marker> cannot start with a '-'\n");
}

int stic_commandline_has_value_after(stic_testrunner_t* runner, int arg)
{
	if(!((arg+1) < runner->argc)) return 0;
	if(runner->argv[arg+1][0]=='-') return 0;
	return 1;
}

int stic_parse_commandline_option_with_value(stic_testrunner_t* runner, int arg, char* option, stic_void_string setter)
{
	if(stic_is_string_equal_i(runner->argv[arg], option))
	{
		if(!stic_commandline_has_value_after(runner, arg))
		{
			printf("Error: The %s option expects to be followed by a value\n", option);
			runner->action = STIC_DO_ABORT;
			return 0;
		}
		setter(runner->argv[arg+1]);
		return 1;
	}
	return 0;
}

void stic_interpret_commandline(stic_testrunner_t* runner)
{
	int arg;
	for(arg=0; (arg < runner->argc) && (runner->action != STIC_DO_ABORT); arg++)
	{
		if(stic_is_string_equal_i(runner->argv[arg], "help"))
		{
			stic_show_help();
			runner->action = STIC_DO_NOTHING;
			return;
		}
		if(stic_is_string_equal_i(runner->argv[arg], "-d")) runner->action = STIC_DISPLAY_TESTS;
		if(stic_is_string_equal_i(runner->argv[arg], "-r")) stic_random_failures = 1;
		if(stic_is_string_equal_i(runner->argv[arg], "-s")) stic_silent = 1;
		if(stic_is_string_equal_i(runner->argv[arg], "-v")) stic_verbose = 1;
		if(stic_is_string_equal_i(runner->argv[arg], "-m")) stic_machine_readable = 1;
		if(stic_parse_commandline_option_with_value(runner,arg,"-t", test_filter)) arg++;
		if(stic_parse_commandline_option_with_value(runner,arg,"-f", fixture_filter)) arg++;
		if(stic_parse_commandline_option_with_value(runner,arg,"-k", set_magic_marker)) arg++;
	}
}

void stic_testrunner_create(stic_testrunner_t* runner, int argc, char** argv )
{
	runner->action = STIC_RUN_TESTS;
	runner->argc = argc;
	runner->argv = argv;
	stic_interpret_commandline(runner);
}

int stic_testrunner(int argc, char** argv, stic_void_void tests, stic_void_void setup, stic_void_void teardown)
{
	stic_testrunner_t runner;
	stic_testrunner_create(&runner, argc, argv);
	switch(runner.action)
	{
	case STIC_DISPLAY_TESTS:
		{
			stic_display_only = 1;
			run_tests(tests);
			break;
		}
	case STIC_RUN_TESTS:
		{
			suite_setup(setup);
			suite_teardown(teardown);
			return run_tests(tests);
		}
	case STIC_DO_NOTHING:
	case STIC_DO_ABORT:
	default:
		{
			/* nothing to do, probably because there was an error which should of been already printed out. */
		}
	}
	return 1;
}

#ifdef STIC_INTERNAL_TESTS

void stic_simple_test_result_nolog(int passed, char* reason, const char* function, const char file[], unsigned int line)
{
	stic_test_last_passed = passed;
}

void stic_assert_last_passed()
{
	assert_int_equal(1, stic_test_last_passed);
}

void stic_assert_last_failed()
{
	assert_int_equal(0, stic_test_last_passed);
}

void stic_disable_logging()
{
	stic_simple_test_result = stic_simple_test_result_nolog;
}

void stic_enable_logging()
{
	stic_simple_test_result = stic_simple_test_result_log;
}

#endif
