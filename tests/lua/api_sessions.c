#include <stic.h>

#include "../../src/cfg/info.h"
#include "../../src/lua/vlua.h"

#include "asserts.h"

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
	GLUA_EQ(vlua, "nil", "print(vifm.sessions.current())");
}

TEST(current_session)
{
	assert_success(sessions_create("test-session"));

	GLUA_EQ(vlua, "test-session", "print(vifm.sessions.current())");

	assert_success(sessions_stop());
}
