/* Fetch cache for the tables kept in XRAM (see include/rp6502xram.h). */
#include "config.h"
#include "permonst.h"
#include "rp6502xram.h"
#include <rp6502.h>
#include <stdint.h>

#define PM_SLOTS 4

static struct permonst pm_slot[PM_SLOTS];
static unsigned char pm_next;

struct permonst *
rpm_pm_get(struct permonst *p)
{
    struct permonst *s = &pm_slot[pm_next];

    pm_next = (pm_next + 1) % PM_SLOTS;
    xram0_read(s, (unsigned) (uintptr_t) p, sizeof *s);
    return s;
}
