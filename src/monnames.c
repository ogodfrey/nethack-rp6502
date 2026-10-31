/* NetHack RP6502 port: mons_names[], the monster names indexed like mons[].
 * monst.c is compiled a second time with MONST_NAMES set, so the names come
 * from the same MON() entries as the table itself. See pm_name() in
 * include/rp6502xram.h.
 */
#define MONST_NAMES
#include "monst.c"
