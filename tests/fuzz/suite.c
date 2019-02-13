#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/engine/cmds.h"
#include "../../src/engine/keys.h"
#include "../../src/engine/mode.h"
#include "../../src/engine/options.h"
#include "../../src/engine/variables.h"
#include "../../src/modes/wk.h"
#include "../../src/utils/macros.h"
#include "../../src/utils/matchers.h"
#include "../../src/utils/str.h"
#include "../../src/utils/string_array.h"

static int fuzz_options(const char input[]);
static int fuzz_variables(const char input[]);
static int fuzz_keys(const char input[]);
static int fuzz_commands(const char input[]);
static int fuzz_matchers(const char input[]);

static void load_options(void);
static void dummy_handler(OPT_OP op, optval_t val);
static int complete_args(int id, const cmd_info_t *cmd_info, int arg_pos,
		void *extra_arg);
static int swap_range(void);
static int resolve_mark(char mark);
static char * expand_macros(const char str[], int for_shell, int *usr1,
		int *usr2);
static char * expand_envvars(const char *str);
static int dummy_cmd(const cmd_info_t *cmd_info);
static void post(int id);
static void select_range(int id, const cmd_info_t *cmd_info);
static int skip_at_beginning(int id, const char *args);

static char *
input(void)
{
	size_t input_len;
	return read_nonseekable_stream(stdin, &input_len, NULL, NULL);
}

int
main(int argc, char *argv[])
{
	if(argc != 2)
	{
		puts("Usage: test-kind-name");
		return EXIT_FAILURE;
	}

	if(strcmp(argv[1], "options") == 0)
	{
		return fuzz_options(input());
	}
	if(strcmp(argv[1], "variables") == 0)
	{
		return fuzz_variables(input());
	}
	if(strcmp(argv[1], "keys") == 0)
	{
		return fuzz_keys(input());
	}
	if(strcmp(argv[1], "commands") == 0)
	{
		return fuzz_commands(input());
	}
	if(strcmp(argv[1], "matchers") == 0)
	{
		return fuzz_matchers(input());
	}

	puts("Unknown test-kind-name");
	return EXIT_FAILURE;
}

static int
fuzz_options(const char input[])
{
	static int option_changed;

	const char *start;

	vle_opts_init(&option_changed, NULL);

	vle_opts_set(input, OPT_ANY);
	vle_opts_complete(input, &start, OPT_GLOBAL);

	load_options();

	vle_opts_set(input, OPT_ANY);
	vle_opts_complete(input, &start, OPT_GLOBAL);

	return EXIT_SUCCESS;
}

static int
fuzz_variables(const char input[])
{
	const char *start;
	init_variables();
	let_variables(input);
	unlet_variables(input);
	complete_variables(input, &start);
	clear_variables();
	clear_envvars();
	return EXIT_SUCCESS;
}

static void
keys_dummy(key_info_t key_info, keys_info_t *keys_info)
{
	/* Do nothing. */
}

static void
silence(int more)
{
	/* Do nothing. */
}

static int
fuzz_keys(const char input[])
{
	static int mode_flags[] = {
		MF_USES_REGS | MF_USES_COUNT,
		MF_USES_INPUT,
		MF_USES_COUNT
	};

	static keys_add_info_t normal_cmds[] = {
		{WK_COLON,            {{&keys_dummy}}},
		{WK_m,                {{&keys_dummy}, FOLLOWED_BY_MULTIKEY}},
		{WK_QUOTE,            {{&keys_dummy}, FOLLOWED_BY_MULTIKEY}},
		{WK_H,                {{&keys_dummy}}},
		{WK_g WK_u,           {{&keys_dummy}, FOLLOWED_BY_SELECTOR}},
		{WK_g WK_u WK_u,      {{&keys_dummy}}},
		{WK_g WK_u WK_g WK_u, {{&keys_dummy}}},
		{WK_g WK_u WK_g WK_g, {{&keys_dummy}, .skip_suggestion = 1}},
		{WK_j,                {{&keys_dummy}}},
		{WK_k,                {{&keys_dummy}}},
		{WK_i,                {{&keys_dummy}}},
		{WK_C_w WK_LT,        {{&keys_dummy}, .nim = 1}},
		{WK_d,                {{&keys_dummy}, FOLLOWED_BY_SELECTOR}},
		{WK_d WK_d,           {{&keys_dummy}, .nim = 1}},
		{WK_v,                {{&keys_dummy}}},
		{WK_y,                {{&keys_dummy}, FOLLOWED_BY_SELECTOR}},
		{WK_Z WK_Q,           {{&keys_dummy}}},
		{WK_Z WK_Z,           {{&keys_dummy}}},
		{WK_n WK_o WK_r WK_m, {{&keys_dummy}}},
	};

	static keys_add_info_t normal_selectors[] = {
		{WK_g WK_g,           {{&keys_dummy}}},
		{WK_QUOTE,            {{&keys_dummy}, FOLLOWED_BY_MULTIKEY}},
	};

	static keys_add_info_t visual_cmds[] = {
		{WK_j,                {{&keys_dummy}}},
		{WK_k,                {{&keys_dummy}}},
		{WK_v,                {{&keys_dummy}}},
		{WK_Z WK_Z,           {{&keys_dummy}}},
	};

	static keys_add_info_t common_selectors[] = {
		{WK_j,                {{&keys_dummy}}},
		{WK_k,                {{&keys_dummy}}},
		{WK_s,                {{&keys_dummy}}},
		{WK_i WK_f,           {{&keys_dummy}}},
	};

	wchar_t *winput = to_wide_force(input);

	vle_keys_init(3, mode_flags, &silence);
	vle_mode_set(0, VMT_PRIMARY);

	vle_keys_add(normal_cmds, ARRAY_LEN(normal_cmds), 0);
	vle_keys_add(visual_cmds, ARRAY_LEN(visual_cmds), 1);

	vle_keys_add_selectors(common_selectors, ARRAY_LEN(common_selectors), 0);
	vle_keys_add_selectors(normal_selectors, ARRAY_LEN(normal_selectors), 0);
	vle_keys_add_selectors(common_selectors, ARRAY_LEN(common_selectors), 1);

	vle_mode_set(0, VMT_PRIMARY);
	vle_keys_exec(winput);
	vle_keys_exec_no_remap(winput);
	vle_keys_exec_timed_out(winput);
	vle_keys_exec_timed_out_no_remap(winput);
	vle_keys_user_exists(winput, 0);
	vle_keys_user_add(winput, winput, 0, KEYS_FLAG_NOREMAP);
	vle_keys_user_exists(winput, 0);
	vle_keys_user_remove(winput, 0);

	vle_mode_set(1, VMT_PRIMARY);
	vle_keys_exec(winput);
	vle_keys_exec_no_remap(winput);
	vle_keys_exec_timed_out(winput);
	vle_keys_exec_timed_out_no_remap(winput);
	vle_keys_user_exists(winput, 1);
	vle_keys_user_add(winput, winput, 1, KEYS_FLAG_NOREMAP);
	vle_keys_user_exists(winput, 1);
	vle_keys_user_remove(winput, 1);

	return EXIT_SUCCESS;
}

static int
fuzz_commands(const char input[])
{
	static cmds_conf_t cmds_conf = {
		.complete_args = &complete_args,
		.swap_range = &swap_range,
		.resolve_mark = &resolve_mark,
		.expand_macros = &expand_macros,
		.expand_envvars = &expand_envvars,
		.post = &post,
		.select_range = &select_range,
		.skip_at_beginning = &skip_at_beginning,
	};

	cmd_add_t commands[] = {
		{ .name = "<USERCMD>",    .abbr = NULL,  .id = -1,      .descr = "descr",
		  .flags = HAS_RANGE,
		  .handler = &dummy_cmd,  .min_args = 0, .max_args = 0, },
		{ .name = "",             .abbr = NULL,  .id = -1,      .descr = "descr",
		  .flags = HAS_RANGE,
		  .handler = &dummy_cmd,  .min_args = 0, .max_args = 0, },
		{ .name = "delete",       .abbr = "d",   .id = -1,      .descr = "descr",
		  .flags = HAS_EMARK | HAS_RANGE,
		  .handler = &dummy_cmd,  .min_args = 0, .max_args = 1, },
	};

	cmds_conf.begin = 10;
	cmds_conf.current = 50;
	cmds_conf.end = 100;

	vle_cmds_init(1, &cmds_conf);

	vle_cmds_add(commands, 3);

	vle_cmds_run(input);

	vle_cmds_reset();

	return EXIT_SUCCESS;
}

static int
fuzz_matchers(const char input[])
{
	char *error = NULL;

	matchers_t *ms;

	ms = matchers_alloc(input, 0, 0, "", &error);
	free(error);
	matchers_free(ms);

	ms = matchers_alloc(input, 0, 1, "", &error);
	free(error);
	matchers_free(ms);

	ms = matchers_alloc(input, 1, 0, "", &error);
	free(error);
	matchers_free(ms);

	ms = matchers_alloc(input, 1, 1, "", &error);
	free(error);
	matchers_free(ms);

	return EXIT_SUCCESS;
}

static void
load_options(void)
{
	optval_t val;

	static const char *sort_enum[][2] = {
		{ "ext",   "descr" },
		{ "name",  "descr" },
		{ "gid",   "descr" },
		{ "gname", "descr" },
	};

	val.str_val = "";
	vle_opts_add("cdpath", "cd", "descr", OPT_STRLIST, OPT_GLOBAL, 0, NULL,
			dummy_handler, val);

	val.bool_val = 0;
	vle_opts_add("fastrun", "fr", "descr", OPT_BOOL, OPT_GLOBAL, 0, NULL,
			dummy_handler, val);

	val.str_val = "fusehome-default";
	vle_opts_add("fusehome", "fh", "descr", OPT_STR, OPT_GLOBAL, 0, NULL,
			dummy_handler, val);

	val.enum_item = 1;
	vle_opts_add("sort", "so", "descr", OPT_ENUM, OPT_GLOBAL, ARRAY_LEN(sort_enum),
			sort_enum, &dummy_handler, val);

	val.bool_val = 1;
	vle_opts_add("sortorder", "", "descr", OPT_BOOL, OPT_GLOBAL, 0, NULL,
			&dummy_handler, val);
}

static void
dummy_handler(OPT_OP op, optval_t val)
{
}

static int
complete_args(int id, const cmd_info_t *cmd_info, int arg_pos, void *extra_arg)
{
	return 0;
}

static int
swap_range(void)
{
	return 1;
}

static int
resolve_mark(char mark)
{
	if(isdigit(mark))
		return -1;
	return 75;
}

static char *
expand_macros(const char str[], int for_shell, int *usr1, int *usr2)
{
	return strdup(str);
}

static char *
expand_envvars(const char *str)
{
	return strdup(str);
}

static int
dummy_cmd(const cmd_info_t *cmd_info)
{
	return 0;
}

static void
post(int id)
{
}

static void
select_range(int id, const cmd_info_t *cmd_info)
{
}

static int
skip_at_beginning(int id, const char *args)
{
	return -1;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
