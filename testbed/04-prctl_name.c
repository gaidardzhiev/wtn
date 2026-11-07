/*
 * Copyright (C) 2026 Ivan Gaydardzhiev
 * Licensed under the GPL-3.0-only
 */

#include "c.h"

int main(void)
{
	if (prctl(PR_SET_NAME, "septicon", 0, 0, 0) < 0)
		sk("PR_SET_NAME");
	sp(getpid());
	hold();
}
