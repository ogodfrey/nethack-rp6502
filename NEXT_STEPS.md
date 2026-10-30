# NetHack RP6502 port: continuation handoff

Snapshot: 2026-10-30. This is the handoff for continuing the NetHack 3.6.7
port on another machine. The private repository is
`ogodfrey/nethack-rp6502`, and `rp6502` is its default branch.

The goal is a playable game on the [Picocomputer RP6502](https://picocomputer.github.io/),
including its emulator and hardware. The 65C02 has a 64 KiB address space
that includes I/O; the additional 64 KiB XRAM is accessed through portals and
cannot execute code.

## Current state

- The console/DECgraphics test ROM works. The RP6502-configured game has run
  as a native host build, including character selection, special levels, and
  save/restore.
- All target modules build with llvm-mos as the `nethack_objs` CMake target.
  **The full game is not yet linked or runnable on the RP6502.**
- [ovlpack.py](sys/rp6502/tools/ovlpack.py) and the
  [overlay runtime](sys/rp6502/ovl/) are a prototype, not yet integrated with
  the game. The small test program matched the host with two slots and with
  one slot, including forced eviction/reload. The `plan` command mentioned in
  the packer's docstring is not implemented.
- Target settings are `-Oz -fno-inline -fno-lto`. The last unlinked module
  measurements were about 2.61 MB of text, 41 KB of data, and 69 KB of BSS;
  final linked sizes can differ. The largest estimated function was about
  21 KB, and seven were over 12 KB. RAM must also hold resident code, overlay
  slots, the heap, and the stack.
- [FEASIBILITY.md](sys/rp6502/FEASIBILITY.md) has more measurements and
  history, including older NetHack 5.0 and cc65 experiments. Its XRAM table
  design is still a proposal; the `mons[]` storage and fetch mechanism is
  validated only by the `xramtest` prototype (see priority 2).

## Set up the new machine

Authenticate to GitHub for the private repository, then clone it:

```sh
git clone --branch rp6502 https://github.com/ogodfrey/nethack-rp6502.git
cd nethack-rp6502
```

The commands below assume a Linux/POSIX development environment with a host C
compiler, Python 3, CMake 3.28 or newer, and a working RP6502-capable
llvm-mos SDK, including its platform files. Put its `bin` directory on
`PATH`; `MOS_BIN` can point the overlay packer at that directory explicitly.
Install `rp6502-emu` for target tests. See the
[RP6502 SDK](https://picocomputer.github.io/sdk.html) for installation.
The previously tested compiler was llvm-mos commit
`a5c31b24a97cc16c582d124167e29e3da8946581`, with matching RP6502 SDK
platform files.

```sh
cmake --preset llvm-mos/Release
cmake --build build/llvm-mos/release --target nethack_objs --parallel

sys/rp6502/tools/hostbuild.sh
(
  cd build/host
  NETHACKDIR=play ./nethack
)
```

`hostbuild.sh` recreates `OUTDIR/play`, so back up saves or choose a fresh
output directory before rebuilding. Use a relative `NETHACKDIR` as above:
the current host port truncates paths to `PATHLEN` (64), and a long absolute
path can cause startup to reject the game directories.

## Reproduce the overlay test

The following builds the test from tracked sources, compares emulator output
with a host build, and requires forced eviction in the one-slot case. It
assumes `mos-rp6502-clang` and `rp6502-emu` are on `PATH`.

```sh
(
  set -eu
  MOS_CC="$(command -v mos-rp6502-clang)"
  MOS_BIN="$(dirname "$MOS_CC")"
  export MOS_BIN
  OUT="$(mktemp -d "${TMPDIR:-/tmp}/nh-overlay.XXXXXX")"
  mkdir -p "$OUT/asm"
  for src in sys/rp6502/ovl/test/ot_*.c; do
    "$MOS_CC" -Os -fno-lto -ffunction-sections -fdata-sections \
      -S "$src" -o "$OUT/asm/$(basename "$src" .c).s"
  done
  cc -o "$OUT/host" sys/rp6502/ovl/test/ot_*.c
  "$OUT/host" > "$OUT/expected.txt"
  for slots in 2 1; do
    python3 -c 'import json,sys; p=json.load(sys.stdin); p["nslots"]=int(sys.argv[1]); json.dump(p,sys.stdout)' \
      "$slots" < sys/rp6502/ovl/test/plan.json > "$OUT/plan.json"
    python3 sys/rp6502/tools/ovlpack.py emit "$OUT/asm" "$OUT/plan.json" "$OUT/out-$slots"
    python3 sys/rp6502/tools/ovlpack.py link "$OUT/out-$slots"
    rp6502-emu "$OUT/out-$slots/program.rp6502" --headless --phi2 0 > "$OUT/target.txt"
    tr -d '\r' < "$OUT/target.txt" > "$OUT/normalized.txt"
    sed '$d' "$OUT/normalized.txt" > "$OUT/comparable.txt"
    diff -u "$OUT/expected.txt" "$OUT/comparable.txt"
    if [ "$slots" -eq 1 ]; then
      tail -n 1 "$OUT/normalized.txt" | grep -E '^loads [0-9]+ forced [1-9][0-9]*$'
    else
      tail -n 1 "$OUT/normalized.txt" | grep -E '^loads [0-9]+ forced [0-9]+$'
    fi
  done
  echo "Test output: $OUT"
)
```

The earlier two-slot run reported 29 loads/11 forced evictions; the one-slot
run reported 2,511 loads/1,250 forced evictions. Counts may vary with the
compiler. Turn this into a maintained automated test and extend it for ABI
preservation, callbacks, tail calls, and pointers to overlay-owned data.

## Priority next steps

1. **Re-establish the baseline** using the builds and overlay test above.
   Keep the host behavior working as target-specific changes are made. Fix the
   host path truncation or retain a tested relative-path workaround.
2. **Reduce RAM usage.** The `mons[]` and `objects[]` tables measured about
   20.6 KB and 11.8 KB respectively. Prototype moving their data to XRAM,
   using [farscan.py](sys/rp6502/tools/farscan.py) as an audit aid; it
   needs Python `libclang` and the generated host headers, and finds the host
   compiler's include directory itself. Treat its counts and findings as
   incomplete until validated. The proposed fake-address/accessor design
   needs proof for pointer identity and arithmetic, array and bitfield
   accesses, mutations, attack/name pointers, and save/restore. Do not rely
   on inaccessible RAM pointers without testing the compiler and ABI.
   Reserve a non-overlapping XRAM layout: the overlay manager uses RIA
   portal 1 and its default descriptor/directory addresses are `$8000` and
   `$C000`.

   Prototype status: `xramtest` ([xramtest.c](sys/rp6502/xramtest.c),
   [rp6502xram.c](sys/rp6502/rp6502xram.c), [rp6502xram.h](include/rp6502xram.h))
   copies `mons[]` to XRAM at `$1000`, reads every record back through
   `pm_get()` (a 4-slot RAM fetch cache), and checksums all fields. The
   rp6502-emu output matches the host (`mons 381 sum 91d5b81f`, span 380).
   This validates the storage, portal reads, and `mons_at()` pointer
   arithmetic.

   Conversion status. With `RP6502_MONS_XRAM` defined, `mons` names the XRAM
   image in [permonst.h](include/permonst.h) via
   [rp6502xram.h](include/rp6502xram.h). Then `&mons[PM_X]` and `mons_at()`
   work unchanged, and `pm_get()` fetches the record. The mondata macros in
   [mondata.h](include/mondata.h) already call `pm_get()`. Without the flag
   (host, and the game target today), `pm_get()` is the identity, so nothing
   changes. `xramtest` also checks 29 of those macros over every monster, and
   the host and emulator outputs match (`flags b187e3ec`). The host build and
   the target `nethack_objs` build pass with the flag off.

   Still to do before the flag can be turned on for the game:
   - Direct permonst accesses: about 330 `->` sites left in `src/` (after
     mondata.c, makemon.c, mon.c, mhitu.c, and uhitm.c). Most of the rest are
     in polyself.c, sounds.c, monmove.c, trap.c, mhitm.c, dogmove.c, eat.c,
     role.c, hack.c, apply.c, muse.c, read.c, and similar files.
   - Local copies of a permonst (for example `pm_copy` in passive() and
     passive_obj(), and `ymon` in attack()) must never be passed to pm_get().
     The XRAM image base overlaps program RAM, so a range check cannot tell
     a local copy from a table pointer. Only table pointers may reach pm_get().
   - Verification gap: the full game is not linked for the target, and the
     host build never uses XRAM, so nothing runs the converted code with the
     table in XRAM. Before converting more, add a host build mode where mons
     lives at an unmapped fake base and pm_get() asserts its argument is in
     the fake range. Then autoplay will catch any missed direct access.
   - The 9 permonst writes need write-back (`pm_put`) or a RAM-side design.
   - The XRAM image rule exists and is proven by `xramtest`. The map is
     [xram.h](include/xram.h), CMake reads `MONS_XRAM_ADDR` from it, and the
     link writes `mons.bin` (a byproduct, `rp6502_byproduct`) with
     [elfsym.py](sys/rp6502/tools/elfsym.py), which copies the table's initial
     data from the ELF. The image is an XRAM asset, so no startup copy is
     needed. `xramtest` reads only XRAM and matches the host output.
   - For the game this still has an open cost: elfsym.py needs the table in
     the link. A RAM copy linked into the game would use RAM, so the game
     needs either a separate dump link with the same string addresses or the
     names-in-XRAM design. Decide before the game's table moves.
   - Name strings (decided): `mname` becomes a 16-bit index into a RAM name
     table of string pointers (about 762 bytes of RAM). The XRAM image then
     has no link addresses, so a small dump program that links only monst.c
     can produce `mons.bin` for the game, and the game no longer needs a
     second link. About 126 `mname` reads change to `pm_name(p)`. Not started.
   - Harness status: the XRAM host harness passes role_init and stops at
     polyself.c:77 (set_uasmon). `polyself.c` has about 74 sites, several
     through local `mdat`/`mptr` pointers, so convert the whole file in one
     pass, then rerun the harness.
   - objclass (`objects[]`) follows the same plan later (about 470 direct
     sites).
3. **Budget the rest of memory.** Moving both tables alone will not make the
   measured unlinked data fit. Investigate `level`, room and display buffers,
   vision data, heap, stack, resident code, and overlay slots; measure the
   actual linked footprint as sections are eliminated or moved.
4. **Complete overlay packing and integration.** Implement a profile-guided
   partitioner for `ovlpack.py`, choose a RAM/slot budget, and split functions
   that exceed the chosen slot size. Test relocation, pointer lifetimes,
   callbacks, nested/forced reloads, and target ABI behavior. Generate the
   game's assembly and assets reproducibly, initialize the overlay runtime
   from the target startup, and wire packing/linking into the build. The
   packer needs `-S` assembly, not the CMake object files; the target's
   sources are `sys/rp6502/sources.txt` plus `sys/share/random.c`. An earlier
   attempt to reassemble raw `extralev.s` failed on a static named `r`;
   compiling the C file directly to an object succeeded.
5. **Generate target-compatible game data.** The host utilities in
   [hostgen.sh](sys/rp6502/tools/hostgen.sh) produce binary level and quest
   files with host-sized structures; they are suitable for the host build,
   not yet the RP6502. Generate or serialize target-sized data, package the
   ROM assets, and verify startup, level creation, gameplay, save/restore,
   and recovery in the emulator before hardware testing.

## Profiling and local-only artifacts

`PROFILE=1 sys/rp6502/tools/hostbuild.sh build/hostprof` builds the
instrumented host game. The profiler currently uses Linux/glibc facilities;
`autoplay.py` also needs Python `pexpect`, and `cgreport.py` needs
`addr2line`. For example, from `build/hostprof`, run
`CGPROF=game.cgprof python3 ../../sys/rp6502/tools/autoplay.py --turns 1500 --seed 1 ./nethack play`,
then run
`python3 ../../sys/rp6502/tools/cgreport.py ./nethack game.cgprof > callgraph.tsv`.
Repeat with varied seeds and play styles. The profiler appends and writes at
normal exit; use fresh output for each rebuilt executable. Autoplay can stall,
so check its exit status and the resulting profile rather than assuming a
timed-out run succeeded. Raw keyboard polling dominated the earlier call
counts, so account for that when using the graph to choose overlays.

The prior machine's `build/hostprof/callgraph.tsv`, raw `cgprof.out`,
`build/mos/` assembly, statistics and helper scripts, and `/tmp/ot/` test
outputs are **not in Git**. Copy the call graph separately before leaving
that machine if the exact prior runs are needed, or regenerate it. Raw profile
addresses require the matching host executable. The installed llvm-mos SDK,
emulator, and Python environments are also machine-local and must be
installed or copied separately.
