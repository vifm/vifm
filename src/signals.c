/* vifm
 * Copyright (C) 2001 Ken Steen.
 * Copyright (C) 2011 xaizek.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "signals.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <curses.h>

#include <signal.h>
#include <stdio.h> /* fprintf() */
#include <string.h> /* strsignal() */

#include "cfg/info.h"
#include "ui/cancellation.h"
#include "utils/log.h"
#include "utils/macros.h"
#include "fuse.h"
#include "term_title.h"

static void _gnuc_noreturn shutdown_nicely(int sig, const char descr[]);

#ifndef _WIN32

#include <sys/types.h> /* pid_t */
#include <sys/wait.h> /* WEXITSTATUS() WIFEXITED() waitpid() */

#include <stdlib.h> /* exit() */

#include "utils/macros.h"
#include "background.h"
#include "status.h"

/* Handle term resizing in X */
static void
received_sigwinch(void)
{
	if(curr_stats.save_msg != 2)
		curr_stats.save_msg = 0;

	if(!isendwin())
	{
		schedule_redraw();
	}
	else
	{
		curr_stats.need_update = UT_FULL;
	}
}

static void
received_sigcont(void)
{
	reset_prog_mode();
	schedule_redraw();
}

static void
received_sigchld(void)
{
	int status;
	pid_t pid;

	/* This needs to be a loop in case of multiple blocked signals. */
	while((pid = waitpid(-1, &status, WNOHANG)) > 0)
	{
		if(WIFEXITED(status))
		{
			add_finished_job(pid, WEXITSTATUS(status));
		}
	}
}

static void
handle_signal(int sig)
{
	switch(sig)
	{
		case SIGINT:
			ui_cancellation_request();
			break;
		case SIGCHLD:
			received_sigchld();
			break;
		case SIGWINCH:
			received_sigwinch();
			break;
		case SIGCONT:
			received_sigcont();
			break;
		/* Shutdown nicely */
		case SIGHUP:
		case SIGQUIT:
		case SIGTERM:
			shutdown_nicely(sig, strsignal(sig));
			break;
		default:
			break;
	}
}

#else
BOOL WINAPI
ctrl_handler(DWORD dwCtrlType)
{
  LOG_FUNC_ENTER;

	switch(dwCtrlType)
	{
		case CTRL_C_EVENT:
		case CTRL_BREAK_EVENT:
			ui_cancellation_request();
			break;
		case CTRL_CLOSE_EVENT:
			shutdown_nicely(dwCtrlType, "Ctrl-C");
			break;
		case CTRL_LOGOFF_EVENT:
			shutdown_nicely(dwCtrlType, "Logoff");
			break;
		case CTRL_SHUTDOWN_EVENT:
			shutdown_nicely(dwCtrlType, "Shutdown");
			break;
	}

	return TRUE;
}
#endif

static void _gnuc_noreturn
shutdown_nicely(int sig, const char descr[])
{
  LOG_FUNC_ENTER;

	endwin();
	set_term_title(NULL);
	fuse_unmount_all();
	write_info_file();
	fprintf(stdout, "Vifm killed by signal: %d (%s).\n", sig, descr);
	exit(0);
}

void
setup_signals(void)
{
  LOG_FUNC_ENTER;

#ifndef _WIN32
	struct sigaction handle_signal_action;

	handle_signal_action.sa_handler = handle_signal;
	sigemptyset(&handle_signal_action.sa_mask);
	handle_signal_action.sa_flags = SA_RESTART;

	sigaction(SIGCHLD, &handle_signal_action, NULL);
	sigaction(SIGHUP, &handle_signal_action, NULL);
	sigaction(SIGINT, &handle_signal_action, NULL);
	sigaction(SIGQUIT, &handle_signal_action, NULL);
	sigaction(SIGCONT, &handle_signal_action, NULL);
	sigaction(SIGTERM, &handle_signal_action, NULL);
	sigaction(SIGWINCH, &handle_signal_action, NULL);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);
	signal(SIGALRM, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
#else
	if(!SetConsoleCtrlHandler(ctrl_handler, TRUE))
	{
		LOG_WERROR(GetLastError());
	}
	signal(SIGINT, SIG_IGN);
#endif
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
