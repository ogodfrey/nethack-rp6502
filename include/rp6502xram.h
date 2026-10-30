/* NetHack RP6502 port: tables kept in XRAM.
 *
 * On the target, mons_at(i) is the XRAM address of mons[i]. It is not a RAM
 * address, so it can be compared and subtracted like &mons[i], but it must
 * not be dereferenced. pm_get(p) copies the record at p from XRAM into a RAM
 * slot; the slot stays valid until pm_get() has been called PM_SLOTS more
 * times. On the host both are identities over the real array.
 */
#ifndef RP6502XRAM_H
#define RP6502XRAM_H

#include "permonst.h"

#ifdef RP6502_HOST
#define mons_at(i) (&mons[i])
#define pm_get(p) (p)
#else
#include <stdint.h>

/* XRAM address of mons_at(0); the table occupies NUMMONS * 60 bytes from here. */
#define MONS_XRAM_ADDR 0x1000

#define mons_at(i) \
    ((struct permonst *) (uintptr_t) (MONS_XRAM_ADDR \
                                      + (unsigned) (i) * sizeof(struct permonst)))
struct permonst *rpm_pm_get(struct permonst *p);
#define pm_get(p) rpm_pm_get(p)
#endif

#endif /* RP6502XRAM_H */
