/* NetHack 3.6	rp6502conf.h */
/* Copyright (c) NetHack RP6502 port contributors, 2026. */
/* NetHack may be freely redistributed.  See license for details. */

/*
 * Picocomputer RP6502 (65C02, cc65).
 *
 * Built on the MICRO (MS-DOS style) code paths: no fork/exec, no
 * termcap library; the tty window port drives the RP6502 console with
 * built-in ANSI sequences (ANSI_DEFAULT) and DEC Special Graphics.
 *
 * RP6502_HOST is defined for the native test build of the same
 * configuration, which runs in a Linux terminal.
 */

#ifndef RP6502CONF_H
#define RP6502CONF_H

#define MICRO
#define PORT_ID "RP6502"

#define ANSI_DEFAULT
#define TEXTCOLOR
#define NOCWD_ASSUMPTIONS
#ifndef RP6502_HOST
#define RANDOM /* sys/share/random.c; ISAAC64 needs 64-bit integers */
#endif
#define NO_SIGNAL
#define SELF_RECOVER /* recover a crashed game from its level files */

#undef USE_ISAAC64
#undef SYSCF
#undef SYSCF_FILE
#undef CHDIR
#undef SHELL
#undef SUSPEND
#undef MAIL
#undef COMPRESS
#undef ZLIB_COMP
#undef DLB
#undef DUMPLOG

#define CONFIG_FILE "nethack.cnf"
#define PATHLEN 64
#define FILENAME 64

#ifndef MICRO_H
#include "micro.h"
#endif

/* system.h holds K&R declarations of libc; use the real headers instead */
#define SYSTEM_H
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef RP6502_HOST
#include <sys/types.h>
#include <sys/stat.h>
#endif
#ifndef S_IREAD
#define S_IREAD 0400
#define S_IWRITE 0200
#endif

#define index strchr
#define rindex strrchr
#define Rand() random()

/* sys/rp6502 */
extern void FDECL(nethack_exit, (int));
extern int NDECL(kbhit);

#define FCMASK 0660

#endif /* RP6502CONF_H */
