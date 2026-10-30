; RP6502 NetHack overlay manager: cross-overlay calls and returns.
;
; Code that does not fit in RAM is split into overlays that run in a few
; page-aligned slots.  ovlpack.py links each overlay at slot 0 and records
; the bytes holding the high byte of an in-overlay address, so an overlay
; can be loaded into any slot (ovlload.c).
;
; A call from one overlay (or from resident code) to a function in another
; overlay is rewritten by ovlpack.py to
;
;       jsr __ovl_far
;       .short DESC                     ; XRAM address of the descriptor
;
; and every function whose address is taken outside its own overlay gets a
; 5-byte thunk in resident RAM, whose address is the function's address
; everywhere else (so function pointers stay valid and unique):
;
;       __ovt_<name>:  jsr __ovl_thunk
;                      .short DESC
;
; The descriptor of a function is 3 bytes in XRAM:
;
;       .byte overlay id, entry lo, entry hi    (entry as linked in slot 0)
;
; __ovl_far/__ovl_thunk push the running overlay (the "current" one, $FF for
; none) on the overlay stack, load the callee's overlay if it is not in a
; slot, make it current and jump to it with __ovl_ret as the return address.
; __ovl_ret pops the previous current overlay and reloads it into the same
; slot if it was evicted.  Overlays on the overlay stack are pinned, so they
; are only evicted when every slot is pinned; because calls and returns
; nest, putting them back in the same slot on return is always safe.
; Resident code that an overlay calls directly runs "inside" that overlay,
; which is why the current overlay is tracked rather than derived from
; return addresses.
;
; Registers: Y is free on entry (not an argument register); A and X carry
; arguments in and results out and are preserved; __rc2.. are not touched
; on the fast path, and are saved around the C loader on the slow path.
; RIA portal 1 belongs to the overlay manager.
;
; ovlpack.py generates ovlcfg.inc (OVL_NSLOTS, OVL_DEPTH) and defines
; __ovl_slot_page (per slot, the page offset from slot 0).

.include "imag.inc"
.include "ovlcfg.inc"

RIA_RW1   = $FFE8
RIA_STEP1 = $FFE9
RIA_ADDR1 = $FFEA

.section .zp.bss.ovl,"za",@nobits
.globl __ovl_cur, __ovl_clock, __ovl_sp
__ovl_a:     .zero 1
__ovl_x:     .zero 1
__ovl_id:    .zero 1
__ovl_tgt:   .zero 2
__ovl_sp:    .zero 1
__ovl_cur:   .zero 1        ; slot of the running overlay, $FF for none
__ovl_clock: .zero 1

.section .bss.ovl,"aw",@nobits
.globl __ovl_slot_ovl, __ovl_slot_pin, __ovl_slot_age
__ovl_stk:      .zero OVL_DEPTH        ; saved __ovl_cur
__ovl_stk_id:   .zero OVL_DEPTH        ; overlay that was in that slot
__ovl_slot_ovl: .zero OVL_NSLOTS       ; overlay in each slot, $FF empty
__ovl_slot_pin: .zero OVL_NSLOTS       ; overlay stack entries per slot
__ovl_slot_age: .zero OVL_NSLOTS       ; __ovl_clock at last entry
__ovl_save:     .zero 18               ; __rc2..__rc19 around the C loader

.section .text.__ovl_far,"ax",@progbits
.globl __ovl_far, __ovl_thunk
.type __ovl_far,@function
__ovl_far:
    sta __ovl_a
    stx __ovl_x
    pla                         ; return address = jsr + 2
    sta __ovl_tgt
    pla
    sta __ovl_tgt+1
    clc                         ; return past the inline descriptor address
    lda __ovl_tgt
    adc #2
    tax
    lda __ovl_tgt+1
    adc #0
    pha
    txa
    pha
    bra 1f
__ovl_thunk:
    sta __ovl_a
    stx __ovl_x
    pla                         ; thunk + 2; the caller's return is below
    sta __ovl_tgt
    pla
    sta __ovl_tgt+1
1:  ldy #1
    lda (__ovl_tgt),y
    sta RIA_ADDR1
    iny
    lda (__ovl_tgt),y
    sta RIA_ADDR1+1
    lda #1
    sta RIA_STEP1
    lda RIA_RW1
    sta __ovl_id
    lda RIA_RW1
    sta __ovl_tgt
    lda RIA_RW1
    sta __ovl_tgt+1

    ldy __ovl_sp                ; push the current overlay and pin it
    cpy #OVL_DEPTH
    bcs .Lovl_overflow
    ldx __ovl_cur
    txa
    sta __ovl_stk,y
    bmi 1f
    lda __ovl_slot_ovl,x
    sta __ovl_stk_id,y
    inc __ovl_slot_pin,x
1:  iny
    sty __ovl_sp

    lda __ovl_id                ; find the callee's overlay in a slot
    ldx #OVL_NSLOTS-1
2:  cmp __ovl_slot_ovl,x
    beq 3f
    dex
    bpl 2b
    jsr __ovl_fault             ; load it; slot in X
3:  stx __ovl_cur
    inc __ovl_clock
    lda __ovl_clock
    sta __ovl_slot_age,x
    lda __ovl_tgt+1             ; entries are relative to slot 0
    clc
    adc __ovl_slot_page,x
    clc
    adc #mos16hi(__ovl_slot0)
    sta __ovl_tgt+1

    lda #mos16hi(__ovl_ret - 1)
    pha
    lda #mos16lo(__ovl_ret - 1)
    pha
    lda __ovl_a
    ldx __ovl_x
    jmp (__ovl_tgt)

.Lovl_overflow:
    lda #1
    jmp __ovl_panic
.size __ovl_far, .-__ovl_far

.section .text.__ovl_ret,"ax",@progbits
.type __ovl_ret,@function
__ovl_ret:
    sta __ovl_a
    stx __ovl_x
    ldy __ovl_sp
    dey
    sty __ovl_sp
    ldx __ovl_stk,y
    stx __ovl_cur
    bmi 1f
    dec __ovl_slot_pin,x
    lda __ovl_stk_id,y
    cmp __ovl_slot_ovl,x
    beq 1f
    sta __ovl_id                ; evicted while pinned: put it back
    jsr __ovl_reload
1:  lda __ovl_a
    ldx __ovl_x
    rts
.size __ovl_ret, .-__ovl_ret

; Slow paths.  __ovl_fault picks a slot for __ovl_id and loads it, leaving
; the slot in X; __ovl_reload loads __ovl_id into slot X.  Both call the C
; loader, which may clobber the caller-saved imaginary registers.
.section .text.__ovl_fault,"ax",@progbits
.type __ovl_fault,@function
__ovl_fault:
    jsr .Lovl_save
    jsr __ovl_pick              ; uint8_t __ovl_pick(void): slot in A
    tax
    jmp .Lovl_load
__ovl_reload:
    jsr .Lovl_save
.Lovl_load:
    txa
    pha
    lda __ovl_id                ; __ovl_load(id = A, slot = X)
    jsr __ovl_load
    ldy #17
1:  lda __ovl_save,y
    sta __rc2,y
    dey
    bpl 1b
    pla
    tax
    rts
.Lovl_save:
    ldy #17
1:  lda __rc2,y
    sta __ovl_save,y
    dey
    bpl 1b
    rts
.size __ovl_fault, .-__ovl_fault
