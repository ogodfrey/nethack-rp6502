/* Fetch cache for the tables kept in XRAM (see include/rp6502xram.h). */
#include "config.h"
#include "permonst.h"
#include "objclass.h"
#include "rp6502xram.h"
#include <rp6502.h>
#include <stdint.h>

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
