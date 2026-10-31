/* NetHack RP6502 port: message text read from storage on demand.
 *
 * sys/rp6502/tools/strtab.py rewrites a string literal in a function body to
 * MSG(id, "text"). MSG returns the text of string number id, read from
 * msg.idx and msg.dat (sys/rp6502/msgfetch.c). The returned pointer is good
 * until MSG_SLOTS more strings have been fetched, so use it at once.
 *
 * With MSG_CHECK (host tests) the fetched text is compared with the literal
 * in the source, so any mismatch aborts the game.
 */
#ifndef RP6502_MSG_H
#define RP6502_MSG_H

const char *msg_get(unsigned id);
unsigned msg_count(void);

/* Tables of text ids: a msgid_t is a string number, MSGID(n) is a constant
 * for a static initializer, and MSGTXT(x) is the text of the id x (valid
 * until MSG_SLOTS more strings are fetched). MSGNULL ends a table. */
typedef unsigned short msgid_t;
#define MSGID(n) ((msgid_t) (n))
#define MSGNULL ((msgid_t) 0xFFFF)
#define MSGTXT(x) msg_get((unsigned) (x))

#ifdef MSG_CHECK
const char *msg_check(unsigned id, const char *lit);
#define MSG(id, lit) msg_check((id), (lit))
#else
#define MSG(id, lit) msg_get(id)
#endif

#endif /* RP6502_MSG_H */
