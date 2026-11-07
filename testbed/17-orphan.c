/*
 * Copyright (C) 2026 Ivan Gaydardzhiev
 * Licensed under the GPL-3.0-only
 */

#include "c.h"

static void leaf(int w)
{
	int v[2], m = getppid();

	while (getppid() == m)
		usleep(1000);
	v[0] = getpid();
	v[1] = getppid();
	if (write(w, v, sizeof v) != sizeof v)
		_exit(1);
	hold();
}

int main(void)
{
	int d[2], v[2];
	pid_t m;

	if (pipe(d) < 0)
		sk("pipe");
	m = fork();
	if (m < 0)
		sk("fork");
	if (!m) {
		if (fork() == 0)
			leaf(d[1]);
		_exit(0);
	}
	waitpid(m, NULL, 0);
	if (read(d[0], v, sizeof v) != sizeof v)
		sk("read");
	if (v[1] != 1) {
		kill(v[0], SIGKILL);
		printf("reparented to subreaper pid %d, not 1\n", v[1]);
		fflush(stdout);
		return 77;
	}
	sp(v[0]);
	return 0;
}
