/* NetHack RP6502 port: storage test for the message tables.
 *
 * Reads every string through msg_get() and prints the count and a checksum
 * of all the bytes. The same file built for the host (with msgfetch.c) must
 * print the same two lines, which shows that the target's file calls read the
 * tables correctly.
 */
#include "msg.h"
#include <stdio.h>

int
main(void)
{
    unsigned id, count = msg_count();
    unsigned long sum = 0;
    const char *s;

    for (id = 0; id < count; id++) {
        for (s = msg_get(id); *s; s++)
            sum = (sum * 31u + (unsigned char) *s) & 0xFFFFFFFFul;
        sum = (sum * 31u + 0) & 0xFFFFFFFFul;
    }
    printf("msgs %u sum %08lx\n", count, sum);
    printf("first %s\n", msg_get(0));
    printf("last %s\n", msg_get(count - 1));
    return 0;
}
