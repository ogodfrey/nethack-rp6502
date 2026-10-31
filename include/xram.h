/* NetHack RP6502 XRAM map. Each XRAM address the port uses is defined here.
 * CMakeLists.txt reads MONS_XRAM_ADDR and OBJS_XRAM_ADDR from this file to
 * place the table images, so keep each definition on one line in this form.
 *
 * Layout (sizes from the tables, with the terminator record; rp6502xram.c
 * and rp6502lev.c check them at compile time):
 *   $0000-$0FFF  overlay directory, see ovlpack.py (8 bytes per overlay)
 *   $1000-$5DD7  mons[]     382 records x 52 bytes
 *   $6100-$8F1B  objects[]  454 records x 26 bytes
 *   $9000-$B79F  level map  80 x 21 cells x 6 bytes (see rm.h)
 *   $D200-$FFFF  overlay descriptor, see ovlpack.py (3 bytes per entry)
 * The SDK does not reserve XRAM; a device (keyboard, sound, video) needs its
 * own area here before the program maps it.
 */
#ifndef RP6502_XRAM_H
#define RP6502_XRAM_H

/* Monster table (mons[], permonst records) when it lives in XRAM. */
#define MONS_XRAM_ADDR 0x1000
/* Object table (objects[], objclass records) when it lives in XRAM. */
#define OBJS_XRAM_ADDR 0x6100
/* Level map (levl[][], rm records) when it lives in XRAM. */
#define LEV_XRAM_ADDR 0x9000
/* Overlay runtime: descriptor and directory (the packer's defaults). */
#define OVL_DESC_XRAM 0xD200
#define OVL_DIR_XRAM 0x0000

#endif /* RP6502_XRAM_H */
