/*
 * Copyright (C) 2026 Ivan Gaydardzhiev
 * Licensed under the GPL-3.0-only
 */

#include "c.h"

static unsigned long long ce(void)
{
	char l[256];
	unsigned long long v = 0;
	FILE *f = fopen("/proc/self/status", "r");

	if (!f)
		sk("status");
	while (fgets(l, sizeof l, f))
		sscanf(l, "CapEff: %llx", &v);
	fclose(f);
	return v;
}

static int run(void)
{
	char b[1];
	int f;

	if (!ce()) {
		errno = EPERM;
		sk("CapEff is 0");
	}
	f = open("/etc/shadow", O_RDONLY);
	if (f >= 0 && read(f, b, 1) < 0)
		sk("read /etc/shadow");
	sp(getpid());
	hold();
	return 0;
}

int main(int ac, char **av)
{
	const char *p = wt("filecaps");
	char c[4200];

	if (ac > 1)
		return run();
	cpf("/proc/self/exe", p, 0755);
	snprintf(c, sizeof c, "setcap cap_dac_read_search+ep %s", p);
	if (system(c) != 0) {
		errno = EPERM;
		sk("setcap failed or absent");
	}
	if (chdir("/") < 0)
		sk("chdir");
	execlp("setpriv", "setpriv", "--reuid=65534", "--regid=65534", "--clear-groups",
		p, "run", (char *)NULL);
	sk("setpriv absent");
}
