/* SPDX-License-Identifier: MIT-0 */
/* Lightly modified from the sd_notify manpage.  */

#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

int
systemd_notify(const char *message)
{
	union sockaddr_union {
		struct sockaddr sa;
		struct sockaddr_un sun;
	} socket_addr = {
		.sun.sun_family = AF_UNIX,
	};

	size_t path_length, message_length;
	int fd = -1;
	const char *socket_path;

	/* Verify the argument first */
	if (!message)
		return -EINVAL;

	message_length = strlen(message);
	if (message_length == 0)
		return -EINVAL;

	/* If the variable is not set, the protocol is a noop */
	socket_path = getenv("NOTIFY_SOCKET");
	if (!socket_path)
		return 0; /* Not set? Nothing to do */

	/* Only AF_UNIX is supported, with path or abstract sockets */
	if (socket_path[0] != '/' && socket_path[0] != '@')
		return -EAFNOSUPPORT;

	path_length = strlen(socket_path);
	/* Ensure there is room for NUL byte */
	if (path_length >= sizeof(socket_addr.sun.sun_path))
		return -E2BIG;

	memcpy(socket_addr.sun.sun_path, socket_path, path_length);

	/* Support for abstract socket */
	if (socket_addr.sun.sun_path[0] == '@')
		socket_addr.sun.sun_path[0] = 0;

	fd = socket(AF_UNIX, SOCK_DGRAM|SOCK_CLOEXEC, 0);
	if (fd < 0)
		return -errno;

	if (connect(fd, &socket_addr.sa, offsetof(struct sockaddr_un, sun_path) + path_length) != 0) {
		close(fd);
		return -errno;
	}

	ssize_t written = write(fd, message, message_length);
	if (written != (ssize_t) message_length) {
		close(fd);
		return written < 0 ? -errno : -EPROTO;
	}

	return 1; /* Notified! */
}
