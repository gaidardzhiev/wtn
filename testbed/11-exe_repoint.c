/*
 * Copyright (C) 2026 Ivan Gaydardzhiev
 * Licensed under the GPL-3.0-only
 */

#include "c.h"

static void mv(unsigned long s, unsigned long e, const char *r)
{
	size_t z = e - s;
	int pr = (r[0] == 'r' ? PROT_READ : 0) | (r[1] == 'w' ? PROT_WRITE : 0) | (r[2] == 'x' ? PROT_EXEC : 0);
	char *t = mmap(NULL, z, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	if (t == MAP_FAILED)
		sk("mmap");
	memcpy(t, (void *)s, z);
	if (mprotect(t, z, pr) < 0 || mremap(t, z, z, MREMAP_MAYMOVE | MREMAP_FIXED, (void *)s) == MAP_FAILED)
		sk("mremap");
}

static void unmap(void)
{
	static unsigned long s[32], e[32];
	static char r[32][8];
	char l[4608], p[4096], x[4096];
	ssize_t n = readlink("/proc/self/exe", x, sizeof x - 1);
	FILE *f = fopen("/proc/self/maps", "r");
	int c = 0, i;

	if (n < 0 || !f)
		sk("maps");
	x[n] = 0;
	while (c < 32 && fgets(l, sizeof l, f)) {
		p[0] = 0;
		sscanf(l, "%lx-%lx %7s %*s %*s %*s %4095s", &s[c], &e[c], r[c], p);
		c += !strcmp(p, x);
	}
	fclose(f);
	for (i = 0; i < c; i++)
		mv(s[i], e[i], r[i]);
}

int main(void)
{
	int f = open("/bin/true", O_RDONLY);
	char *p;

	if (f < 0)
		sk("open /bin/true");
	if (prctl(PR_SET_MM, PR_SET_MM_EXE_FILE, (unsigned long)f, 0, 0) < 0) {
		if (errno != EBUSY)
			sk("PR_SET_MM_EXE_FILE");
		unmap();
		if (prctl(PR_SET_MM, PR_SET_MM_EXE_FILE, (unsigned long)f, 0, 0) < 0)
			sk("PR_SET_MM_EXE_FILE after unmap");
	}
	p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE, f, 0x1000);
	if (p == MAP_FAILED)
		sk("mmap /bin/true");
	p[100] ^= 0xff;
	if (mprotect(p, 4096, PROT_READ | PROT_EXEC) < 0)
		sk("mprotect r-x");
	sp(getpid());
	hold();
}
