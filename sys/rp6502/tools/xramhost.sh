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

CFLAGS="-O1 -g -w -DRP6502 -DRP6502_HOST -DRP6502_MONS_XRAM -DMSG_CHECK -fno-pie"
CFLAGS="$CFLAGS -I$OUT/gen/include -I$SRC/include"
# String literals in function bodies are read from msg.idx/msg.dat, and every
# fetch is checked against its literal (see include/msg.h).
python3 "$TOOLS/strtab.py" "$SRC" "$OUT/msgsrc" --sources "$TOOLS/../sources.txt" > /dev/null
export CC CFLAGS SRC OUT
grep -v '^#' "$TOOLS/../sources.txt" | grep . |
    xargs -P "$JOBS" -I{} sh -c \
        'f="$SRC/{}"; [ -f "$OUT/msgsrc/{}" ] && f="$OUT/msgsrc/{}"; extra=""; case "{}" in src/monst.c) extra="-DRP6502_MONS_RAM -Dmons=rp6502_ram_mons";; src/objects.c) extra="-DRP6502_MONS_RAM -Dobjects=rp6502_ram_objects";; esac; o="$OUT/obj/$(basename {} .c).o"; $CC $CFLAGS $extra -c "$f" -o "$o"'
$CC $CFLAGS -c "$TOOLS/../rp6502xram_host.c" -o "$OUT/obj/rp6502xram_host.o"
$CC $CFLAGS -c "$TOOLS/../msgfetch.c" -o "$OUT/obj/msgfetch.o"
# objects[] has no RAM copy here: the link gives the symbol the fake XRAM
# address, so &objects[i] is an address that rpo_get() can map back.
OBJS_ADDR=$(sed -n 's/^#define OBJS_XRAM_ADDR[[:space:]]*//p' "$SRC/include/xram.h")
$CC -no-pie -o "$OUT/nethack" "$OUT"/obj/*.o -Wl,--defsym,objects="$OBJS_ADDR"

rm -rf "$OUT/play"
mkdir -p "$OUT/play"
cd "$OUT/gen/dat"
cp ./*.lev bogusmon cmdhelp data dungeon engrave epitaph help hh history \
    keyhelp license opthelp options oracles quest.dat rumors symbols \
    tribute wizhelp "$OUT/play/"
touch "$OUT/play/record"
cp "$OUT/msgsrc/msg.dat" "$OUT/msgsrc/msg.idx" "$OUT/play/"
echo "Built $OUT/nethack (monster table at a fake XRAM address)"
