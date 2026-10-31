/* NetHack RP6502 port: the level map kept in XRAM (see rm.h, RP6502_LEV_XRAM).
 *
 * Cell (x, y) is an rm record at LEV_XRAM_ADDR + (x * ROWNO + y) * sizeof
 * (struct rm). Only the target build has this file; the host harness has its
 * own copy in rp6502xram_host.c.
 */
#include "config.h"
#include "hack.h"
#include "xram.h"
#include <rp6502.h>

_Static_assert(LEV_XRAM_ADDR + COLNO * ROWNO * sizeof(struct rm)
                   <= OVL_DESC_XRAM,
               "the level map overlaps the overlay descriptor in XRAM");

#define LEV_ADDR(x, y) \
    ((unsigned) (LEV_XRAM_ADDR + ((x) * ROWNO + (y)) * sizeof(struct rm)))

struct rm
rpl_get(int x, int y)
{
    struct rm r;

    xram0_read(&r, LEV_ADDR(x, y), sizeof r);
    return r;
}

void
rpl_put(int x, int y, struct rm v)
{
    xram0_write(LEV_ADDR(x, y), &v, sizeof v);
}

/* XRAM holds no defined data at power-up: zero the map before the first
 * level is made. Each column is one write, since its cells are contiguous. */
void
rpl_clear(void)
{
    static const unsigned char zero[ROWNO * sizeof(struct rm)];
    int x;

    for (x = 0; x < COLNO; x++)
        xram0_write(LEV_ADDR(x, 0), zero, sizeof zero);
}
