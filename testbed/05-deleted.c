/*
 * Copyright (C) 2026 Ivan Gaydardzhiev
 * Licensed under the GPL-3.0-only
 */

#include "c.h"

int main(void)
{
	const char *p = wt("sleep");
	char l[4096];
	ssize_t n = 0;
	char e[64];
	pid_t c;

	cpf("/bin/sleep", p, 0755);
	c = fork();
	if (c < 0)
		sk("fork");
	if (!c) {
		execl(p, "sleep", "1000", (char *)NULL);
		_exit(1);
	}
	snprintf(e, sizeof e, "/proc/%d/exe", c);
	while (n <= 0 || strncmp(l, p, (size_t)n)) {
		n = readlink(e, l, sizeof l);
		usleep(1000);
	}
	if (unlink(p) < 0)
		sk("unlink");
	sp(c);
	waitpid(c, NULL, 0);
	return 0;
}
