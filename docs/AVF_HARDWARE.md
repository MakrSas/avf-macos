# AVF / crosvm virtual hardware (Milestone 2)

Status: not yet populated with confirmed values — to be filled in from real DTB dumps
(Milestone 3) and crosvm source, not guessed.

Known so far (from prior experiments, see prompt history / docs/PROGRESS.md):

- Interrupt controller: GICv3 (seen in Linux boot log: `GIC: PPI INTID31 is secure or misconfigured`)
- RTC: `pl030` at an address whose DT node name is `2000.rtc`
- Console for `kernel`-mode microdroid VMs: virtio (`console=hvc0`), not the raw UART
  the bootloader-mode payload will need to use before any drivers exist.

Everything else (RAM base/size, GIC distributor/redistributor addresses, timer, PSCI,
UART base for bootloader-mode boot, virtio-mmio device list, boot register state) is
TBD pending:

1. First successful bootloader run with FDT dump (Milestone 1/3).
2. Cross-reference against crosvm source (`aarch64` arch module) for the exact
   memory map crosvm assigns in `bootloader`-mode VMs specifically, which may differ
   from `kernel`-mode VMs.
