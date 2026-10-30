# NetHack on the RP6502 — Feasibility Report

Target: [Picocomputer RP6502](https://picocomputer.github.io/sdk.html) (WDC 65C02 at up to
8 MHz), display through the console terminal with DECgraphics. Measurements were taken with
cc65 V2.19 (`-t rp6502 -Os -Cl`) using [tools/measure-cc65.sh](tools/measure-cc65.sh).

## 0. Decisions (2026-10-30)

* **Base: NetHack 3.6** (no Lua). The `rp6502` branch starts from upstream `NetHack-3.6`
  (3.6.7). Sections 1–6 below are the original analysis of 5.0; §9 has the 3.6 numbers.
* **Compiler: llvm-mos** (changed from cc65 the same day). cc65 cannot compile 76 of the
  111 3.6 modules: its code generator (`CheckLocalOffs` in `codegen.c`, still present in
  upstream master) rejects any parameter or scalar local more than 255 bytes into the stack
  frame, which is every function with a `BUFSZ` (256) buffer declared before its scalars.
  Arrays at large offsets are fine; only scalar access fails. `-Cl` (static locals) avoids it
  but breaks recursion and bloats BSS. llvm-mos has no prebuilt Linux arm64 build
  ([llvm-mos#273](https://github.com/llvm-mos/llvm-mos/issues/273)), so it is built from
  source on the development Pi 5 together with the picocomputer `llvm-mos-sdk` fork, which is
  what the picocomputer CI uses for its SDK releases.
* **Display: tty window port with DECgraphics** on the RP6502 console. Verified by
  milestone 1 (`sys/rp6502/dectest.c`).

## 1. Summary (NetHack 5.0)

| Question | Answer |
| --- | --- |
| Does the display side work? | **Yes, easily.** The console is a VT102/Linux-console terminal, 80 columns, ANSI color, and supports `ESC ( 0` / SO / SI DEC Special Graphics. NetHack's tty window port with `DECgraphics` should work almost unchanged. |
| Can the 6502 address more than 64 KB? | **No.** 16-bit bus, 63.75 KB RAM. XRAM (64 KB) is reachable only through the RIA portals (`xram0_read()` etc.), not as executable/addressable memory. There is deliberately no banking; the design expects data to be streamed from storage (~800 KB/s) into RAM/XRAM. |
| Does NetHack 5.0 fit natively? | **Not without a large overlay system and a data restructure.** ~3 MB of 6502 code+rodata vs ~64 KB of RAM. |
| Is cc65 a good fit? | **No — recommend llvm-mos.** cc65 lacks 64-bit integers, floating point (Lua needs it), C99 mixed declarations/some initializers, and limits stack frames to 256 bytes. 43 of 132 modules fail to compile even with sizing shortcuts. |
| Biggest single obstacle | **Lua.** 5.0 generates every special level and the dungeon layout by running 131 Lua scripts (616 KB) in an embedded Lua 5.4 interpreter, which needs floats and a large heap. |

Bottom line: the port is possible in principle, but it is a multi-month systems project, not
a recompile. The decisions in §6 should be made before any porting code is written.

## 2. Platform facts that drive the design

| Resource | Size | Notes |
| --- | --- | --- |
| 6502 RAM | 63.75 KB ($0000–$FEFF) | All of it free, including zero page. Code must run from here. |
| XRAM | 64 KB | Data only. Accessed through two portals; also where console/VGA/input devices are mapped (console mode 0 needs none). |
| ROM assets | Size of the `.rp6502` file | Named assets open as read-only files `ROM:name`; several can be open at once. Ideal for overlays and static data. |
| USB storage | FAT | Save files, bones, level swap files. |
| Load speed | ~800 KB/s file → XRAM | A 16 KB overlay loads in ~20 ms. |
| CPU | 65C02, up to 8 MHz | |

## 3. Code size

### 3.1 Totals (NetHack 5.0 `src/`, cc65)

| | Bytes |
| --- | --- |
| 89 modules that compile — CODE | 1,516,696 |
| 89 modules that compile — RODATA | 317,330 |
| 89 modules that compile — DATA / BSS | 55,729 / 236,733 |
| 43 failing modules — estimated code+rodata¹ | ~1,252,000 |
| **Total code+rodata (excluding Lua, window port)** | **~3.1 MB** |

¹ Estimated from each module's native object size × 0.736, the measured cc65/x86-64 ratio
across the 89 modules that compiled.

The tty window port (`win/tty`) and the Lua interpreter come on top of this. The Lua 5.4 core
cannot be built with cc65 at all (`Floating point arithmetic not supported`); with llvm-mos it
would add an estimated 150–250 KB of code.

### 3.2 Largest modules (code+rodata)

| Module | Bytes | | Module | Bytes |
| --- | --- | --- | --- | --- |
| options | ~102 K (est) | | objnam | 74.6 K |
| trap | 97.3 K | | mon | ~71 K (est) |
| shk | 85.5 K | | apply | 69.0 K |
| uhitm | 81.8 K | | hack | ~64 K (est) |
| sp_lev | ~78 K (est) | | cmd | ~58 K (est) |
| zap | ~76 K (est) | | invent | ~58 K (est) |

61 modules are larger than 16 KB, so **overlays must be function-granular, not file-granular.**

### 3.3 Function granularity

* 5,558 functions.
* Largest (estimated 6502 size): `attributes_enlightenment` ~11 KB, `flip_level` ~9.7 KB,
  `newsym` ~9 KB, `throwit` ~8.4 KB. 43 functions exceed 4 KB, 4 exceed 8 KB, none exceed 16 KB.
* So a 16 KB overlay window can hold any single function, and an 8 KB window holds all but 4.

## 4. Data size (6502 sizes, measured with cc65 `sizeof`)

| Object | Bytes | Notes |
| --- | --- | --- |
| `glyphmap[]` | **134,736** | Larger than the whole address space. Must be replaced with a computed glyph→symbol mapping for tty. |
| `gg` (instance globals "g") | 35,803 | Mutable. Needs splitting/trimming. |
| `mons[]` (383 × 60) | 22,980 | Mostly constant — can live in ROM assets. |
| `objects[]` (481 × 42) | 20,202 | Partly mutable (discoveries, shuffled descriptions). Split const/mutable. |
| `svl` (saved level globals) | 19,013 | Includes `svl.level` 16,821 (map + per-square object/monster pointers). |
| `svr` | 5,658 | |
| `struct you` | 1,681 | |
| `struct monst` / `struct obj` | 106 / 54 each | A typical level with ~50 monsters and ~200 objects is ~16 KB of heap. |

Static DATA+BSS of only the 89 compiling modules is already ~292 KB. Mutable game state alone
cannot fit in RAM next to code; it must be split into a hot set in RAM and a cold set in XRAM
(and per-level data swapped to storage, as NetHack already does for non-current levels).

## 5. Compiler findings (cc65)

Error categories from the full build (after sizing shortcuts):

| Issue | Impact |
| --- | --- |
| No `long long` | Used by ISAAC64 RNG, `FITSint_`, misc. Workable (disable ISAAC64, patch). |
| No floating point | **Blocks Lua entirely.** |
| 256-byte stack frame limit | `char buf[BUFSZ]` (256) appears in hundreds of functions; needs `-Cl` static locals (breaks recursion) or smaller buffers. |
| No mixed declarations and code | 25+ sites; C99 style throughout 5.0. |
| Designated/complex initializers, `Constant expression expected` | `cmd.c`, `nhlua.c`, `selvar.c`, … |
| Passing 6–8 byte structs by value | A few sites. |
| Headers (`curses.h`, `term.h`, POSIX) | Expected; replaced by an rp6502 sys port. |

llvm-mos supports full C99/C11, 64-bit integers, soft float, and does whole-program static stack
allocation (non-recursive functions get static frames automatically), and usually produces
noticeably smaller code than cc65. It removes almost every item in this table except the
headers.

## 6. Decisions needed

### 6.1 Compiler: **recommend llvm-mos**
cc65 would require invasive source changes across 5.0 and makes Lua impossible.

### 6.2 What to do about Lua

| Option | Pros | Cons |
| --- | --- | --- |
| A. Run Lua 5.4 on the 6502 (llvm-mos) | Source stays close to upstream | +150–250 KB code, needs a large heap that does not exist; level creation would be very slow. Not realistic. |
| B. Replace Lua with build-time output | No interpreter at run time | Scripts contain logic and randomness; needs a translator (Lua → C, or Lua → compact custom bytecode) for the subset of the `des.*` API they use. Big but bounded tool. |
| C. Base the port on NetHack 3.6 | No Lua: special levels are binary data compiled by `lev_comp`; ~25% less C (6.2 MB vs 8.2 MB source → ~2.3 MB 6502 code) | Older game; diverges from this repo's 5.0 branch. |

### 6.3 Execution model

**Native with overlays (recommended to prototype first).**
* Resident (~16–20 KB): C runtime, RIA calls, overlay manager, hottest code (RNG, alloc,
  `hacklib`, tty output, core display/vision).
* Overlay window(s) (~16 KB): function-granular overlays packed into ROM assets.
* XRAM as an overlay cache, so recently used overlays reload from XRAM rather than storage.
* A build tool generates the overlay packing and call thunks: cross-overlay calls go through
  a thunk that loads the callee's overlay and, on return, reloads the caller's overlay if it
  was evicted. Packing is profile-guided: record a call graph from a native NetHack run, then
  cluster functions that call each other heavily into the same overlay to avoid thrashing.
* There is precedent: earlier MS-DOS NetHack 3.x builds split source files into overlay
  segments to fit in 640 KB.

**Alternative: a virtual machine.** Compile NetHack for a compact 32-bit target (for example
RV32 with compressed instructions) and run an interpreter on the 6502 with demand-paged code
and data (RAM cache ← XRAM ← ROM/USB). This needs almost no NetHack source changes and would
even run Lua, but it would likely be 20–50× slower than native code. At 8 MHz that probably
means seconds per turn and minutes per level creation. Keep it as a fallback only.

### 6.4 Data plan (either model)
* Replace `glyphmap` with computed mapping (tty only needs glyph → symbol + color).
* Move constant tables (`mons[]` stats, object names/descriptions, rumors, quest text,
  `data.base`, help) into ROM assets and read them on demand.
* Shrink `BUFSZ` (256 → 128 or less) and audit large static buffers in `gg`.
* Split mutable state: hot in RAM, cold in XRAM through accessors; keep only the current level
  in memory, swap others to USB.

## 7. Proposed milestones

1. **Scaffold.** Add the RP6502 SDK template (CMake, `tools/`, presets) at the repository
   root, build a test ROM, verify DECgraphics, color and key input on the emulator.
2. **sys port + tty.** `sys/rp6502` (file I/O on `ROM:` and USB, time, RNG, exit) and the tty
   window port compiled for the target, driven by a stub game loop.
3. **Overlay toolchain.** Per-function sections, call-graph profiler on a native build,
   packer, thunk generator, overlay manager with XRAM cache. Prove it on a mid-sized module set.
4. **Data restructure.** Constant tables to ROM, BUFSZ, hot/cold split (with 3.6 there is no
   `glyphmap` to remove).
5. **Levels.** With the 3.6 base: package the `lev_comp` binary level files and `dungeon`
   as ROM assets.
6. **Full game, then performance tuning** (overlay packing from real play profiles).

Status: milestone 1 is done — the SDK project is at the repository root (`CMakeLists.txt`,
`CMakePresets.json`, `tools/`, `.vscode/`) and `dectest.rp6502` draws a DECgraphics map with
color and moves an `@` with raw keys from `TTY:` (non-blocking raw console input, fd 4).

Milestone 2 status: the port layer is written and the full game runs as a native host build
of the RP6502 configuration (`sys/rp6502/tools/hostbuild.sh`, `RP6502_HOST`): character
selection, DECgraphics map via SO/SI, ANSI color, arrow keys, special levels, and
save/restore all work. The port pieces are:

* `include/rp6502conf.h` — port configuration (`MICRO`, `ANSI_DEFAULT`, no signals, mail,
  shell, compression, DLB or SYSCF), included from `global.h`.
* `sys/rp6502/rp6502sys.c` — `main`, file prefixes (data from `ROM:`, writable files in
  `nethack/` on USB), option parsing, lock and restore logic, `error`.
* `sys/rp6502/rp6502tty.c` — raw keyboard on `TTY:` (termios on the host), cursor keys,
  `kbhit`, screen size.
* Small hooks in `config1.h`, `dlb.h` (data files go through `DATAPREFIX`), `tcap.h`
  (`ASCIIGRAPH`), `termcap.c` (DEC G1 set, full color table) and `options.c`
  (DECgraphics and color by default).
* `sys/rp6502/sources.txt` — the module list shared by the host script and CMake.

Still to do for milestone 2: compile for the target with llvm-mos. Known gaps: a stale lock
from a crashed game is not detected, and the screen size is fixed at 80x24.

## 8. Reproducing the measurements

```sh
# NetHack 5.0 (needs network for the Lua headers)
sys/rp6502/tools/measure-cc65.sh /tmp/nhcc /path/to/nethack-5.0
# NetHack 3.6 (generate pm.h, onames.h, vis_tab.h, date.h with util/makedefs first)
CC65_EXTRA=-D__GO32__ sys/rp6502/tools/measure-cc65.sh /tmp/nhcc36
cat /tmp/nhcc36/res_src.tsv                # module, ok|FAIL, CODE, RODATA, DATA, BSS
```

Native object sizes (for the 5.0 estimate ratio) come from `size src/*.o` of a normal Linux
build.

## 9. NetHack 3.6 measurements

Upstream `NetHack-3.6` (3.6.7), same cc65 harness, `-D__GO32__` to skip the K&R libc
redeclarations in `system.h`.

| | NetHack 5.0 | NetHack 3.6 |
| --- | --- | --- |
| Modules compiling with cc65 | 89 / 132 | **94 / 109** |
| Measured code+rodata | 1.83 MB | 2.13 MB |
| Estimated total code+rodata | ~3.1 MB | **~2.6 MB**² |
| Modules > 16 KB | 61 | 54 |
| Lua | 131 scripts + interpreter | **none** (binary level files from `lev_comp`) |
| Glyph map table | 134,736 bytes | **none** (`mapglyph()` computes it) |
| `struct monst` / `struct obj` | 106 / 54 | 74 / 44 |
| `struct permonst` × count | 60 × 383 | 54 × 382 (`mons[]` 20.6 KB) |
| `struct objclass` × count | 42 × 481 | 26 × 454 (`objects[]` 11.8 KB) |
| `level` (map + square pointers) | 16,821 | 16,815 |
| `struct you` | 1,681 | 1,370 |

² The 15 failing 3.6 modules are estimated at 0.398 bytes of 6502 output per byte of C
source, the ratio measured on the 94 that compile.

The 15 failures are mostly system-dependent modules that the port replaces or stubs
anyway (`files`, `mail`, `topten`, `version`, `options`, `end`, `save`/`restore`), plus
a few small C issues (struct return of 9 bytes, a 64-bit constant in `u_init`, missing
`getuid`/`stat`). 3.6 is C89, so cc65's lack of C99 is not a problem.

The code is not much smaller than 5.0's, so the overlay design in §6.3 still applies.
The data side is much easier: no glyph map, smaller structs, no Lua heap.
