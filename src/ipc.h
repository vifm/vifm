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

/* Opaque handle type for this unit that represents an IPC instance. */
typedef struct ipc_t ipc_t;

/* Type of function that is invoked when arguments are received.  args is a NULL
 * terminated array of arguments, args[0] is absolute path at which they should
 * be processed. */
typedef void (*ipc_args_cb)(char *args[]);

/* Type of function that is invoked when expression is received.  Evaluates
 * expression from a remote instance.  Should return newly allocated string
 * with the result or NULL on error. */
typedef char * (*ipc_eval_cb)(const char expr[]);

/* Checks whether IPC is in use.  Returns non-zero if so, otherwise zero is
 * returned. */
int ipc_enabled(void);

/* Retrieves list with names of all servers available for IPC.  Returns the list
 * which is of the *len length. */
char ** ipc_list(int *len);

/* Initializes IPC unit state.  name can be NULL, which will use the default
 * one (VIFM).  Callbacks will be called on ipc_check(). */
ipc_t * ipc_init(const char name[], ipc_args_cb args_cb, ipc_eval_cb eval_cb);

/* Frees resources associated with an instance of IPC.  The parameter can be
 * NULL. */
void ipc_free(ipc_t *ipc);

/* Retrieves name of the IPC server.  Returns the name. */
const char * ipc_get_name(const ipc_t *ipc);

/* Checks for incoming messages.  Calls callback passed to ipc_init().  Returns
 * non-zero if something was received, otherwise zero is returned. */
int ipc_check(ipc_t *ipc);

/* Sends data to server.  If whom argument is NULL, target instance is
 * automatically determined.  The data array should end with NULL.  Returns zero
 * on successful send and non-zero otherwise. */
int ipc_send(ipc_t *ipc, const char whom[], char *data[]);

/* Evaluates expression in a remote instance.  Rules for arguments match those
 * of ipc_send().  Returns result converted to a string or NULL on error. */
char * ipc_eval(ipc_t *ipc, const char whom[], const char expr[]);

#endif /* VIFM__IPC_H__ */

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
