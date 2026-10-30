#include "ot.h"

static int count;

static int
a_helper(int n)
{
    return n * 3 + count;
}

int
a_fib(int n)
{
    count++;
    return n < 2 ? n : b_fib(n - 1) + a_fib(n - 2);
}

const char *
a_name(int n)
{
    static const char *const names[] = { "zero", "one", "two", "three" };

    return names[n & 3];
}

int
a_switch(int n)
{
    switch (n) {
    case 0: return a_helper(7);
    case 1: return 11;
    case 2: return r_hot(5);
    case 3: return b_twice(n);
    case 4: return 104;
    case 5: return a_helper(n) - 1;
    case 6: return 99;
    default: return -1;
    }
}

int
a_count(void)
{
    return count;
}

int
a_sum(int n, ...)
{
    va_list ap;
    int s = 0;

    va_start(ap, n);
    while (n--)
        s += va_arg(ap, int);
    va_end(ap);
    return s;
}

intfn
a_ptr(void)
{
    return b_twice;
}

int
a_chain(int depth)
{
    char buf[40];

    if (!depth)
        return 1;
    sprintf(buf, "a%d", depth);
    return b_chain(depth - 1) + buf[1] - '0';
}
