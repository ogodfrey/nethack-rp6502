/* Call-graph edge profiler for the RP6502 host build (hostbuild.sh with
 * PROFILE=1).  Every module is compiled with -finstrument-functions; this
 * file (compiled without it) counts caller -> callee edges and writes them
 * at exit to $CGPROF (default cgprof.out) as lines of
 *
 *     call_site_address callee_address count
 *
 * relative to the executable's load base, so they can be mapped to
 * functions with the symbol table (see cgreport.py).
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <link.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define NBUCKETS (1 << 16)

struct edge {
    uintptr_t site, fn;
    unsigned long count;
    struct edge *next;
};

static struct edge *buckets[NBUCKETS];
static uintptr_t base;
static int ready, busy;

#define NOINST __attribute__((no_instrument_function))

static NOINST void
dump(void)
{
    const char *name = getenv("CGPROF");
    FILE *f;
    int i;
    struct edge *e;

    busy = 1;
    f = fopen(name ? name : "cgprof.out", "a");
    if (!f)
        return;
    for (i = 0; i < NBUCKETS; i++)
        for (e = buckets[i]; e; e = e->next)
            fprintf(f, "%lx %lx %lu\n", (unsigned long) (e->site - base),
                    (unsigned long) (e->fn - base), e->count);
    fclose(f);
}

static NOINST void
setup(void)
{
    Dl_info info;
    void *extra;

    if (dladdr1((void *) setup, &info, &extra, RTLD_DL_LINKMAP) && extra)
        base = ((struct link_map *) extra)->l_addr;
    atexit(dump);
    ready = 1;
}

NOINST void
__cyg_profile_func_enter(void *fn, void *site)
{
    uintptr_t h;
    struct edge *e;

    if (busy)
        return;
    if (!ready) {
        busy = 1;
        setup();
        busy = 0;
    }
    h = (((uintptr_t) fn >> 2) ^ ((uintptr_t) site * 2654435761u))
        & (NBUCKETS - 1);
    for (e = buckets[h]; e; e = e->next)
        if (e->fn == (uintptr_t) fn && e->site == (uintptr_t) site) {
            e->count++;
            return;
        }
    busy = 1;
    e = malloc(sizeof *e);
    busy = 0;
    if (!e)
        return;
    e->site = (uintptr_t) site;
    e->fn = (uintptr_t) fn;
    e->count = 1;
    e->next = buckets[h];
    buckets[h] = e;
}

NOINST void
__cyg_profile_func_exit(void *fn, void *site)
{
    (void) fn;
    (void) site;
}
