/* NetHack RP6502 port: tables kept in XRAM.
 *
 * Included from permonst.h after struct permonst is defined.
 *
 * With RP6502_MONS_XRAM (target only) the monster table has no RAM copy:
 * mons names the XRAM image, so &mons[i] (mons_at(i)) is an XRAM address.
 * It can be compared, subtracted and indexed, but never dereferenced.
 * pm_get(p) copies the record at p from XRAM into a RAM slot; the slot is
 * valid until pm_get() has been called PM_SLOTS more times, so use the result
 * at once and do not keep it.
 *
 * Without the flag, mons is the RAM array and pm_get is the identity.
 */
#ifndef RP6502XRAM_H
#define RP6502XRAM_H

#include <stdint.h>

/* Address of mons[0] in XRAM. It is used only as a number and is never
   dereferenced by the CPU. */
#define MONS_XRAM_ADDR 0x1000

#if defined(RP6502_MONS_XRAM) && !defined(RP6502_MONS_RAM)
/* With the table in XRAM, mons names the XRAM image: &mons[i] is an XRAM
   address. It can be compared, subtracted and indexed, but not dereferenced
   (use pm_get()). */
#define mons ((struct permonst *) (uintptr_t) MONS_XRAM_ADDR)
#define pm_get(p) rpm_pm_get(p)
struct permonst *rpm_pm_get(const struct permonst *p);
#else
extern NEARDATA struct permonst mons[]; /* the master list of monster types */
#define pm_get(p) ((struct permonst *) (p))
#endif

#define mons_at(i) (&mons[i])

#endif /* RP6502XRAM_H */
