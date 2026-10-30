/* NetHack 3.6	rp6502tty.c */
/* Copyright (c) NetHack RP6502 port contributors, 2026. */
/* NetHack may be freely redistributed.  See license for details. */

/*
 * Keyboard and terminal control for the Picocomputer RP6502.
 *
 * Output goes to stdout, which is the RP6502 console (a VT102-style
 * terminal).  Input is read raw and non-blocking from the "TTY:" device.
 * ANSI cursor key sequences are translated into movement commands.
 * The RP6502_HOST test build gets the same behavior from a Linux
 * terminal in raw mode.
 */

#include "hack.h"
#include "wintty.h"

#ifdef RP6502_HOST
#include <termios.h>
#include <sys/select.h>
static struct termios rp_orig_tio;
static boolean rp_tio_saved = FALSE;
#endif

char erase_char, kill_char;

static int tty_fd = -1;

/* keys read but not yet returned (cursor sequence tails, kbhit peeks) */
static unsigned char keybuf[8];
static int keyhead = 0, keycount = 0;

static int NDECL(rawread);
static void FDECL(pushkey, (int));
static int NDECL(popkey);
static int FDECL(waitkey, (unsigned));
static int FDECL(cursorkey, (int));

/* read one byte if one is waiting; -1 if none */
static int
rawread()
{
    unsigned char c;

#ifdef RP6502_HOST
    fd_set fds;
    struct timeval tv;

    FD_ZERO(&fds);
    FD_SET(tty_fd, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    if (select(tty_fd + 1, &fds, (fd_set *) 0, (fd_set *) 0, &tv) <= 0)
        return -1;
#endif
    if (read(tty_fd, &c, 1) == 1)
        return c;
    return -1;
}

static void
pushkey(c)
int c;
{
    if (keycount < (int) sizeof keybuf) {
        keybuf[(keyhead + keycount) % sizeof keybuf] = (unsigned char) c;
        keycount++;
    }
}

static int
popkey()
{
    int c = keybuf[keyhead];

    keyhead = (keyhead + 1) % sizeof keybuf;
    keycount--;
    return c;
}

/* wait up to about ticks clock ticks for a byte; -1 on timeout */
static int
waitkey(ticks)
unsigned ticks;
{
    clock_t start = clock();
    int c;

    do {
        if ((c = rawread()) >= 0)
            return c;
    } while ((unsigned) (clock() - start) < ticks);
    return -1;
}

static int
cursorkey(c)
int c;
{
    static const char vikeys[] = "kjlh"; /* A B C D: up down right left */
    static const char numkeys[] = "8264";

    if (c < 'A' || c > 'D')
        return 0;
    return Cmd.num_pad ? numkeys[c - 'A'] : vikeys[c - 'A'];
}

int
tgetch()
{
    int c, c2, c3;

    (void) fflush(stdout);
    if (keycount)
        return popkey();
    while ((c = rawread()) < 0)
        continue;
    if (c != '\033')
        return c;

    /* ESC alone is a key; ESC [ X or ESC O X is a cursor key */
    if ((c2 = waitkey(CLOCKS_PER_SEC / 20 + 1)) < 0)
        return c;
    if (c2 != '[' && c2 != 'O') {
        pushkey(c2);
        return c;
    }
    if ((c3 = waitkey(CLOCKS_PER_SEC / 20 + 1)) < 0) {
        pushkey(c2);
        return c;
    }
    if (cursorkey(c3))
        return cursorkey(c3);
    /* unknown sequence: discard the rest of it */
    while (c3 >= 0 && !isalpha(c3) && c3 != '~')
        c3 = waitkey(CLOCKS_PER_SEC / 20 + 1);
    return tgetch();
}

int
kbhit()
{
    int c;

    if (keycount)
        return 1;
    if ((c = rawread()) < 0)
        return 0;
    pushkey(c);
    return 1;
}

void
gettty()
{
    erase_char = '\b';
    kill_char = 21; /* ^U */
    iflags.cbreak = TRUE;
#ifdef RP6502_HOST
    tty_fd = 0;
    if (!rp_tio_saved && tcgetattr(tty_fd, &rp_orig_tio) == 0) {
        struct termios tio = rp_orig_tio;

        rp_tio_saved = TRUE;
        tio.c_lflag &= ~(ICANON | ECHO | ISIG);
        tio.c_iflag &= ~(ICRNL | IXON);
        tio.c_cc[VMIN] = 1;
        tio.c_cc[VTIME] = 0;
        (void) tcsetattr(tty_fd, TCSANOW, &tio);
    }
#else
    if (tty_fd < 0)
        tty_fd = open("TTY:", O_RDONLY);
#endif
}

void
settty(s)
const char *s;
{
    end_screen();
    if (s)
        raw_print(s);
#ifdef RP6502_HOST
    if (rp_tio_saved) {
        (void) tcsetattr(0, TCSANOW, &rp_orig_tio);
        rp_tio_saved = FALSE;
    }
#endif
}

void
setftty()
{
    start_screen();
}

void
get_scr_size()
{
    CO = 80;
    LI = 24;
}
