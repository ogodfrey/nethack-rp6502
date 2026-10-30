#!/bin/sh
# Build the host game with the monster table at a fake XRAM address, to test
# the pm_get() conversion without the target. Every module except monst.c
# sees mons as an unmapped address (direct accesses crash) and reads records
# through rp6502xram_host.c, which aborts on a non-table pointer.
#
#   xramhost.sh [OUTDIR]      default OUTDIR: build/hostxram
#
# Then play with:  cd OUTDIR && NETHACKDIR=play ./nethack
set -e

TOOLS=$(cd "$(dirname "$0")" && pwd)
SRC=$(cd "$TOOLS/../../.." && pwd)
OUT=${1:-$SRC/build/hostxram}
CC=${CC:-cc}
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 2)}

mkdir -p "$OUT/obj"
OUT=$(cd "$OUT" && pwd)

"$TOOLS/hostgen.sh" "$SRC" "$OUT/gen" data

CFLAGS="-O1 -g -w -DRP6502 -DRP6502_HOST -DRP6502_MONS_XRAM"
CFLAGS="$CFLAGS -I$OUT/gen/include -I$SRC/include"
export CC CFLAGS SRC OUT
grep -v '^#' "$TOOLS/../sources.txt" | grep . |
    xargs -P "$JOBS" -I{} sh -c \
        'extra=""; [ "{}" = src/monst.c ] && extra="-DRP6502_MONS_RAM -Dmons=rp6502_ram_mons"; o="$OUT/obj/$(basename {} .c).o"; $CC $CFLAGS $extra -c "$SRC/{}" -o "$o"'
$CC $CFLAGS -c "$TOOLS/../rp6502xram_host.c" -o "$OUT/obj/rp6502xram_host.o"
$CC -o "$OUT/nethack" "$OUT"/obj/*.o

rm -rf "$OUT/play"
mkdir -p "$OUT/play"
cd "$OUT/gen/dat"
cp ./*.lev bogusmon cmdhelp data dungeon engrave epitaph help hh history \
    keyhelp license opthelp options oracles quest.dat rumors symbols \
    tribute wizhelp "$OUT/play/"
touch "$OUT/play/record"
echo "Built $OUT/nethack (monster table at a fake XRAM address)"
