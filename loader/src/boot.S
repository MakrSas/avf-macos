/*
 * Entry point for AVF custom bootloader.
 * crosvm jumps here with x0 = DTB pointer (Linux boot protocol convention),
 * MMU off, caches off, running at whatever EL crosvm started us in.
 */
.section ".text.boot"
.global _start

_start:
    /* x0 holds dtb pointer (if crosvm follows Linux boot convention) */
    mov     x19, x0

    /* Isolation probe: write 'A' 'B' 'C' '\r' '\n' directly to the ns16550a
     * UART at 0x3f8 before touching x0/DTB at all. LSR-THRE wait is bounded
     * (not infinite) since the first byte showed the bit does not reliably
     * come back quickly -- if it stays clear we just write anyway rather
     * than hang forever. */
    mov     x9, #0x3f8
    mov     w11, #'A'
    bl      probe_putc
    mov     w11, #'B'
    bl      probe_putc
    mov     w11, #'C'
    bl      probe_putc
    mov     w11, #'\r'
    bl      probe_putc
    mov     w11, #'\n'
    bl      probe_putc
    b       probe_done

probe_putc:
    mov     x12, #100000
probe_wait_bounded:
    ldrb    w10, [x9, #5]
    tst     w10, #0x20
    b.ne    probe_do_write
    subs    x12, x12, #1
    b.ne    probe_wait_bounded
probe_do_write:
    strb    w11, [x9]
    ret
probe_done:

    /* set up a small stack, PC-relative so it works regardless of link vs load base */
    adrp    x1, __stack_top
    add     x1, x1, :lo12:__stack_top
    mov     sp, x1

    /* zero .bss */
    adrp    x1, __bss_start
    add     x1, x1, :lo12:__bss_start
    adrp    x2, __bss_end
    add     x2, x2, :lo12:__bss_end
1:
    cmp     x1, x2
    b.ge    2f
    str     xzr, [x1], #8
    b       1b
2:
    mov     x0, x19
    bl      loader_main

    /* should never return; spin */
hang:
    wfe
    b       hang

.section ".bss"
.align  4
.space  16384
__stack_top:
