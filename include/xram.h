/* NetHack RP6502 XRAM map. Each XRAM address the port uses is defined here.
 * CMakeLists.txt reads MONS_XRAM_ADDR from this file to place the monster
 * table image, so keep the definition on one line in this form.
 */
#ifndef RP6502_XRAM_H
#define RP6502_XRAM_H

/* Monster table (mons[], permonst records) when it lives in XRAM. */
#define MONS_XRAM_ADDR 0x1000
/* Object table (objects[], objclass records) when it lives in XRAM. Follows
 * the monster table (20628 bytes, ends at 0x6094). */
#define OBJS_XRAM_ADDR 0x6100

#endif /* RP6502_XRAM_H */
