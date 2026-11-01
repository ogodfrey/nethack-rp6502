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

## Decisions in force

- **Scope: the RP6502 build is the only target.** The other ports (win32,
  Qt, Mac, Amiga, and the rest under `sys/` and `win/`) are out of scope, so
  portability constraints for them do not apply. The build uses `src/`,
  `sys/rp6502`, `sys/share/random.c`, and `win/tty`. The host builds are kept
  only as test harnesses (autoplay, the checked XRAM stand-ins).
- **Turn-based play.** Latency matters per player move, not per memory access.
  An XRAM or storage access that costs tens of cycles is acceptable if the
  whole move (vision, monsters, display) stays responsive. Measure a move on
  the target before accepting a new access path in a hot loop.
- **Consequence: XRAM is nearly full.** The fixed tables use about 54 KB of
  the 64 KB XRAM: `mons[]` 19.9 KB, `objects[]` 11.8 KB, the level map 10.1 KB,
  `lastseentyp` and `SpLev_Map` 1.7 KB each, the glyph buffer 5.0 KB, and the
  vision planes 5.0 KB. The overlay directory and descriptor use the rest.
  The static RAM that remains (about 138 KB: `.rodata` 85 KB, `.bss` 44 KB,
  `.data` 9 KB, measured from the link map) cannot go to XRAM, so it lives in
  storage (read on demand, as `msg.dat` is) or is shrunk.
- **Consequence: overlays and storage streaming both fit a turn budget.** Code
  overlays (priority 4) and streamed read-only tables are the main path.
- **Cleanup done:** the non-RP6502 port directories were removed (commit
  3ee0142f3). `sys/share` and `win/tty` stay.

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
   portal 1; its descriptor and directory addresses are set in
   [xram.h](include/xram.h) and checked by the packer.

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
   - Direct permonst and objclass accesses are converted across `src/`
     (`pm_get()`, `pm_name()`, and `oc_get()`/`oc_set()`). Remaining direct
     reads are intentional local copies, `#if 0` blocks, or comments. With
     `-DRP6502_XRAM_TABLES=ON`, all 117 modules compile for llvm-mos.
   - Local copies of a permonst (for example `pm_copy` in passive() and
     passive_obj(), and `ymon` in attack()) must never be passed to pm_get().
     The XRAM image base overlaps program RAM, so a range check cannot tell
     a local copy from a table pointer. Only table pointers may reach pm_get().
     The host harness aborts on any such pointer, and autoplay runs it.
   - The 9 permonst writes use `pm_put`; `role.c` is the only caller.
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
   - Name strings (done): `struct permonst` no longer has a name pointer.
     `mons_names[]` (RAM, src/monnames.c) holds the names, indexed like
     `mons[]`, and `pm_name(p)` reads it at `pm_index(p)`, which needs no
     fetch. `monst.c` builds both arrays from one set of `MON()` entries
     (`MONST_NAMES` selects the names). The image is now pure data:
     `mons.bin` is 382 records of 52 bytes. The target `xramtest` output
     (monster sums with names, flags, and objects) matches the host run.
     `pm_index()` aborts in the host harness on a non-table pointer.
   - Harness status: the XRAM host harness (`xramhost.sh`) builds the whole game
     with the tables at fake addresses, and autoplay runs it without aborts
     (normal, wizard-mode, and seed 5 runs). Its aborts name any direct access
     that was missed.
   - objclass (`objects[]`): converted to `oc_get()`/`oc_put()`/`oc_set()`
     across `src/` and `include/obj.h`, `objclass.h`, and `hack.h`. The table
     is a plain extern; with `RP6502_MONS_XRAM` the link gives the symbol
     `OBJS_XRAM_ADDR` (`--defsym`), and the host harness does the same with
     `-no-pie`. `xramtest` also writes `objects.bin` and checks it on target.
   - Measured by linking the whole game (`--target nethack`, excluded from
     `all` because it cannot link yet). Both configurations fail only on
     region overflow, with no undefined symbols. Sum of the modules' sections,
     before placement:
       flag off: .text 2605019, .rodata 292845, .data 40690, .bss 67137
       XRAM tables: .text 2665371 (+60 KB accessor code), .rodata 292729,
         .data 40690 in the objects (the table copies are dropped by the
         link, which the placement overflow confirms: 31668 bytes less data),
         .bss 67451.
     So the game needs about 2.6 MB of code (overlays) and about 293 KB of
     rodata, most of it strings. The rodata is the next RAM problem to plan
     for: it would need XRAM placement or a smaller string set.
3. **Budget the rest of memory.** Moving both tables alone will not make the
   measured unlinked data fit. Investigate `level`, room and display buffers,
   vision data, heap, stack, resident code, and overlay slots; measure the
   actual linked footprint as sections are eliminated or moved.
   Strings (in progress): string literals total 265 KB, almost all short
   messages (10.5k under 20 characters, 4.9k at 20–59, 71 longer), so they go
   to storage, not XRAM. Done: [strtab.py](sys/rp6502/tools/strtab.py) rewrites
   every function-body literal in `src/` (except the monster and object
   tables) to `MSG(id, "text")`, and the static tables named in
   [msgids.txt](sys/rp6502/msgids.txt) to `MSGID(id)`. The text is in
   `msg.dat`/`msg.idx`, read by [msgfetch.c](sys/rp6502/msgfetch.c) with
   open/lseek/read. Every build uses the rewrite: CMake `nethack_objs`
   (generated sources in the build directory), `hostbuild.sh`, and
   `xramhost.sh`, all compiled with `MSG_CHECK`, which compares each fetch with
   its literal. Verified: the full verification passes; autoplay runs; the
   storage test [msgtest.c](sys/rp6502/msgtest.c) reads all 9502 strings in the
   emulator from the ROM's named assets and matches the host checksum. The
   tables are packaged with `rp6502_asset` as `ROM:msg.dat` and `ROM:msg.idx`
   (the SDK's named-asset path, which the game opens like any file); the
   `nethack` and `msgtest` targets both carry them. The `compopt` option
   descriptions now use `MSG_ST("text")` markers (rewritten to `MSGID(n)`),
   read with `MSGTXT`; names stay in RAM because they are matched
   at runtime. The `extcmdlist` descriptions use the same marker and are read
   with `MSGTXT`; the one caller that returns a description (`key2extcmddesc`)
   copies it into a static buffer first. Still in RAM: about 44 KB of literals
   in static tables, mostly names that are matched at runtime, and the largest
   others are `loadsyms`, `roles`, `boolopt`, `propertynames`; `mons_names` and
   the object names are outside the rewrite. Next: convert the remaining
   description-type tables the same way (the compiler lists each use once the
   type changes).
   XRAM map (defined in [xram.h](include/xram.h)): `mons[]` at $1000
   (19864 bytes), `objects[]` at $6100 (11804 bytes, ending at $8F1B), the
   level map at $9000 (10080 bytes), `lastseentyp` at $B7A0 and `SpLev_Map`
   at $BE30 (1680 bytes each), the glyph buffer at $C4C0 (5040 bytes), the
   vision planes at $D880, $DF10 and $E5A0 (1680 bytes each), the overlay
   directory at $0000 and the overlay descriptor at $F000. The earlier
   overlay defaults ($8000 for the descriptor, $A000/$C000 for the directory
   and descriptor) overlapped the tables, so they moved. Compile-time checks
   in [rp6502xram.c](sys/rp6502/rp6502xram.c) and
   [rp6502lev.c](sys/rp6502/rp6502lev.c) keep the regions apart. The SDK
   reserves no XRAM; device areas (keyboard, sound, video) must be added to
   the map before the program maps them.
   Level map (done): the map `levl[][]` (`level.locations`, 1680 cells of 6
   bytes, 10080 bytes) lives in XRAM at $9000 with `RP6502_XRAM_TABLES`. All
   uses go through `lev_get()`/`lev_put()` (rm.h), which are plain RAM in the
   host build and XRAM accessors in [rp6502lev.c](sys/rp6502/rp6502lev.c)
   (target) and [rp6502xram_host.c](sys/rp6502/rp6502xram_host.c) (host
   harness, bounds-checked). Converted by
   [levrw.py](sys/rp6502/tools/levrw.py) (accesses to `levl[x][y]` become
   read calls or read-modify-write blocks), [levalias.py](sys/rp6502/tools/levalias.py)
   (local `struct rm *` aliases of one cell), and by hand (pointer walks,
   helpers that took a cell pointer: `does_block`, `cvt_sdoor_to_door`,
   `set_seenv`, `unset_seenv`, `t_warn`, `wall_angle`, and the lock and kick
   state). Verified: the host RAM build, the XRAM host harness (autoplay seeds
   1, 2, 4, 5, 6 without aborts), and the target objects with the XRAM tables
   (`nethack_objs`, `nethack` compile; link fails only on region overflow).
   `.bss` for the XRAM tables is about 55 KB (was 67 KB). `lastseentyp[][]`
   ($B7A0) and `SpLev_Map[][]` ($BE30) also moved, one byte per cell, through
   `lst_get`/`lst_put` and `splm_get`/`splm_put`. Save and restore write and
   read the maps one column at a time, the same bytes as before; a save and a
   restore round trip on the XRAM host build works. The glyph buffer `gbuf`
   (5 KB, row-major, `gb_get(row, col)`/`gb_put`) moved too. The vision
   planes moved as well: `viz_clear`, `left_ptrs` and `right_ptrs` (1680
   bytes each) are read and written through `vcl_`, `lpt_` and `rpt_`
   accessors in [vision.c](src/vision.c), converted by
   [visrw.py](sys/rp6502/tools/visrw.py). `cs_rows0/1` and `could_see` (3360
   bytes, the double-buffered visibility rows that `viz_array` points into)
   stay in RAM: `cansee()` and `couldsee()` read them through row pointers
   from about 30 places outside vision.c, so they need their own pass.
   On the target, [planetest.c](sys/rp6502/planetest.c) writes a pattern to
   every cell of every XRAM plane and reads it back: `planes: 0 bad of 16800`.
   The accessors use the portal registers directly (`RIA.addr0`, `RIA.rw0`)
   instead of `xram0_read`/`xram0_write`. The cost of those accessors is
   estimated, not measured: each vision access is a portal access with address
   setup, and vision runs on every move. Measure it on the target before
   deciding whether the cs rows should follow. Still in RAM: `rooms` (5 KB),
   `obufs` (3 KB, `char *` text buffers), `cs_rows0/1` and `could_see` (3.4 KB),
   and the per-cell object and monster list heads (3360 bytes each, pointers
   to RAM structures). `STATUS_HILITES` is off for the RP6502 build
   (`rp6502conf.h`): its entries are linked by pointers, and the feature is
   not worth its RAM here; `botl.c` skips the highlight-rule checks without
   it. That also shrank `.text` by about 48 KB. `.bss` is now about 41 KB.
   Open: one SIGSEGV in the XRAM host harness (seed 1, during a prayer,
   after the glyph-buffer move). It has not reproduced in 14 further runs or
   under gdb, and no core file was written. If it recurs, take a core dump
   and check the glyph buffer and the prayer path first.
   Budget plan (measured from the linker's kept sections with
   [mapsizes.py](sys/rp6502/tools/mapsizes.py) on the map that CMake writes to
   `build/.../nethack.map` in the XRAM-tables configuration):
   `.text` 2,766,517 bytes (overlays), `.rodata` 85,122 bytes, `.data` 9,040
   bytes, `.bss` 43,929 bytes. The earlier figure of 41 KB of `.data` counted
   the RAM copies of `mons[]` and `objects[]` that the linker drops, so the
   static RAM is about 138 KB, and `.rodata` is most of it. The 64 KB CPU
   address space must also hold the stack, the zero page and system area,
   resident code, and overlay slots, so the static data has to drop to a few
   tens of KB. The largest parts, in order:
   - `.rodata` (85 KB): about 44 KB of literal names and text still in RAM
     (the `compopt`, `extcmdlist`, `boolopt`, `roles` and similar tables keep
     their names; `mons_names` and the object names are outside the rewrite),
     plus the const structure tables (`roles`, `races`, `defsyms`,
     `def_monsyms`, `shtypes`, and others). Storage streaming is the plan
     (decided 2026-11-01): packaged as ROM assets and read on demand. Names
     are matched at run time, so they need a fetch slot and a copy-out, not a
     plain `MSGID`. Progress on the names sweep: `developers[]` (mplayer.c,
     116 bytes) is converted through `msgids.txt` and `MSGTXT` at each use;
     `propertynames[]` (timeout.c, 67 names) is converted with an `MSG_ST`
     marker per name, a `msgid_t` field and a `MSGNULL` sentinel, and each
     loop copies the name into a `PROPNAME_LEN` buffer with
     `prop_name_text()`. Its `#timeout` and `#wizintrinsic` paths compile and
     pass the autoplay, but were not run interactively (the test driver could
     not reach the extended-command prompt); check them by hand in wizard
     mode. `spellings[]` (objnam.c, 34 alternate names) is converted the same
     way, and a wizard-mode wish for "silver sabre" returns the silver saber
     on the host (wishtest.py). The rest are next. Plain `const char *` arrays take the msgids
     route;
     struct tables need a per-field `MSG_ST` marker (as `compopt` and
     `extcmdlist` do); char-array tables (`blind_writing[][21]`) need their
     own accessor. Check each use: a `MSGTXT` pointer is valid for eight more
     fetches, so copy it out before the next message.
   - `.bss` (44 KB): `rooms` (5 KB, 199 uses), `obufs` (3 KB, `char *` text
     buffers), `cs_rows0/1` with `could_see` (3.4 KB, see the vision note),
     and the per-cell object and monster list heads (6.7 KB, RAM pointers).
     `status_hilites` is off for this port.
   - `.data` (9 KB): small; the `mons`/`objects` RAM mirrors are dropped at
     link, and CMake still compiles them into `nethack_objs`. Removing those
     copies from `nethack_objs` would make the object sums match the linked
     sizes; it does not change the linked size.
   Each moved structure needs the same kind of accessor as the level map, and
   each access costs a storage or XRAM transfer. The per-access cost on the
   target is estimated, not measured; measure a whole move, not one access.
   Priority 4 (overlays) is still required for the code itself.
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
normal exit; use fresh output for each rebuilt executable. Autoplay used to
stall when the game sat at a prompt whose text had output after it; it now
answers a prompt once output has been idle, so check its exit status and the
resulting profile rather than assuming a timed-out run succeeded. Raw keyboard
polling dominated the earlier call counts, so account for that when using the
graph to choose overlays.

The prior machine's `build/hostprof/callgraph.tsv`, raw `cgprof.out`,
`build/mos/` assembly, statistics and helper scripts, and `/tmp/ot/` test
outputs are **not in Git**. Copy the call graph separately before leaving
that machine if the exact prior runs are needed, or regenerate it. Raw profile
addresses require the matching host executable. The installed llvm-mos SDK,
emulator, and Python environments are also machine-local and must be
installed or copied separately.
