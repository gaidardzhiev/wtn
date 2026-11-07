/*
 * Copyright (C) 2026 Ivan Gaydardzhiev
 * Licensed under the GPL-3.0-only
 */

#include "c.h"

int main(void)
{
	sp(getpid());
	execl("/bin/sleep", "camouflage", "1000", (char *)NULL);
	sk("exec");
}
