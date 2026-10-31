/* NetHack RP6502 port: reads message text from storage (see include/msg.h).
 *
 * The tables are msg.idx (little-endian uint32 offsets into msg.dat, one more
 * entry than there are strings) and msg.dat (the strings, each with its NUL).
 * The files are opened from the play directory (NETHACKDIR) on the host, and
 * from MSG_DIR on the target, which is ROM: for the tables packaged as ROM
 * assets. Only POSIX calls are used, so the same code runs on both.
 */
#include "config.h"
#include "msg.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef MSG_DIR
#define MSG_DIR "" /* the target sets this to "ROM:" */
#endif
#define MSG_SLOTS 8
#define MSG_MAX 128 /* longest string in the table is 119 characters */

static int idx_fd = -1;
static int dat_fd = -1;
static char slot[MSG_SLOTS][MSG_MAX];
static unsigned char next_slot;
static unsigned msg_total;

static void
msg_fatal(const char *what, unsigned id)
{
    fprintf(stderr, "msg: %s (id %u)\n", what, id);
    abort();
}

/* The path of a table file: the play directory on the host, MSG_DIR on the
 * target (which has no environment). */
static void
msg_path(char *path, const char *name)
{
#ifdef RP6502_HOST
    const char *dir = getenv("NETHACKDIR");

    strcpy(path, (dir && *dir) ? dir : ".");
    strcat(path, "/");
#else
    strcpy(path, MSG_DIR);
#endif
    strcat(path, name);
}

static void
msg_open(void)
{
    char path[128];
    long idx_size;

    msg_path(path, "msg.idx");
    idx_fd = open(path, O_RDONLY);
    msg_path(path, "msg.dat");
    dat_fd = open(path, O_RDONLY);
    if (idx_fd < 0 || dat_fd < 0)
        msg_fatal("cannot open the message tables", 0);
    idx_size = (long) lseek(idx_fd, 0, SEEK_END);
    lseek(idx_fd, 0, SEEK_SET);
    if (idx_size < 8)
        msg_fatal("message index is empty", 0);
    msg_total = (unsigned) (idx_size / 4 - 1);
}

/* The number of strings in the tables. */
unsigned
msg_count(void)
{
    if (idx_fd < 0)
        msg_open();
    return msg_total;
}

/* Byte offsets of string id and of the string after it, in msg.dat. */
static void
msg_span(unsigned id, unsigned long *start, unsigned long *end)
{
    unsigned char b[8];

    if (lseek(idx_fd, (off_t) id * 4, SEEK_SET) < 0 || read(idx_fd, b, 8) != 8)
        msg_fatal("bad message index", id);
    *start = b[0] | ((unsigned long) b[1] << 8) | ((unsigned long) b[2] << 16)
             | ((unsigned long) b[3] << 24);
    *end = b[4] | ((unsigned long) b[5] << 8) | ((unsigned long) b[6] << 16)
           | ((unsigned long) b[7] << 24);
}

const char *
msg_get(unsigned id)
{
    unsigned long start, end, count;
    char *s;

    if (idx_fd < 0)
        msg_open();
    msg_span(id, &start, &end);
    if (end <= start || end - start > MSG_MAX)
        msg_fatal("bad message length", id);
    count = end - start;
    s = slot[next_slot];
    next_slot = (next_slot + 1) % MSG_SLOTS;
    if (lseek(dat_fd, (off_t) start, SEEK_SET) < 0
        || read(dat_fd, s, (unsigned) count) != (int) count
        || s[count - 1] != '\0')
        msg_fatal("bad message text", id);
    return s;
}

#ifdef MSG_CHECK
/* Host tests: the fetched text must equal the literal the source had. */
const char *
msg_check(unsigned id, const char *lit)
{
    const char *s = msg_get(id);

    if (strcmp(s, lit) != 0) {
        fprintf(stderr, "msg: id %u is \"%s\" but the source says \"%s\"\n",
                id, s, lit);
        abort();
    }
    return s;
}
#endif
