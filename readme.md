wtn
====

## NAME

wtn - compare what /proc says about a running process

## SYNOPSIS

```
wtn [-a] [-x] [-v] [-T] [pid ...]
```

## DESCRIPTION

*wtn* reads the files under */proc* for each process it is given, compares them, and prints one line for each disagreement. It is one C file for x86-64 Linux, linked statically.

The files under */proc* have two kinds of producer. The kernel produces *exe*, *maps*, *status*, the fields of *stat*, and the contents of *mem* as mapped from its own bookkeeping. The bytes of *cmdline* and *environ* belong to the process: they sit in its writable memory and the kernel passes them through. A tool that prints the second kind repeats the process's statement about itself. *comm* lies between the two: the kernel stores it and the thread may set it. *wtn* compares sources whose producers differ.

Three witnesses carry the comparison. EXE is the bytes of the file opened through */proc/PID/exe*. PATH is the file named by that link, opened as ROOT through */proc/PID/root* and as HOST by the bare path. MEM is the bytes of the mapped code, read through */proc/PID/mem* and compared with the backing file.

A disagreement is a fact about the system. Many are innocent: a multi-call binary, a symlinked interpreter, a container, a binary replaced by a package upgrade.

*wtn* reads and prints. Every file is opened read-only, and it repairs nothing.

## BUILDING

A C99 compiler and a static C library are needed. The Makefile uses *gcc*.

```
make
```

builds *wtn* with

```
gcc -std=c99 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -O2 -Wall -Wextra -Werror -static -o wtn wtn.c
```

The default target is static because a statically linked program does not pass through *ld.so*, so the loader variables that *wtn* inspects in other processes have no effect on it, and the single file can be copied into a container or another root. With gcc 16.2.0 and glibc the binary is 831120 bytes: 703638 of text, 24680 of data, and 17107776 of bss.

```
make dynamic
```

writes the same name without `-static`, 40232 bytes. `make` then counts *wtn* as up to date, so `make clean` must come before a static rebuild. `make clean` removes *wtn*.

## INSTALLATION

The binary is self-contained. Copy it to a directory in the search path:

```
install -m 755 wtn /usr/local/bin
```

There is no install target and nothing else to install.

## OPTIONS

```
-a    Examine every process: the pids named on the command
      line, then every numeric entry of /proc, skipping wtn
      itself and the pids already done.  After that walk,
      stat /etc/ld.so.preload and report it if it is not
      empty.  That check runs only with -a.
-x    Follow PATH_ROOT_MISMATCH, PATH_HOST_MISMATCH and
      MEM_MISMATCH with a hex dump of both sides.
-v    Print the lines of level `.'.
-T    Hash three FIPS 180-4 test vectors, the empty string,
      abc, and a million a, print ok or FAIL, and exit 0 or
      2.  Any pids are ignored.
```

A pid is a positive decimal number no larger than `INT_MAX`. At most 4096 distinct pids may be named. A repeated pid counts once and is processed once. Pid 0, any other argument, a 4097th distinct pid, or no pid without *-a* prints the usage line on standard error and exits 2.

## WITNESSES

EXE opens */proc/PID/exe* and hashes the whole file with SHA-256, in reads of 64 KiB. Its producer is the kernel's record of the file. The open reached the file for a binary whose name was deleted and for a memfd.

PATH takes the text of the same link, removes a trailing ` (deleted)`, and opens it two ways. ROOT opens */proc/PID/root* followed by the path, which resolves the name in the process's view of the file system. HOST opens the bare path in the view of *wtn*, which is what `sha256sum $(readlink /proc/PID/exe)` does. Each is hashed whole and the hash is compared with EXE. Both opens use `O_NONBLOCK` and accept only regular files, because the target controls the path and a fifo there would hang the tool. The producer is the file system's name lookup at this moment.

MEM compares mapped bytes with the file behind them. A mapping is compared when its permissions begin `r-x` and its path begins `/`. A path beginning */memfd:* is compared only if it is the main executable, which is the mapping whose path equals the text of the link exactly, ` (deleted)` included. The producer is the pages present.

The backing file is chosen in this order:

```
via=exe        the main executable: the descriptor EXE opened,
               so a deleted file and a memfd are compared with
               their real bytes
via=map_files  any other mapping: /proc/PID/map_files/START-END
via=root_path  if that fails and the path has no ` (deleted)'
               suffix: /proc/PID/root followed by the path
```

A deleted library has no third choice. The length is the size of the mapping, clamped to what the file holds past the mapping's offset. A file that ends at or before the offset is skipped and not counted. *wtn* then reads memory at `start + o` and the file at `offset + o`, in chunks of up to 64 KiB, counts each differing byte, keeps the position of the first, and feeds each side to its own SHA-256. Anonymous mappings have no file and are not compared. Step 4 reports the executable ones.

## PROCEDURE

*wtn* runs steps 0 to 6 for each pid, in order.

Step 0 reads *status*. Without a `VmSize:` line the process has no user address space, as with a kernel thread or a zombie, and *wtn* prints `KTHREAD` and ends the pid. A failed read also ends it.

Step 1 reads *comm*, the *exe* link, and *cmdline*, whose first string up to a NUL is `argv[0]`. *comm* holds at most 15 visible bytes for a user process, so comparisons with it use the first 15 bytes of each side. `ID_COMM_EXE` compares *comm* with the basename of the link target, `ID_COMM_ARGV` compares *comm* with the basename of `argv[0]`, and `ID_ARGV_EXE` compares the two basenames in full. An empty *comm*, an empty `argv[0]`, or an unreadable link skips the checks that need it. The information line `EXE` follows.

Step 2 tests the link text: a suffix ` (deleted)` gives `EXE_DELETED`, and a prefix `/memfd:` gives `EXE_MEMFD`. A memfd link carries both, so a memfd gives both. It then opens *exe*, keeps the descriptor, hashes the file, and prints `HASH witness=exe`. If the open or the hash fails, step 3 is skipped.

Step 3 hashes ROOT and HOST. For a deleted file or a memfd it prints `PATH_GONE` and does nothing else. A differing ROOT gives `PATH_ROOT_MISMATCH`, at level `!`, because the process's own view contradicts the file it runs. `ENOENT` or `ENOTDIR` on ROOT can also mean the process died, so *wtn* tests */proc/PID/stat*: `GONE` if that has vanished, otherwise `PATH_ROOT_MISSING`. A differing HOST gives `PATH_HOST_MISMATCH`, at level `?`, because a container is an innocent cause. An absent HOST gives the information line `PATH_HOST_MISSING`. Each hash that succeeds prints a `HASH` line.

Step 4 reads *maps* in blocks of 64 KiB and splits it into lines of at most 4095 bytes. Each line gives start, end, permissions, offset, and path, after skipping device and inode. `\012` in a path is decoded to a newline, because `readlink` returns the raw byte and the two strings are compared. *[stack]* and *[heap]* are kept apart from the table. MEM then runs and gives `MEM_MISMATCH` for a mapping that differs, with the count, the address of the first difference, and both hashes. Four walks of the table follow, and they run even when *mem* could not be read, because they need only *maps*: `ANON_EXEC`, `WX_MAP`, `LIB_DELETED`, `LIB_ODD_PATH`. The step ends with `MAPS count=N exec_checked=M`, where M counts the mappings compared.

Step 5 reads *stat*. *comm* is field 2 and may hold any bytes, so *wtn* searches for the last `)` and starts at field 3 two bytes later, then splits on spaces. It keeps field 7, `tty_nr`, and fields 48 to 51: `arg_start`, `arg_end`, `env_start`, `env_end`. The kernel prints those four addresses as 0 for a reader without ptrace access. When all four are 0, *wtn* prints `UNVERIFIED which=stat_mm`, a failure to look and not a finding, and skips the range test, the stray scan, and `LINEAGE`. Otherwise an environment range outside *[stack]* gives `ENV_RELOCATED` and an argument range outside it gives `ARGV_RELOCATED`. *environ* is then read, up to 128 KiB, and split on NUL. An entry beginning `LD_PRELOAD=`, `LD_LIBRARY_PATH=`, or `LD_AUDIT=` gives `ENV_LOADER_VAR` with its value. This check runs even after `stat_mm`. Last comes the byte scan, which needs an open *mem* and a parsed *stat*. It reads *[stack]* and then *[heap]*, at most 64 MiB of each, in chunks of 64 KiB that overlap by 32 bytes, looking for the same three names followed by `=` anywhere in the bytes. A hit inside the current environment range is the live environment and is ignored. Any other hit gives `ENV_STRAY_LOADER_VAR` with its address. Reports stop at 8 per pid, across both regions.

Step 6 reads *status* again. A non-zero effective uid with a non-zero `CapEff` gives `CAPS_UNPRIVILEGED`. The executable's mode is not read. `fgetxattr` on the descriptor from step 2 asks for `security.capability`, and the finding carries `filecaps=yes` or `filecaps=no`. `ENODATA` and `ENOTSUP` mean no. A real uid different from the effective uid gives `IDS_SPLIT`. More than one value in `NSpid:` gives `NS_NESTED`, with the values joined by `/`. A `PPid:` of 1 with *stat* parsed gives `LINEAGE`, with the first line of *cgroup* after its last `:` and the `tty_nr` value.

## OUTPUT

Each line has fields separated by single spaces:

```
<pid> <level> <KIND> <key=value ...>
```

The pid is `-` for the */etc/ld.so.preload* check, the one check tied to no process. There are four levels:

```
!    two witnesses contradict each other
?    a disagreement that needs a person's judgement
u    a witness could not look
.    information, printed only with -v
```

Strings in values are escaped as `\ooo`, three octal digits: every byte up to and including space, DEL, and backslash. *comm* and argv are chosen by the process, and a newline in either would otherwise print a line that reads as a finding. Bytes above DEL pass unchanged. Decimal numbers are plain, addresses have `0x`, a range is two addresses joined by `-`, and `capeff` has 16 hex digits.

With *-x*, a byte comparison is followed by a dump of the two sides. Each side prints the row of 16 bytes holding the first difference and the rows before and after it, when they exist. A `diff` line after the middle row of the second side has `^^` under each differing byte of that row. Row labels are `mem` and `file` for MEM, and `exe` with `root` or `host` for PATH. Memory rows carry virtual addresses and file rows carry offsets. Bytes past the end of a side print as `..`. From case 12, a process that flipped one byte of its own text, with the first line cut:

```
11150 ! MEM_MISMATCH map=0x560b1d8c0000-0x560b1d8c1000 off=0x1000 path=/home ...
  mem  0000560b1d8c0240: f3 0f 1e fa e9 77 ff ff  ff 0f 1f 80 00 00 00 00  |.....w..........|
  mem  0000560b1d8c0250: 72 44 7f 01 c3 66 66 2e  0f 1f 84 00 00 00 00 00  |rD...ff.........|
  mem  0000560b1d8c0260: 55 48 89 fd 53 48 83 ec  08 e8 c2 fd ff ff 8b 18  |UH..SH..........|
  file 0000000000001240: f3 0f 1e fa e9 77 ff ff  ff 0f 1f 80 00 00 00 00  |.....w..........|
  file 0000000000001250: 8d 44 7f 01 c3 66 66 2e  0f 1f 84 00 00 00 00 00  |.D...ff.........|
  diff                   ^^
  file 0000000000001260: 55 48 89 fd 53 48 83 ec  08 e8 c2 fd ff ff 8b 18  |UH..SH..........|
```

## FINDINGS

Every kind in *wtn.c*, in source order, with its level and the condition that emits it:

```
GONE                  .  a read of a /proc/PID file found the process
                         vanished
UNVERIFIED            u  a witness could not look; see DIAGNOSTICS
KTHREAD               .  status has no VmSize: line
ID_COMM_EXE           ?  comm differs from the link basename in 15
                         bytes
ID_COMM_ARGV          ?  comm differs from the argv[0] basename in 15
                         bytes
ID_ARGV_EXE           ?  the argv[0] and link basenames differ
EXE                   .  step 1 finished; prints path, comm, argv0
HASH                  .  a witness was hashed; witness= is exe, root,
                         or host
EXE_DELETED           ?  the link text ends ` (deleted)'
EXE_MEMFD             ?  the link text begins /memfd:
PATH_ROOT_MISMATCH    !  the ROOT hash differs from EXE
PATH_ROOT_MISSING     ?  the ROOT path is absent and the process is
                         alive
PATH_HOST_MISSING     .  the HOST path is absent
PATH_HOST_MISMATCH    ?  the HOST hash differs from EXE
PATH_GONE             .  the exe is deleted or a memfd: no path to open
MEM_MISMATCH          !  a compared mapping differs from its backing
                         file
ANON_EXEC             ?  an executable mapping whose name is empty or
                         [...]
WX_MAP                ?  a mapping whose permissions begin rwx
LIB_DELETED           ?  an executable non-main file mapping ends `
                         (deleted)'
LIB_ODD_PATH          ?  an executable non-main file mapping is outside
                         the six directories below
MAPS                  .  maps was read; prints mappings and mappings
                         compared
ENV_RELOCATED         !  the environ range lies outside [stack]
ARGV_RELOCATED        ?  the argument range lies outside [stack]
ENV_LOADER_VAR        ?  environ holds LD_PRELOAD, LD_LIBRARY_PATH, or
                         LD_AUDIT
ENV_STRAY_LOADER_VAR  ?  one of those names and = is in [stack] or
                         [heap], outside the environ range
CAPS_UNPRIVILEGED     ?  non-zero effective uid and non-zero CapEff
LINEAGE               .  PPid is 1 and stat was parsed
IDS_SPLIT             .  the real uid differs from the effective uid
NS_NESTED             .  NSpid has more than one value
LDSO_PRELOAD          ?  with -a, /etc/ld.so.preload is not empty; pid
                         is -
```

The six directories of `LIB_ODD_PATH` are */lib/*, */lib64/*, */usr/lib/*, */usr/lib64/*, */usr/local/lib/*, and */usr/libexec/*. Names beginning with */memfd:* are not reported by it. `ANON_EXEC` leaves out *[vdso]*, *[vsyscall]*, and *[vvar]*.

## EXIT STATUS

*wtn* exits 1 if it printed any line of level `!` or `?`. Otherwise it exits 3 if it printed any line of level `u`. Otherwise it exits 0, which includes a pid that does not exist, a kernel thread, a zombie, and a process with only `.` lines. *wtn -T* exits 0 when all three vectors match.

*wtn* exits 2 for an unknown option, an argument that is not a valid pid, a 4097th distinct pid, no pid without *-a*, a failed *-T*, a failure to open */proc* under *-a*, and a failure to flush standard output.

## DIAGNOSTICS

A witness that could not look prints

```
<pid> u UNVERIFIED which=<name>
```

at level `u`, with or without *-v*. A failure to read is always printed, because a check that fails silently looks like a check that passed, and exit status 3 keeps the two apart. The names are

```
status         status could not be read; in step 0 this ends the pid
comm           comm could not be read
cmdline        cmdline could not be read
exe_link       readlink of exe failed
exe            exe could not be opened or hashed
root           ROOT could not be opened or hashed, for a reason other
               than a missing path, or is not a regular file
host           the same for HOST
maps           maps could not be read, or a line did not parse
maps_line      a line of maps was longer than 4095 bytes; printed once
maps_cap       the process has more than 4096 mappings; printed once
mem            mem could not be opened or read; printed once, and no
               further comparison or stray scan runs
map_file       the file behind a mapping could not be opened or read;
               the line also carries map=START-END
stat           stat could not be read, has no ), or has too few fields
stat_mm        fields 48 to 51 are all 0: the reader has no ptrace access
stack          maps has no [stack], so the range test did not run
environ        environ could not be read
cgroup         cgroup could not be read
filecaps       fgetxattr failed other than with ENODATA or ENOTSUP, or
               no exe descriptor was open
ld_so_preload  stat of /etc/ld.so.preload failed other than with ENOENT;
               the pid field is -
```

A process that vanishes is not a failure. When a read of a */proc/PID* file fails with `ENOENT` or `ESRCH`, *wtn* prints

```
<pid> . GONE
```

at level `.`, ends that pid, and goes on. Without *-v* this prints nothing, so a pid that does not exist exits 0.

## PRIVILEGE

Everything below was measured on 7 November 2026 on Linux 7.1.5+kali-amd64, with `kernel.yama.ptrace_scope` at 0 unless stated.

As uid 1000 on pid 1, the address fields read as zero, and the *exe* link, *maps*, and *environ* are refused. *wtn* exits 1 here, because of the `?` line:

```
$ cut -d' ' -f48-51 /proc/1/stat
0 0 0 0
$ ./wtn 1
1 u UNVERIFIED which=exe_link
1 ? ID_COMM_ARGV comm=systemd argv0=/sbin/init
1 u UNVERIFIED which=exe
1 u UNVERIFIED which=maps
1 u UNVERIFIED which=stat_mm
1 u UNVERIFIED which=environ
```

*map_files* entries opened for real root only, as root on its own process and on a process of another uid. Both the owner and another non-root uid were denied. As the owner with `CapEff` 0, listing the directory and `readlink` of an entry worked and the open failed with `EPERM`. Without it, *wtn* falls back to `via=root_path`.

Yama limited *mem* and left *maps* and *environ* open in every row, with the target a *sleep* of uid 1000:

```
scope   root       same uid, unrelated   same uid, parent
0       all open   all open              all open
1       all open   mem denied            all open
2       all open   mem denied            mem denied
```

*wtn* run as an unrelated process of the same uid printed `u UNVERIFIED which=mem` at scopes 1 and 2 and no other `u` line. It loses the MEM witness and the stray scan.

Scope 3 was not run, because *ptrace(2)* says the value cannot be changed once written. A single capability such as `CAP_SYS_PTRACE` without real root was not tried.

## IMPLEMENTATION

The program is one source file, *wtn.c*. It uses only libc.

It does not call `malloc`. All storage is static, because the bounds are known and a comparison then has no allocation failure to handle. `opendir` and stdio do allocate inside libc.

```
iob    131072 bytes   two 64 KiB I/O buffers
mt     16908288 bytes 4096 map records of 4128 bytes, each holding
                      a path of up to 4095 bytes
lb     4096 bytes     one line of maps; the joined NSpid values
pl     16384 bytes    the 4096 pids from the command line
pb     4160 bytes     the current path
rl     4096 bytes     the link text as read
ex     4096 bytes     the link text without ` (deleted)'
```

Untouched pages of *mt* cost nothing. SHA-256 is written in the file after FIPS 180-4, and *-T* tests it. Files and memory that are compared or hashed are read with `pread`, so the descriptor shared between steps keeps no position. Whole */proc* files and *maps* are read with `read`.

Every read of */proc* is racy: the process may change or exit between any two reads, so a finding describes a moment.

## TESTING

```
sudo testbed/run.sh
```

builds *wtn* and every case into *testbed/bin/*, runs them, and writes *results/*, which it creates. It needs real root. `id -u` alone is not enough, because uid 0 in a user namespace passes that test and lacks what the cases need, so the script also requires */proc/self/uid_map* to read `0 0 4294967295`, the identity map of the initial user namespace, and exits 2 otherwise. It runs `./wtn -T` after the build and exits 2 if that fails, before it touches *results/*. It deletes *results/\*.txt* and leaves the subdirectories.

A case is one small C program in *testbed/*. It prints its pid on the first line of output and waits. If it cannot run, it prints the reason and exits 77, which *run.sh* records as a skip. *run.sh* starts each case, waits 0.3 seconds, runs `./wtn -x -v PID` into *results/NN-name.txt*, and kills the case.

A case passes when every expected kind appears, no forbidden kind appears, and no `!` kind appears that was not expected. A control also fails on any `?` line. Cases 11 and 12 must match a pattern as well, `via=exe` and `diff_bytes=1`. Case 08 forbids `ENV_LOADER_VAR`, case 16 forbids `PATH_ROOT_MISMATCH`, and case 18 forbids `PATH_HOST_MISMATCH`.

Cases 00 and 01 are controls, and neither changes its testimony:

```
00-control         a plain copy of sleep, run as sleep 1000
01-longname        a copy named with 26 bytes, longer than comm holds
02-argv_exec       executes sleep with argv[0] set to camouflage
03-argv_overwrite  overwrites its argv bytes with [kworker/0:1]
04-prctl_name      sets comm to septicon with PR_SET_NAME
05-deleted         its binary is unlinked after exec
06-memfd           executes from a memfd named ghost
07-preload         loads libp.so through LD_PRELOAD=./libp.so
08-preload_scrub   as 07, and libs.so wipes the variable from environ
09-libpath         resolves libhello.so through LD_LIBRARY_PATH=.
10-env_prctl       points environ at a heap copy without the LD_ variables
11-exe_repoint     moves its code to anonymous memory, points exe at
                   /bin/true, and flips a byte in a mapped page of it
12-selfpatch       flips one byte of its own text
13-anon_exec       runs from an anonymous rwx page
14-filecaps        uid 65534 holding cap_dac_read_search from a file
15-setuid          real uid 65534, effective uid 0
16-mntns_pidns     another program bound over /bin/sleep before exec,
                   in a private mount and pid namespace
17-orphan          forks twice, so the middle process exits and PPid is 1
18-root_swap       another program bound over /bin/sleep after exec
```

*run.sh* then runs `./wtn -a` into *results/scan-all.txt*, runs *testbed/facts.sh*, and writes *results/table.txt* and *results/exit.txt*. *testbed/facts.sh* measures *map_files* and Yama scopes 0 to 2, needs a non-root `SUDO_UID`, and restores the scope on exit. The last run as real root, on 7 November 2026, ended with this line and status 0:

```
19 pass, 0 fail, 0 skip
```

The idle scan of that run held 56 lines, with no `!` and no `u`.

## THE PAPER

"Reflections on Trusting /proc", by Ivan Gaydardzhiev, explains what each */proc* file is, who produces it, and what a disagreement can and cannot mean. The source is *reflections_on_trusting_proc.typ* and the compiled paper is *reflections_on_trusting_proc.pdf*, 18 A4 pages. The source is self-contained: every listing, table, and captured line is written into it, and it uses only the fonts built into Typst, so it compiles alone in any directory.

```
make paper
```

runs

```
typst compile reflections_on_trusting_proc.typ reflections_on_trusting_proc.pdf
```

This was tested with Typst 0.15.1, alone in an empty directory and with `--ignore-system-fonts`. The listings were copied from *testbed/* and from the runs of 7 November 2026, and the table of findings from *wtn.c*, so a later change to those files does not reach the paper.

## FILES

```
COPYING                                       the GNU General Public License, version 3
Makefile                                      build targets: wtn, dynamic, paper, clean
readme.md                                     this file
results/                                      output of a run, created by testbed/run.sh
reflections_on_trusting_proc.pdf
                                              the paper, made by make paper
reflections_on_trusting_proc.typ
                                              the paper, source
testbed/                                      the cases and their scripts
testbed/00-control.c                          case 00
testbed/01-longname.c                         case 01
testbed/02-argv_exec.c                        case 02
testbed/03-argv_overwrite.c                   case 03
testbed/04-prctl_name.c                       case 04
testbed/05-deleted.c                          case 05
testbed/06-memfd.c                            case 06
testbed/07-preload.c                          case 07
testbed/08-preload_scrub.c                    case 08
testbed/09-libpath.c                          case 09
testbed/10-env_prctl.c                        case 10
testbed/11-exe_repoint.c                      case 11
testbed/12-selfpatch.c                        case 12
testbed/13-anon_exec.c                        case 13
testbed/14-filecaps.c                         case 14
testbed/15-setuid.c                           case 15
testbed/16-mntns_pidns.c                      case 16
testbed/16-payload.c                          static stand-in bound over /bin/sleep by cases 16, 18
testbed/17-orphan.c                           case 17
testbed/18-root_swap.c                        case 18
testbed/bin/                                  built by run.sh: 20 programs and 3 libraries
testbed/c.h                                   helpers shared by the cases
testbed/facts.sh                              measures map_files and Yama scopes 0 to 2
testbed/libhello.c                            library of case 09
testbed/libp.c                                preloaded library of cases 07, 10
testbed/libs.c                                preloaded library of case 08, wipes its variable
testbed/run.sh                                builds, runs, and judges every case
wtn                                           the binary, made by make
wtn.c                                         the program
```

## SEE ALSO

proc(5), proc_pid_stat(5), proc_pid_maps(5), proc_pid_exe(5), prctl(2), ptrace(2), memfd_create(2), namespaces(7), capabilities(7), ld.so(8), *reflections_on_trusting_proc.pdf*

## BUGS

A finding describes a moment. The reads are separate, so one report can mix two states of a process.

Uprobes, user-space live patching, kernel live patching, and text relocations were not run. Any write to compared text gives `MEM_MISMATCH`: a breakpoint planted with `PTRACE_POKETEXT` gave `diff_bytes=1`, the same line as a patched byte.

JIT code lives in anonymous memory, has no file to compare with, and is only reported, as `ANON_EXEC` and `WX_MAP`.

Wrappers, interpreters, and multi-call binaries give `ID_*` lines on a healthy machine. The idle scan of 7 November 2026 had 25 of them.

The table holds 4096 mappings per process. Later mappings are counted and not compared, and `UNVERIFIED which=maps_cap` says so.

Kernel threads and zombies are skipped after step 0. A zombie is reported as `KTHREAD`.

The stray scan matches text. It cannot tell an old environment from a script that mentions the variable. In the idle scan of 7 November 2026 it reported two *run.sh* shells, 8 lines each. It reads at most 64 MiB of *[stack]* and of *[heap]*, stops at 8 reports, and reads no other mapping.

`CAPS_UNPRIVILEGED` does not read the executable's mode, so a setuid or setgid program holding capabilities gives it too.

ROOT, HOST, and *map_files* accept only regular files. A path that names anything else gives `UNVERIFIED`. A chrooted process gives `PATH_ROOT_MISSING`, because the link text is the path from the reader's root.

Only `r-x` file mappings are compared. A file mapping with `rwx` gives `WX_MAP` and is not compared. A non-main memfd mapping is not compared and gives `LIB_DELETED`.

Executable mappings whose names begin with neither `/` nor `[`, such as `anon_inode:`, match no check.

`LIB_ODD_PATH` is a list of six directories, so a library placed in one of them gives no line.

A file really named `x (deleted)` cannot be told from a deleted one by the link text.

*cmdline* and *status* are read up to 65535 bytes and *environ* up to 131071. A path in *maps* is kept up to 4095 bytes.

A pid that does not exist exits 0 and prints nothing without *-v*.

*wtn* takes the kernel's own file, memory, and process code as given, and everything below it: hypervisor, SMM, management engine or platform security processor, microcode, silicon. A layer that can alter what the layer above observes cannot be audited by it.

The only target is x86-64 Linux.

## LICENSE

Copyright (C) 2026 Ivan Gaydardzhiev. *wtn* is distributed under the GNU General Public License, version 3 only. See *COPYING*.
