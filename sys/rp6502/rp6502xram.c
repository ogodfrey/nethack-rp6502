/* Fetch cache for the tables kept in XRAM (see include/rp6502xram.h). */
#include "config.h"
#include "permonst.h"
#include "objclass.h"
#include "rp6502xram.h"
#include "pm.h"     /* NUMMONS */
#include "onames.h" /* NUM_OBJECTS */
#include <rp6502.h>
#include <stdint.h>

/* The XRAM map (include/xram.h) must keep the tables apart. Each array has a
 * terminator record past NUMMONS / NUM_OBJECTS. */
_Static_assert(MONS_XRAM_ADDR + (NUMMONS + 1) * sizeof(struct permonst)
                   <= OBJS_XRAM_ADDR,
               "mons[] overlaps objects[] in XRAM");
_Static_assert(OBJS_XRAM_ADDR + (NUM_OBJECTS + 1) * sizeof(struct objclass)
                   <= OVL_DESC_XRAM,
               "objects[] overlaps the overlay descriptor in XRAM");
_Static_assert(OVL_DESC_XRAM < OVL_DIR_XRAM,
               "overlay descriptor overlaps the directory in XRAM");

#define PM_SLOTS 4

static struct permonst pm_slot[PM_SLOTS];
static unsigned char pm_next;
static struct objclass oc_slot[PM_SLOTS];
static unsigned char oc_next;

struct permonst *
rpm_pm_get(const struct permonst *p)
{
    struct permonst *s = &pm_slot[pm_next];

    pm_next = (pm_next + 1) % PM_SLOTS;
    xram0_read(s, (unsigned) (uintptr_t) p, sizeof *s);
    return s;
}

void
rpm_pm_put(struct permonst *p, const struct permonst *rec)
{
    xram0_write((unsigned) (uintptr_t) p, rec, sizeof *rec);
}

int
rpm_index(const struct permonst *p)
{
    return (int) (((uintptr_t) p - MONS_XRAM_ADDR) / sizeof(struct permonst));
}

struct objclass *
rpo_get(const struct objclass *p)
{
    struct objclass *s = &oc_slot[oc_next];

    oc_next = (oc_next + 1) % PM_SLOTS;
    xram0_read(s, (unsigned) (uintptr_t) p, sizeof *s);
    return s;
}

void
rpo_put(struct objclass *p, const struct objclass *rec)
{
    xram0_write((unsigned) (uintptr_t) p, rec, sizeof *rec);
}
