#include <sys/un.h>

#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

#define SOCK_PATH_MAX sizeof(((struct sockaddr_un *)0)->sun_path)

enum {
	EUNSET,
	EEMPTY,
	ELONG,
};

static const char *errors[] = {
	[EUNSET] = "$XDG_RUNTIME_DIR is unset",
	[EEMPTY] = "$XDG_RUNTIME_DIR is empty",
	[ELONG] = "Pathname $XDG_RUNTIME_DIR/ewd is too long",
};

const char *
ewd_sock_path(void)
{
#define _die(x) diex("%s", errors[x])
	char *dir;
	static char buf[SOCK_PATH_MAX];

	if (!*buf) {
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
