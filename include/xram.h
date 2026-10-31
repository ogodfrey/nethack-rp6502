/* NetHack RP6502 XRAM map. Each XRAM address the port uses is defined here.
 * CMakeLists.txt reads MONS_XRAM_ADDR and OBJS_XRAM_ADDR from this file to
 * place the table images, so keep each definition on one line in this form.
 *
 * Layout (sizes from the tables, with the terminator record; rp6502xram.c
 * checks them at compile time):
 *   $1000-$5DD7  mons[]     382 records x 52 bytes
 *   $6100-$8F1B  objects[]  454 records x 26 bytes
 *   $A000        overlay descriptor (3 bytes per overlay), see ovlpack.py
 *   $C000        overlay directory, see ovlpack.py
 * The SDK does not reserve XRAM; a device (keyboard, sound, video) needs its
 * own area here before the program maps it.
 */
#ifndef RP6502_XRAM_H
#define RP6502_XRAM_H

/* Monster table (mons[], permonst records) when it lives in XRAM. */
#define MONS_XRAM_ADDR 0x1000
/* Object table (objects[], objclass records) when it lives in XRAM. */
#define OBJS_XRAM_ADDR 0x6100
/* Overlay runtime: descriptor and directory (the packer's defaults). */
#define OVL_DESC_XRAM 0xA000
#define OVL_DIR_XRAM 0xC000

#endif /* RP6502_XRAM_H */
