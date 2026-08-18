/*
 * Minimal flattened-devicetree walker.
 * Just enough to locate a UART node, /memory, and count /cpus children.
 * Not a general-purpose libfdt replacement.
 */
#include "fdt.h"

#define FDT_BEGIN_NODE 0x00000001U
#define FDT_END_NODE   0x00000002U
#define FDT_PROP       0x00000003U
#define FDT_NOP        0x00000004U
#define FDT_END        0x00000009U

uint32_t fdt_be32(uint32_t v)
{
    return ((v & 0xff) << 24) | ((v & 0xff00) << 8) |
           ((v & 0xff0000) >> 8) | ((v >> 24) & 0xff);
}

static uint64_t rd_be64(const uint8_t *p)
{
    uint32_t hi = fdt_be32(*(const uint32_t *)p);
    uint32_t lo = fdt_be32(*(const uint32_t *)(p + 4));
    return ((uint64_t)hi << 32) | lo;
}

int fdt_valid(const void *dtb)
{
    const struct fdt_header *h = (const struct fdt_header *)dtb;
    return fdt_be32(h->magic) == FDT_MAGIC;
}

static const char *strz_at(const void *dtb, uint32_t off_dt_strings, uint32_t nameoff)
{
    return (const char *)dtb + off_dt_strings + nameoff;
}

static int str_starts_with(const char *s, const char *prefix)
{
    while (*prefix) {
        if (*s != *prefix) return 0;
        s++; prefix++;
    }
    return 1;
}

static int str_contains(const char *hay, const char *needle)
{
    for (; *hay; hay++) {
        const char *h = hay, *n = needle;
        while (*n && *h == *n) { h++; n++; }
        if (!*n) return 1;
    }
    return 0;
}

/* Generic walk: calls back for each node with depth, name, and lets caller
 * inspect properties via next_prop-style iteration. Implemented inline below
 * per-purpose instead of generic callbacks, to keep this freestanding & simple.
 */

int fdt_find_compatible_reg(const void *dtb, const char *compat,
                             uint64_t *reg_addr, uint64_t *reg_size)
{
    const struct fdt_header *h = (const struct fdt_header *)dtb;
    uint32_t off_struct = fdt_be32(h->off_dt_struct);
    uint32_t off_strings = fdt_be32(h->off_dt_strings);
    const uint8_t *p = (const uint8_t *)dtb + off_struct;
    const uint8_t *end = (const uint8_t *)dtb + off_struct + fdt_be32(h->size_dt_struct);

    int match_depth = -1;
    uint64_t last_reg_addr = 0, last_reg_size = 0;
    int have_reg = 0;

    while (p < end) {
        uint32_t tok = fdt_be32(*(const uint32_t *)p);
        p += 4;
        if (tok == FDT_BEGIN_NODE) {
            while (*p) p++;
            p++;
            while (((uintptr_t)p) % 4 != 0) p++;
        } else if (tok == FDT_PROP) {
            uint32_t len = fdt_be32(*(const uint32_t *)p); p += 4;
            uint32_t nameoff = fdt_be32(*(const uint32_t *)p); p += 4;
            const char *pname = strz_at(dtb, off_strings, nameoff);
            const uint8_t *val = p;
            if (str_starts_with(pname, "compatible") && str_contains((const char*)val, compat)) {
                match_depth = 1;
            }
            if (str_starts_with(pname, "reg") && len >= 16) {
                last_reg_addr = rd_be64(val);
                last_reg_size = rd_be64(val + 8);
                have_reg = 1;
            }
            p += len;
            while (((uintptr_t)p) % 4 != 0) p++;
            if (match_depth == 1 && have_reg) {
                *reg_addr = last_reg_addr;
                *reg_size = last_reg_size;
                return 1;
            }
        } else if (tok == FDT_END_NODE) {
            match_depth = -1;
            have_reg = 0;
        } else if (tok == FDT_NOP) {
            /* nothing */
        } else if (tok == FDT_END) {
            break;
        } else {
            break;
        }
    }
    return 0;
}

int fdt_find_memory(const void *dtb, uint64_t *base, uint64_t *size)
{
    return fdt_find_compatible_reg(dtb, "memory", base, size);
}

int fdt_count_cpus(const void *dtb)
{
    const struct fdt_header *h = (const struct fdt_header *)dtb;
    uint32_t off_struct = fdt_be32(h->off_dt_struct);
    uint32_t off_strings = fdt_be32(h->off_dt_strings);
    const uint8_t *p = (const uint8_t *)dtb + off_struct;
    const uint8_t *end = (const uint8_t *)dtb + off_struct + fdt_be32(h->size_dt_struct);

    int in_cpus = 0;
    int cpus_depth = -1;
    int depth = 0;
    int count = 0;

    while (p < end) {
        uint32_t tok = fdt_be32(*(const uint32_t *)p);
        p += 4;
        if (tok == FDT_BEGIN_NODE) {
            const char *name = (const char *)p;
            depth++;
            if (in_cpus && str_starts_with(name, "cpu")) count++;
            if (!in_cpus) {
                /* check if this node's name is exactly "cpus" */
                const char *n = name;
                if (n[0]=='c'&&n[1]=='p'&&n[2]=='u'&&n[3]=='s'&&(n[4]=='\0'||n[4]=='@')) {
                    in_cpus = 1;
                    cpus_depth = depth;
                }
            }
            /* advance past name */
            while (*p) p++;
            p++;
            while (((uintptr_t)p) % 4 != 0) p++;
        } else if (tok == FDT_PROP) {
            uint32_t len = fdt_be32(*(const uint32_t *)p); p += 4;
            p += 4; /* nameoff */
            p += len;
            while (((uintptr_t)p) % 4 != 0) p++;
        } else if (tok == FDT_END_NODE) {
            if (in_cpus && depth == cpus_depth) in_cpus = 0;
            depth--;
        } else if (tok == FDT_NOP) {
        } else if (tok == FDT_END) {
            break;
        } else {
            break;
        }
    }
    (void)off_strings;
    return count;
}
