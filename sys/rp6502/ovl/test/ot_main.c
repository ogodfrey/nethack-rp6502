/* Overlay manager test driver (resident).  Prints results that are
 * compared with a native build of the same sources. */
#include "ot.h"

#ifdef __RP6502__
void __ovl_init(void);
extern unsigned long __ovl_loads, __ovl_forced;
#endif

int r_calls;

int
r_hot(int n)
{
    r_calls++;
    return n + 1000;
}

static int
r_neg(int n)
{
    return -n;
}

int
main(void)
{
    int v[8] = { 5, -3, 9, 0, 12, 7, -8, 1 };
    int i;
    intfn f;

#ifdef __RP6502__
    __ovl_init();
#endif
    printf("fib %d %d\n", a_fib(15), b_fib(12));
    for (i = 0; i < 8; i++)
        printf("switch %d %d\n", i, a_switch(i));
    printf("name %s %s\n", a_name(1), a_name(6));
    printf("greet %s\n", b_greeting());
    printf("sum %d\n", a_sum(5, 1, 2, 3, 4, 5));
    f = a_ptr();
    printf("ptr %d %d %d\n", f(21), f == b_twice, c_apply(f, 4));
    printf("apply %d %d\n", c_apply(r_neg, 3), c_apply(r_hot, 1));
    b_sort(v, 8);
    for (i = 0; i < 8; i++)
        printf("%d ", v[i]);
    printf("\n");
    printf("chain %d %d\n", a_chain(4), a_chain(9));
    printf("mix %ld\n", c_mix(100000L, 3, 'x', 77L));
    printf("count %d calls %d\n", a_count(), r_calls);
#ifdef __RP6502__
    printf("loads %lu forced %lu\n", __ovl_loads, __ovl_forced);
#endif
    return 0;
}
