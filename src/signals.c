/* vifm
 * Copyright (C) 2001 Ken Steen.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include<errno.h>
#include<ncurses.h>
#include<signal.h>
#include<unistd.h> /* alarm() */
#include<sys/stat.h> /* stat */
#include<sys/time.h> /* timeval */
#include<sys/types.h> /* waitpid */
#include<sys/wait.h> /* waitpid */
#include<time.h>

#include"background.h"
#include"file_info.h"
#include"filelist.h"
#include"filetype.h"
#include"keys.h"
#include"menus.h"
#include"signals.h"
#include"status.h"
#include"config.h"
#include"ui.h"


/* Handle term resizing in X */
static void
received_sigwinch(void)
{
	if(!isendwin())
	{
		if (curr_stats.menu)
			curr_stats.redraw_menu = 1;
		else
			redraw_window();
	}
	else
		curr_stats.need_redraw = 1;
}

static void
received_sigtstp(void)
{
	/* End program so key strokes are not registered while stopped. */
	def_prog_mode();
	endwin();
	system("clear");
	pause();
	refresh();
	curs_set(0);
}


static void
received_sigchld(void)
{
	int status;
	pid_t pid;

	/* This needs to be a loop in case of multiple blocked signals. */
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
		add_finished_job(pid, status);
}

static void
shutdown_nicely(void)
{
	endwin();
	write_config_file();
	system("clear");
	fprintf(stdout, "Vifm killed by signal.\n");
	exit(0);
}

static void
handle_signal(int sig)
{
	switch(sig)
	{
		case SIGCHLD:
			received_sigchld();
			break;
		case SIGWINCH:
			received_sigwinch();
			break;
		case SIGTSTP:
			received_sigtstp();
			break;
		/* Shutdown nicely */
		case SIGHUP:
		case SIGQUIT:
		case SIGTERM:
			shutdown_nicely();
			break;
		default:
			break;
	}
}


void
setup_signals(void)
{
	struct sigaction handle_signal_action;

	handle_signal_action.sa_handler = handle_signal;
	sigemptyset(&handle_signal_action.sa_mask);
	handle_signal_action.sa_flags = SA_RESTART;

	sigaction(SIGCHLD, &handle_signal_action, NULL);
	sigaction(SIGHUP, &handle_signal_action, NULL);
	sigaction(SIGQUIT, &handle_signal_action, NULL);
	sigaction(SIGTSTP, &handle_signal_action, NULL);
	sigaction(SIGCONT, &handle_signal_action, NULL);
	sigaction(SIGTERM, &handle_signal_action, NULL);
	sigaction(SIGWINCH, &handle_signal_action, NULL);

	signal(SIGINT, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);
	signal(SIGALRM, SIG_IGN);
}
