/* Overlay manager test: functions spread over several overlays. */
#include <stdarg.h>
#include <stdio.h>

typedef int (*intfn)(int);

/* ot_a.c */
int a_fib(int n);
const char *a_name(int n);
int a_switch(int n);
int a_count(void);
int a_sum(int n, ...);
intfn a_ptr(void);
int a_chain(int depth);

/* ot_b.c */
int b_fib(int n);
void b_sort(int *v, int n);
int b_twice(int n);
const char *b_greeting(void);
int b_chain(int depth);

/* ot_c.c */
int c_cmp(const void *x, const void *y);
int c_apply(intfn f, int n);
int c_chain(int depth);
long c_mix(long x, int y, char z, long w);

/* ot_main.c (resident) */
int r_hot(int n);
extern int r_calls;
