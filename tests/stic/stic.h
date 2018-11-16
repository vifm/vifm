/* stic
 * Copyright (C) 2010 Keith Nicholas (as seatest project).
 * Copyright (C) 2015 xaizek.
 *
 * See LICENSE.txt for license text.
 */

#ifndef STIC__STIC_H__
#define STIC__STIC_H__

#include <stddef.h>

/* Global data. */

#define STIC_VERSION "0.6"
#define STIC_PROJECT_HOME "https://github.com/xaizek/stic"
#define STIC_PRINT_BUFFER_SIZE 100000

/* Widely used function types. */

typedef void (*stic_void_void)(void);
typedef void (*stic_void_string)(char[]);

/* Declarations. */

void (*stic_simple_test_result)(int passed, char* reason, const char* function, const char file[], unsigned int line);
void stic_test_fixture_start(const char filepath[]);
void stic_test_fixture_end( void );
void stic_simple_test_result_log(int passed, char* reason, const char* function, const char file[], unsigned int line);
void stic_assert_true(int test, const char* function, const char file[], unsigned int line);
void stic_assert_false(int test, const char* function, const char file[], unsigned int line);
void stic_assert_success(int test, const char function[], const char file[], unsigned int line);
void stic_assert_failure(int test, const char function[], const char file[], unsigned int line);
void stic_assert_null(const void *value, const char function[], const char file[], unsigned int line);
void stic_assert_non_null(const void *value, const char function[], const char file[], unsigned int line);
void stic_assert_int_equal(int expected, int actual, const char* function, const char file[], unsigned int line);
void stic_assert_ulong_equal(unsigned long expected, unsigned long actual, const char* function, const char file[], unsigned int line);
void stic_assert_float_equal(float expected, float actual, float delta, const char* function, const char file[], unsigned int line);
void stic_assert_double_equal(double expected, double actual, double delta, const char* function, const char file[], unsigned int line);
void stic_assert_string_equal(const char* expected, const char* actual, const char* function, const char file[], unsigned int line);
void stic_assert_wstring_equal(const wchar_t expected[], const wchar_t actual[], const char function[], const char file[], unsigned int line);
void stic_assert_string_ends_with(const char* expected, const char* actual, const char* function, const char file[], unsigned int line);
void stic_assert_string_starts_with(const char* expected, const char* actual, const char* function, const char file[], unsigned int line);
void stic_assert_string_contains(const char* expected, const char* actual, const char* function, const char file[], unsigned int line);
void stic_assert_string_doesnt_contain(const char* expected, const char* actual, const char* function, const char file[], unsigned int line);
int  stic_should_run(const char fixture[], const char test[]);
void stic_before_run( char* fixture, char* test);
void stic_run_test(const char fixture[], const char test[]);
void stic_skip_test(const char fixture[], const char test[]);
void stic_setup( void );
void stic_teardown( void );
void stic_suite_teardown( void );
void stic_suite_setup( void );
int stic_positive_predicate( void );
void stic_printf(char buf[], const char format[], ...);

/* Assert macros. */

#define assert_true(test) do { stic_assert_true(test, __FUNCTION__, __FILE__, __LINE__); } while (0)
#define assert_false(test) do {  stic_assert_false(test, __FUNCTION__, __FILE__, __LINE__); } while (0)
#define assert_success(test) do {  stic_assert_success(test, __FUNCTION__, __FILE__, __LINE__); } while (0)
#define assert_failure(test) do {  stic_assert_failure(test, __FUNCTION__, __FILE__, __LINE__); } while (0)
#define assert_null(value) do {  stic_assert_null(value, __FUNCTION__, __FILE__, __LINE__); } while (0)
#define assert_non_null(value) do {  stic_assert_non_null(value, __FUNCTION__, __FILE__, __LINE__); } while (0)
#define assert_int_equal(expected, actual) do {  stic_assert_int_equal(expected, actual, __FUNCTION__, __FILE__, __LINE__); } while (0)
#define assert_ulong_equal(expected, actual) do {  stic_assert_ulong_equal(expected, actual, __FUNCTION__, __FILE__, __LINE__); } while (0)
#define assert_string_equal(expected, actual) do {  stic_assert_string_equal(expected, actual, __FUNCTION__, __FILE__, __LINE__); } while (0)
#define assert_wstring_equal(expected, actual) do {  stic_assert_wstring_equal(expected, actual, __FUNCTION__, __FILE__, __LINE__); } while (0)
#define assert_n_array_equal(expected, actual, n) do { int stic_count; for(stic_count=0; stic_count<n; stic_count++) { char s_seatest[STIC_PRINT_BUFFER_SIZE]; stic_printf(s_seatest,"Expected %d to be %d at position %d", actual[stic_count], expected[stic_count], stic_count); stic_simple_test_result((expected[stic_count] == actual[stic_count]), s_seatest, __FUNCTION__, __FILE__, __LINE__);} } while (0)
#define assert_bit_set(bit_number, value) { stic_simple_test_result(((1 << bit_number) & value), " Expected bit to be set" ,  __FUNCTION__, __FILE__, __LINE__); } while (0)
#define assert_bit_not_set(bit_number, value) { stic_simple_test_result(!((1 << bit_number) & value), " Expected bit not to to be set" ,  __FUNCTION__, __FILE__, __LINE__); } while (0)
#define assert_bit_mask_matches(value, mask) { stic_simple_test_result(((value & mask) == mask), " Expected all bits of mask to be set" ,  __FUNCTION__, __FILE__, __LINE__); } while (0)
#define assert_fail(message) { stic_simple_test_result(0, message,  __FUNCTION__, __FILE__, __LINE__); } while (0)
#define assert_float_equal(expected, actual, delta) do {  stic_assert_float_equal(expected, actual, delta, __FUNCTION__, __FILE__, __LINE__); } while (0)
#define assert_double_equal(expected, actual, delta) do {  stic_assert_double_equal(expected, actual, delta, __FUNCTION__, __FILE__, __LINE__); } while (0)
#define assert_string_contains(expected, actual) do {  stic_assert_string_contains(expected, actual, __FUNCTION__, __FILE__, __LINE__); } while (0)
#define assert_string_doesnt_contain(expected, actual) do {  stic_assert_string_doesnt_contain(expected, actual, __FUNCTION__, __FILE__, __LINE__); } while (0)
#define assert_string_starts_with(expected, actual) do {  stic_assert_string_starts_with(expected, actual, __FUNCTION__, __FILE__, __LINE__); } while (0)
#define assert_string_ends_with(expected, actual) do {  stic_assert_string_ends_with(expected, actual, __FUNCTION__, __FILE__, __LINE__); } while (0)

/* Fixture/test management. */

void fixture_setup(void (*setup)( void ));
void fixture_teardown(void (*teardown)( void ));
#define run_test(test) do { if(stic_should_run(__FILE__, #test)) {stic_suite_setup(); stic_setup(); test(); stic_teardown(); stic_suite_teardown(); stic_run_test(__FILE__, #test);  }} while (0)
#define test_fixture_start() do { stic_test_fixture_start(__FILE__); } while (0)
#define test_fixture_end() do { stic_test_fixture_end();} while (0)
void fixture_filter(char* filter);
void test_filter(char* filter);
void suite_teardown(stic_void_void teardown);
void suite_setup(stic_void_void setup);
int run_tests(stic_void_void tests);
int stic_testrunner(int argc, char** argv, stic_void_void tests, stic_void_void setup, stic_void_void teardown);

/* Names of variables for storing test descriptions. */

/* Number of the last line on which test may be defined. */
#define STIC_MAX_LINES 800

#define TEST_DATA_STRUCTS \
    l1,l2,l3,l4,l5,l6,l7,l8,l9,l10,l11,l12,l13,l14,l15,l16,l17,l18,l19,l20,l21,l22,l23,l24,l25,l26,l27,l28,l29,l30,l31,l32,l33,l34,l35,l36,l37,l38,l39,l40,l41,l42,l43,l44,l45,l46,l47,l48,l49,l50,\
    l51,l52,l53,l54,l55,l56,l57,l58,l59,l60,l61,l62,l63,l64,l65,l66,l67,l68,l69,l70,l71,l72,l73,l74,l75,l76,l77,l78,l79,l80,l81,l82,l83,l84,l85,l86,l87,l88,l89,l90,l91,l92,l93,l94,l95,l96,l97,l98,l99,l100,\
    l101,l102,l103,l104,l105,l106,l107,l108,l109,l110,l111,l112,l113,l114,l115,l116,l117,l118,l119,l120,l121,l122,l123,l124,l125,l126,l127,l128,l129,l130,l131,l132,l133,l134,l135,l136,l137,l138,l139,l140,l141,l142,l143,l144,l145,l146,l147,l148,l149,l150,\
    l151,l152,l153,l154,l155,l156,l157,l158,l159,l160,l161,l162,l163,l164,l165,l166,l167,l168,l169,l170,l171,l172,l173,l174,l175,l176,l177,l178,l179,l180,l181,l182,l183,l184,l185,l186,l187,l188,l189,l190,l191,l192,l193,l194,l195,l196,l197,l198,l199,l200,\
    l201,l202,l203,l204,l205,l206,l207,l208,l209,l210,l211,l212,l213,l214,l215,l216,l217,l218,l219,l220,l221,l222,l223,l224,l225,l226,l227,l228,l229,l230,l231,l232,l233,l234,l235,l236,l237,l238,l239,l240,l241,l242,l243,l244,l245,l246,l247,l248,l249,l250,\
    l251,l252,l253,l254,l255,l256,l257,l258,l259,l260,l261,l262,l263,l264,l265,l266,l267,l268,l269,l270,l271,l272,l273,l274,l275,l276,l277,l278,l279,l280,l281,l282,l283,l284,l285,l286,l287,l288,l289,l290,l291,l292,l293,l294,l295,l296,l297,l298,l299,l300,\
    l301,l302,l303,l304,l305,l306,l307,l308,l309,l310,l311,l312,l313,l314,l315,l316,l317,l318,l319,l320,l321,l322,l323,l324,l325,l326,l327,l328,l329,l330,l331,l332,l333,l334,l335,l336,l337,l338,l339,l340,l341,l342,l343,l344,l345,l346,l347,l348,l349,l350,\
    l351,l352,l353,l354,l355,l356,l357,l358,l359,l360,l361,l362,l363,l364,l365,l366,l367,l368,l369,l370,l371,l372,l373,l374,l375,l376,l377,l378,l379,l380,l381,l382,l383,l384,l385,l386,l387,l388,l389,l390,l391,l392,l393,l394,l395,l396,l397,l398,l399,l400,\
    l401,l402,l403,l404,l405,l406,l407,l408,l409,l410,l411,l412,l413,l414,l415,l416,l417,l418,l419,l420,l421,l422,l423,l424,l425,l426,l427,l428,l429,l430,l431,l432,l433,l434,l435,l436,l437,l438,l439,l440,l441,l442,l443,l444,l445,l446,l447,l448,l449,l450,\
    l451,l452,l453,l454,l455,l456,l457,l458,l459,l460,l461,l462,l463,l464,l465,l466,l467,l468,l469,l470,l471,l472,l473,l474,l475,l476,l477,l478,l479,l480,l481,l482,l483,l484,l485,l486,l487,l488,l489,l490,l491,l492,l493,l494,l495,l496,l497,l498,l499,l500,\
    l501,l502,l503,l504,l505,l506,l507,l508,l509,l510,l511,l512,l513,l514,l515,l516,l517,l518,l519,l520,l521,l522,l523,l524,l525,l526,l527,l528,l529,l530,l531,l532,l533,l534,l535,l536,l537,l538,l539,l540,l541,l542,l543,l544,l545,l546,l547,l548,l549,l550,\
    l551,l552,l553,l554,l555,l556,l557,l558,l559,l560,l561,l562,l563,l564,l565,l566,l567,l568,l569,l570,l571,l572,l573,l574,l575,l576,l577,l578,l579,l580,l581,l582,l583,l584,l585,l586,l587,l588,l589,l590,l591,l592,l593,l594,l595,l596,l597,l598,l599,l600,\
    l601,l602,l603,l604,l605,l606,l607,l608,l609,l610,l611,l612,l613,l614,l615,l616,l617,l618,l619,l620,l621,l622,l623,l624,l625,l626,l627,l628,l629,l630,l631,l632,l633,l634,l635,l636,l637,l638,l639,l640,l641,l642,l643,l644,l645,l646,l647,l648,l649,l650,\
    l651,l652,l653,l654,l655,l656,l657,l658,l659,l660,l661,l662,l663,l664,l665,l666,l667,l668,l669,l670,l671,l672,l673,l674,l675,l676,l677,l678,l679,l680,l681,l682,l683,l684,l685,l686,l687,l688,l689,l690,l691,l692,l693,l694,l695,l696,l697,l698,l699,l700,\
    l701,l702,l703,l704,l705,l706,l707,l708,l709,l710,l711,l712,l713,l714,l715,l716,l717,l718,l719,l720,l721,l722,l723,l724,l725,l726,l727,l728,l729,l730,l731,l732,l733,l734,l735,l736,l737,l738,l739,l740,l741,l742,l743,l744,l745,l746,l747,l748,l749,l750,\
    l751,l752,l753,l754,l755,l756,l757,l758,l759,l760,l761,l762,l763,l764,l765,l766,l767,l768,l769,l770,l771,l772,l773,l774,l775,l776,l777,l778,l779,l780,l781,l782,l783,l784,l785,l786,l787,l788,l789,l790,l791,l792,l793,l794,l795,l796,l797,l798,l799,l800
#define TEST_DATA_STRUCTS_REF \
    &l1,&l2,&l3,&l4,&l5,&l6,&l7,&l8,&l9,&l10,&l11,&l12,&l13,&l14,&l15,&l16,&l17,&l18,&l19,&l20,&l21,&l22,&l23,&l24,&l25,&l26,&l27,&l28,&l29,&l30,&l31,&l32,&l33,&l34,&l35,&l36,&l37,&l38,&l39,&l40,&l41,&l42,&l43,&l44,&l45,&l46,&l47,&l48,&l49,&l50,\
    &l51,&l52,&l53,&l54,&l55,&l56,&l57,&l58,&l59,&l60,&l61,&l62,&l63,&l64,&l65,&l66,&l67,&l68,&l69,&l70,&l71,&l72,&l73,&l74,&l75,&l76,&l77,&l78,&l79,&l80,&l81,&l82,&l83,&l84,&l85,&l86,&l87,&l88,&l89,&l90,&l91,&l92,&l93,&l94,&l95,&l96,&l97,&l98,&l99,&l100,\
    &l101,&l102,&l103,&l104,&l105,&l106,&l107,&l108,&l109,&l110,&l111,&l112,&l113,&l114,&l115,&l116,&l117,&l118,&l119,&l120,&l121,&l122,&l123,&l124,&l125,&l126,&l127,&l128,&l129,&l130,&l131,&l132,&l133,&l134,&l135,&l136,&l137,&l138,&l139,&l140,&l141,&l142,&l143,&l144,&l145,&l146,&l147,&l148,&l149,&l150,\
    &l151,&l152,&l153,&l154,&l155,&l156,&l157,&l158,&l159,&l160,&l161,&l162,&l163,&l164,&l165,&l166,&l167,&l168,&l169,&l170,&l171,&l172,&l173,&l174,&l175,&l176,&l177,&l178,&l179,&l180,&l181,&l182,&l183,&l184,&l185,&l186,&l187,&l188,&l189,&l190,&l191,&l192,&l193,&l194,&l195,&l196,&l197,&l198,&l199,&l200,\
    &l201,&l202,&l203,&l204,&l205,&l206,&l207,&l208,&l209,&l210,&l211,&l212,&l213,&l214,&l215,&l216,&l217,&l218,&l219,&l220,&l221,&l222,&l223,&l224,&l225,&l226,&l227,&l228,&l229,&l230,&l231,&l232,&l233,&l234,&l235,&l236,&l237,&l238,&l239,&l240,&l241,&l242,&l243,&l244,&l245,&l246,&l247,&l248,&l249,&l250,\
    &l251,&l252,&l253,&l254,&l255,&l256,&l257,&l258,&l259,&l260,&l261,&l262,&l263,&l264,&l265,&l266,&l267,&l268,&l269,&l270,&l271,&l272,&l273,&l274,&l275,&l276,&l277,&l278,&l279,&l280,&l281,&l282,&l283,&l284,&l285,&l286,&l287,&l288,&l289,&l290,&l291,&l292,&l293,&l294,&l295,&l296,&l297,&l298,&l299,&l300,\
    &l301,&l302,&l303,&l304,&l305,&l306,&l307,&l308,&l309,&l310,&l311,&l312,&l313,&l314,&l315,&l316,&l317,&l318,&l319,&l320,&l321,&l322,&l323,&l324,&l325,&l326,&l327,&l328,&l329,&l330,&l331,&l332,&l333,&l334,&l335,&l336,&l337,&l338,&l339,&l340,&l341,&l342,&l343,&l344,&l345,&l346,&l347,&l348,&l349,&l350,\
    &l351,&l352,&l353,&l354,&l355,&l356,&l357,&l358,&l359,&l360,&l361,&l362,&l363,&l364,&l365,&l366,&l367,&l368,&l369,&l370,&l371,&l372,&l373,&l374,&l375,&l376,&l377,&l378,&l379,&l380,&l381,&l382,&l383,&l384,&l385,&l386,&l387,&l388,&l389,&l390,&l391,&l392,&l393,&l394,&l395,&l396,&l397,&l398,&l399,&l400,\
    &l401,&l402,&l403,&l404,&l405,&l406,&l407,&l408,&l409,&l410,&l411,&l412,&l413,&l414,&l415,&l416,&l417,&l418,&l419,&l420,&l421,&l422,&l423,&l424,&l425,&l426,&l427,&l428,&l429,&l430,&l431,&l432,&l433,&l434,&l435,&l436,&l437,&l438,&l439,&l440,&l441,&l442,&l443,&l444,&l445,&l446,&l447,&l448,&l449,&l450,\
    &l451,&l452,&l453,&l454,&l455,&l456,&l457,&l458,&l459,&l460,&l461,&l462,&l463,&l464,&l465,&l466,&l467,&l468,&l469,&l470,&l471,&l472,&l473,&l474,&l475,&l476,&l477,&l478,&l479,&l480,&l481,&l482,&l483,&l484,&l485,&l486,&l487,&l488,&l489,&l490,&l491,&l492,&l493,&l494,&l495,&l496,&l497,&l498,&l499,&l500,\
    &l501,&l502,&l503,&l504,&l505,&l506,&l507,&l508,&l509,&l510,&l511,&l512,&l513,&l514,&l515,&l516,&l517,&l518,&l519,&l520,&l521,&l522,&l523,&l524,&l525,&l526,&l527,&l528,&l529,&l530,&l531,&l532,&l533,&l534,&l535,&l536,&l537,&l538,&l539,&l540,&l541,&l542,&l543,&l544,&l545,&l546,&l547,&l548,&l549,&l550,\
    &l551,&l552,&l553,&l554,&l555,&l556,&l557,&l558,&l559,&l560,&l561,&l562,&l563,&l564,&l565,&l566,&l567,&l568,&l569,&l570,&l571,&l572,&l573,&l574,&l575,&l576,&l577,&l578,&l579,&l580,&l581,&l582,&l583,&l584,&l585,&l586,&l587,&l588,&l589,&l590,&l591,&l592,&l593,&l594,&l595,&l596,&l597,&l598,&l599,&l600,\
    &l601,&l602,&l603,&l604,&l605,&l606,&l607,&l608,&l609,&l610,&l611,&l612,&l613,&l614,&l615,&l616,&l617,&l618,&l619,&l620,&l621,&l622,&l623,&l624,&l625,&l626,&l627,&l628,&l629,&l630,&l631,&l632,&l633,&l634,&l635,&l636,&l637,&l638,&l639,&l640,&l641,&l642,&l643,&l644,&l645,&l646,&l647,&l648,&l649,&l650,\
    &l651,&l652,&l653,&l654,&l655,&l656,&l657,&l658,&l659,&l660,&l661,&l662,&l663,&l664,&l665,&l666,&l667,&l668,&l669,&l670,&l671,&l672,&l673,&l674,&l675,&l676,&l677,&l678,&l679,&l680,&l681,&l682,&l683,&l684,&l685,&l686,&l687,&l688,&l689,&l690,&l691,&l692,&l693,&l694,&l695,&l696,&l697,&l698,&l699,&l700,\
    &l701,&l702,&l703,&l704,&l705,&l706,&l707,&l708,&l709,&l710,&l711,&l712,&l713,&l714,&l715,&l716,&l717,&l718,&l719,&l720,&l721,&l722,&l723,&l724,&l725,&l726,&l727,&l728,&l729,&l730,&l731,&l732,&l733,&l734,&l735,&l736,&l737,&l738,&l739,&l740,&l741,&l742,&l743,&l744,&l745,&l746,&l747,&l748,&l749,&l750,\
    &l751,&l752,&l753,&l754,&l755,&l756,&l757,&l758,&l759,&l760,&l761,&l762,&l763,&l764,&l765,&l766,&l767,&l768,&l769,&l770,&l771,&l772,&l773,&l774,&l775,&l776,&l777,&l778,&l779,&l780,&l781,&l782,&l783,&l784,&l785,&l786,&l787,&l788,&l789,&l790,&l791,&l792,&l793,&l794,&l795,&l796,&l797,&l798,&l799,&l800

/* Names of variables for fixture functions. */

#define FIXTURES_VARIABLES \
    t0,t1,t2,t3,t4,t5,t6,t7,t8,t9,t10,t11,t12,t13,t14,t15,t16,t17,t18,t19,\
    t20,t21,t22,t23,t24,t25,t26,t27,t28,t29,t30,t31,t32,t33,t34,t35,t36,t37,t38,t39,\
    t40,t41,t42,t43,t44,t45,t46,t47,t48,t49,t50,t51,t52,t53,t54,t55,t56,t57,t58,t59,\
    t60,t61,t62,t63,t64,t65,t66,t67,t68,t69,t70,t71,t72,t73,t74,t75,t76,t77,t78,t79,\
    t80,t81,t82,t83,t84,t85,t86,t87,t88,t89,t90,t91,t92,t93,t94,t95,t96,t97,t98,t99

/* Test description. */

struct stic_test_data
{
    const char *const n;
    const char *const f;
    stic_void_void t;
    int (*const p)(void);
};

/* Auxiliary macros for internal use. */

#define STIC_ARRAY_LEN(x) (sizeof(x)/sizeof((x)[0]))

#define STIC_STATIC_ASSERT(msg, cond) \
    typedef int msg[(cond) ? 1 : -1]; \
    /* Fake use to suppress "Unused local variable" warning. */ \
    enum { STIC_CAT(msg, _use) = (size_t)(msg *)0 }

#define STIC_CAT(X, Y) STIC_CAT_(X, Y)
#define STIC_CAT_(X, Y) X##Y

#define STIC_STR(X) STIC_STR_(X)
#define STIC_STR_(X) #X

/* Test declaration macros. */

#ifdef TEST
# undef TEST
#endif

#if __STDC_VERSION__ >= 199901L || (__GNUC__ >= 4 && !defined(__STRICT_ANSI__))
#define STIC_C99
#endif

#ifdef STIC_C99

# define IF(...) .p = __VA_ARGS__,

# define TEST(name, ...) \
    STIC_STATIC_ASSERT(STIC_CAT(too_many_lines_in_file_, __LINE__), \
                       __LINE__ < STIC_MAX_LINES); \
    static void name(void); \
    static struct stic_test_data STIC_CAT(stic_test_data_, name) = { \
        .n = #name, \
        .t = &name, \
        .f = __FILE__, \
        __VA_ARGS__ \
    }; \
    static struct stic_test_data *STIC_CAT(l, __LINE__) = &STIC_CAT(stic_test_data_, name); \
    static void name(void)

#else

# define TEST(name) \
    STIC_STATIC_ASSERT(STIC_CAT(too_many_lines_in_file_, __LINE__), \
                       __LINE__ < STIC_MAX_LINES); \
    static void name(void); \
    static struct stic_test_data STIC_CAT(stic_test_data_, name) = { \
        /* .n = */ #name, \
        /* .t = */ &name, \
        /* .f = */ __FILE__ \
    }; \
    static struct stic_test_data *STIC_CAT(l, __LINE__) = &STIC_CAT(stic_test_data_, name); \
    static void name(void)

#endif

/* Setup/teardown declaration macros. */

#define SETUP_ONCE() \
    static void stic_setup_once_func_impl(void); \
    static void (*stic_setup_once_func)(void) = &stic_setup_once_func_impl; \
    static void stic_setup_once_func_impl(void)

#define SETUP() \
    static void stic_setup_func_impl(void); \
    static void (*stic_setup_func)(void) = &stic_setup_func_impl; \
    static void stic_setup_func_impl(void)

#define TEARDOWN_ONCE() \
    static void stic_teardown_once_func_impl(void); \
    static void (*stic_teardown_once_func)(void) = &stic_teardown_once_func_impl; \
    static void stic_teardown_once_func_impl(void)

#define TEARDOWN() \
    static void stic_teardown_func_impl(void); \
    static void (*stic_teardown_func)(void) = &stic_teardown_func_impl; \
    static void stic_teardown_func_impl(void)

/* Test suite entry point macro. */

#ifndef SUITE_NAME
#define SUITE_NAME
#endif

#define DEFINE_SUITE() \
    typedef void (*stic_ft)(void); \
    stic_ft FIXTURES_VARIABLES; \
    \
    const char *stic_suite_name = STIC_STR(SUITE_NAME); \
    \
    void stic_suite(void) \
    { \
        stic_ft fixtures[] = { FIXTURES_VARIABLES }; \
        STIC_STATIC_ASSERT(too_many_fixtures, \
                           STIC_ARRAY_LEN(fixtures) >= MAXTESTID); \
        size_t i; \
        for(i = 0; i < STIC_ARRAY_LEN(fixtures); ++i) \
        { \
            if(fixtures[i] != NULL) (*fixtures[i])(); \
        } \
    } \
    \
    int main(int argc, char *argv[]) \
    { \
        int r; \
        const int file_has_tests = (stic_get_fixture_name() != NULL); \
        if(!file_has_tests && stic_setup_once_func != NULL) \
        { \
            stic_setup_once_func(); \
        } \
        r = stic_testrunner(argc, argv, stic_suite, stic_setup_func, stic_teardown_func) == 0; \
        if(!file_has_tests && stic_teardown_once_func != NULL) \
        { \
            stic_teardown_once_func(); \
        } \
        return r; \
    }

#ifdef TESTID

/* Test fixture body. */

typedef struct stic_test_data *stic_test_data_p;
static stic_test_data_p TEST_DATA_STRUCTS;

static void (*stic_setup_func)(void);
static void (*stic_setup_once_func)(void);
static void (*stic_teardown_func)(void);
static void (*stic_teardown_once_func)(void);

static struct stic_test_data *const *const stic_test_data[] = {
    TEST_DATA_STRUCTS_REF
};

static const char * stic_get_fixture_name(void)
{
    size_t i;
    for(i = 0U; i < STIC_ARRAY_LEN(stic_test_data); ++i)
    {
        if(*stic_test_data[i])
        {
            return (*stic_test_data[i])->f;
        }
    }
    return NULL;
}

static void stic_fixture(void)
{
    extern const char *stic_current_test_name;
    extern stic_void_void stic_current_test;

    size_t i;
    int has_any_tests = 0;

    const char *fixture_name = stic_get_fixture_name();
    if(fixture_name == NULL || !stic_should_run(fixture_name, NULL))
    {
        return;
    }

    stic_test_fixture_start(fixture_name);

    fixture_setup(stic_setup_func);
    fixture_teardown(stic_teardown_func);

    for(i = 0; i < STIC_ARRAY_LEN(stic_test_data); ++i)
    {
        struct stic_test_data *td = *stic_test_data[i];
        if(td == NULL) continue;
        if(!stic_should_run(fixture_name, td->n)) continue;

        /* Since predicates can rely on setup once, check predicates after
         * invoking setup once. */
        if(!has_any_tests)
        {
            has_any_tests = 1;
            if(stic_setup_once_func != NULL)
            {
                stic_current_test_name = "<setup once>";
                stic_current_test = stic_setup_once_func;
                stic_setup_once_func();
            }
        }

        if(td->p != NULL && !td->p())
        {
            stic_skip_test(fixture_name, td->n);
            continue;
        }

        stic_current_test_name = td->n;
        stic_current_test = td->t;
        stic_suite_setup();
        stic_setup();
        td->t();
        stic_teardown();
        stic_suite_teardown();
        stic_run_test(fixture_name, td->n);
    }

    if(has_any_tests && stic_teardown_once_func != NULL)
    {
        stic_current_test_name = "<teardown once>";
        stic_current_test = stic_teardown_once_func;
        stic_teardown_once_func();
    }
    test_fixture_end();
}
void (*STIC_CAT(t, TESTID))(void) = &stic_fixture;

#endif /* TESTID */

#endif /* STIC__STIC_H__ */

#ifdef STIC_INTERNAL_TESTS

void stic_simple_test_result_nolog(int passed, char* reason, const char* function, const char file[], unsigned int line);
void stic_assert_last_passed();
void stic_assert_last_failed();
void stic_enable_logging();
void stic_disable_logging();

#endif /* STIC_INTERNAL_TESTS */
