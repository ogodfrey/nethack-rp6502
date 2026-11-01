/* NetHack RP6502 port: self-check of the XRAM tables that the game keeps in
 * XRAM (see rp6502lev.c). Writes a pattern to every cell of each plane, reads
 * it back and prints the number of mismatches; the expected output is
 * "planes: 0 bad". Built for the target only (CMakeLists.txt, planetest).
 */
#include "config.h"
#include "hack.h"
#include "vision.h"
#include <stdio.h>

static int checked, bad;

static int
pat(int k, int x, int y)
{
    return (k * 131 + x * 37 + y * 11) & 0xff;
}

static void
expect(int ok, const char *what, int x, int y)
{
    checked++;
    if (!ok) {
        if (bad < 5)
            printf("bad %s at (%d,%d)\n", what, x, y);
        bad++;
    }
}

int
main(void)
{
    int x, y;

    rpl_clear();
    rpl_lst_clear();
    rpl_splm_clear();
    rpg_clear();
    rpv_planes_clear();

    for (x = 0; x < COLNO; x++)
        for (y = 0; y < ROWNO; y++) {
            struct rm r;
            gbuf_entry e;

            r.glyph = pat(1, x, y) * 3;
            r.typ = (schar) (pat(2, x, y) & 0x3f);
            r.seenv = (uchar) pat(3, x, y);
            rpl_put(x, y, r);
            rpl_lst_put(x, y, (schar) pat(4, x, y));
            rpl_splm_put(x, y, (char) pat(5, x, y));
            e.new = (xchar) (pat(6, x, y) & 1);
            e.glyph = pat(7, x, y);
            rpg_put(y, x, e);
            rpv_vcl_put(y, x, (char) pat(8, x, y));
            rpv_lpt_put(y, x, (char) pat(9, x, y));
            rpv_rpt_put(y, x, (char) pat(10, x, y));
        }

    for (x = 0; x < COLNO; x++)
        for (y = 0; y < ROWNO; y++) {
            struct rm r = rpl_get(x, y);
            gbuf_entry e = rpg_get(y, x);

            expect(r.glyph == pat(1, x, y) * 3, "level glyph", x, y);
            expect(r.typ == (schar) (pat(2, x, y) & 0x3f), "level typ", x, y);
            expect(r.seenv == (uchar) pat(3, x, y), "level seenv", x, y);
            expect(rpl_lst_get(x, y) == (schar) pat(4, x, y), "lastseentyp",
                   x, y);
            expect(rpl_splm_get(x, y) == (char) pat(5, x, y), "SpLev_Map", x,
                   y);
            expect(e.new == (xchar) (pat(6, x, y) & 1), "gbuf new", x, y);
            expect(e.glyph == pat(7, x, y), "gbuf glyph", x, y);
            expect(rpv_vcl_get(y, x) == (char) pat(8, x, y), "viz_clear", x,
                   y);
            expect(rpv_lpt_get(y, x) == (char) pat(9, x, y), "left_ptrs", x,
                   y);
            expect(rpv_rpt_get(y, x) == (char) pat(10, x, y), "right_ptrs", x,
                   y);
        }

    printf("planes: %d bad of %d\n", bad, checked);
    return bad != 0;
}
