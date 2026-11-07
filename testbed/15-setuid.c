/*
 * Copyright (C) 2026 Ivan Gaydardzhiev
 * Licensed under the GPL-3.0-only
 */

#include "c.h"

int main(void)
{
	const char *p = wt("sleep");

	cpf("/bin/sleep", p, 0755);
	if (chown(p, 0, 0) < 0 || chmod(p, 04755) < 0)
		sk("setuid-root copy");
	if (chdir("/") < 0)
		sk("chdir");
	sp(getpid());
	execlp("setpriv", "setpriv", "--reuid=65534", p, "1000", (char *)NULL);
	sk("setpriv absent");
}
