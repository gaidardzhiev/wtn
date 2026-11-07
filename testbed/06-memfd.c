/*
 * Copyright (C) 2026 Ivan Gaydardzhiev
 * Licensed under the GPL-3.0-only
 */

#include "c.h"

int main(void)
{
	char *a[] = { "sleep", "1000", NULL };
	int s = open("/bin/sleep", O_RDONLY);
	int m = memfd_create("ghost", MFD_CLOEXEC);

	if (s < 0 || m < 0)
		sk("memfd_create");
	cpd(s, m);
	sp(getpid());
	fexecve(m, a, environ);
	sk("fexecve");
}
