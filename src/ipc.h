/* vifm
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

#ifndef VIFM__IPC_H__
#define VIFM__IPC_H__

typedef void (*recieve_callback)(char *args[]);

/* Initializes IPC unit basic state. */
void ipc_pre_init(void);
/* Initializes IPC unit state. The callback_func will be called by ipc_check. */
void ipc_init(recieve_callback callback_func);
/* Checks for incoming messages. Calls callback passed to ipc_init. */
void ipc_check(void);
/* Sends data to server.  The data array should end with NULL. */
void ipc_send(char *data[]);
/* Returns non-zero value if current instance is a server. */
int ipc_server(void);

#endif /* VIFM__IPC_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
