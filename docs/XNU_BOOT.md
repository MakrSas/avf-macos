# Windows ARM64 boot notes (formerly XNU notes — project pivoted, see PROGRESS.md 2026-08-18)

## EDK2 firmware target

Building `ArmVirtPkg/ArmVirtQemuKernel.dsc` (not the "flash" `ArmVirtQemu.dsc`
variant): this target produces a firmware image meant to be loaded directly
via the Linux kernel boot protocol convention (entered with x0 = DTB
pointer, no separate flash/reset-vector dance) — exactly the boot
convention our own hand-written loader already confirmed AVF's `bootloader`
config field uses on real hardware.

## Known mismatch vs QEMU's virt board — needs verification, not assumed

EDK2's ArmVirtQemu platform code targets QEMU's `virt` board conventions.
Some of that is genuinely FDT-driven (RAM size/base, GIC address, PSCI
method), which should adapt fine to AVF's crosvm hardware map (see
`docs/AVF_HARDWARE.md`) automatically. But at least one piece is very
likely hardcoded rather than FDT-generic:

- **Serial console driver**: ArmVirtPkg conventionally links a
  `PL011SerialPortLib` for its debug/console UART. Our confirmed hardware
  is **ns16550a**, not PL011 (see `docs/AVF_HARDWARE.md`). If EDK2's serial
  library isn't swapped or made chip-generic, console output will likely
  fail silently the same way our own loader's first attempt did, before we
  discovered this exact mismatch.

Plan: build stock first, test on-device, and treat "no serial output" as
expected-and-diagnosable (same debugging playbook as the custom loader:
check whether execution reaches guest code at all via other signals, e.g.
`vm run` exit state, before assuming the whole firmware failed) rather than
grounds to declare the whole approach broken. If confirmed, the fix is
either patching in a 16550-compatible serial lib for this platform DSC, or
patching `PL011SerialPortLib` calls to the confirmed address as a quick
unblock.

## Reference prior art

[DroidVM](https://github.com/Droid-VM/DroidVM) has already booted Windows
ARM64 under crosvm + a forked EDK2 (`edk2-gunyah`) + modified virtio
drivers — but only demonstrated with root + Qualcomm Gunyah hypervisor on
Snapdragon 8 Gen3+, not AVF/KVM on Tensor. Worth diffing their EDK2 fork
against stock once building successfully, to see which of their patches
are Gunyah-specific (skip) vs. generic crosvm virtio/hardware adaptations
(potentially reusable).

## Disk transport

Confirmed virtio-PCI (not virtio-mmio) — see `docs/AVF_HARDWARE.md`. This
is the mainline-supported transport for both EDK2's own virtio-blk driver
(`OvmfPkg`/`ArmVirtPkg` virtio stack targets PCI) and for Windows'
`virtio-win` drivers post-boot, which is favorable.

## Not yet attempted

- Actually booting the built firmware via `vm run` and capturing output.
- Windows ARM64 install media (waiting on user to download a Windows
  Insider ARM64 ISO).
- ACPI vs. DeviceTree: Windows ARM64 conventionally boots via ACPI, not
  DT. EDK2 can generate ACPI tables from the DT it's given (this is
  standard practice for `ArmVirtPkg`), so this should be handled by the
  firmware layer rather than something we need to construct by hand — but
  unverified until an actual Windows boot attempt.
