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
