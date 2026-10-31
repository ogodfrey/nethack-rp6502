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

#ifdef MSG_CHECK
const char *msg_check(unsigned id, const char *lit);
#define MSG(id, lit) msg_check((id), (lit))
#else
#define MSG(id, lit) msg_get(id)
#endif

#endif /* RP6502_MSG_H */
