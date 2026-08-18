#include <stdint.h>
#include "fdt.h"

/* PL011 UART registers (offsets), used once we know the base address from the DTB. */
#define UARTDR   0x00
#define UARTFR   0x18
#define UARTFR_TXFF (1U << 5)

static volatile uint32_t *g_uart_base = 0;

static void uart_putc(char c)
{
    if (!g_uart_base) return;
    while (g_uart_base[UARTFR / 4] & UARTFR_TXFF) { }
    g_uart_base[UARTDR / 4] = (uint32_t)(uint8_t)c;
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

/* Candidate UART compatible strings to try, in order, if DTB parsing succeeds. */
static const char *uart_compat_candidates[] = {
    "arm,pl011",
    "ns16550a",
    0
};

void loader_main(void *dtb)
{
    uint64_t el = read_current_el();
    uint64_t pc = read_pc();
    uint64_t uart_addr = 0, uart_size = 0;
    int have_uart = 0;
    int dtb_ok = 0;

    if (dtb && fdt_valid(dtb)) {
        dtb_ok = 1;
        for (int i = 0; uart_compat_candidates[i]; i++) {
            if (fdt_find_compatible_reg(dtb, uart_compat_candidates[i], &uart_addr, &uart_size)) {
                have_uart = 1;
                break;
            }
        }
    }

    if (have_uart) {
        g_uart_base = (volatile uint32_t *)(uintptr_t)uart_addr;
    } else {
        /* Fallback: crosvm/QEMU-virt convention PL011 base, used only if DTB
         * parsing failed to find a UART node. This is a documented fallback,
         * not a guess presented as fact. */
        g_uart_base = (volatile uint32_t *)(uintptr_t)0x9000000ULL;
    }

    uart_puts("\n=== Hello from AVF bootloader ===\n");

    uart_puts("CurrentEL      : EL");
    uart_put_dec(el);
    uart_puts("\n");

    uart_puts("PC (approx)    : ");
    uart_put_hex64(pc);
    uart_puts("\n");

    uart_puts("DTB pointer    : ");
    uart_put_hex64((uint64_t)(uintptr_t)dtb);
    uart_puts("\n");

    uart_puts("DTB valid magic: ");
    uart_puts(dtb_ok ? "yes\n" : "no\n");

    uart_puts("UART source    : ");
    uart_puts(have_uart ? "found in DTB\n" : "fallback guess (0x9000000)\n");

    uart_puts("UART base      : ");
    uart_put_hex64(uart_addr ? uart_addr : 0x9000000ULL);
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
