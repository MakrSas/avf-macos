# avf-macos

Experiment: bring up Darwin/XNU as an Android Virtualization Framework (AVF) /
crosvm / pKVM guest on a Pixel 7, without root.

See [docs/PROGRESS.md](docs/PROGRESS.md) for the running log and
[docs/AVF_HARDWARE.md](docs/AVF_HARDWARE.md) / [docs/XNU_BOOT.md](docs/XNU_BOOT.md)
as they get filled in.

## Layout

- `loader/` — bare-metal ARM64 bootloader payload used as the AVF `bootloader` field.
- `avf/` — VM configs and scripts to push/run on-device via `adb` + `vm run`.
- `tools/dtb/` — devicetree dump/inspection helpers.
- `xnu/patches/` — XNU source patches for VMAPPLE/crosvm compatibility (no XNU source committed here).
- `.github/workflows/` — CI that cross-builds the loader and uploads it as an artifact.

## Build

Builds run in GitHub Actions (`gcc-aarch64-linux-gnu`). To build locally with a cross toolchain:

```bash
cd loader
make CROSS=aarch64-linux-gnu-
```

## Run on device

```bash
avf/scripts/run_bootloader.sh loader/build/bootloader.bin
```

Requires `adb` connected to the target device and AVF's `vm` CLI present
(`/apex/com.android.virt/bin/vm`), which is stock on recent Android builds.

## Constraints

- No root, no unlocked bootloader, no modification of `/system`, `/vendor`, APEX,
  VirtualizationService, or the crosvm binary on the phone.
- No QEMU/TCG — this targets real hardware virtualization (AVF -> crosvm -> KVM/pKVM).
- No proprietary Apple firmware, IPSW contents, or macOS binaries are committed to this repo.
