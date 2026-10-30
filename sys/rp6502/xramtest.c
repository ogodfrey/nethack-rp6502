/* Prototype check for the XRAM tables. mons[] is copied to XRAM, read back
 * through pm_get(), and every field is folded into a checksum. The host build
 * runs the same code over the real array, so the two outputs must match.
 */
#include "config.h"
#include "pm.h"
#include "monsym.h"
#include "permonst.h"
#include "mondata.h"
#include <stdint.h>
#include <stdio.h>

#ifndef RP6502_HOST
/* monst.c is built with mons renamed to this RAM copy, the XRAM image source.
   This volatile pointer keeps the copy in the link, so elfsym.py can write
   the XRAM image from it. The program itself reads only XRAM. */
extern struct permonst rp6502_ram_mons[];
const struct permonst *volatile rp6502_keep_mons = rp6502_ram_mons;
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

static uint32_t
fold_flags(uint32_t sum, struct permonst *p)
{
    uint32_t f = 0;
    unsigned b = 0;

#define BIT(x) (f |= (uint32_t) ((x) != 0) << (b++))
    BIT(is_flyer(p));
    BIT(is_undead(p));
    BIT(is_were(p));
    BIT(touch_petrifies(p));
    BIT(is_rider(p));
    BIT(nohands(p));
    BIT(emits_light(p));
    BIT(is_mind_flayer(p));
    BIT(is_bat(p));
    BIT(is_longworm(p));
    BIT(is_placeholder(p));
    BIT(is_mplayer(p));
    BIT(is_watch(p));
    BIT(webmaker(p));
    BIT(flaming(p));
    BIT(is_wooden(p));
    BIT(vegan(p));
    BIT(vegetarian(p));
    BIT(nonliving(p));
    BIT(unique_corpstat(p));
    BIT(is_domestic(p));
    BIT(is_demon(p));
    BIT(is_lord(p));
    BIT(is_unicorn(p));
    BIT(likes_fire(p));
    BIT(pm_invisible(p));
    BIT(is_vampire(p));
    BIT(is_bird(p));
#undef BIT
    sum = sum * 31u + f;
    return sum * 31u + (uint32_t) eyecount(p);
}

int
main(void)
{
    uint32_t sum = 0;
    unsigned i;

#ifndef RP6502_HOST
    if (rp6502_keep_mons == 0)
        return 1;
#endif
    for (i = 0; i < NUMMONS; i++)
        sum = fold_pm(sum, mons_at(i));
    printf("mons %d sum %08lx\n", NUMMONS, (unsigned long) sum);
    printf("span %d\n", (int) (mons_at(NUMMONS - 1) - mons_at(0)));
    sum = 0;
    for (i = 0; i < NUMMONS; i++)
        sum = fold_flags(sum, mons_at(i));
    printf("flags %08lx\n", (unsigned long) sum);
    return 0;
}
