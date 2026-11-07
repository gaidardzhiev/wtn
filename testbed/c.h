/*
 * Copyright (C) 2026 Ivan Gaydardzhiev
 * Licensed under the GPL-3.0-only
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>

extern char **environ;

static inline void sk(const char *m)
{
	printf("%s: %s (errno %d)\n", m, strerror(errno), errno);
	fflush(stdout);
	exit(77);
}

static inline void sp(int p)
{
	if (dprintf(1, "%d\n", p) < 0)
		exit(1);
}

static inline void hold(void)
{
	for (;;)
		pause();
}

static inline const char *wt(const char *n)
{
	static char b[2][4096];
	static int i;
	const char *d = getenv("WT");

	if (!d) {
		errno = EINVAL;
		sk("WT not set");
	}
	i ^= 1;
	snprintf(b[i], sizeof b[i], "%s/%s", d, n);
	return b[i];
}

static inline void cpd(int s, int d)
{
	char b[65536];
	ssize_t r;

	while ((r = read(s, b, sizeof b)) > 0)
		if (write(d, b, (size_t)r) != r)
			sk("write");
	if (r < 0)
		sk("read");
}

static inline void cpf(const char *s, const char *d, int m)
{
	int a = open(s, O_RDONLY);
	int b = open(d, O_WRONLY | O_CREAT | O_TRUNC, m);

	if (a < 0 || b < 0)
		sk("open copy");
	cpd(a, b);
	if (close(a) < 0 || close(b) < 0)
		sk("close");
}

static inline int kid(int c)
{
	char n[64];
	int k = 0;
	FILE *f;

	snprintf(n, sizeof n, "/proc/%d/task/%d/children", c, c);
	f = fopen(n, "r");
	if (!f)
		return 0;
	if (fscanf(f, "%d", &k) != 1)
		k = 0;
	fclose(f);
	return k;
}

static inline int up(int k)
{
	char n[64], l[4096];
	ssize_t r;

	snprintf(n, sizeof n, "/proc/%d/exe", k);
	r = readlink(n, l, sizeof l - 1);
	if (r < 10)
		return 0;
	l[r] = 0;
	return !strcmp(l + r - 10, "/bin/sleep");
}

static inline void wk(pid_t c)
{
	int k = 0, st;

	while (!k || !up(k)) {
		if (waitpid(c, &st, WNOHANG) == c) {
			errno = EPERM;
			sk("unshare failed");
		}
		k = kid(c);
		usleep(1000);
	}
	sp(k);
	waitpid(c, NULL, 0);
}
