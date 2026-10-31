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
                   <= LST_XRAM_ADDR,
               "the level map overlaps lastseentyp in XRAM");
_Static_assert(LST_XRAM_ADDR + COLNO * ROWNO <= SPLM_XRAM_ADDR,
               "lastseentyp overlaps SpLev_Map in XRAM");
_Static_assert(SPLM_XRAM_ADDR + COLNO * ROWNO <= OVL_DESC_XRAM,
               "SpLev_Map overlaps the overlay descriptor in XRAM");

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

#define BYTE_ADDR(base, x, y) ((unsigned) ((base) + (x) * ROWNO + (y)))

schar
rpl_lst_get(int x, int y)
{
    schar v;

    xram0_read(&v, BYTE_ADDR(LST_XRAM_ADDR, x, y), 1);
    return v;
}

void
rpl_lst_put(int x, int y, schar v)
{
    xram0_write(BYTE_ADDR(LST_XRAM_ADDR, x, y), &v, 1);
}

void
rpl_lst_clear(void)
{
    static const unsigned char zero[ROWNO];
    int x;

    for (x = 0; x < COLNO; x++)
        xram0_write(BYTE_ADDR(LST_XRAM_ADDR, x, 0), zero, sizeof zero);
}

char
rpl_splm_get(int x, int y)
{
    char v;

    xram0_read(&v, BYTE_ADDR(SPLM_XRAM_ADDR, x, y), 1);
    return v;
}

void
rpl_splm_put(int x, int y, char v)
{
    xram0_write(BYTE_ADDR(SPLM_XRAM_ADDR, x, y), &v, 1);
}

void
rpl_splm_clear(void)
{
    static const unsigned char zero[ROWNO];
    int x;

    for (x = 0; x < COLNO; x++)
        xram0_write(BYTE_ADDR(SPLM_XRAM_ADDR, x, 0), zero, sizeof zero);
}
