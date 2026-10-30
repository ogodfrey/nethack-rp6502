/* RP6502 NetHack overlay loader: the slow path of the overlay manager
 * (ovlrt.s).
 *
 * Overlay images are in the ROM asset "nethack.ovl"; the directory of
 * overlays is preloaded into XRAM at __ovl_dir_xram, 8 bytes per overlay:
 *
 *     uint32 file offset, uint16 image size, uint16 relocation count
 *
 * At the file offset is the image as linked in slot 0, followed by the
 * relocations: uint16 offsets of bytes that hold the high byte of an
 * address inside the overlay.  Slots are page aligned, so loading into
 * another slot only adds that slot's page offset to those bytes.
 */
#include <fcntl.h>
#include <rp6502.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ovlcfg.h"

extern uint8_t __ovl_slot_ovl[OVL_NSLOTS], __ovl_slot_pin[OVL_NSLOTS],
    __ovl_slot_age[OVL_NSLOTS];
extern const uint8_t __ovl_slot_page[OVL_NSLOTS];
extern uint8_t __ovl_clock, __ovl_cur, __ovl_sp;
extern char __ovl_slot0[];
extern char __ovl_dir_xram[]; /* an XRAM address, not a RAM one */

unsigned long __ovl_loads, __ovl_forced;

static int ovl_fd = -1;

void __ovl_panic(uint8_t code);

void
__ovl_init(void)
{
    uint8_t i;

    for (i = 0; i < OVL_NSLOTS; i++)
        __ovl_slot_ovl[i] = 0xFF;
    __ovl_cur = 0xFF;
    __ovl_sp = 0;
    ovl_fd = open("ROM:" OVL_FILE, O_RDONLY);
    if (ovl_fd < 0)
        __ovl_panic(2);
}

uint8_t
__ovl_pick(void)
{
    uint8_t i, best = 0xFF, best_pin = 1, best_age = 0, pin, age;

    /* An empty slot, else the least recently entered unpinned slot, else
       (every slot pinned) the least recently entered slot. */
    for (i = 0; i < OVL_NSLOTS; i++) {
        pin = __ovl_slot_pin[i] != 0;
        if (__ovl_slot_ovl[i] == 0xFF && !pin)
            return i;
        age = (uint8_t) (__ovl_clock - __ovl_slot_age[i]);
        if (best == 0xFF || pin < best_pin
            || (pin == best_pin && age > best_age)) {
            best = i;
            best_pin = pin;
            best_age = age;
        }
    }
    if (best_pin)
        __ovl_forced++;
    return best;
}

static uint16_t
xram_word(void)
{
    uint16_t w = RIA.rw1;

    return w | ((uint16_t) RIA.rw1 << 8);
}

void
__ovl_load(uint8_t id, uint8_t slot)
{
    static uint16_t rel[32];
    uint32_t off;
    uint16_t size, nrel, n, i;
    uint8_t delta = __ovl_slot_page[slot];
    char *base = __ovl_slot0 + ((uint16_t) delta << 8);

    RIA.addr1 = (uint16_t) (uintptr_t) __ovl_dir_xram + (uint16_t) id * 8;
    RIA.step1 = 1;
    off = xram_word();
    off |= (uint32_t) xram_word() << 16;
    size = xram_word();
    nrel = xram_word();

    __ovl_slot_ovl[slot] = 0xFF;
    if (lseek(ovl_fd, (off_t) off, SEEK_SET) < 0
        || read(ovl_fd, base, size) != (int) size)
        __ovl_panic(3);
    if (delta) {
        while (nrel) {
            n = nrel > 32 ? 32 : nrel;
            if (read(ovl_fd, rel, n * 2) != (int) (n * 2))
                __ovl_panic(3);
            for (i = 0; i < n; i++)
                base[rel[i]] += delta;
            nrel -= n;
        }
    }
    __ovl_slot_ovl[slot] = id;
    __ovl_loads++;
}

void
__ovl_panic(uint8_t code)
{
    static const char *const why[] = { "?", "overlay stack overflow",
                                       "cannot open " OVL_FILE,
                                       "cannot read " OVL_FILE };

    printf("\r\noverlay manager: %s\r\n", why[code < 4 ? code : 0]);
    exit(1);
}
