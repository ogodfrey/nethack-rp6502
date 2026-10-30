/* Prototype check for the XRAM tables. mons[] is copied to XRAM, read back
 * through pm_get(), and every field is folded into a checksum. The host build
 * runs the same code over the real array, so the two outputs must match.
 */
#include "config.h"
#include "pm.h"
#include "rp6502xram.h"
#include <stdint.h>
#include <stdio.h>

#ifndef RP6502_HOST
#include <rp6502.h>
/* monst.c is built with mons renamed to this RAM copy, the XRAM image source. */
extern struct permonst rp6502_ram_mons[];
#endif

static uint32_t
fold_pm(uint32_t sum, struct permonst *p)
{
    struct permonst *q = pm_get(p);
    const char *s;
    int j;

#define FOLD(x) sum = sum * 31u + (uint32_t) (x)
    FOLD(q->mlet);
    FOLD(q->mlevel);
    FOLD(q->mmove);
    FOLD(q->ac);
    FOLD(q->mr);
    FOLD(q->maligntyp);
    FOLD(q->geno);
    for (j = 0; j < NATTK; j++) {
        FOLD(q->mattk[j].aatyp);
        FOLD(q->mattk[j].adtyp);
        FOLD(q->mattk[j].damn);
        FOLD(q->mattk[j].damd);
    }
    FOLD(q->cwt);
    FOLD(q->cnutrit);
    FOLD(q->msound);
    FOLD(q->msize);
    FOLD(q->mresists);
    FOLD(q->mconveys);
    FOLD(q->mflags1);
    FOLD(q->mflags2);
    FOLD(q->mflags3);
    FOLD(q->difficulty);
#ifdef TEXTCOLOR
    FOLD(q->mcolor);
#endif
#undef FOLD
    for (s = q->mname; *s; s++)
        sum = sum * 31u + (uint32_t) (unsigned char) *s;
    return sum;
}

int
main(void)
{
    uint32_t sum = 0;
    unsigned i;

#ifndef RP6502_HOST
    xram0_write(MONS_XRAM_ADDR, rp6502_ram_mons,
                (unsigned) (NUMMONS * sizeof(struct permonst)));
#endif
    for (i = 0; i < NUMMONS; i++)
        sum = fold_pm(sum, mons_at(i));
    printf("mons %d sum %08lx\n", NUMMONS, (unsigned long) sum);
    printf("span %d\n", (int) (mons_at(NUMMONS - 1) - mons_at(0)));
    return 0;
}
