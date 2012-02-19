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

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#endif
#include <sys/types.h>
#include <unistd.h> /* getcwd() */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include "log.h"
#include "macros.h"
#include "string_array.h"
#include "utils.h"

#include "ipc.h"

#define PORT 31230

static void cleanup_on_exit(void);
static void try_become_a_server(void);
static void recieve_data(void);
static void parse_data(const char *buf);

static recieve_callback callback;
static int initialized;
static int server;
static int sock;

void
ipc_init(recieve_callback callback_func)
{
#ifdef _WIN32
	int result;
	WSADATA wsaData;
#endif

	assert(!initialized);
	callback = callback_func;

#ifdef _WIN32
	result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if(result != 0)
	{
		LOG_ERROR_MSG("Can't initialize Windows sockets");
		initialized = -1;
		return;
	}
#endif

	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(sock == -1)
	{
#ifdef _WIN32
		WSACleanup();
#endif
		LOG_ERROR_MSG("Can't create socket");
		initialized = -1;
		return;
	}

	try_become_a_server();

	atexit(&cleanup_on_exit);
	initialized = 1;
}

static void
cleanup_on_exit(void)
{
#ifdef _WIN32
	closesocket(sock);
	WSACleanup();
#else
	close(sock);
#endif
}

void
ipc_check(void)
{
	/* unsigned long n; */
	fd_set ready;
	int maxfd;
	struct timeval ts = { 0, 0 };

	assert(initialized);
	if(initialized < 0)
		return;

	try_become_a_server();
	if(!server)
		return;

	FD_ZERO(&ready);
	if(sock >= 0)
		FD_SET(sock, &ready);
	maxfd = MAX(sock, 0);

	if(select(maxfd + 1, &ready, NULL, NULL, &ts) > 0)
		recieve_data();
}

static void
try_become_a_server(void)
{
	struct sockaddr_in addr;

	if(server)
		return;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(PORT);
	server = bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != -1;
	if(!server)
	{
		LOG_SERROR_MSG(errno, "Can't become an IPC sever");
	}
}

static void
recieve_data(void)
{
	char buf[8192];
	int result;

	result = recv(sock, buf, sizeof(buf), 0);
	if(result == -1)
	{
		LOG_ERROR_MSG("Can't read socket data");
		return;
	}

	parse_data(buf);
}

static void
parse_data(const char *buf)
{
	char **array = NULL;
	size_t len = 0;
	while(*buf != '\0')
	{
		len = add_to_string_array(&array, len, 1, buf);
		buf += strlen(buf) + 1;
	}
	len = add_to_string_array(&array, len, 1, NULL);
	callback(array);
	free_string_array(array, len);
}

void
ipc_send(char *data[])
{
	char buf[8192];
	size_t len;
	struct sockaddr_in addr;
	int result;

	if(server)
		return;

	assert(initialized);
	if(initialized < 0)
		return;

	if(getcwd(buf, sizeof(buf)) == NULL)
	{
		LOG_ERROR_MSG("Can't get working directory");
		return;
	}
	len = strlen(buf) + 1;
#ifdef _WIN32
	to_forward_slash(buf);
#endif

	while(*data != NULL)
	{
		strcpy(buf + len, *data);
		len += strlen(*data) + 1;
		data++;
	}
	buf[len++] = '\0';

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(PORT);
	result = sendto(sock, buf, len, 0, (struct sockaddr *)&addr, sizeof(addr));
	if(result == -1)
	{
		LOG_ERROR_MSG("Can't send data over a socket");
	}
}

int
ipc_server(void)
{
	return (server);
}

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 : */
