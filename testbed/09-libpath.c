/*
 * Copyright (C) 2026 Ivan Gaydardzhiev
 * Licensed under the GPL-3.0-only
 */

#include "c.h"

int hello(void);

int main(void)
{
	if (hello() != 42)
		return 1;
	sp(getpid());
	hold();
}
