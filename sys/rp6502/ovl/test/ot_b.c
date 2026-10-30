#include <stdlib.h>
#include <string.h>

#include "ot.h"

int
b_fib(int n)
{
    return n < 2 ? n : a_fib(n - 1) + b_fib(n - 2);
}

void
b_sort(int *v, int n)
{
    qsort(v, n, sizeof *v, c_cmp);
}

int
b_twice(int n)
{
    return 2 * n;
}

const char *
b_greeting(void)
{
    return "hello from overlay b";
}

int
b_chain(int depth)
{
    const char *s = "b-string";

    if (!depth)
        return 2;
    return c_chain(depth - 1) + (int) strlen(s);
}
