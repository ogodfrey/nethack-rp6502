/* Host stand-in for the target fetch cache in rp6502xram.c, used by
 * sys/rp6502/tools/xramhost.sh. The table has no RAM behind it here: pm_get()
 * copies the record from the RAM image rp6502_ram_mons and aborts if its
 * argument is not a table address, so any access that was not converted
 * fails at once instead of reading the wrong data.
 */
#include "config.h"
#include "pm.h"
#include "onames.h"
#include "permonst.h"
#include "objclass.h"
#include "hack.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PM_SLOTS 4

extern struct permonst rp6502_ram_mons[];
extern struct objclass rp6502_ram_objects[];

static struct permonst pm_slot[PM_SLOTS];
static unsigned char pm_next;
static struct objclass oc_slot[PM_SLOTS];
static unsigned char oc_next;

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

int
rpm_index(const struct permonst *p)
{
    uintptr_t off = (uintptr_t) p - (uintptr_t) MONS_XRAM_ADDR;

    if (off % sizeof(struct permonst) != 0
        || off / sizeof(struct permonst) >= NUMMONS) {
        fprintf(stderr, "pm_name: %p is not a monster table address\n",
                (const void *) p);
        abort();
    }
    return (int) (off / sizeof(struct permonst));
}

struct objclass *
rpo_get(const struct objclass *p)
{
    uintptr_t off = (uintptr_t) p - (uintptr_t) OBJS_XRAM_ADDR;
    struct objclass *s;

    if (off % sizeof(struct objclass) != 0
        || off / sizeof(struct objclass) >= NUM_OBJECTS) {
        fprintf(stderr, "oc_get: %p is not an object table address\n",
                (const void *) p);
        abort();
    }
    s = &oc_slot[oc_next];
    oc_next = (oc_next + 1) % PM_SLOTS;
    *s = rp6502_ram_objects[off / sizeof(struct objclass)];
    return s;
}

void
rpo_put(struct objclass *p, const struct objclass *rec)
{
    uintptr_t off = (uintptr_t) p - (uintptr_t) OBJS_XRAM_ADDR;

    if (off % sizeof(struct objclass) != 0
        || off / sizeof(struct objclass) >= NUM_OBJECTS) {
        fprintf(stderr, "oc_put: %p is not an object table address\n",
                (void *) p);
        abort();
    }
    rp6502_ram_objects[off / sizeof(struct objclass)] = *rec;
}

#ifdef RP6502_LEV_XRAM
/* The level map: a host array stands in for its XRAM image. Every access is
 * checked, because a cell outside the map would overwrite another XRAM table
 * on the target. */
static struct rm rp6502_lev_image[COLNO][ROWNO];

static void
lev_check(int x, int y, const char *what)
{
    if (x < 0 || x >= COLNO || y < 0 || y >= ROWNO) {
        fprintf(stderr, "%s: cell (%d,%d) is outside the level map\n", what,
                x, y);
        abort();
    }
}

struct rm
rpl_get(int x, int y)
{
    lev_check(x, y, "lev_get");
    return rp6502_lev_image[x][y];
}

void
rpl_put(int x, int y, struct rm v)
{
    lev_check(x, y, "lev_put");
    rp6502_lev_image[x][y] = v;
}

void
rpl_clear(void)
{
    memset(rp6502_lev_image, 0, sizeof rp6502_lev_image);
}
#endif /* RP6502_LEV_XRAM */
