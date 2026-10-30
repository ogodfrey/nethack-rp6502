#!/bin/sh
# Build NetHack's host utilities and generate files for the RP6502 build.
#
#   hostgen.sh SRCDIR OUTDIR          headers: pm.h, onames.h, date.h
#   hostgen.sh SRCDIR OUTDIR data     also the data files in OUTDIR/dat
#
# The utilities write to ../include and ../dat relative to their working
# directory, so they run inside a mirror tree under OUTDIR to keep the
# source tree clean.
#
# Note: lev_comp, dgn_comp and makedefs -q write binary structures, so the
# data files match the host's integer sizes.  They are right for the
# RP6502_HOST test build; the target needs a cross-sized build (later).
#
# The utilities are built with the port configuration (RP6502) so that
# conditional objects and monsters (e.g. SCR_MAIL) match the game.
set -e

SRC=$(cd "$1" && pwd)
OUT=$2
MODE=${3:-headers}
HOSTCC=${HOSTCC:-cc}

mkdir -p "$OUT/include" "$OUT/util" "$OUT/dat" "$OUT/src"
OUT=$(cd "$OUT" && pwd)
CFLAGS="-O1 -w -DRP6502 -DRP6502_HOST -I$OUT/include -I$SRC/include"

cp "$SRC"/dat/* "$OUT/dat/"

cd "$OUT/util"
# shellcheck disable=SC2086
$HOSTCC $CFLAGS -o makedefs \
    "$SRC/util/makedefs.c" "$SRC/src/monst.c" "$SRC/src/objects.c"

./makedefs -o
./makedefs -p
./makedefs -v

[ "$MODE" = data ] || exit 0

COMMON="$SRC/src/alloc.c $SRC/util/panic.c"
NAMING="$SRC/src/drawing.c $SRC/src/decl.c $SRC/src/monst.c $SRC/src/objects.c"
# shellcheck disable=SC2086
$HOSTCC $CFLAGS -o lev_comp "$SRC/sys/share/lev_yacc.c" \
    "$SRC/sys/share/lev_lex.c" "$SRC/util/lev_main.c" $COMMON $NAMING
# shellcheck disable=SC2086
$HOSTCC $CFLAGS -o dgn_comp "$SRC/sys/share/dgn_yacc.c" \
    "$SRC/sys/share/dgn_lex.c" "$SRC/util/dgn_main.c" $COMMON

./makedefs -d
./makedefs -r
./makedefs -h
./makedefs -q
./makedefs -s
./makedefs -e

cd "$OUT/dat"
../util/dgn_comp dungeon.pdf
for d in *.des; do
    ../util/lev_comp "$d"
done
touch record
