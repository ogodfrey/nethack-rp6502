/* Host stand-in for the target fetch cache in rp6502xram.c, used by
 * sys/rp6502/tools/xramhost.sh. The table has no RAM behind it here: pm_get()
 * copies the record from the RAM image rp6502_ram_mons and aborts if its
 * argument is not a table address, so any access that was not converted
 * fails at once instead of reading the wrong data.
 */
#include "config.h"
#include "pm.h"
#include "permonst.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define PM_SLOTS 4

extern struct permonst rp6502_ram_mons[];

static struct permonst pm_slot[PM_SLOTS];
static unsigned char pm_next;

struct permonst *
rpm_pm_get(const struct permonst *p)
{
    uintptr_t off = (uintptr_t) p - (uintptr_t) MONS_XRAM_ADDR;
    struct permonst *s;

    if (off % sizeof(struct permonst) != 0
        || off / sizeof(struct permonst) >= NUMMONS) {
        fprintf(stderr, "pm_get: %p is not a monster table address\n",
                (const void *) p);
        abort();
    }
    s = &pm_slot[pm_next];
    pm_next = (pm_next + 1) % PM_SLOTS;
    *s = rp6502_ram_mons[off / sizeof(struct permonst)];
    return s;
}

void
rpm_pm_put(struct permonst *p, const struct permonst *rec)
{
    uintptr_t off = (uintptr_t) p - (uintptr_t) MONS_XRAM_ADDR;

    if (off % sizeof(struct permonst) != 0
        || off / sizeof(struct permonst) >= NUMMONS) {
        fprintf(stderr, "pm_put: %p is not a monster table address\n",
                (void *) p);
        abort();
    }
    rp6502_ram_mons[off / sizeof(struct permonst)] = *rec;
}
