#include "seatest.h"
#include <string.h>
#ifdef WIN32
#include "windows.h"
int seatest_is_string_equal_i(const char* s1, const char* s2)
{
	#pragma warning(disable: 4996)
	return stricmp(s1, s2) == 0;
}

#else
#include <strings.h>
unsigned int GetTickCount() { return 0;}
void _getch( void ) { }
int seatest_is_string_equal_i(const char* s1, const char* s2)
{
	return strcasecmp(s1, s2) == 0;
}
#endif

#ifdef SEATEST_INTERNAL_TESTS
static int sea_test_last_passed = 0;
#endif


typedef enum
{
	SEATEST_DISPLAY_TESTS,
	SEATEST_RUN_TESTS,
	SEATEST_DO_NOTHING,
	SEATEST_DO_ABORT
} seatest_action_t;

typedef struct
{
	int argc;
	char** argv;
	seatest_action_t action;
} seatest_testrunner_t;
static int seatest_screen_width = 70;
static int sea_tests_run = 0;
static int sea_tests_passed = 0;
static int sea_tests_failed = 0;
static int seatest_display_only = 0;
static int seatest_verbose = 0;
static int seatest_machine_readable = 0;
static char* seatest_current_fixture;
static char* seatest_current_fixture_path;
static char seatest_magic_marker[20] = "";

static seatest_void_void seatest_suite_setup_func = 0;
static seatest_void_void seatest_suite_teardown_func = 0;
static seatest_void_void seatest_fixture_setup = 0;
static seatest_void_void seatest_fixture_teardown = 0;

void (*seatest_simple_test_result)(int passed, char* reason, const char* function, unsigned int line) = seatest_simple_test_result_log;

void suite_setup(seatest_void_void setup)
{
	seatest_suite_setup_func = setup;
}
void suite_teardown(seatest_void_void teardown)
{
	seatest_suite_teardown_func = teardown;
}

int seatest_is_display_only()
{
	return seatest_display_only;
}

void seatest_suite_setup( void )
{
	if(seatest_suite_setup_func != 0) seatest_suite_setup_func();
}

void seatest_suite_teardown( void )
{
	if(seatest_suite_teardown_func != 0) seatest_suite_teardown_func();
}

void fixture_setup(void (*setup)( void ))
{
	seatest_fixture_setup = setup;
}
void fixture_teardown(void (*teardown)( void ))
{
	seatest_fixture_teardown = teardown;
}

void seatest_setup( void )
{
	if(seatest_fixture_setup != 0) seatest_fixture_setup();
}

void seatest_teardown( void )
{
	if(seatest_fixture_teardown != 0) seatest_fixture_teardown();
}

char* test_file_name(char* path)
{
	char* file = path + strlen(path);
	while(file != path && *file!= '\\' ) file--;
	if(*file == '\\') file++;
	return file;
}

static int seatest_fixture_tests_run;
static int seatest_fixture_tests_failed;

void seatest_simple_test_result_log(int passed, char* reason, const char* function, unsigned int line)
{
	if (!passed)
	{

		if(seatest_machine_readable)
		{
			printf("%s%s,%s,%u,%s\r\n", seatest_magic_marker, seatest_current_fixture_path, function, line, reason );
		}
		else
		{
			printf("%-30s Line %-5d %s\r\n", function, line, reason );
		}
		sea_tests_failed++;
	}
	else
	{
		if(seatest_verbose)
		{
			if(seatest_machine_readable)
			{
				printf("%s%s,%s,%u,Passed\r\n", seatest_magic_marker, seatest_current_fixture_path, function, line );
			}
			else
			{
				printf("%-30s Line %-5d Passed\r\n", function, line);
			}
		}
		sea_tests_passed++;
	}
}

void seatest_assert_true(int test, const char* function, unsigned int line)
{
	seatest_simple_test_result(test, "Should have been true", function, line);

}

void seatest_assert_false(int test, const char* function, unsigned int line)
{
	seatest_simple_test_result(!test, "Should have been false", function, line);
}


void seatest_assert_int_equal(int expected, int actual, const char* function, unsigned int line)
{
	char s[SEATEST_PRINT_BUFFER_SIZE];
	sprintf(s, "Expected %d but was %d", expected, actual);
	seatest_simple_test_result(expected==actual, s, function, line);
}

void seatest_assert_ulong_equal(unsigned long expected, unsigned long actual, const char* function, unsigned int line)
{
	char s[SEATEST_PRINT_BUFFER_SIZE];
	sprintf(s, "Expected %lu but was %lu", expected, actual);
	seatest_simple_test_result(expected==actual, s, function, line);
}

void seatest_assert_float_equal( float expected, float actual, float delta, const char* function, unsigned int line )
{
	char s[SEATEST_PRINT_BUFFER_SIZE];
	float result = expected-actual;
	sprintf(s, "Expected %f but was %f", expected, actual);
	if(result < 0.0) result = 0.0f - result;
	seatest_simple_test_result( result <= delta, s, function, line);
}

void seatest_assert_double_equal( double expected, double actual, double delta, const char* function, unsigned int line )
{
	char s[SEATEST_PRINT_BUFFER_SIZE];
	double result = expected-actual;
	sprintf(s, "Expected %f but was %f", expected, actual);
	if(result < 0.0) result = 0.0 - result;
	seatest_simple_test_result( result <= delta, s, function, line);
}

void seatest_assert_string_equal(const char* expected, const char* actual, const char* function, unsigned int line)
{
        int comparison;
	char s[SEATEST_PRINT_BUFFER_SIZE];

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

	seatest_simple_test_result(comparison, s, function, line);
}

void seatest_assert_string_ends_with(const char* expected, const char* actual, const char* function, unsigned int line)
{
	char s[SEATEST_PRINT_BUFFER_SIZE];
	sprintf(s, "Expected \"%s\" to end with \"%s\"", actual, expected);
	seatest_simple_test_result(strcmp(expected, actual+(strlen(actual)-strlen(expected)))==0, s, function, line);
}

void seatest_assert_string_starts_with(const char* expected, const char* actual, const char* function, unsigned int line)
{
	char s[SEATEST_PRINT_BUFFER_SIZE];
	sprintf(s, "Expected \"%s\" to start with \"%s\"", actual, expected);
	seatest_simple_test_result(strncmp(expected, actual, strlen(expected))==0, s, function, line);
}

void seatest_assert_string_contains(const char* expected, const char* actual, const char* function, unsigned int line)
{
	char s[SEATEST_PRINT_BUFFER_SIZE];
	sprintf(s, "Expected \"%s\" to be in \"%s\"", expected, actual);
	seatest_simple_test_result(strstr(actual, expected)!=0, s, function, line);
}

void seatest_assert_string_doesnt_contain(const char* expected, const char* actual, const char* function, unsigned int line)
{
	char s[SEATEST_PRINT_BUFFER_SIZE];
	sprintf(s, "Expected \"%s\" not to have \"%s\" in it", actual, expected);
	seatest_simple_test_result(strstr(actual, expected)==0, s, function, line);
}

void seatest_run_test(char* fixture, char* test)
{
	sea_tests_run++;
}

void seatest_header_printer(char* s, int length, char f)
{
	int l = strlen(s);
	int d = (length- (l + 2)) / 2;
	int i;
	if(seatest_is_display_only() || seatest_machine_readable) return;
	for(i = 0; i<d; i++) printf("%c",f);
	if(l==0) printf("%c%c", f, f);
	else printf(" %s ", s);
	for(i = (d+l+2); i<length; i++) printf("%c",f);
	printf("\r\n");
}


void seatest_test_fixture_start(char* filepath)
{
	seatest_current_fixture_path = filepath;
	seatest_current_fixture = test_file_name(filepath);
	seatest_header_printer(seatest_current_fixture, seatest_screen_width, '-');
	seatest_fixture_tests_failed = sea_tests_failed;
	seatest_fixture_tests_run = sea_tests_run;
	seatest_fixture_teardown = 0;
	seatest_fixture_setup = 0;
}

void seatest_test_fixture_end()
{
	char s[SEATEST_PRINT_BUFFER_SIZE];
	sprintf(s, "%d run  %d failed", sea_tests_run-seatest_fixture_tests_run, sea_tests_failed-seatest_fixture_tests_failed);
	seatest_header_printer(s, seatest_screen_width, ' ');
	printf("\r\n");
}

static char* seatest_fixture_filter = 0;
static char* seatest_test_filter = 0;

void fixture_filter(char* filter)
{
	seatest_fixture_filter = filter;
}


void test_filter(char* filter)
{
	seatest_test_filter = filter;
}

void set_magic_marker(char* marker)
{
	if(marker == NULL) return;
	strcpy(seatest_magic_marker, marker);
}

void seatest_display_test(char* fixture_name, char* test_name)
{
	if(test_name == NULL) return;
	printf("%s,%s\r\n", fixture_name, test_name);
}

int seatest_should_run( char* fixture, char* test)
{
	int run = 1;
	if(seatest_fixture_filter)
	{
		if(strncmp(seatest_fixture_filter, fixture, strlen(seatest_fixture_filter)) != 0) run = 0;
	}
	if(seatest_test_filter && test != NULL)
	{
		if(strncmp(seatest_test_filter, test, strlen(seatest_test_filter)) != 0) run = 0;
	}

	if(run && seatest_display_only)
	{
		seatest_display_test(fixture, test);
		run = 0;
	}
	return run;
}

int run_tests(seatest_void_void tests)
{
	unsigned long end;
	unsigned long start = GetTickCount();
	char version[40];
	char s[40];
	tests();
	end = GetTickCount();

	if(seatest_is_display_only() || seatest_machine_readable) return 1;
	sprintf(version, "SEATEST v%s", SEATEST_VERSION);
	printf("\r\n\r\n");
	seatest_header_printer(version, seatest_screen_width, '=');
	printf("\r\n");
	if (sea_tests_failed > 0) {
		seatest_header_printer("Failed", seatest_screen_width, ' ');
	}
	else {
		seatest_header_printer("ALL TESTS PASSED", seatest_screen_width, ' ');
	}
	sprintf(s,"%d tests run", sea_tests_run);
	seatest_header_printer(s, seatest_screen_width, ' ');
	sprintf(s,"in %lu ms",end - start);
	seatest_header_printer(s, seatest_screen_width, ' ');
	printf("\r\n");
	seatest_header_printer("", seatest_screen_width, '=');

	return sea_tests_failed == 0;
}


void seatest_show_help( void )
{
	printf("Usage: [-t <testname>] [-f <fixturename>] [-d] [help] [-v] [-m] [-k <marker>\r\n");
	printf("Flags:\r\n");
	printf("\thelp:\twill display this help\r\n");
	printf("\t-t:\twill only run tests that match <testname>\r\n");
	printf("\t-f:\twill only run fixtures that match <fixturename>\r\n");
	printf("\t-d:\twill just display test names and fixtures without\r\n");
	printf("\t-d:\trunning the test\r\n");
	printf("\t-v:\twill print a more verbose version of the test run\r\n");
	printf("\t-m:\twill print a machine readable format of the test run, ie :- \r\n");
	printf("\t   \t<textfixture>,<testname>,<linenumber>,<testresult><EOL>\r\n");
	printf("\t-k:\twill prepend <marker> before machine readable output \r\n");
	printf("\t   \t<marker> cannot start with a '-'\r\n");
}


int seatest_commandline_has_value_after(seatest_testrunner_t* runner, int arg)
{
	if(!((arg+1) < runner->argc)) return 0;
	if(runner->argv[arg+1][0]=='-') return 0;
	return 1;
}

int seatest_parse_commandline_option_with_value(seatest_testrunner_t* runner, int arg, char* option, seatest_void_string setter)
{
	if(seatest_is_string_equal_i(runner->argv[arg], option))
	{
		if(!seatest_commandline_has_value_after(runner, arg))
		{
			printf("Error: The %s option expects to be followed by a value\r\n", option);
			runner->action = SEATEST_DO_ABORT;
			return 0;
		}
		setter(runner->argv[arg+1]);
		return 1;
	}
	return 0;
}

void seatest_interpret_commandline(seatest_testrunner_t* runner)
{
	int arg;
	for(arg=0; (arg < runner->argc) && (runner->action != SEATEST_DO_ABORT); arg++)
	{
		if(seatest_is_string_equal_i(runner->argv[arg], "help"))
		{
			seatest_show_help();
			runner->action = SEATEST_DO_NOTHING;
			return;
		}
		if(seatest_is_string_equal_i(runner->argv[arg], "-d")) runner->action = SEATEST_DISPLAY_TESTS;
		if(seatest_is_string_equal_i(runner->argv[arg], "-v")) seatest_verbose = 1;
		if(seatest_is_string_equal_i(runner->argv[arg], "-m")) seatest_machine_readable = 1;
		if(seatest_parse_commandline_option_with_value(runner,arg,"-t", test_filter)) arg++;
		if(seatest_parse_commandline_option_with_value(runner,arg,"-f", fixture_filter)) arg++;
		if(seatest_parse_commandline_option_with_value(runner,arg,"-k", set_magic_marker)) arg++;
	}
}

void seatest_testrunner_create(seatest_testrunner_t* runner, int argc, char** argv )
{
	runner->action = SEATEST_RUN_TESTS;
	runner->argc = argc;
	runner->argv = argv;
	seatest_interpret_commandline(runner);
}

int seatest_testrunner(int argc, char** argv, seatest_void_void tests, seatest_void_void setup, seatest_void_void teardown)
{
	seatest_testrunner_t runner;
	seatest_testrunner_create(&runner, argc, argv);
	switch(runner.action)
	{
	case SEATEST_DISPLAY_TESTS:
		{
			seatest_display_only = 1;
			run_tests(tests);
			break;
		}
	case SEATEST_RUN_TESTS:
		{
			suite_setup(setup);
			suite_teardown(teardown);
			return run_tests(tests);
		}
	case SEATEST_DO_NOTHING:
	case SEATEST_DO_ABORT:
	default:
		{
			/* nothing to do, probably because there was an error which should of been already printed out. */
		}
	}
	return 1;
}

#ifdef SEATEST_INTERNAL_TESTS
void seatest_simple_test_result_nolog(int passed, char* reason, const char* function, unsigned int line)
{
  sea_test_last_passed = passed;
}

void seatest_assert_last_passed()
{
  assert_int_equal(1, sea_test_last_passed);
}

void seatest_assert_last_failed()
{
  assert_int_equal(0, sea_test_last_passed);
}

void seatest_disable_logging()
{
  seatest_simple_test_result = seatest_simple_test_result_nolog;
}

void seatest_enable_logging()
{
  seatest_simple_test_result = seatest_simple_test_result_log;
}
#endif
