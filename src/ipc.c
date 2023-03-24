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

#include "ipc.h"

#ifdef ENABLE_REMOTE_CMDS

#if defined(_WIN32) || defined(__CYGWIN__)
/* Named pipes don't work very well on Cygwin neither directly nor indirectly
 * (using //./pipe/ paths with POSIX API), so use Win32 API on Cygwin. */
# define WIN32_PIPE_READ
#endif

#ifndef WIN32_PIPE_READ
# include <sys/types.h>
# include <sys/select.h> /* FD_* select() */
#else
# define O_NONBLOCK 0
# include <windows.h>
# ifndef PIPE_REJECT_REMOTE_CLIENTS
#  define PIPE_REJECT_REMOTE_CLIENTS 1
# endif
# ifdef KEY_EVENT
#  undef KEY_EVENT /* curses also defines a macro with such name. */
# endif
#endif

#include <sys/stat.h> /* mkfifo() stat() */
#include <dirent.h> /* DIR closedir() opendir() readdir() */
#include <fcntl.h>
#include <unistd.h> /* close() open() select() unlink() usleep() */

#include <errno.h> /* EACCES EEXIST EDQUOT ENOSPC ENXIO errno */
#include <stddef.h> /* NULL size_t ssize_t */
#include <stdio.h> /* FILE fclose() fdopen() fread() fwrite() */
#include <stdlib.h> /* free() malloc() snprintf() */
#include <string.h> /* strcmp() strcpy() strlen() */

#include "compat/os.h"
#include "utils/fs.h"
#include "utils/log.h"
#include "utils/macros.h"
#include "utils/path.h"
#include "utils/str.h"
#include "utils/string_array.h"
#include "utils/utils.h"
#include "status.h"

/**
 * When IPC is enabled, it exchanges packets between instances which are just
 * arrays of strings.  Each package looks like this:
 *
 *     "version:{...}" -\
 *     "from:{name}"   --\
 *     "body:{type}"   ---\
 *                         some sort of a header that ends on `body:`
 *     "string #1"     -\
 *     "string #2"     --\
 *     {...}           ---\
 *     "string #N"     ----\
 *                          payload (might be prepended by this unit)
 *     "\0"            -\
 *                       terminator
 *
 * Or in sequential form:
 *     "version:{...}\0body:{type}\0string #1\0string #2\0{...}string #N\0\0"
 *
 * {name} is a name of another instance.
 *
 * {type} can be:
 *  - "args" to pass list of arguments, in which case body is prepended with CWD
 *    unconditionally;
 *  - "eval" to pass an expression for evaluation in a single line;
 *  - "eval-result" to communicate result of successful evaluation in a single
 *    line;
 *  - "eval-error" to communicate failure of evaluation with no strings.
 *
 * On version mismatch or unknown field name, packet is discarded which is
 * logged.
 */

/* Prefix for names of all pipes to distinguish them from other pipes. */
#define PREFIX "vifm-ipc-"

#ifndef WIN32_PIPE_READ
typedef FILE *read_pipe_t;
#define NULL_READ_PIPE NULL
#else
typedef HANDLE read_pipe_t;
#define NULL_READ_PIPE INVALID_HANDLE_VALUE
#endif

/* Holds list information for add_to_list(). */
typedef struct
{
	char **lst;          /* List of strings. */
	size_t len;          /* Number of items. */
	const char *ipc_dir; /* Root of IPC objects. */
	const ipc_t *ipc;    /* Handle to an IPC instance or NULL. */
}
list_data_t;

/* Storage of data of an instance. */
struct ipc_t
{
	/* Stores callback to report received messages. */
	ipc_args_cb args_cb;
	/* Stores callback used for evaluation of expressions. */
	ipc_eval_cb eval_cb;
	/* Whether this IPC instance should ignore check requests from outside. */
	int locked;
	/* Path to the pipe used by this instance. */
	char pipe_path[PATH_MAX + 1];
	/* Opened file of the pipe. */
	read_pipe_t pipe_file;
	/* Holds result of expression evaluation or NULL on evaluation error. */
	char *eval_result;
};

static read_pipe_t create_pipe(const char name[], char path_buf[], size_t len);
static char * receive_pkg(ipc_t *ipc, int *len);
static read_pipe_t try_use_pipe(const char path[], int *fatal);
static void handle_pkg(ipc_t *ipc, const char pkg[], const char *end);
static void handle_args(ipc_t *ipc, char ***array, int len);
static void handle_expr(ipc_t *ipc, const char from[], char *array[], int len);
static void handle_eval_result(ipc_t *ipc, char *array[], int len);
static int format_and_send(ipc_t *ipc, const char whom[], char *data[],
		const char type[]);
static int send_pkg(const char whom[], const char what[], size_t len);
static char * get_the_only_target(const ipc_t *ipc);
static char ** list_servers(const ipc_t *ipc, int *len);
static int add_to_list(const char name[], const void *data, void *param);
static const char * get_ipc_dir(void);
#ifndef WIN32_PIPE_READ
static int pipe_is_in_use(const char path[]);
#endif

/* Current version string. */
static const char IPC_VERSION[] = "version:1";
/* Request to process remote arguments. */
static const char ARGS_TYPE[] = "args";
/* Request to process remote expression. */
static const char EVAL_TYPE[] = "eval";
/* Reply to remote expression with the result after successful evaluation. */
static const char EVAL_RESULT_TYPE[] = "eval-result";
/* Reply to remote expression on error. */
static const char EVAL_ERROR_TYPE[] = "eval-error";

int
ipc_enabled(void)
{
	return 1;
}

ipc_t *
ipc_init(const char name[], ipc_args_cb args_cb, ipc_eval_cb eval_cb)
{
	ipc_t *const ipc = malloc(sizeof(*ipc));
	if(ipc == NULL)
	{
		return NULL;
	}

	ipc->args_cb = args_cb;
	ipc->eval_cb = eval_cb;
	ipc->locked = 0;

	if(name == NULL)
	{
		name = "vifm";
	}

	ipc->pipe_file = create_pipe(name, ipc->pipe_path, sizeof(ipc->pipe_path));
	if(ipc->pipe_file == NULL_READ_PIPE)
	{
		free(ipc);
		return NULL;
	}

	return ipc;
}

void
ipc_free(ipc_t *ipc)
{
	if(ipc == NULL)
	{
		return;
	}

#ifndef WIN32_PIPE_READ
	fclose(ipc->pipe_file);
	unlink(ipc->pipe_path);
#else
	CloseHandle(ipc->pipe_file);
#endif
	free(ipc);
}

const char *
ipc_get_name(const ipc_t *ipc)
{
	return get_last_path_component(ipc->pipe_path) + (sizeof(PREFIX) - 1U);
}

int
ipc_check(ipc_t *ipc)
{
	int len;
	char *pkg;

	if(ipc->locked)
	{
		return 0;
	}

	pkg = receive_pkg(ipc, &len);
	if(pkg != NULL)
	{
		handle_pkg(ipc, pkg, pkg + len);
		free(pkg);
		return 1;
	}
	return 0;
}

/* Receives message addressed to this instance.  Returns NULL if there was no
 * message or on failure to read it, otherwise newly allocated string is
 * returned. */
static char *
receive_pkg(ipc_t *ipc, int *len)
{
#ifndef WIN32_PIPE_READ
	uint32_t size;
	char *pkg;
	char *p;

	fd_set ready;
	int max_fd;
	struct timeval ts = { .tv_sec = 0, .tv_usec = 10000 };

	/* At least on OS X pipe might get into EOF state, so reset it.  This will
	 * also reset any errors, which is fine with us. */
	clearerr(ipc->pipe_file);

	if(fread(&size, sizeof(size), 1U, ipc->pipe_file) != 1U ||
			size >= 4294967294U)
	{
		return NULL;
	}

	pkg = malloc(size + 1U);
	if(pkg == NULL)
	{
		LOG_ERROR_MSG("Failed to allocate memory: %lu", (unsigned long)(size + 1));
		return NULL;
	}

	max_fd = fileno(ipc->pipe_file);
	FD_ZERO(&ready);
	FD_SET(max_fd, &ready);

	p = pkg;
	while(size != 0U && select(max_fd + 1, &ready, NULL, NULL, &ts) > 0)
	{
		const size_t nread = fread(p, 1U, size, ipc->pipe_file);
		size -= nread;
		p += nread;

		if(nread == 0U)
		{
			break;
		}

		ts.tv_sec = 0;
		ts.tv_usec = 10000;
	}

	if(size != 0U)
	{
		free(pkg);
		LOG_ERROR_MSG("Failed to read whole packet, left: %lu",
				(unsigned long)size);
		return NULL;
	}

	/* Make sure we have a trailing zero. */
	*p = '\0';
	*len = p - pkg;

	return pkg;
#else
	uint32_t size;
	char *pkg;
	char *p;
	DWORD nread;

	if(ReadFile(ipc->pipe_file, &size, sizeof(size), &nread, NULL) == FALSE ||
			size >= 4294967294U)
	{
		return NULL;
	}

	pkg = malloc(size + 1U);
	if(pkg == NULL)
	{
		return NULL;
	}

	p = pkg;
	while(size != 0U)
	{
		/* TODO: maybe use OVERLAPPED I/O on Windows instead, it's just so
		 *       inconvenient... */
		usleep(10000);

		if(ReadFile(ipc->pipe_file, p, size, &nread, NULL) == FALSE || nread == 0U)
		{
			break;
		}

		size -= nread;
		p += nread;
	}

	/* Weird requirement for named pipes, need to break and set connection every
	 * time. */
	DisconnectNamedPipe(ipc->pipe_file);
	ConnectNamedPipe(ipc->pipe_file, NULL);

	if(size != 0U)
	{
		free(pkg);
		return NULL;
	}

	/* Make sure we have a trailing zero. */
	*p = '\0';
	*len = p - pkg;

	return pkg;
#endif
}

/* Tries to open a pipe for communication.  Returns NULL_READ_PIPE on error or
 * opened file descriptor otherwise. */
static read_pipe_t
create_pipe(const char name[], char path_buf[], size_t len)
{
	unsigned int id = 0U;
	read_pipe_t rp;
	int fatal;

	/* Try to use name as is at first. */
	snprintf(path_buf, len, "%s/" PREFIX "%s", get_ipc_dir(), name);
	rp = try_use_pipe(path_buf, &fatal);
	while(rp == NULL_READ_PIPE && !fatal)
	{
		snprintf(path_buf, len, "%s/" PREFIX "%s%u", get_ipc_dir(), name, ++id);

		if(id == 0)
		{
			return NULL_READ_PIPE;
		}

		rp = try_use_pipe(path_buf, &fatal);
	}

	return rp;
}

/* Either creates a pipe or reused previously abandoned one.  Returns
 * NULL_READ_PIPE on failure (with *fatal set to non-zero if further tries don't
 * make any sense) or valid file descriptor otherwise. */
static read_pipe_t
try_use_pipe(const char path[], int *fatal)
{
#ifndef WIN32_PIPE_READ
	FILE *f;
	int fd;

	*fatal = 0;

	/* Try to create a pipe. */
	if(mkfifo(path, 0600) == 0)
	{
		/* Open if created. */
		const int fd = open(path, O_RDONLY | O_NONBLOCK);
		if(fd == -1)
		{
			if(errno == EACCES)
			{
				/* If we created a file that we can't open, remove it and assume that
				 * something is fundamentally wrong with the system setup. */
				(void)unlink(path);
				*fatal = 1;
			}
			return NULL_READ_PIPE;
		}
		return fdopen(fd, "r");
	}

	/* Fail fast if the error is not related to existence of the file or pipe
	 * exists and is in use. */
	if(errno != EEXIST || pipe_is_in_use(path))
	{
		/* No retries if file-system is unable to create more files. */
		*fatal = (errno == EDQUOT || errno == ENOSPC);
		return NULL_READ_PIPE;
	}

	/* Use this file if we're the only one willing to read from it. */
	fd = open(path, O_RDONLY | O_NONBLOCK);

	if(fd == -1)
	{
		return NULL_READ_PIPE;
	}

	f = fdopen(fd, "r");
	if(f == NULL_READ_PIPE)
	{
		close(fd);
	}
	return f;
#else
	*fatal = 0;
	return CreateNamedPipeA(path,
			PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE,
			PIPE_TYPE_BYTE | PIPE_NOWAIT | PIPE_REJECT_REMOTE_CLIENTS,
			PIPE_UNLIMITED_INSTANCES, 4096, 4096, 10, NULL);
#endif
}

/* Parses pkg into array of strings and invokes callback. */
static void
handle_pkg(ipc_t *ipc, const char pkg[], const char *end)
{
	char **array = NULL;
	size_t len = 0U;
	int in_body = 0;
	const char *type = NULL;
	const char *from = NULL;

	while(pkg != end)
	{
		if(in_body)
		{
			len = add_to_string_array(&array, len, pkg);
		}
		else if(starts_with_lit(pkg, "version:"))
		{
			if(strcmp(pkg, IPC_VERSION) != 0)
			{
				break;
			}
		}
		else if(starts_with_lit(pkg, "from:"))
		{
			from = after_first(pkg, ':');
		}
		else if(starts_with_lit(pkg, "body:"))
		{
			type = after_first(pkg, ':');
			in_body = 1;
		}
		else
		{
			break;
		}
		pkg += strlen(pkg) + 1;
	}

	if(pkg != end)
	{
		LOG_ERROR_MSG("Discarded remote package due to field: `%s`", pkg);
	}
	else if(from == NULL)
	{
		LOG_ERROR_MSG("Discarded remote package due to missing from field");
	}
	else if(type == NULL)
	{
		LOG_ERROR_MSG("Discarded remote package due to missing body field");
	}
	else if(strcmp(type, ARGS_TYPE) == 0)
	{
		handle_args(ipc, &array, len);
	}
	else if(strcmp(type, EVAL_TYPE) == 0)
	{
		handle_expr(ipc, from, array, len);
	}
	else if(strcmp(type, EVAL_RESULT_TYPE) == 0)
	{
		handle_eval_result(ipc, array, len);
	}
	else if(strcmp(type, EVAL_ERROR_TYPE) == 0)
	{
		ipc->eval_result = NULL;
	}
	else
	{
		LOG_ERROR_MSG("Discarded remote package due to unknown type: `%s`", type);
	}

	free_string_array(array, len);
}

/* Handles received message with arguments. */
static void
handle_args(ipc_t *ipc, char ***array, int len)
{
	if(len == 0U)
	{
		return;
	}

	if(put_into_string_array(array, len, NULL) == len + 1)
	{
		ipc->locked = 1;
		ipc->args_cb(*array);
		ipc->locked = 0;
	}
}

/* Handles received message with expression to evaluate. */
static void
handle_expr(ipc_t *ipc, const char from[], char *array[], int len)
{
	char *result;

	if(len != 1U)
	{
		LOG_ERROR_MSG("Incorrect number of lines in expr packet: %d", len);
		return;
	}

	ipc->locked = 1;
	result = ipc->eval_cb(array[0]);
	ipc->locked = 0;
	if(result == NULL)
	{
		char *data[] = { NULL };
		if(format_and_send(ipc, from, data, EVAL_ERROR_TYPE) != 0)
		{
			LOG_ERROR_MSG("Failed to report evaluation failure");
		}
	}
	else
	{
		char *data[] = { result, NULL };
		if(format_and_send(ipc, from, data, EVAL_RESULT_TYPE) != 0)
		{
			LOG_ERROR_MSG("Failed to report evaluation result");
		}
		free(result);
	}
}

/* Handles answer about successful evaluation of expression. */
static void
handle_eval_result(ipc_t *ipc, char *array[], int len)
{
	if(len == 1U)
	{
		ipc->eval_result = array[0];
		array[0] = NULL;
	}
}

int
ipc_send(ipc_t *ipc, const char whom[], char *data[])
{
	return format_and_send(ipc, whom, data, ARGS_TYPE);
}

char *
ipc_eval(ipc_t *ipc, const char whom[], const char expr[])
{
	enum { MAX_USEC = 1000000, MAX_REPEATS = 20 };
	int repeats;

	char *data[] = { (char *)expr, NULL };
	if(format_and_send(ipc, whom, data, EVAL_TYPE) != 0)
	{
		LOG_ERROR_MSG("Failed to send expression");
		return NULL;
	}

	/* Using sleep is just easier than doing read with timeout due to differences
	 * between platforms... */
	repeats = 0;
	while(!ipc_check(ipc))
	{
		if(++repeats > MAX_REPEATS)
		{
			LOG_ERROR_MSG("Timed out on waiting for --remote-expr response");
			return NULL;
		}
		usleep(MAX_USEC/MAX_REPEATS);
	}

	return ipc->eval_result;
}

/* Formats and sends a message of specified type.  The data array should be NULL
 * terminated.  Returns zero on successful send and non-zero otherwise. */
static int
format_and_send(ipc_t *ipc, const char whom[], char *data[], const char type[])
{
	/* FIXME: this shouldn't have fixed size.  Or maybe it should be PIPE_BUF to
	 * guarantee atomic operation. */
	char pkg[8192];
	size_t len;
	char *name = NULL;
	int ret;

	/* Compose "header". */
	len = copy_str(pkg, sizeof(pkg), IPC_VERSION);
	len += MIN(snprintf(pkg + len, sizeof(pkg) - len, "from:%s",
				ipc_get_name(ipc)) + 1,
			(int)(sizeof(pkg) - len));
	len += MIN(snprintf(pkg + len, sizeof(pkg) - len, "body:%s", type) + 1,
			(int)(sizeof(pkg) - len));

	if(strcmp(type, ARGS_TYPE) == 0)
	{
		if(get_cwd(pkg + len, sizeof(pkg) - len) == NULL)
		{
			LOG_ERROR_MSG("Can't get working directory");
			return 1;
		}
		len += strlen(pkg + len) + 1;
	}

	while(*data != NULL)
	{
		len += copy_str(pkg + len, sizeof(pkg) - len, *data);
		++data;
	}

	if(whom == NULL)
	{
		name = get_the_only_target(ipc);
		if(name == NULL)
		{
			return 1;
		}
		whom = name;
	}

	ret = send_pkg(whom, pkg, len);

	free(name);
	return ret;
}

/* Performs actual sending of package to another instance.  Returns zero on
 * success and non-zero otherwise. */
static int
send_pkg(const char whom[], const char what[], size_t len)
{
#ifndef WIN32_PIPE_READ
	char path[PATH_MAX + 1];
	int fd;
	FILE *dst;
	uint32_t size;

	snprintf(path, sizeof(path), "%s/" PREFIX "%s", get_ipc_dir(), whom);

	fd = open(path, O_WRONLY);
	if(fd == -1)
	{
		LOG_SERROR_MSG(errno, "Failed to open destination pipe");
		return 1;
	}

	dst = fdopen(fd, "w");
	if(dst == NULL)
	{
		LOG_SERROR_MSG(errno, "Failed to turn file descriptor into a FILE");
		close(fd);
		return 1;
	}

	size = len;
	if(fwrite(&size, sizeof(size), 1U, dst) != 1U ||
			fwrite(what, len, 1U, dst) != 1U)
	{
		LOG_SERROR_MSG(errno, "Failed to write into a pipe");
		(void)fclose(dst);
		return 1;
	}

	if(fclose(dst) != 0)
	{
		LOG_SERROR_MSG(errno, "Failure on close a pipe");
	}
	return 0;
#else
	char path[PATH_MAX + 1];
	HANDLE h;
	uint32_t size;
	DWORD nwritten;

	snprintf(path, sizeof(path), "%s/" PREFIX "%s", get_ipc_dir(), whom);

	h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
			0, NULL);
	if(h == INVALID_HANDLE_VALUE)
	{
		return 1;
	}

	size = len;
	if(WriteFile(h, &size, sizeof(size), &nwritten, NULL) == FALSE ||
			nwritten != sizeof(size) ||
			WriteFile(h, what, len, &nwritten, NULL) == FALSE || nwritten != len)
	{
		CloseHandle(h);
		return 1;
	}

	CloseHandle(h);
	return 0;
#endif
}

/* Automatically picks target instance to send data to.  Returns newly allocated
 * string or NULL on error (no other instances or memory allocation failure). */
static char *
get_the_only_target(const ipc_t *ipc)
{
	int len;
	char *name;
	char **list = list_servers(ipc, &len);

	if(len == 0)
	{
		return NULL;
	}

	name = list[0];
	list[0] = NULL;
	free_string_array(list, len);

	return name;
}

char **
ipc_list(int *len)
{
	return list_servers(NULL, len);
}

/* Retrieves list with names of servers available for IPC excluding the one
 * specified by the ipc parameter, which can be NULL.  Returns the list which is
 * of the *len length. */
static char **
list_servers(const ipc_t *ipc, int *len)
{
	list_data_t data = { .ipc_dir = get_ipc_dir(), .ipc = ipc };

#ifndef WIN32_PIPE_READ
	if(enum_dir_content(data.ipc_dir, &add_to_list, &data) != 0)
	{
		*len = 0;
		return NULL;
	}
#else
	{
		char find_pat[PATH_MAX + 1];
		HANDLE hfind;
		WIN32_FIND_DATAA ffd;

		snprintf(find_pat, sizeof(find_pat), "%s/*", data.ipc_dir);
		hfind = FindFirstFileA(find_pat, &ffd);

		if(hfind == INVALID_HANDLE_VALUE)
		{
			*len = 0;
			return NULL;
		}

		do
		{
			if(add_to_list(ffd.cFileName, &ffd, &data) != 0)
			{
				break;
			}
		}
		while(FindNextFileA(hfind, &ffd));
		FindClose(hfind);
	}
#endif

	safe_qsort(data.lst, data.len, sizeof(*data.lst), &strsorter);

	*len = data.len;
	return data.lst;
}

/* Analyzes pipe and adds it to the list of pipes.  Returns zero on success or
 * non-zero on error. */
static int
add_to_list(const char name[], const void *data, void *param)
{
	list_data_t *const list_data = param;
	const ipc_t *ipc = list_data->ipc;

	if(!starts_with_lit(name, PREFIX))
	{
		return 0;
	}

	/* Skip ourself. */
	if(ipc != NULL &&
			stroscmp(name, get_last_path_component(ipc->pipe_path)) == 0)
	{
		return 0;
	}

	/* On Windows it's guaranteed to be a valid pipe. */
#ifndef WIN32_PIPE_READ
	{
		char path[PATH_MAX + 1];
		struct stat statbuf;
		snprintf(path, sizeof(path), "%s/%s", list_data->ipc_dir, name);
		if(stat(path, &statbuf) != 0 || !S_ISFIFO(statbuf.st_mode) ||
				!pipe_is_in_use(path) || os_access(path, R_OK) != 0)
		{
			return 0;
		}
	}
#endif

	list_data->len = add_to_string_array(&list_data->lst, list_data->len,
			name + strlen(PREFIX));
	return 0;
}

/* Retrieves directory where FIFO objects are created.  Returns the path. */
static const char *
get_ipc_dir(void)
{
#ifndef WIN32_PIPE_READ
	return get_tmpdir();
#else
	return "//./pipe";
#endif
}

#ifndef WIN32_PIPE_READ

/* Tries to open a pipe to check whether it has any readers or it's
 * abandoned.  Returns non-zero if somebody is reading from the pipe and zero
 * otherwise. */
static int
pipe_is_in_use(const char path[])
{
	const int fd = open(path, O_WRONLY | O_NONBLOCK);
	if(fd != -1 || errno != ENXIO)
	{
		if(fd != -1)
		{
			close(fd);
		}
		return 1;
	}
	return 0;
}

#endif

#else

#include <stddef.h> /* NULL */

int
ipc_enabled(void)
{
	return 0;
}

char **
ipc_list(int *len)
{
	*len = 0;
	return NULL;
}

ipc_t *
ipc_init(const char name[], ipc_args_cb args_cb, ipc_eval_cb eval_cb)
{
	return NULL;
}

void
ipc_free(ipc_t *ipc)
{
}

const char *
ipc_get_name(const ipc_t *ipc)
{
	return "";
}

int
ipc_check(ipc_t *ipc)
{
	return 0;
}

int
ipc_send(ipc_t *ipc, const char whom[], char *data[])
{
	return 1;
}

char *
ipc_eval(ipc_t *ipc, const char whom[], const char expr[])
{
	return NULL;
}

#endif

/* vim: set tabstop=2 softtabstop=2 shiftwidth=2 noexpandtab cinoptions-=(0 : */
/* vim: set cinoptions+=t0 filetype=c : */
