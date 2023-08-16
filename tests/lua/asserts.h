#ifndef VIFM_TESTS__LUA__ASSERTS_H__
#define VIFM_TESTS__LUA__ASSERTS_H__

#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"

/*
 * Assumption: <stic.h> is already included.
 *
 * These are macros so that other tests can use them by just including the
 * header.
 *
 * Macros name structure:
 *  - GLUA_   prefix means Lua code execution should succeed (Good)
 *  - BLUA_   prefix means Lua code execution should fail (Bad)
 *  - _EQ     suffix checks output match exactly (use "" for no output)
 *  - _STARTS suffix checks that output starts with a string
 *  - _ENDS   suffix checks that output ends with a string
 */

#define GLUA_EQ(vlua, expected_output, code) \
	do \
	{ \
		ui_sb_msg(""); \
		assert_success(vlua_run_string(vlua, code)); \
		assert_string_equal(expected_output, ui_sb_last()); \
	} \
	while(0)

#define GLUA_STARTS(vlua, expected_prefix, code) \
	do \
	{ \
		ui_sb_msg(""); \
		assert_success(vlua_run_string(vlua, code)); \
		assert_string_starts_with(expected_prefix, ui_sb_last()); \
	} \
	while(0)

#define GLUA_ENDS(vlua, expected_suffix, code) \
	do \
	{ \
		ui_sb_msg(""); \
		assert_success(vlua_run_string(vlua, code)); \
		assert_string_ends_with(expected_suffix, ui_sb_last()); \
	} \
	while(0)

#define BLUA_EQ(vlua, expected_output, code) \
	do \
	{ \
		ui_sb_msg(""); \
		assert_failure(vlua_run_string(vlua, code)); \
		assert_string_equal(expected_output, ui_sb_last()); \
	} \
	while(0)

#define BLUA_STARTS(vlua, expected_prefix, code) \
	do \
	{ \
		ui_sb_msg(""); \
		assert_failure(vlua_run_string(vlua, code)); \
		assert_string_starts_with(expected_prefix, ui_sb_last()); \
	} \
	while(0)

#define BLUA_ENDS(vlua, expected_suffix, code) \
	do \
	{ \
		ui_sb_msg(""); \
		assert_failure(vlua_run_string(vlua, code)); \
		assert_string_ends_with(expected_suffix, ui_sb_last()); \
	} \
	while(0)

#endif /* VIFM_TESTS__LUA__ASSERTS_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
