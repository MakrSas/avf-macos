#include <stdint.h>
#include "fdt.h"
#include "mmu.h"

/* ns16550a UART registers (byte-addressable, offsets from base). Confirmed
 * from a real DTB dump off this exact AVF bootloader-mode VM: the console
 * node is "ns16550a" compatible, not PL011 (see docs/AVF_HARDWARE.md). */
#define UART_THR 0x00  /* transmit holding register (write) */
#define UART_LSR 0x05  /* line status register */
#define UART_LSR_THRE (1U << 5)  /* transmit holding register empty */

static volatile uint8_t *g_uart_base = 0;

static void uart_putc(char c)
{
    if (!g_uart_base) return;
    /* Bounded wait, not infinite: on real hardware the THRE bit does not
     * reliably come back quickly between bytes (see boot.S probe). Write
     * anyway after the bound rather than risk hanging forever. */
    for (volatile int i = 0; i < 100000; i++) {
        if (g_uart_base[UART_LSR] & UART_LSR_THRE) break;
    }
    g_uart_base[UART_THR] = (uint8_t)c;
}

static void uart_puts(const char *s)
{
    for (; *s; s++) {
        if (*s == '\n') uart_putc('\r');
        uart_putc(*s);
    }
}

static void uart_put_hex64(uint64_t v)
{
    char buf[16];
    for (int i = 15; i >= 0; i--) {
        uint32_t nib = v & 0xf;
        buf[i] = (char)(nib < 10 ? '0' + nib : 'a' + (nib - 10));
        v >>= 4;
    }
    uart_puts("0x");
    for (int i = 0; i < 16; i++) uart_putc(buf[i]);
}

static void uart_put_dec(uint64_t v)
{
    char buf[20];
    int i = 19;
    buf[i--] = '\0';
    if (v == 0) {
        uart_putc('0');
        return;
    }
    int start = i;
    while (v > 0 && i >= 0) {
        buf[i--] = (char)('0' + (v % 10));
        v /= 10;
    }
    uart_puts(&buf[i + 1]);
    (void)start;
}

static uint64_t read_current_el(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(v));
    return (v >> 2) & 0x3;
}

static uint64_t read_pc(void)
{
    uint64_t pc;
    __asm__ volatile("adr %0, ." : "=r"(pc));
    return pc;
}

/* Candidate UART compatible strings to try, in order, if DTB parsing succeeds.
 * Confirmed on real hardware: AVF bootloader-mode VMs use ns16550a, not PL011
 * (PL011 kept as a secondary candidate in case that ever changes). */
static const char *uart_compat_candidates[] = {
    "ns16550a",
    "arm,pl011",
    0
};

/* Confirmed real hardware layout for this AVF bootloader-mode VM, from an
 * earlier --dump-device-tree capture (docs/AVF_HARDWARE.md). Used as the
 * default so we can print diagnostics before risking any dereference of a
 * possibly-invalid x0/dtb pointer. */
#define KNOWN_UART_BASE   0x3f8ULL
#define KNOWN_RAM_BASE    0x80000000ULL
#define KNOWN_RAM_SIZE    0x10000000ULL

void loader_main(void *dtb)
{
    uint64_t el = read_current_el();
    uint64_t pc = read_pc();
    uint64_t dtb_addr = (uint64_t)(uintptr_t)dtb;
    uint64_t uart_addr = 0, uart_size = 0;
    int have_uart = 0;
    int dtb_ok = 0;
    int dtb_in_ram = (dtb_addr >= KNOWN_RAM_BASE) &&
                      (dtb_addr < KNOWN_RAM_BASE + KNOWN_RAM_SIZE);

    /* Bring up UART with the known-good address first so every subsequent
     * print is safe, before touching the (possibly bogus) dtb pointer. */
    g_uart_base = (volatile uint8_t *)(uintptr_t)KNOWN_UART_BASE;

    uart_puts("\n=== Hello from AVF bootloader ===\n");

    uart_puts("CurrentEL      : EL");
    uart_put_dec(el);
    uart_puts("\n");

    uart_puts("PC (approx)    : ");
    uart_put_hex64(pc);
    uart_puts("\n");

    uart_puts("DTB pointer (x0): ");
    uart_put_hex64(dtb_addr);
    uart_puts("\n");

    uart_puts("DTB in known RAM range: ");
    uart_puts(dtb_in_ram ? "yes\n" : "no -- skipping FDT parse to avoid fault\n");

    /* Bring up the MMU (identity map, RAM as Normal cacheable+executable,
     * everything else as Device-nGnRnE) before doing any general-purpose
     * struct walk over the DTB. Without this, ARMv8 reset-state defaults
     * all memory to Device-nGnRnE, which restricts unaligned/multi-word
     * access patterns a struct walker relies on -- confirmed on real
     * hardware to hang even a single aligned 4-byte read (see
     * docs/PROGRESS.md, 2026-08-18 entry). */
    mmu_init();
    uart_puts("MMU             : enabled (identity map, RAM as Normal)\n");

    if (dtb_in_ram && fdt_valid(dtb)) {
        dtb_ok = 1;
        for (int i = 0; uart_compat_candidates[i]; i++) {
            if (fdt_find_compatible_reg(dtb, uart_compat_candidates[i], &uart_addr, &uart_size)) {
                have_uart = 1;
                break;
            }
        }
    }

    if (have_uart) {
        g_uart_base = (volatile uint8_t *)(uintptr_t)uart_addr;
    }

    uart_puts("DTB valid magic: ");
    uart_puts(dtb_ok ? "yes\n" : "no\n");

    uart_puts("UART source    : ");
    uart_puts(have_uart ? "found in DTB\n" : "fallback (0x3f8)\n");

    uart_puts("UART base      : ");
    uart_put_hex64(uart_addr ? uart_addr : 0x3f8ULL);
    uart_puts("\n");

    if (dtb_ok) {
        uint64_t mem_base = 0, mem_size = 0;
        if (fdt_find_memory(dtb, &mem_base, &mem_size)) {
            uart_puts("Memory base    : ");
            uart_put_hex64(mem_base);
            uart_puts("\n");
            uart_puts("Memory size    : ");
            uart_put_hex64(mem_size);
            uart_puts("\n");
        } else {
            uart_puts("Memory node    : not found\n");
        }

        int ncpus = fdt_count_cpus(dtb);
        uart_puts("CPU count      : ");
        uart_put_dec((uint64_t)ncpus);
        uart_puts("\n");
    }

    uart_puts("=== end of bootloader report ===\n");

    for (;;) {
        __asm__ volatile("wfe");
    }
}
