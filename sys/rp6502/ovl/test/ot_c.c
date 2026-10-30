#include "ot.h"

int
c_cmp(const void *x, const void *y)
{
    return *(const int *) x - *(const int *) y;
}

int
c_apply(intfn f, int n)
{
    return f(n) + f(n + 1);
}

int
c_chain(int depth)
{
    if (!depth)
        return 3;
    return a_chain(depth - 1) * 2;
}

long
c_mix(long x, int y, char z, long w)
{
    return x * y + z - w;
}
