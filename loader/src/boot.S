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

    /* Isolation probe: write 'A' '\r' '\n' directly to the ns16550a UART at
     * 0x3f8 before touching x0/DTB at all, to tell apart "UART address/MMIO
     * write is wrong" from "DTB parsing crashes the VM". Poll LSR bit 5
     * (THR empty) before each byte, per 16550A protocol. */
    mov     x9, #0x3f8
probe_wait1:
    ldrb    w10, [x9, #5]
    tst     w10, #0x20
    b.eq    probe_wait1
    mov     w10, #'A'
    strb    w10, [x9]
probe_wait2:
    ldrb    w10, [x9, #5]
    tst     w10, #0x20
    b.eq    probe_wait2
    mov     w10, #'\r'
    strb    w10, [x9]
probe_wait3:
    ldrb    w10, [x9, #5]
    tst     w10, #0x20
    b.eq    probe_wait3
    mov     w10, #'\n'
    strb    w10, [x9]

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
