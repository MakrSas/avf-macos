/*
 * void mmu_enable(uint64_t ttbr0, uint64_t mair, uint64_t tcr);
 * Programs MAIR_EL1/TCR_EL1/TTBR0_EL1 and turns the MMU + caches on via
 * SCTLR_EL1. Since the table is an identity map, there is no VA/PA jump
 * required -- execution simply continues from the same PC.
 */
.section .text
.global mmu_enable
mmu_enable:
    msr     mair_el1, x1
    msr     tcr_el1, x2
    msr     ttbr0_el1, x0
    isb

    tlbi    vmalle1
    dsb     ish
    isb

    mrs     x3, sctlr_el1
    orr     x3, x3, #1          /* M: MMU enable */
    orr     x3, x3, #(1 << 2)   /* C: data cache enable */
    orr     x3, x3, #(1 << 12)  /* I: instruction cache enable */
    dsb     sy
    msr     sctlr_el1, x3
    isb
    ret
