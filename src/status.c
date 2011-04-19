#include<limits.h>

#include "status.h"

Status curr_stats;

void
init_status(void)
{
	curr_stats.need_redraw = 0;
	curr_stats.getting_input = 0;
	curr_stats.menu = 0;
	curr_stats.redraw_menu = 0;
	curr_stats.is_updir = 0;
	curr_stats.last_char = 0;
	curr_stats.is_console = 0;
	curr_stats.search = 0;
	curr_stats.save_msg = 0;
	curr_stats.use_register = 0;
	curr_stats.curr_register = -1;
	curr_stats.register_saved = 0;
	curr_stats.show_full = 0;
	curr_stats.view = 0;
	curr_stats.setting_change = 0;
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab : */
