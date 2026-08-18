#include <stdint.h>
#include "mmu.h"

#define PT_VALID      (1ULL << 0)
#define PT_AF         (1ULL << 10)
#define PT_SH_INNER   (3ULL << 8)
#define PT_AP_RW_EL1  (0ULL << 6)
#define PT_ATTR(n)    (((uint64_t)(n)) << 2)
#define PT_PXN        (1ULL << 53)
#define PT_UXN        (1ULL << 54)

#define MAIR_DEVICE_IDX 0
#define MAIR_NORMAL_IDX 1

/* RAM index per the confirmed hardware map: base 0x80000000, so it falls in
 * 1 GiB block index 2 (0x80000000-0xBFFFFFFF). See mmu.h limitation note. */
#define RAM_L1_INDEX 2

extern void mmu_enable(uint64_t ttbr0, uint64_t mair, uint64_t tcr);

/* One page (4KiB / 512 entries), used as the single L1 table for a 39-bit
 * VA space (T0SZ=25) with 4KiB granule -- each entry here is a 1 GiB block,
 * so this single table covers the entire 512 GiB VA range at 1 GiB
 * granularity. Only indices 0-3 (first 4 GiB) are populated. */
__attribute__((aligned(4096)))
static uint64_t l1_table[512];

void mmu_init(void)
{
    for (int i = 0; i < 512; i++) {
        l1_table[i] = 0;
    }

    for (int idx = 0; idx < 4; idx++) {
        uint64_t base = (uint64_t)idx << 30;
        if (idx == RAM_L1_INDEX) {
            /* Normal, cacheable, executable -- our own code runs from here. */
            l1_table[idx] = base | PT_AF | PT_SH_INNER | PT_AP_RW_EL1 |
                             PT_ATTR(MAIR_NORMAL_IDX) | PT_VALID;
        } else {
            /* Device-nGnRnE, non-executable -- MMIO region. */
            l1_table[idx] = base | PT_AF | PT_SH_INNER | PT_AP_RW_EL1 |
                             PT_ATTR(MAIR_DEVICE_IDX) | PT_PXN | PT_UXN | PT_VALID;
        }
    }

    uint64_t mair = (0x00ULL << (8 * MAIR_DEVICE_IDX)) |   /* Device-nGnRnE */
                     (0xFFULL << (8 * MAIR_NORMAL_IDX));    /* Normal WB RWA */

    uint64_t mmfr0;
    __asm__ volatile("mrs %0, ID_AA64MMFR0_EL1" : "=r"(mmfr0));
    uint64_t parange = mmfr0 & 0xF;

    uint64_t tcr = (25ULL << 0)      /* T0SZ = 25 -> 39-bit input VA */
                  | (1ULL << 8)      /* IRGN0 = write-back, write-allocate */
                  | (1ULL << 10)     /* ORGN0 = write-back, write-allocate */
                  | (3ULL << 12)     /* SH0 = inner shareable */
                  | (0ULL << 14)     /* TG0 = 4KiB granule */
                  | (parange << 16)  /* IPS: match ID_AA64MMFR0_EL1.PARange */
                  | (1ULL << 23);    /* EPD1 = 1: no TTBR1 walks, unused */

    mmu_enable((uint64_t)(uintptr_t)l1_table, mair, tcr);
}
