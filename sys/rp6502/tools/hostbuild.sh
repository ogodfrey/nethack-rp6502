#!/bin/sh
# Build the RP6502 port configuration as a native program for testing on
# the development host (RP6502_HOST).  It uses the same sources, options
# and tty code as the target, with termios standing in for TTY:.
#
#   hostbuild.sh [OUTDIR]      default OUTDIR: build/host
#
# Then play with:  NETHACKDIR=OUTDIR/play OUTDIR/nethack
#
# PROFILE=1 builds with call-graph edge counting (cgprof.c) for the
# overlay packer; runs append to $CGPROF (default cgprof.out).
set -e

TOOLS=$(cd "$(dirname "$0")" && pwd)
SRC=$(cd "$TOOLS/../../.." && pwd)
OUT=${1:-$SRC/build/host}
CC=${CC:-cc}
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 2)}

mkdir -p "$OUT/obj"
OUT=$(cd "$OUT" && pwd)

"$TOOLS/hostgen.sh" "$SRC" "$OUT/gen" data

# String literals in function bodies are read from msg.idx/msg.dat (see
# msgfetch.c). The rewritten copies are compiled with MSG_CHECK, which compares
# every fetched string with the literal it replaced.
python3 "$TOOLS/strtab.py" "$SRC" "$OUT/msgsrc" --sources "$TOOLS/../sources.txt" > /dev/null

CFLAGS="${CFLAGS:--O1 -g} -w -DRP6502 -DRP6502_HOST -DMSG_CHECK"
CFLAGS="$CFLAGS -I$OUT/gen/include -I$SRC/include"
LIBS=
if [ -n "$PROFILE" ]; then
    # No inlining, so the edges match the functions the 6502 build keeps.
    $CC -O1 -c "$TOOLS/cgprof.c" -o "$OUT/cgprof.o"
    CFLAGS="$CFLAGS -finstrument-functions -fno-inline"
    LIBS="$OUT/cgprof.o -ldl"
fi
export CC CFLAGS SRC OUT
grep -v '^#' "$TOOLS/../sources.txt" | grep . |
    xargs -P "$JOBS" -I{} sh -c \
        'f="$SRC/{}"; [ -f "$OUT/msgsrc/{}" ] && f="$OUT/msgsrc/{}"; o="$OUT/obj/$(basename {} .c).o"; $CC $CFLAGS -c "$f" -o "$o"'
$CC $CFLAGS -c "$TOOLS/../msgfetch.c" -o "$OUT/obj/msgfetch.o"
$CC -o "$OUT/nethack" "$OUT"/obj/*.o $LIBS

rm -rf "$OUT/play"
mkdir -p "$OUT/play"
cd "$OUT/gen/dat"
cp ./*.lev bogusmon cmdhelp data dungeon engrave epitaph help hh history \
    keyhelp license opthelp options oracles quest.dat rumors symbols \
    tribute wizhelp "$OUT/play/"
cp "$OUT/msgsrc/msg.dat" "$OUT/msgsrc/msg.idx" "$OUT/play/"
touch "$OUT/play/record"
echo "Built $OUT/nethack; run: NETHACKDIR=$OUT/play $OUT/nethack"
