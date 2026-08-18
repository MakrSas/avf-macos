# Progress log

## 2026-08-18

- Confirmed environment: Pixel 7 (panther), non-root ADB shell (uid=2000), AVF `vm` CLI at
  `/apex/com.android.virt/bin/vm`, `/dev/kvm` accessible, custom `bootloader` field in VM config
  accepted and actually executed by crosvm (see prior experiments in the source prompt).
- No local ARM64 cross-toolchain or adb on the build machine's PATH by default:
  - `adb.exe` found bundled with a local scrcpy install (`C:\Folders\scrcpy-win64-v4.0`).
  - No aarch64-linux-gnu-gcc / clang available locally -> builds go through GitHub Actions
    (`.github/workflows/build.yml`, `gcc-aarch64-linux-gnu` from Ubuntu runner apt).
- Wrote Milestone 1 minimal bootloader (`loader/`):
  - `boot.S`: entry point, saves x0 (dtb ptr), sets up stack, zeroes .bss, calls `loader_main`.
  - `fdt.c`/`fdt.h`: minimal flattened-devicetree walker (no libfdt dependency) to find a UART
    node by `compatible` string, `/memory` reg, and count `/cpus` children. Deliberately avoids
    hardcoding crosvm MMIO addresses per the "do not guess" constraint — falls back to the
    QEMU-virt-convention PL011 address (0x9000000) only if DTB parsing fails to find a UART node,
    and reports which path was taken.
  - `main.c`: prints EL, approx PC, DTB pointer + validity, UART source/base, memory base/size,
    CPU count, over PL011.
  - Linked with default AArch64 -mcmodel=small so addressing is adrp/add (PC-relative in
    practice), tolerant of crosvm loading the bootloader at an address different from the nominal
    link base (0x80000000).
- Not yet confirmed against real hardware: pending first CI build + `vm run` + serial capture.

### Next
- Run the build workflow, download the artifact, push+run on device, capture serial log.
- If no output appears: verify whether crosvm actually attaches a console for `bootloader`-mode
  VMs the same way it does for `kernel`-mode VMs, and whether x0 really holds a DTB pointer for
  this boot path (may need `vm run` verbose/debug flags, or instrumentation via GPIO/PSCI as
  a fallback signal if UART discovery fails silently).

## 2026-08-18 (later)

- **Milestone 1 confirmed on real hardware.** Full bring-up cycle, bugs found and fixed in order:
  1. First run: empty console. Root cause: UART driver assumed PL011 (32-bit regs, offset 0x18
     status), but `vm run --dump-device-tree` proved the real console is **ns16550a** (byte
     registers, LSR at offset 5) at physical address `0x3f8` (matches `/chosen/stdout-path`).
     Fixed driver protocol + address.
  2. Second run: still empty. Isolated with a raw-asm probe writing directly to the UART before
     any C/DTB code ran. First byte ('A') transmitted fine, but the *second* byte's busy-wait
     (polling LSR THRE bit) hung forever — the emulated UART doesn't reliably re-assert
     THRE quickly between bytes. Fixed by bounding the wait (fixed retry count, write anyway
     after the bound) instead of polling infinitely.
  3. Third run: got "Hello from AVF bootloader" header + EL1/PC/DTB-pointer output, then hung
     again right after the DTB-in-range check. Root cause: `loader_main` dereferenced `x0` (the
     DTB pointer) with **no exception vector table installed** — any bad/misclassified access
     hangs the VM silently. Even a single aligned 4-byte read of the DTB magic hung, despite the
     pointer being in-range (`0x80000000`, exactly RAM base). Suspected cause: with no MMU/page
     tables set up, ARMv8 reset-state defaults all memory to Device-nGnRnE type, which
     architecturally restricts unaligned/multi-word access patterns a general struct walker
     relies on. **Deferred** in-guest FDT parsing pending MMU bring-up; using the confirmed
     hardware map from out-of-band `--dump-device-tree` captures instead for now.
- Final stable output on-device:
  ```
  === Hello from AVF bootloader ===
  CurrentEL      : EL1
  PC (approx)    : 0x00000000802001d4
  DTB pointer (x0): 0x0000000080000000
  DTB in known RAM range: yes
  DTB valid magic: no
  UART source    : fallback (0x3f8)
  UART base      : 0x00000000000003f8
  === end of bootloader report ===
  ```
- **Pivoted overall project goal from XNU/macOS to Windows ARM64.** Rationale: a real macOS
  guest with GUI is not achievable via this path (or any non-Apple-Silicon host) because macOS
  userspace/kernelcache are signed and cryptographically tied to Apple's Secure Enclave chain,
  which isn't a solvable engineering problem here — declined to pursue firmware
  decryption/SEP-emulation as a workaround since that's DRM circumvention, not bring-up
  engineering. Windows ARM64 has no equivalent hardware-tied trust chain and has a known
  working precedent: the DroidVM project (github.com/Droid-VM/DroidVM) boots Windows ARM64
  under crosvm+EDK2+virtio, though only demonstrated with **root** + Qualcomm Gunyah hypervisor
  on Snapdragon 8 Gen3+ — our path (Pixel 7 Tensor G2, no root, plain AVF/KVM) is a novel,
  unverified combination reusing the same underlying crosvm virtio device model.
- **Probed AVF non-root limits for the Windows path:**
  - `memory_mib: 4096` accepted and the VM actually ran (not just accepted-then-killed), despite
    the host reporting only ~400-580 MB `MemFree`/`MemAvailable` at the time — crosvm/KVM
    memory allocation for the guest appears to be lazy/overcommitted rather than pre-committed.
  - `"disks": [{"image": "<path>", "writable": true}]` (plain path string; a nested
    `{"kind":"raw","path":...}` form is rejected by the config schema) is accepted without root
    and crosvm logs actually attaching the block device. **Disk transport is virtio-PCI**, not
    virtio-mmio — confirmed by diffing `--dump-device-tree` with/without the disk entry: no new
    DT node, only one added `interrupt-map` entry on the existing `pci-host-cam-generic` node.
    This is favorable for Windows: virtio-win drivers target virtio-pci as the primary transport.

## 2026-08-18 (MMU bring-up)

- Implemented identity-mapped MMU (`loader/src/mmu.c`, `mmu_asm.S`): single L1 table, 39-bit VA,
  4KiB granule, 1GiB block descriptors, indices 0-3 covering the first 4GiB. RAM
  (0x80000000-0xBFFFFFFF, index 2) mapped Normal write-back cacheable + executable; everything
  else (UART, GICv3, RTC, watchdog, PCI ECAM/IO/MMIO) mapped Device-nGnRnE, non-executable.
  Identity map means no VA/PA jump needed on enable.
- **Confirmed working on real hardware, unblocking the previously-deferred FDT parse:**
  ```
  === Hello from AVF bootloader ===
  CurrentEL      : EL1
  PC (approx)    : 0x00000000802002d8
  DTB pointer (x0): 0x0000000080000000
  DTB in known RAM range: yes
  MMU             : enabled (identity map, RAM as Normal)
  DTB valid magic: yes
  UART source    : found in DTB
  UART base      : 0x00000000000003f8
  Memory node    : not found   <- fixed below
  CPU count      : 1
  === end of bootloader report ===
  ```
- UART address is now discovered dynamically from the DTB (matches the confirmed 0x3f8), not the
  hardcoded fallback. CPU count correct (1, matches config default).
- Fixed a small bug found immediately after: `/memory`'s node identifies itself via
  `device_type = "memory"`, not a `compatible` property, so `fdt_find_memory` needed its own walk
  rather than reusing the compatible-string search.

### Next
- Waiting on user to download a Windows Insider ARM64 ISO.
- Meanwhile: start porting/adapting EDK2 (ArmVirtQemu as a reference) to this confirmed hardware
  map as the `bootloader` payload, targeting a handoff to `bootmgfw.efi` from the eventual
  Windows ARM64 install media. The MMU groundwork here (identity map, Normal-vs-Device
  classification) is a direct prerequisite EDK2 will also need, just at a larger scale (need to
  extend the L1 table / add L2 tables to cover >1GiB of RAM once testing with 4GiB configs).
