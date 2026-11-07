# Copyright (C) 2026 Ivan Gaydardzhiev
# Licensed under the GPL-3.0-only

CC=gcc
CFLAGS=-std=c99 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -O2 -Wall -Wextra -Werror

wtn: wtn.c
	$(CC) $(CFLAGS) -static -o wtn wtn.c

dynamic: wtn.c
	$(CC) $(CFLAGS) -o wtn wtn.c

paper: reflections_on_trusting_proc.pdf

reflections_on_trusting_proc.pdf: reflections_on_trusting_proc.typ
	typst compile reflections_on_trusting_proc.typ reflections_on_trusting_proc.pdf

clean:
	rm -f wtn

.PHONY: dynamic paper clean
