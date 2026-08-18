# XNU ARM64_VMAPPLE boot notes (Milestone 4)

Not started yet — this file is a placeholder until Milestone 1-3 (bootloader, hardware
map, FDT dump) are done. To be filled with findings on:

- ARM64_VMAPPLE boot protocol vs stock ARM64 XNU
- Mach-O loading requirements / kernelcache format
- boot-args and DeviceTree format XNU expects
- Required CPU features, interrupt controller, timer, console assumptions
- Apple Virtualization.framework-specific virtual devices
- Gaps between VMAPPLE expectations and crosvm's actual virtual hardware
