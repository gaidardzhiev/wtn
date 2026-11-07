/*
 * Copyright (C) 2026 Ivan Gaydardzhiev
 * Licensed under the GPL-3.0-only
 */

#include <string.h>

extern char **environ;

__attribute__((constructor)) static void scrub(void)
{
	char **e;

	for (e = environ; *e; e++)
		if (!strncmp(*e, "LD_PRELOAD=", 11))
			memset(*e, 0, strlen(*e));
}
