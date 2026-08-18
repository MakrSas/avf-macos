#ifndef FDT_H
#define FDT_H

#include <stdint.h>
#include <stddef.h>

#define FDT_MAGIC 0xd00dfeedU

struct fdt_header {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
};

int fdt_valid(const void *dtb);
uint32_t fdt_be32(uint32_t v);

/* Find first node whose "compatible" property contains the given string.
 * Returns 1 and fills *reg_addr/*reg_size from its first "reg" cell pair, or 0 if not found.
 * addr_cells/size_cells of the parent are assumed to be 2/2 (standard for arm64 virt DTs);
 * falls back gracefully by scanning root #address-cells/#size-cells first.
 */
int fdt_find_compatible_reg(const void *dtb, const char *compat,
                             uint64_t *reg_addr, uint64_t *reg_size);

/* Count immediate child nodes of "/cpus" whose name starts with "cpu@" or "cpu" */
int fdt_count_cpus(const void *dtb);

/* Find "/memory" node reg (base,size) */
int fdt_find_memory(const void *dtb, uint64_t *base, uint64_t *size);

#endif
