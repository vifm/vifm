#include <stic.h>

#include <stdlib.h> /* free() */

#include "../../src/cfg/config.h"
#include "../../src/cfg/info.h"

static void admixture_is_moved(const char import[]);
static void admixture_is_inserted(const char source[], const char import[]);

TEST(empty_import)
{
	admixture_is_moved("{}");
}

TEST(gtabs_import)
{
	admixture_is_moved("{\"gtabs\":[{}]}");
	admixture_is_inserted("{\"gtabs\":[]}", "{\"gtabs\":[{\"panes\":[{},{}]}]}");
	admixture_is_inserted("{\"gtabs\":[{}]}",
			"{\"gtabs\":[{\"panes\":[{},{}]}]}");
	admixture_is_inserted("{\"gtabs\":[{\"panes\":[]}]}",
			"{\"gtabs\":[{\"panes\":[{},{}]}]}");
	admixture_is_inserted("{\"gtabs\":[{\"panes\":[{},{}]}]}",
			"{\"gtabs\":[{\"panes\":[{\"ptabs\":[{}]},{}]}]}");
	admixture_is_inserted("{\"gtabs\":[{\"panes\":[{\"ptabs\":[{}]},{}]}]}",
			"{\"gtabs\":[{\"panes\":[{\"ptabs\":[{\"history\":[{\"dot\":true}]}]},{}]}]}");
}

TEST(regs_import)
{
	admixture_is_moved("{\"regs\":{\"a\":[\"\\/path1\",\"\\/path2\"]}}");
}

TEST(trash_import)
{
	const char *import = "{\"trash\":["
		"{\"trashed\":\"\\/trash\\/0_file\",\"original\":\"\\/file\"}]"
		"}";
	admixture_is_moved(import);
}

TEST(bmarks_import)
{
	const char *import = "{\"bmarks\":{"
		"\"\\/bookmarked\\/path\":{\"tags\":\"tag1,tag2\",\"ts\":1440801895}}"
		"}";
	admixture_is_moved(import);
}

TEST(marks_import)
{
	const char *import = "{\"marks\":{"
		"\"h\":{\"dir\":\"\\/path\",\"file\":\"file.jpg\",\"ts\":1440801895}}"
		"}";
	admixture_is_moved(import);
}

TEST(cmds_import)
{
	admixture_is_moved("{\"cmds\":{\"cmd-name\":\"echo hi\"}}");
}

TEST(viewers_import)
{
	const char *import = "{\"viewers\":["
		"{\"matchers\":\"{*.jpg}\",\"cmd\":\"echo hi\"}]"
		"}";
	admixture_is_moved(import);
}

TEST(assocs_import)
{
	const char *import = "{\"assocs\":["
		"{\"matchers\":\"{*.jpg}\",\"cmd\":\"echo hi\"}]"
		"}";
	admixture_is_moved(import);
}

TEST(xassocs_import)
{
	const char *import = "{\"xassocs\":["
		"{\"matchers\":\"{*.jpg}\",\"cmd\":\"echo hi\"}]"
		"}";
	admixture_is_moved(import);
}

TEST(dir_stack_import)
{
	const char *import = "{\"dir-stack\":[{"
		"\"left-dir\":\"\\/left\\/dir\",\"left-file\":\"left-file\","
		"\"right-dir\":\"\\/right\\/dir\",\"right-file\":\"right-file\""
		"}]}";
	admixture_is_moved(import);
}

TEST(options_import)
{
	admixture_is_moved("{\"options\":[\"opt1=val1\",\"opt2=val2\"]}");
}

TEST(cmd_hist_import)
{
	admixture_is_moved("{\"cmd-hist\":[{\"text\":\"item1\",\"ts\":1440801895}]}");
}

TEST(search_hist_import)
{
	const char *import = "{\"search-hist\":["
		"{\"text\":\"item1\",\"ts\":1440801895}"
		"]}";
	admixture_is_moved(import);
}

TEST(prompt_hist_import)
{
	const char *import = "{\"prompt-hist\":["
		"{\"text\":\"item1\",\"ts\":1440801895}"
		"]}";
	admixture_is_moved(import);
}

TEST(lfilt_hist_import)
{
	const char *import = "{\"lfilt-hist\":["
		"{\"text\":\"item1\",\"ts\":1440801895}"
		"]}";
	admixture_is_moved(import);
}

TEST(fields_of_root_import)
{
	const char *import = "{"
		"\"active-gtab\":0,"
		"\"use-term-multiplexer\":true,"
		"\"color-scheme\":\"almost-default\""
		"}";
	admixture_is_moved(import);
}

TEST(fields_of_gtab_import)
{
	admixture_is_inserted("{\"gtabs\":[{}]}", "{\"gtabs\":"
			"[{\"name\":\"gtab-name\",\"active-pane\":0,\"preview\":false}]"
			"}");

	admixture_is_inserted("{\"gtabs\":[{\"panes\":[]}]}", "{\"gtabs\":"
			"[{\"panes\":[{}],"
			  "\"name\":\"gtab-name\",\"active-pane\":0,\"preview\":false}]"
			"}");

	admixture_is_inserted("{\"gtabs\":[{\"panes\":[{}]}]}", "{\"gtabs\":"
			"[{\"panes\":[{}],"
			  "\"name\":\"gtab-name\",\"active-pane\":0,\"preview\":false}]"
			"}");

	admixture_is_inserted("{\"gtabs\":[{\"panes\":[{\"ptabs\":[{}]},{}]}]}",
			"{\"gtabs\":[{\"panes\":[{\"ptabs\":[{\"name\":\"ptab-name\"}]},{}]}]}");
}

static void
admixture_is_moved(const char import[])
{
	admixture_is_inserted("{}", import);
}

static void
admixture_is_inserted(const char source[], const char import[])
{
	JSON_Value *current = json_parse_string(source);

	JSON_Value *admixture = json_parse_string(import);
	merge_states(FULL_VINFO, 1, json_object(current), json_object(admixture));
	json_value_free(admixture);

	char *result = json_serialize_to_string(current);
	json_value_free(current);

	assert_string_equal(import, result);
	free(result);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
