/*
 * Copyright (C) 2026 Ivan Gaydardzhiev
 * Licensed under the GPL-3.0-only
 */

#include "c.h"

int main(void)
{
	char **e;
	size_t n = 1, l = 0;
	char *b;

	for (e = environ; *e; e++)
		n += strlen(*e) + 1;
	b = malloc(n);
	if (!b)
		sk("malloc");
	for (e = environ; *e; e++) {
		if (!strncmp(*e, "LD_", 3))
			continue;
		strcpy(b + l, *e);
		l += strlen(*e) + 1;
	}
	if (prctl(PR_SET_MM, PR_SET_MM_ENV_START, (unsigned long)b, 0, 0) < 0)
		sk("PR_SET_MM_ENV_START");
	if (prctl(PR_SET_MM, PR_SET_MM_ENV_END, (unsigned long)(b + l), 0, 0) < 0)
		sk("PR_SET_MM_ENV_END");
	sp(getpid());
	hold();
}
