#!/bin/bash
# Copyright (C) 2026 Ivan Gaydardzhiev
# Licensed under the GPL-3.0-only

cd "$(dirname "$0")/.." || exit 2
Y=/proc/sys/kernel/yama/ptrace_scope
U=${SUDO_UID:-0}
G=${SUDO_GID:-0}
if [ "$U" = 0 ]; then
	echo "facts.sh: needs a non-root SUDO_UID"
	exit 2
fi
o=$(cat $Y) || exit 2
trap 'echo "$o" >$Y; echo "restored ptrace_scope=$(cat $Y)"' EXIT
P='pr() { local f r=""; for f in mem maps environ; do if { : <"/proc/$1/$f"; } 2>/dev/null; then r="$r $f=ok"; else r="$r $f=denied"; fi; done; echo "$r"; }'

A=(setpriv --reuid="$U" --regid="$G" --clear-groups)

asu() {
	"${A[@]}" "$@"
}

who() {
	echo "target pid=$1 comm=$(cat /proc/$1/comm) uid=$(stat -c %u /proc/$1)"
}

mf() {
	local l=$1 p=$2 m
	shift 2
	m=$(ls /proc/$p/map_files | head -n 1)
	if "$@" head -c 4 "/proc/$p/map_files/$m" >/dev/null 2>&1; then
		echo "map_files reader=$l open=ok"
	else
		echo "map_files reader=$l open=denied"
	fi
}

sleep 30 &
p=$!
"${A[@]}" sleep 30 &
q=$!
sleep 0.2
who $p
who $q
mf root $p env
mf root_on_other_uid $q env
mf owner $q asu
mf other_uid $p asu
kill $p $q
for s in 0 1 2; do
	echo "$s" >$Y || continue
	echo "scope=$s set=$(cat $Y)"
	"${A[@]}" sleep 30 &
	p=$!
	sleep 0.2
	who $p
	echo "reader=root:$(bash -c "$P; pr $p")"
	echo "reader=same_uid_unrelated:$(asu bash -c "$P; pr $p")"
	echo "reader=same_uid_parent:$(asu bash -c "$P; sleep 30 & q=\$!; sleep 0.2; pr \$q; kill \$q")"
	echo "wtn_as_same_uid_unrelated: $(asu ./wtn "$p" | grep -E '^[0-9]+ u ' | cut -d' ' -f2- | tr '\n' ';')"
	kill $p
done
