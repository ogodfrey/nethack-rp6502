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
_Static_assert(SPLM_XRAM_ADDR + COLNO * ROWNO <= GB_XRAM_ADDR,
               "SpLev_Map overlaps the glyph buffer in XRAM");
_Static_assert(GB_XRAM_ADDR + ROWNO * COLNO * sizeof(gbuf_entry)
                   <= OVL_DESC_XRAM,
               "the glyph buffer overlaps the overlay descriptor in XRAM");

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

/* The glyph buffer is row-major: entry (row, col) is at row * COLNO + col. */
#define GBUF_ADDR(row, col) \
    ((unsigned) (GB_XRAM_ADDR + ((row) * COLNO + (col)) * sizeof(gbuf_entry)))

gbuf_entry
rpg_get(int row, int col)
{
    gbuf_entry e;

    xram0_read(&e, GBUF_ADDR(row, col), sizeof e);
    return e;
}

void
rpg_put(int row, int col, gbuf_entry e)
{
    xram0_write(GBUF_ADDR(row, col), &e, sizeof e);
}

void
rpg_clear(void)
{
    static const unsigned char zero[COLNO * sizeof(gbuf_entry)];
    int y;

    for (y = 0; y < ROWNO; y++)
        xram0_write(GBUF_ADDR(y, 0), zero, sizeof zero);
}
