#!/bin/bash
# Measure the 6502 code/data size of each NetHack source module with cc65.
# This is a sizing harness, not a build: it compiles patched copies of the
# sources in a scratch directory and never modifies the repository.
#
# Patches applied to the copies (size-estimation shortcuts only):
#   - long long -> long, USE_ISAAC64 off   (cc65 has no 64-bit integers)
#   - BUFSZ 256 -> 128, QBUFSZ 128 -> 64   (cc65 frames are limited to 256 bytes)
#   - LUA_32BITS                           (headers only; Lua itself needs floats)
#   - empty stubs for missing POSIX headers
# Modules are compiled with -Cl (static locals) so large frames still compile.
#
# usage: sys/rp6502/tools/measure-cc65.sh [workdir] [nethack-tree]
#   workdir defaults to /tmp/nhcc; nethack-tree defaults to this repository.
#   A 3.6 tree must have its makedefs headers generated first (pm.h, onames.h,
#   vis_tab.h, date.h). Extra cc65 flags can be passed in CC65_EXTRA; for 3.6
#   use CC65_EXTRA=-D__GO32__ to skip the K&R libc redeclarations in system.h.
# output: <workdir>/res_src.tsv  module, ok|FAIL, CODE, RODATA, DATA, BSS
set -u
H=${1:-/tmp/nhcc}
REPO=${2:-$(cd "$(dirname "$0")/../../.." && pwd)}
LUAVER=5.4.8
read -r -a EXTRA <<< "${CC65_EXTRA:-}"

command -v cc65 >/dev/null || { echo "cc65 not on PATH" >&2; exit 1; }
rm -rf "$H"; mkdir -p "$H"/{include,src,stub/sys,out,lua}
cp "$REPO"/include/*.h "$H/include/"
cp "$REPO"/src/*.c "$H/src/"

if [ -f "$H/include/nhlua.h" ]; then
    curl -fsSL "https://www.lua.org/ftp/lua-$LUAVER.tar.gz" | tar xz -C "$H"
    rm -rf "$H/lua"; mv "$H/lua-$LUAVER/src" "$H/lua"
    sed -i 's/^#define LUA_32BITS\s*0/#define LUA_32BITS 1/' "$H/lua/luaconf.h"
fi

grep -rl "long long" "$H/include" "$H/src" | xargs -r sed -i \
    's/unsigned long long/unsigned long/g; s/long long/long/g'
sed -i 's|^#define USE_ISAAC64|/* #define USE_ISAAC64 */|' "$H/include/config.h"
sed -i 's/^#define BUFSZ 256/#define BUFSZ 128/; s/^#define QBUFSZ 128/#define QBUFSZ 64/' \
    "$H/include/global.h"
cat >> "$H/include/config.h" <<'EOF'
#ifndef NHCC_SHIM
#define NHCC_SHIM
#include <string.h>
#include <stdlib.h>
extern long lrand48(void);
extern void srand48(long);
#endif
EOF
for f in wait ioctl time types stat file param; do : > "$H/stub/sys/$f.h"; done

: > "$H/res_src.tsv"
cd "$H/src"
for f in *.c; do
    b=${f%.c}; L=$H/out/$b.log
    if cc65 -t rp6502 -Os -Cl "${EXTRA[@]}" -I "$H/stub" -I "$H/include" -I "$H/lua" \
            -o "$H/out/$b.s" "$f" >"$L" 2>&1 \
       && ca65 -t rp6502 -o "$H/out/$b.o" "$H/out/$b.s" >>"$L" 2>&1; then
        od65 --dump-segsize "$H/out/$b.o" | awk -v b="$b" '
            /CODE:/{c=$2} /RODATA:/{r=$2} /BSS:/{s=$2} / DATA:/{d=$2}
            END{print b"\tok\t"c"\t"r"\t"d"\t"s}' >>"$H/res_src.tsv"
    else
        echo -e "$b\tFAIL\t$(grep -c 'Error:' "$L")" >>"$H/res_src.tsv"
    fi
done

awk -F'\t' '$2=="ok"{n++; c+=$3; r+=$4; d+=$5; s+=$6} $2=="FAIL"{f++}
    END{printf "compiled %d, failed %d\nCODE %d  RODATA %d  DATA %d  BSS %d\n", n, f, c, r, d, s}' \
    "$H/res_src.tsv"
