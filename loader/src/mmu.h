#ifndef MMU_H
#define MMU_H

/* Sets up an identity-mapped MMU covering the first 4 GiB of physical
 * address space: index 2 (0x80000000-0xBFFFFFFF, where RAM lives per the
 * confirmed hardware map) as Normal cacheable+executable memory, everything
 * else in that range as Device-nGnRnE (covers UART, GICv3, RTC, watchdog,
 * PCI ECAM/IO/MMIO). Returns nothing; halts via the caller's own means if
 * something is badly wrong (this code does not itself detect failure --
 * enabling the MMU with a bad table would fault on the next access).
 *
 * KNOWN LIMITATION: only identity-maps up to 4 GiB (single L1 table, 1 GiB
 * blocks, indices 0-3), and treats index 2 only as RAM -- a `memory_mib`
 * value that pushes RAM past 0xC0000000 needs a wider table. Fine for the
 * 256 MiB config currently used for on-device testing.
 */
void mmu_init(void);

#endif
