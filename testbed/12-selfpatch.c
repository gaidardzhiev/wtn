/*
 * Copyright (C) 2026 Ivan Gaydardzhiev
 * Licensed under the GPL-3.0-only
 */

#include "c.h"

__attribute__((noinline, used)) static int unu(int x)
{
	return x * 3 + 1;
}

int main(void)
{
	volatile unsigned char *p = (volatile unsigned char *)unu;
	char *g = (char *)((unsigned long)unu & ~4095UL);

	if (mprotect(g, 4096, PROT_READ | PROT_WRITE | PROT_EXEC) < 0)
		sk("mprotect rwx");
	*p ^= 0xff;
	if (mprotect(g, 4096, PROT_READ | PROT_EXEC) < 0)
		sk("mprotect r-x");
	sp(getpid());
	hold();
}
