/* NetHack 3.6	rp6502sys.c */
/* Copyright (c) NetHack RP6502 port contributors, 2026. */
/* NetHack may be freely redistributed.  See license for details. */

/*
 * Startup, file locations and miscellaneous system services for the
 * Picocomputer RP6502.  Modeled on sys/share/pcmain.c and pcsys.c.
 *
 * Read-only game data comes from ROM assets ("ROM:name").  Everything
 * the game writes (levels, saves, bones, record) goes in RP6502_DIR on
 * the USB drive.  The RP6502_HOST test build uses $NETHACKDIR (or the
 * current directory) for both.
 */

#define NEED_VARARGS /* uses ... */
#include "hack.h"
#include "dlb.h"

#ifndef RP6502_HOST
#define RP6502_DATA "ROM:"
#define RP6502_DIR "nethack"
#endif

static void FDECL(process_options, (int, char **));
static void FDECL(set_prefixes, (char *));
static boolean NDECL(rp_main);
static void NDECL(getlock);

int FDECL(main, (int, char **));

static int rp_argc;
static char **rp_argv;

int
main(argc, argv)
int argc;
char **argv;
{
    boolean resuming;

    rp_argc = argc;
    rp_argv = argv;
    sys_early_init();
    resuming = rp_main();
    moveloop(resuming);
    nethack_exit(EXIT_SUCCESS);
    /*NOTREACHED*/
    return 0;
}

static void
set_prefixes(dir)
char *dir;
{
    int i;
    char *wdir;

    wdir = (char *) alloc(strlen(dir) + 2);
    Strcpy(wdir, dir);
    append_slash(wdir);
    for (i = 0; i < PREFIX_COUNT; i++)
        fqn_prefix[i] = wdir;
#ifdef RP6502_DATA
    fqn_prefix[HACKPREFIX] = fqn_prefix[DATAPREFIX] = RP6502_DATA;
#endif
}

static boolean
rp_main()
{
    int fd;
    char failbuf[BUFSZ];
    boolean resuming = FALSE;
    char *dir = (char *) 0;

    hname = "NetHack";
    choose_windows(DEFAULT_WINDOW_SYS);

#ifdef RP6502_HOST
    dir = nh_getenv("NETHACKDIR");
    if (!dir)
        dir = ".";
#else
    (void) mkdir(RP6502_DIR, 0777);
    dir = RP6502_DIR;
#endif
    (void) strncpy(hackdir, dir, PATHLEN - 1);
    hackdir[PATHLEN - 1] = '\0';
    set_prefixes(hackdir);

    initoptions();
    if (!validate_prefix_locations(failbuf)) {
        raw_printf("Some invalid directory locations were specified:\n\t%s\n",
                   failbuf);
        nethack_exit(EXIT_FAILURE);
    }

    if (rp_argc > 1 && argcheck(rp_argc, rp_argv, ARG_VERSION) == 2)
        nethack_exit(EXIT_SUCCESS);

#ifdef DLB
    if (!dlb_init())
        error("dlb_init failure.");
#endif

    u.uhp = 1; /* prevent RIP on early quits */
    u.ux = 0;  /* prevent flush_screen() */

    init_nhwindows(&rp_argc, rp_argv);
    process_options(rp_argc, rp_argv);

    set_playmode(); /* sets plname to "wizard" for -D */
    iflags.renameallowed = FALSE;
    if (!*plname)
        askname();
    plnamesuffix();
    getlock();

    vision_init();
    display_gamewindows();

attempt_restore:
    if ((fd = restore_saved_game()) >= 0) {
        pline("Restoring save file...");
        mark_synch();
        if (dorecover(fd)) {
            resuming = TRUE;
            if (discover)
                You("are in non-scoring discovery mode.");
            if (discover || wizard) {
                if (yn("Do you want to keep the save file?") == 'n')
                    (void) delete_savefile();
            }
        }
    }

    if (!resuming) {
        if (!iflags.renameinprogress) {
            player_selection();
            if (iflags.renameinprogress)
                goto attempt_restore;
        }
        newgame();
        if (discover)
            You("are in non-scoring discovery mode.");
    }
    return resuming;
}

/*
 * The level 0 file is the lock.  If one is left over from a game that was
 * cut off (reset or power loss), offer to rebuild a save file from the
 * level files (SELF_RECOVER) or to discard them.
 */
static void
getlock()
{
    int fd, c, lev;

    Strcpy(lock, plname);
    Strcat(lock, ".99");
    regularize(lock);

    if ((fd = open_levelfile(0, (char *) 0)) >= 0) {
        nhclose(fd);
        raw_printf("There is already a game in progress under the name %s.",
                   plname);
        raw_print("Recover it (y), discard it (n), or quit (q)? ");
        do {
            c = lowc(tgetch());
        } while (!index("ynq\033", c));
        if (c == 'y') {
            if (!recover_savefile())
                error("Couldn't recover the old game.");
        } else if (c == 'n') {
            /* delete_levelfile() only removes files this run created */
            for (lev = 255; lev >= 0; lev--) {
                set_levelfile_name(lock, lev);
                (void) unlink(fqname(lock, LEVELPREFIX, 0));
            }
        } else {
            nethack_exit(EXIT_SUCCESS);
        }
    }

    fd = create_levelfile(0, (char *) 0);
    if (fd < 0) {
        error("Cannot create lock file %s.", lock);
    } else {
        hackpid = 1;
        (void) write(fd, (genericptr_t) &hackpid, sizeof hackpid);
        nhclose(fd);
    }
}

static void
process_options(argc, argv)
int argc;
char **argv;
{
    int i;

    while (argc > 1 && argv[1][0] == '-') {
        argv++;
        argc--;
        switch (argv[0][1]) {
        case 'D':
            wizard = TRUE, discover = FALSE;
            break;
        case 'X':
            discover = TRUE, wizard = FALSE;
            break;
        case 'u':
            if (argv[0][2]) {
                (void) strncpy(plname, argv[0] + 2, sizeof plname - 1);
            } else if (argc > 1) {
                argc--;
                argv++;
                (void) strncpy(plname, argv[0], sizeof plname - 1);
            }
            break;
        case 'p':
            if (argv[0][2]) {
                if ((i = str2role(&argv[0][2])) >= 0)
                    flags.initrole = i;
            } else if (argc > 1) {
                argc--;
                argv++;
                if ((i = str2role(argv[0])) >= 0)
                    flags.initrole = i;
            }
            break;
        case 'r':
            if (argv[0][2]) {
                if ((i = str2race(&argv[0][2])) >= 0)
                    flags.initrace = i;
            } else if (argc > 1) {
                argc--;
                argv++;
                if ((i = str2race(argv[0])) >= 0)
                    flags.initrace = i;
            }
            break;
        case '@':
            flags.randomall = 1;
            break;
        default:
            raw_printf("Unknown option: %s", *argv);
            break;
        }
    }
}

void
nethack_exit(code)
int code;
{
    settty((char *) 0);
    exit(code);
}

boolean
authorize_wizard_mode()
{
    return TRUE; /* single-user machine */
}

void
append_slash(name)
char *name;
{
    char *ptr;

    if (!*name)
        return;
    ptr = eos(name) - 1;
    if (*ptr != '/' && *ptr != ':')
        Strcat(name, "/");
}

/* FAT is case-insensitive and dislikes some characters */
void
regularize(s)
char *s;
{
    for (; *s; s++)
        if (!isalnum((uchar) *s) && *s != '.')
            *s = '_';
}

void
getreturn(str)
const char *str;
{
    int c;

    msmsg("Hit <Enter> %s.", str);
    do
        c = tgetch();
    while (c != '\n' && c != '\r' && c != EOF);
}

void msmsg
VA_DECL(const char *, fmt)
{
    VA_START(fmt);
    VA_INIT(fmt, const char *);
    Vprintf(fmt, VA_ARGS);
    (void) fflush(stdout);
    VA_END();
}

void error
VA_DECL(const char *, s)
{
    char buf[BUFSZ];

    VA_START(s);
    VA_INIT(s, const char *);
    if (iflags.window_inited)
        end_screen();
    Vsprintf(buf, s, VA_ARGS);
    Strcat(buf, "\n");
    raw_print(buf);
    VA_END();
    nethack_exit(EXIT_FAILURE);
}

unsigned long
sys_random_seed()
{
    unsigned long seed = (unsigned long) time((time_t *) 0);

    seed ^= (unsigned long) clock() << 8;
#ifdef RP6502_HOST
    seed ^= (unsigned long) getpid() << 16;
#endif
    return seed;
}
