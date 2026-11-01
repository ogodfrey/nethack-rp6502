/* NetHack RP6502 port: the level map kept in XRAM (see rm.h, RP6502_LEV_XRAM).
 *
 * Cell (x, y) is an rm record at LEV_XRAM_ADDR + (x * ROWNO + y) * sizeof
 * (struct rm). Only the target build has this file; the host harness has its
 * own copy in rp6502xram_host.c.
 */
#include "config.h"
#include "hack.h"
#include "vision.h"
#include "xram.h"
#include <rp6502.h>

/* Portal 0 without a call: set ADDR0, then each access to RW0 moves on by
 * STEP0 (set to 1 here). The same operations xram0_read and xram0_write do. */
static void
ria_read(void *dest, unsigned src, unsigned count)
{
    unsigned char *p = dest;

    RIA.addr0 = src;
    RIA.step0 = 1;
    while (count--)
        *p++ = RIA.rw0;
}

static void
ria_write(unsigned dst, const void *src, unsigned count)
{
    const unsigned char *p = src;

    RIA.addr0 = dst;
    RIA.step0 = 1;
    while (count--)
        RIA.rw0 = *p++;
}

_Static_assert(LEV_XRAM_ADDR + COLNO * ROWNO * sizeof(struct rm)
                   <= LST_XRAM_ADDR,
               "the level map overlaps lastseentyp in XRAM");
_Static_assert(LST_XRAM_ADDR + COLNO * ROWNO <= SPLM_XRAM_ADDR,
               "lastseentyp overlaps SpLev_Map in XRAM");
_Static_assert(SPLM_XRAM_ADDR + COLNO * ROWNO <= GB_XRAM_ADDR,
               "SpLev_Map overlaps the glyph buffer in XRAM");
_Static_assert(GB_XRAM_ADDR + ROWNO * COLNO * sizeof(gbuf_entry)
                   <= VCL_XRAM_ADDR,
               "the glyph buffer overlaps the vision clear map in XRAM");
_Static_assert(VCL_XRAM_ADDR + ROWNO * COLNO <= LPT_XRAM_ADDR,
               "the vision clear map overlaps the left pointers in XRAM");
_Static_assert(LPT_XRAM_ADDR + ROWNO * COLNO <= RPT_XRAM_ADDR,
               "the left pointers overlap the right pointers in XRAM");
_Static_assert(RPT_XRAM_ADDR + ROWNO * COLNO <= OVL_DESC_XRAM,
               "the right pointers overlap the overlay descriptor in XRAM");
_Static_assert(GB_XRAM_ADDR + ROWNO * COLNO * sizeof(gbuf_entry)
                   <= OVL_DESC_XRAM,
               "the glyph buffer overlaps the overlay descriptor in XRAM");

#define LEV_ADDR(x, y) \
    ((unsigned) (LEV_XRAM_ADDR + ((x) * ROWNO + (y)) * sizeof(struct rm)))

struct rm
rpl_get(int x, int y)
{
    struct rm r;

    ria_read(&r, LEV_ADDR(x, y), sizeof r);
    return r;
}

void
rpl_put(int x, int y, struct rm v)
{
    ria_write(LEV_ADDR(x, y), &v, sizeof v);
}

/* XRAM holds no defined data at power-up: zero the map before the first
 * level is made. Each column is one write, since its cells are contiguous. */
void
rpl_clear(void)
{
    static const unsigned char zero[ROWNO * sizeof(struct rm)];
    int x;

    for (x = 0; x < COLNO; x++)
        ria_write(LEV_ADDR(x, 0), zero, sizeof zero);
}

#define BYTE_ADDR(base, x, y) ((unsigned) ((base) + (x) * ROWNO + (y)))

schar
rpl_lst_get(int x, int y)
{
    schar v;

    ria_read(&v, BYTE_ADDR(LST_XRAM_ADDR, x, y), 1);
    return v;
}

void
rpl_lst_put(int x, int y, schar v)
{
    ria_write(BYTE_ADDR(LST_XRAM_ADDR, x, y), &v, 1);
}

void
rpl_lst_clear(void)
{
    static const unsigned char zero[ROWNO];
    int x;

    for (x = 0; x < COLNO; x++)
        ria_write(BYTE_ADDR(LST_XRAM_ADDR, x, 0), zero, sizeof zero);
}

char
rpl_splm_get(int x, int y)
{
    char v;

    ria_read(&v, BYTE_ADDR(SPLM_XRAM_ADDR, x, y), 1);
    return v;
}

void
rpl_splm_put(int x, int y, char v)
{
    ria_write(BYTE_ADDR(SPLM_XRAM_ADDR, x, y), &v, 1);
}

void
rpl_splm_clear(void)
{
    static const unsigned char zero[ROWNO];
    int x;

    for (x = 0; x < COLNO; x++)
        ria_write(BYTE_ADDR(SPLM_XRAM_ADDR, x, 0), zero, sizeof zero);
}

/* The glyph buffer is row-major: entry (row, col) is at row * COLNO + col. */
#define GBUF_ADDR(row, col) \
    ((unsigned) (GB_XRAM_ADDR + ((row) * COLNO + (col)) * sizeof(gbuf_entry)))

gbuf_entry
rpg_get(int row, int col)
{
    gbuf_entry e;

    ria_read(&e, GBUF_ADDR(row, col), sizeof e);
    return e;
}

void
rpg_put(int row, int col, gbuf_entry e)
{
    ria_write(GBUF_ADDR(row, col), &e, sizeof e);
}

void
rpg_clear(void)
{
    static const unsigned char zero[COLNO * sizeof(gbuf_entry)];
    int y;

    for (y = 0; y < ROWNO; y++)
        ria_write(GBUF_ADDR(y, 0), zero, sizeof zero);
}

/* The vision planes are row-major by row: entry (r, c) is at r * COLNO + c.
 * vision.c reads and writes them through the vcl_, lpt_ and rpt_ names. */
#define VIS_ADDR(base, r, c) ((unsigned) ((base) + (r) * COLNO + (c)))

char
rpv_vcl_get(int r, int c)
{
    char v;

    ria_read(&v, VIS_ADDR(VCL_XRAM_ADDR, r, c), 1);
    return v;
}

void
rpv_vcl_put(int r, int c, char v)
{
    ria_write(VIS_ADDR(VCL_XRAM_ADDR, r, c), &v, 1);
}

char
rpv_lpt_get(int r, int c)
{
    char v;

    ria_read(&v, VIS_ADDR(LPT_XRAM_ADDR, r, c), 1);
    return v;
}

void
rpv_lpt_put(int r, int c, char v)
{
    ria_write(VIS_ADDR(LPT_XRAM_ADDR, r, c), &v, 1);
}

char
rpv_rpt_get(int r, int c)
{
    char v;

    ria_read(&v, VIS_ADDR(RPT_XRAM_ADDR, r, c), 1);
    return v;
}

void
rpv_rpt_put(int r, int c, char v)
{
    ria_write(VIS_ADDR(RPT_XRAM_ADDR, r, c), &v, 1);
}

void
rpv_planes_clear(void)
{
    static const unsigned char zero[COLNO];
    int r;

    for (r = 0; r < ROWNO; r++) {
        ria_write(VIS_ADDR(VCL_XRAM_ADDR, r, 0), zero, sizeof zero);
        ria_write(VIS_ADDR(LPT_XRAM_ADDR, r, 0), zero, sizeof zero);
        ria_write(VIS_ADDR(RPT_XRAM_ADDR, r, 0), zero, sizeof zero);
    }
}
