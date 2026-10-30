/* NetHack for the RP6502 - milestone 1 console test.
 *
 * Draws a NetHack-style screen with DEC Special Graphics and ANSI color,
 * and moves an @ with raw keys read from TTY:. This exercises everything
 * the tty window port needs from the RP6502 console.
 */

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define COLNO 80
#define ROWNO 21
#define MAP_TOP 1 /* screen row of map row 0 (row 0 is the message line) */

#define ESC "\033"
#define SO '\016' /* select G1, loaded with DEC Special Graphics */
#define SI '\017' /* select G0, US ASCII */

enum cell {
    STONE = 0, HWALL, VWALL, TLCORN, TRCORN, BLCORN, BRCORN,
    ROOM, CORR, DOOR, FOUNTAIN, STAIRS
};

/* Glyph for each cell: DEC flag, character, SGR color (0 = default). */
static const struct {
    unsigned char dec;
    char ch;
    unsigned char color;
} cellsym[] = {
    { 0, ' ', 0 },  /* STONE */
    { 1, 'q', 0 },  /* HWALL */
    { 1, 'x', 0 },  /* VWALL */
    { 1, 'l', 0 },  /* TLCORN */
    { 1, 'k', 0 },  /* TRCORN */
    { 1, 'm', 0 },  /* BLCORN */
    { 1, 'j', 0 },  /* BRCORN */
    { 1, '~', 0 },  /* ROOM: centered dot */
    { 1, 'a', 0 },  /* CORR: checkerboard */
    { 0, '+', 33 }, /* DOOR: brown */
    { 0, '{', 34 }, /* FOUNTAIN: blue */
    { 0, '>', 0 },  /* STAIRS */
};

static const struct {
    unsigned char x, y;
    char ch;
    unsigned char color;
} things[] = {
    { 14, 6, 'd', 33 },  /* jackal */
    { 30, 9, 'F', 92 },  /* lichen */
    { 60, 14, 'r', 33 }, /* sewer rat */
    { 18, 10, '$', 93 }, /* gold */
    { 34, 5, ')', 36 },  /* weapon */
    { 64, 12, '!', 95 }, /* potion */
};
#define NTHINGS (sizeof things / sizeof things[0])

static unsigned char map[ROWNO][COLNO];
static unsigned char ux = 12, uy = 7;
static unsigned turns = 1;

/* Output is collected in a buffer and written to the console in chunks. */
static char obuf[256];
static unsigned char olen;
static unsigned char dec_on;

static void flush_out(void)
{
    if (olen) {
        write(STDOUT_FILENO, obuf, olen);
        olen = 0;
    }
}

static void outc(char c)
{
    if (olen == sizeof obuf - 1)
        flush_out();
    obuf[olen++] = c;
}

static void outs(const char *s)
{
    while (*s)
        outc(*s++);
}

static void outn(unsigned n)
{
    char tmp[6];
    unsigned char i = 0;

    do {
        tmp[i++] = (char) ('0' + n % 10);
        n /= 10;
    } while (n);
    while (i)
        outc(tmp[--i]);
}

static void set_dec(unsigned char on)
{
    if (on != dec_on) {
        outc(on ? SO : SI);
        dec_on = on;
    }
}

static void set_color(unsigned char color)
{
    outs(ESC "[");
    if (color)
        outn(color);
    outc('m');
}

/* Move the cursor to a 0-based screen position. */
static void gotoxy(unsigned char x, unsigned char y)
{
    outs(ESC "[");
    outn(y + 1);
    outc(';');
    outn(x + 1);
    outc('H');
}

static void room(unsigned char lx, unsigned char ly,
                 unsigned char hx, unsigned char hy)
{
    unsigned char x, y;

    for (y = ly; y <= hy; ++y)
        for (x = lx; x <= hx; ++x)
            map[y][x] = (y == ly || y == hy) ? HWALL
                        : (x == lx || x == hx) ? VWALL : ROOM;
    map[ly][lx] = TLCORN;
    map[ly][hx] = TRCORN;
    map[hy][lx] = BLCORN;
    map[hy][hx] = BRCORN;
}

static void build_map(void)
{
    unsigned char x;

    memset(map, STONE, sizeof map);
    room(8, 3, 40, 12);
    room(56, 9, 72, 17);
    map[7][40] = DOOR;
    map[13][56] = DOOR;
    for (x = 41; x <= 48; ++x)
        map[7][x] = CORR;
    for (x = 48; x <= 55; ++x)
        map[13][x] = CORR;
    for (x = 8; x <= 13; ++x)
        map[x][48] = CORR;
    map[5][24] = FOUNTAIN;
    map[15][68] = STAIRS;
}

static void draw_cell(unsigned char x, unsigned char y)
{
    unsigned char i, c = map[y][x];

    if (x == ux && y == uy) {
        set_dec(0);
        set_color(97);
        outc('@');
        return;
    }
    for (i = 0; i < NTHINGS; ++i) {
        if (things[i].x == x && things[i].y == y) {
            set_dec(0);
            set_color(things[i].color);
            outc(things[i].ch);
            return;
        }
    }
    set_dec(cellsym[c].dec);
    set_color(cellsym[c].color);
    outc(cellsym[c].ch);
}

static void draw_map(void)
{
    unsigned char x, y, lo, hi;

    for (y = 0; y < ROWNO; ++y) {
        for (lo = 0; lo < COLNO && map[y][lo] == STONE; ++lo)
            ;
        if (lo == COLNO)
            continue;
        for (hi = COLNO - 1; map[y][hi] == STONE; --hi)
            ;
        gotoxy(lo, MAP_TOP + y);
        for (x = lo; x <= hi; ++x)
            draw_cell(x, y);
    }
}

static void message(const char *s, char key)
{
    set_dec(0);
    set_color(0);
    gotoxy(0, 0);
    outs(ESC "[K");
    outs(s);
    if (key) {
        outs(" key ");
        outn((unsigned char) key);
        if (key > ' ' && key < 0x7f) {
            outs(" '");
            outc(key);
            outc('\'');
        }
    }
}

static void status(void)
{
    set_dec(0);
    set_color(0);
    gotoxy(0, MAP_TOP + ROWNO);
    outs("Agent the Stripling      St:16 Dx:14 Co:15 In:9 Wi:11 Ch:8 Neutral");
    gotoxy(0, MAP_TOP + ROWNO + 1);
    outs("Dlvl:1 $:0 HP:14(14) Pw:2(2) AC:7 Xp:1/0 T:");
    outn(turns);
    outs(ESC "[K");
}

static unsigned char walkable(unsigned char x, unsigned char y)
{
    unsigned char c;

    if (x >= COLNO || y >= ROWNO)
        return 0;
    c = map[y][x];
    return c == ROOM || c == CORR || c == DOOR || c == FOUNTAIN
           || c == STAIRS;
}

static int tty_fd;

static char getkey(void)
{
    char c;

    while (read(tty_fd, &c, 1) < 1)
        ;
    return c;
}

int main(void)
{
    static const char dirkeys[] = "hjklyubn";
    static const signed char ddx[] = { -1, 0, 0, 1, -1, 1, -1, 1 };
    static const signed char ddy[] = { 0, 1, -1, 0, -1, -1, 1, 1 };
    const char *p;
    char key;
    unsigned char d, ox, oy;

    tty_fd = open("TTY:", O_RDONLY);
    build_map();

    /* Load DEC Special Graphics into G1, clear, hide the cursor. */
    outs(ESC ")0" ESC "[0m" ESC "[2J" ESC "[?25l");
    dec_on = 1;
    set_dec(0);
    draw_map();
    status();
    message("Welcome to NetHack-RP6502! hjklyubn or arrows move, q quits.", 0);
    flush_out();

    for (;;) {
        key = getkey();
        if (key == '\033') {
            /* Arrow keys arrive as ESC [ A..D. */
            if (getkey() != '[')
                continue;
            switch (getkey()) {
            case 'A': key = 'k'; break;
            case 'B': key = 'j'; break;
            case 'C': key = 'l'; break;
            case 'D': key = 'h'; break;
            default: continue;
            }
        }
        if (key == 'q')
            break;
        p = strchr(dirkeys, key);
        if (p && key) {
            d = (unsigned char) (p - dirkeys);
            ox = ux;
            oy = uy;
            if (walkable(ux + ddx[d], uy + ddy[d])) {
                ux += ddx[d];
                uy += ddy[d];
                ++turns;
                gotoxy(ox, MAP_TOP + oy);
                draw_cell(ox, oy);
                gotoxy(ux, MAP_TOP + uy);
                draw_cell(ux, uy);
                message("You move.", key);
            } else {
                message("You cannot pass that way.", key);
            }
        } else {
            message("Unknown command.", key);
        }
        status();
        flush_out();
    }

    set_dec(0);
    outs(ESC "[0m" ESC "[2J" ESC "[H" ESC "[?25h" "Goodbye.\r\n");
    flush_out();
    return 0;
}
