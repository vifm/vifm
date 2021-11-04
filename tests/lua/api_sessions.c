#include <stic.h>

#include "../../src/cfg/info.h"
#include "../../src/lua/vlua.h"
#include "../../src/ui/statusbar.h"

static vlua_t *vlua;

SETUP()
{
	vlua = vlua_init();
}

TEARDOWN()
{
	vlua_finish(vlua);
}

TEST(no_current_session)
{
	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(vifm.sessions.current())"));
	assert_string_equal("nil", ui_sb_last());
}

TEST(current_session)
{
	assert_success(sessions_create("test-session"));

	ui_sb_msg("");
	assert_success(vlua_run_string(vlua, "print(vifm.sessions.current())"));
	assert_string_equal("test-session", ui_sb_last());

	assert_success(sessions_stop());
}
