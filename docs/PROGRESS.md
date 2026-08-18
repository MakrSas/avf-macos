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

## 2026-08-18 (EDK2 DEBUG build: still silent)

Switched EDK2 build to DEBUG target (RELEASE strips DEBUG()-gated boot log
messages, a real and separate issue from the earlier PL011-vs-16550 fix).
Build succeeded, but on-device console is STILL completely empty -- no
output at all, not even a single byte, despite DEBUG builds normally
printing SEC-phase entry banners immediately via DebugLib before almost
anything else happens.

This means the empty console is not (only) explained by the RELEASE
stripping theory -- EDK2 is likely failing/hanging even earlier than SEC's
first DEBUG() call, before our serial fix would even have a chance to run.
Plausible causes, not yet isolated:
- MMU/memory-map assumptions in ArmVirtMemoryInitPeiLib or ArmGicLib
  (redistributor stride calculation, etc.) not matching our confirmed
  hardware map exactly.
- SEC-phase PCD access assumptions (FixedAtBuild should work, but worth
  verifying PcdSerialUseMmio is actually being read as expected that early).
- Something in the "Kernel" boot-protocol image header handling specific
  to ArmVirtQemuKernel.dsc that differs from our own hand-rolled loader's
  entry assumptions.

This is real EDK2 platform bring-up work -- meaningfully harder than the
from-scratch loader (which is small enough to fully reason about) and not
yet resolved. Deprioritized in favor of the Android app path (below),
which is closer to a genuinely new capability (real GPU/display) rather
than further serial debugging on a firmware whose ultimate payoff (Windows
boot) is still several unproven steps away regardless.

## 2026-08-18 (Android app: builds successfully)

`android-app/` (Gradle multi-module: `:app` + `:stubs`) now builds a debug
APK in CI. Key fixes along the way:
- `android.system.virtualmachine` is not in any compileSdk platform stub
  jar at any API level tried (35, 36) -- confirmed via
  `/apex/com.android.virt/etc/classpaths/bootclasspath.pb` on the real
  Pixel 7 that the `com.android.virt` mainline module puts
  `framework-virtualization.jar` directly on the device's BOOTCLASSPATH
  instead, meaning apps get it automatically at runtime with **no**
  `<uses-sdk-library>` manifest entry needed -- but Gradle/javac has
  nothing to compile against.
- Pulling that jar from the device didn't help either: it contains
  `classes.dex` (ART bytecode), not `.class` files, so it's not readable
  by javac as a classpath entry regardless of how it's wired in.
- Fix: `:stubs` module with hand-written stand-in classes (real method
  signatures sourced from the AOSP `framework-virtualization` README/src,
  empty bodies) used `compileOnly` from `:app` -- never packaged into the
  APK. At runtime the actual on-device classes resolve via BOOTCLASSPATH,
  unaffected by these stubs existing at compile time only.

### Next
- Install the built APK on-device, grant `MANAGE_VIRTUAL_MACHINE` +
  `USE_CUSTOM_VIRTUAL_MACHINE` via `adb shell pm grant` (confirmed
  grantable without root), launch, and see whether `vm.run()` actually
  succeeds and produces visible output via the SurfaceView -- this directly
  tests the real display/GPU path that the bare `vm run` CLI structurally
  cannot provide (see AVF_HARDWARE.md).

## 2026-08-18 (Android app: real progress, GPU backend fix)

Huge milestone: after `adb shell settings put global hidden_api_policy 1`
(disables Android's hidden-API enforcement for debuggable apps -- no root
needed), the app's `vm.run()` call actually succeeded end to end:
permissions, SurfaceView, VirtualMachineManager, custom bootloader config,
display/gpu config all wired through correctly and virtmgr spawned a real
crosvm process with `--android-display-service=cid:N` (confirmed via
logcat `virtmgr` tag showing the exact args).

It then crashed almost immediately:
```
crosvm: thread 'v_gpu' panicked at .../virtio/gpu/mod.rs:1687:14:
Failed to create virtio gpu worker thread: invalid rutabaga build parameters
```
Root cause: used `backend=virglrenderer` + `context_types=[virgl2]` in
GpuConfig, whereas the confirmed-working Debian Terminal VM's actual
crosvm invocation (inspected earlier via `ps`/`/proc/PID/cmdline`) uses
`backend=2d` with no context_types at all. Fixed to match.

Also confirmed via runtime reflection dump: the real on-device
`VirtualMachineCustomImageConfig`/`DisplayConfig`/`GpuConfig` API is
**hidden-API-blocked** (`domain=platform, api=blocked`) for regular apps
by default -- this is not a wrong-signature problem, it's Android's
non-SDK interface enforcement. `hidden_api_policy=1` is the standard,
non-root, debuggable-app workaround (well known from Android app dev
tooling) and is what actually unblocked it, not the earlier stub/jar
compile-time work (which only solved the *compile-time* half of the
problem -- runtime access needed this separately).

### Next
- Rebuild with the 2d GPU backend fix, retest -- if crosvm progresses
  further, check whether it actually reaches our EDK2 firmware (which
  itself still produces no serial output for reasons not yet isolated --
  see the DEBUG-build entry above) or produces any visible frame via the
  SurfaceView regardless of firmware silence.
- Note hidden_api_policy=1 is a device-wide, non-persistent-across-reboot
  (likely) developer setting -- fine for our own testing, but worth
  remembering this app won't work this way for an end user without it set.

## 2026-08-18 (Android app: no more crash, VM runs clean)

With backend=2d, the crosvm GPU worker panic is gone entirely. Confirmed
via logcat: `virtmgr` spawns crosvm with the full `--android-display-service`
+ `--gpu=backend=2d,...` args, crosvm creates the KVM hypervisor, and there
is **no crash/stop event** -- the VM just runs. Screen stays black (expected:
our EDK2 firmware itself produces no visible output for reasons still
unresolved -- see the DEBUG-build silence entry above; this is a firmware
problem, not an app/display-pipeline problem).

**This is the milestone the whole app detour was for: the full chain
(non-root permissions -> SurfaceView -> hidden-API-gated
VirtualMachineCustomImageConfig -> virtmgr -> crosvm --android-display-service
+ --gpu) is now proven to work end-to-end without crashing**, on a stock
Pixel 7 without root. What's left blocking an actually visible boot is
entirely the separate, not-yet-isolated EDK2 early-boot silence.

### Summary of everything solved to get here
1. Custom bootloader identity confirmed executing via AVF `vm run`, no root.
2. Real hardware map extracted via `--dump-device-tree` (GICv3, ns16550a
   UART @0x3f8, PCI ECAM, PSCI/hvc, RTC, watchdog) -- see AVF_HARDWARE.md.
3. MMU/identity-map bring-up in our own loader, unblocking safe FDT parsing.
4. EDK2 (ArmVirtQemuKernel.dsc) built successfully after fixing: GCC5->GCC
   toolchain rename, PL011->16550 serial swap, cascading PciExpressLib/
   PlatformHookLib module-type gaps exposed by that swap, DEBUG vs RELEASE
   DEBUG()-stripping.
5. Windows ARM64 ISO (`Win11_25H2_Russian_Arm64_v2.iso`, 7.9GB) already on
   device at `/sdcard/Download/`, not yet wired into any boot attempt.
6. Discovered GPU/display fundamentally requires an app-hosted VM
   (`--android-display-service`), not the bare `vm run` CLI -- traced to
   the real Debian Terminal app's actual crosvm invocation.
7. Built a minimal Android app (`android-app/`) using the public-but-hidden-
   API-gated `android.system.virtualmachine.VirtualMachineCustomImageConfig`
   surface: solved compile-time linking (hand-written `:stubs` module,
   since the on-device jar is ART bytecode not `.class` files) and
   runtime linking (`hidden_api_policy=1` via adb, no root) separately.
   Fixed a crosvm GPU-backend crash (`2d` not `virglrenderer`) by matching
   the real working Debian VM's exact flags.

### Still open
- **EDK2 firmware produces zero output** (serial or, now confirmed,
  display) even after the PL011->16550 fix and DEBUG target -- likely
  hangs/crashes before reaching either output path, for reasons not yet
  isolated (see the "EDK2 DEBUG build: still silent" entry). This blocks
  actually seeing anything boot, independent of the now-working app/GPU
  pipeline.
- Windows ISO not yet wired into any config (needs the EDK2 firmware
  question resolved first, or a different bootloader entirely).
