#include <sys/un.h>

#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#define SOCK_PATH_MAX sizeof(((struct sockaddr_un *)0)->sun_path)

#define EUNSET "$XDG_RUNTIME_DIR is unset"
#define EEMPTY "$XDG_RUNTIME_DIR is empty"
#define ELONG  "Pathname $XDG_RUNTIME_DIR/ewd is too long"

const char *
ewd_sock_path(void)
{
#define _die(x) diex("%s", x)
	char *dir;
	static char buf[SOCK_PATH_MAX];

	if (!*buf) {
		dir = getenv("EWD_SOCK_PATH");
		if (dir == NULL || *dir == 0)
			dir = getenv("XDG_RUNTIME_DIR");

		if (dir == NULL)
			_die(EUNSET);
		if (*dir == 0)
			_die(EEMPTY);

		if ((size_t)snprintf(buf, sizeof(buf), "%s/ewd", dir) >= SOCK_PATH_MAX)
			_die(ELONG);
	}
	return buf;
#undef _die
}
