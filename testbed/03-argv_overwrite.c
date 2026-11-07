/*
 * Copyright (C) 2026 Ivan Gaydardzhiev
 * Licensed under the GPL-3.0-only
 */

#include "c.h"

int main(int ac, char **av)
{
	char *e = av[ac - 1] + strlen(av[ac - 1]);

	memset(av[0], 0, (size_t)(e - av[0]));
	strcpy(av[0], "[kworker/0:1]");
	sp(getpid());
	hold();
}
