/*
 * Copyright (C) 2026 Ivan Gaydardzhiev
 * Licensed under the GPL-3.0-only
 */

#include "c.h"

int main(void)
{
	const char *p = wt("systemd-resolved-long-name");

	cpf("/bin/sleep", p, 0755);
	sp(getpid());
	execl(p, "systemd-resolved-long-name", "1000", (char *)NULL);
	sk("exec");
}
