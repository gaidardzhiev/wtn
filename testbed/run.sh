#!/bin/bash
# Copyright (C) 2026 Ivan Gaydardzhiev
# Licensed under the GPL-3.0-only

cd "$(dirname "$0")/.." || exit 2

if [ "$(id -u)" != 0 ] || ! grep -Eq '^ +0 +0 +4294967295$' /proc/self/uid_map; then
	echo "run.sh: must run as real root" >&2
	exit 2
fi
B=testbed/bin
CF="-O2 -Wall -D_GNU_SOURCE -Itestbed"
np=0
nf=0
ns=0

bld() {
	local f n
	make -s wtn || exit 2
	if ! ./wtn -T >/dev/null; then
		echo "run.sh: ./wtn does not run" >&2
		exit 2
	fi
	rm -rf $B
	mkdir -p $B results || exit 2
	rm -f results/*.txt
	gcc $CF -shared -fPIC -o $B/libp.so testbed/libp.c || exit 2
	gcc $CF -shared -fPIC -o $B/libs.so testbed/libs.c || exit 2
	gcc $CF -shared -fPIC -o $B/libhello.so testbed/libhello.c || exit 2
	gcc $CF -static -o $B/16-payload testbed/16-payload.c || exit 2
	for f in testbed/[0-9]*.c; do
		n=$(basename "$f" .c)
		case $n in
		16-payload) ;;
		09-libpath) gcc $CF -o $B/$n "$f" -L$B -lhello || exit 2 ;;
		11-exe_repoint) gcc $CF -static -o $B/$n "$f" || exit 2 ;;
		*) gcc $CF -o $B/$n "$f" || exit 2 ;;
		esac
	done
}

l1() {
	local i
	for i in $(seq 100); do
		[ -s "$1" ] && break
		kill -0 "$2" 2>/dev/null || break
		sleep 0.05
	done
	head -n 1 "$1"
}

kinds() {
	grep -E "^[0-9-]+ $2 " "$1" | cut -d' ' -f3 | sort -u | tr '\n' ' '
}

judge() {
	local k bad=""
	for k in $2; do
		[ "$k" = none ] && continue
		grep -Eq "^[0-9]+ . $k( |$)" "$1" || bad="$bad missing:$k"
	done
	for k in $3; do
		grep -Eq "^[0-9]+ . $k( |$)" "$1" && bad="$bad forbidden:$k"
	done
	for k in $(kinds "$1" '!'); do
		[[ " $2 " == *" $k "* ]] || bad="$bad extra:$k"
	done
	if [ "$2" = none ] && [ -n "$(kinds "$1" '[?]')" ]; then
		bad="$bad extra:?"
	fi
	if [ -n "$4" ] && ! grep -Eq "$4" "$1"; then
		bad="$bad nomatch:$4"
	fi
	echo "$bad"
}

fk() {
	grep -E '^[0-9-]+ [!?u.] ' "$1" | cut -d' ' -f3 | grep -vxE 'EXE|HASH|MAPS|PATH_GONE|PATH_HOST_MISSING' | sort -u | tr '\n' ' '
}

row() {
	printf '%-20s %-34s %-60s %s\n' "$1" "$2" "$3" "$4"
}

skip() {
	ns=$((ns + 1))
	row "$1" "$2" - "SKIP($3)"
}

rc() {
	local c=$1 ex=$2 fo=$3 re=$4 d=$WB/$1 o p j st r bad
	shift 4
	o=$d.out
	mkdir -p "$d"
	chmod 755 "$d"
	(cd $B && WT=$d exec "$@") >"$o" 2>&1 &
	j=$!
	p=$(l1 "$o" $j)
	if ! [[ $p =~ ^[0-9]+$ ]]; then
		wait $j
		st=$?
		if [ $st = 77 ]; then
			skip "$c" "$ex" "$p"
		else
			nf=$((nf + 1))
			row "$c" "$ex" - "FAIL(exit $st: $p)"
		fi
		return
	fi
	sleep 0.3
	r=results/$c.txt
	./wtn -x -v "$p" >"$r" 2>&1
	kill -9 "$p" 2>/dev/null
	kill -9 $j 2>/dev/null
	wait $j 2>/dev/null
	bad=$(judge "$r" "$ex" "$fo" "$re")
	if [ -z "$bad" ]; then
		np=$((np + 1))
		row "$c" "$ex" "$(fk "$r")" PASS
	else
		nf=$((nf + 1))
		row "$c" "$ex" "$(fk "$r")" "FAIL($bad )"
	fi
}

run() {
	row case expected found result
	rc 00-control none "" "" ./00-control
	rc 01-longname none "" "" ./01-longname
	rc 02-argv_exec "ID_ARGV_EXE ID_COMM_ARGV" "" "" ./02-argv_exec
	rc 03-argv_overwrite "ID_ARGV_EXE ID_COMM_ARGV" "" "" ./03-argv_overwrite
	rc 04-prctl_name "ID_COMM_EXE ID_COMM_ARGV" "" "" ./04-prctl_name
	rc 05-deleted EXE_DELETED "" "" ./05-deleted
	rc 06-memfd EXE_MEMFD "" "" ./06-memfd
	rc 07-preload "LIB_ODD_PATH ENV_LOADER_VAR" "" "" env LD_PRELOAD=./libp.so ./07-preload
	rc 08-preload_scrub LIB_ODD_PATH ENV_LOADER_VAR "" env LD_PRELOAD=./libs.so ./08-preload_scrub
	rc 09-libpath "LIB_ODD_PATH ENV_LOADER_VAR" "" "" env LD_LIBRARY_PATH=. ./09-libpath
	rc 10-env_prctl "ENV_RELOCATED ENV_STRAY_LOADER_VAR" "" "" env LD_PRELOAD=./libp.so ./10-env_prctl
	rc 11-exe_repoint MEM_MISMATCH "" " MEM_MISMATCH .* via=exe " ./11-exe_repoint
	rc 12-selfpatch MEM_MISMATCH "" " MEM_MISMATCH .* diff_bytes=1 " ./12-selfpatch
	rc 13-anon_exec "ANON_EXEC WX_MAP" "" "" ./13-anon_exec
	rc 14-filecaps CAPS_UNPRIVILEGED "" "" ./14-filecaps
	rc 15-setuid IDS_SPLIT "" "" ./15-setuid
	rc 16-mntns_pidns "PATH_HOST_MISMATCH NS_NESTED" PATH_ROOT_MISMATCH "" ./16-mntns_pidns ./16-payload
	rc 17-orphan LINEAGE "" "" ./17-orphan
	rc 18-root_swap PATH_ROOT_MISMATCH PATH_HOST_MISMATCH "" ./18-root_swap ./16-payload
	./wtn -a >results/scan-all.txt 2>&1
	testbed/facts.sh >results/facts.txt 2>&1
	echo "$np pass, $nf fail, $ns skip"
	[ $nf = 0 ]
}

bld
WB=$(mktemp -d /var/tmp/wt.XXXXXX) || exit 2
chmod 755 "$WB"
trap 'rm -rf "$WB"' EXIT
run | tee results/table.txt
st=${PIPESTATUS[0]}
echo "$st" >results/exit.txt
if [ -n "$SUDO_UID" ]; then
	chown -R "$SUDO_UID:$SUDO_GID" results $B wtn
fi
exit "$st"
