# AVF / crosvm virtual hardware (Milestone 2)

Confirmed by real device tree dump captured on Pixel 7 via
`vm run --dump-device-tree`, from a `bootloader`-mode custom VM config
(`memory_mib: 256`, no root). Raw dump: `tools/dtb/pixel7_avf_bootloader.dtb`.
Decoded with `tools/dtb/parse_dtb.py`.

Root compatible: `linux,dummy-virt`. `#address-cells = 2`, `#size-cells = 2`.

## Memory

```
reg = <0x0 0x80000000 0x0 0x10000000>
```

RAM base: `0x80000000`. Size: `0x10000000` (256 MiB, matches the configured
`memory_mib: 256` — memory size is guest-configurable via VM config/`-m`).

## CPU

Single `cpu@0` node, `compatible = "arm,armv8"`, `reg = <0x0>`.
(`cpu-topology` unspecified in our config -> `vm run` default is `one_cpu`.)

## Interrupt controller — GICv3

```
intc: compatible = "arm,gic-v3"
  reg = <dist: 0x3fff0000 size 0x10000> <redist: 0x3ffd0000 size 0x20000>
  msic (ITS): compatible = "arm,gic-v3-its", reg = <0x40000000 size 0x20000>
```

## Timer

```
compatible = "arm,armv8-timer"
interrupts = <1 0xd 0x108> <1 0xe 0x108> <1 0xb 0x108> <1 0xa 0x108>
always-on
```
Standard ARM generic timer PPIs (secure/non-secure/hyp/virt), level-triggered (0x108 = IRQ_TYPE_LEVEL_LOW | PPI flags per Linux DT convention).

## PSCI

```
compatible = "arm,psci-1.0", "arm,psci-0.2"
method = "hvc"
```
CPU power management / start via `HVC` calls, standard PSCI 1.0/0.2.

## Console — ns16550a (NOT PL011)

Four UART nodes, all `compatible = "ns16550a"`, byte-addressable legacy-port-style addresses:

```
U6_16550A@3f8  reg = <0x0 0x3f8 0x0 0x8>  clock-frequency=0x1c2000  irq: <0 0x0 1>
U6_16550A@2f8  reg = <0x0 0x2f8 0x0 0x8>  clock-frequency=0x1c2000  irq: <0 0x2 1>
U6_16550A@3e8  reg = <0x0 0x3e8 0x0 0x8>  clock-frequency=0x1c2000  irq: <0 0x0 1>
U6_16550A@2e8  reg = <0x0 0x2e8 0x0 0x8>  clock-frequency=0x1c2000  irq: <0 0x2 1>
```

`/chosen/stdout-path = "/U6_16550A@3f8"` — the first one is the designated
console. This matches the address literally (root has no intermediate bus
node with a `ranges` translation for these — `reg` is the real guest
physical address, just numerically reusing familiar x86-legacy-port values
as MMIO offsets).

Register layout is standard 16550A, not PL011:
- offset 0x0: THR (write) / RBR (read)
- offset 0x5: LSR, bit 5 (0x20) = THR empty
No initialization (baud/LCR) appears to be required to just poll-and-write
for output — confirmed working from bare metal with no other setup.

## PCI host

```
compatible = "pci-host-cam-generic"
reg = <0x2e000000 size 0x1000000>          (ECAM config space)
ranges: io  0x2c000000 size 0x2000000
        mem 0xd0000000 size 0x2f0000000   (64-bit range 0xff30000000 high)
msi-parent = <&msic>  (routed through the GICv3 ITS)
```

## RTC

```
rtc@2000: compatible = "arm,primecell" (PL030), reg = <0x0 0x2000 0x0 0x1000>
```

## Watchdog

```
vmwdt@3000: compatible = "qemu,vcpu-stall-detector", reg = <0x0 0x3000 0x0 0x1000>
```
crosvm/AVF-specific vCPU stall detector, not a generic watchdog.

## Display/GPU: requires an app-hosted VM, not the bare `vm run` CLI

Confirmed by inspecting the real `crosvm` command line of a working graphical
VM (Debian, launched via the stock `com.android.virtualization.terminal`
app after enabling it in Developer Options):

```
--android-display-service=cid:2071
--gpu=backend=2d,egl=true,gles=true,surfaceless=true,displays=[[mode=windowed[1280,720],dpi=[160,160],refresh-rate=60]]
```

`--android-display-service=cid:N` is a crosvm flag that binds to an Android
Binder service the *hosting app* stands up (backed by its own `SurfaceView`)
-- only `virtmgr` spawned by a real app process can wire this up. This is
why adding `"gpu"`/`"display"` keys to a `vm run` JSON config (schema
accepts them, confirmed empirically) does nothing observable: the shell-run
`vm` CLI has no Surface to hand over, so crosvm never gets
`--android-display-service` at all, regardless of what's in the config.

**Implication:** a graphical guest (including Windows with visible output)
needs an actual Android app process hosting a `SurfaceView`/`SurfaceHolder`
and using the public `android.system.virtualmachine` API
(`VirtualMachineCustomImageConfig.Builder`, `DisplayConfig`) to create the
VM -- not the bare `vm run` CLI we've used for headless testing so far.
`MANAGE_VIRTUAL_MACHINE` and `USE_CUSTOM_VIRTUAL_MACHINE` permissions are
grantable via plain `adb shell pm grant` (confirmed, no root needed), so
this is achievable without root -- it just needs a real (if minimal) app,
not a CLI invocation.

## Other

- `cpufreq`: `compatible = "virtual,kvm-cpufreq"` — paravirt cpufreq, no reg (MSR/hypercall based presumably).
- `avf` node: `secretkeeper_public_key`, and an `untrusted` child with
  `instance-id` — AVF-specific attestation/secretkeeper plumbing, not
  relevant to early boot.
- **Disk transport is virtio-PCI, not virtio-mmio.** Confirmed by adding a
  `"disks": [{"image": "<path>", "writable": true}]` entry to the VM config
  (plain path string, not a nested object -- a nested `{"kind":"raw","path":...}`
  form is rejected by the JSON schema) and diffing the resulting
  `--dump-device-tree` output against the no-disk baseline: no new DT node
  appears, only one extra `interrupt-map` entry on the existing
  `pci-host-cam-generic` node (slot `0x3800`, IRQ pin `0xa`), and crosvm's
  own stdout logs `Trying to attach block device: /proc/self/fd/N`. This
  means the disk enumerates as a PCI device at runtime (via ECAM config
  space, base `0x2e000000` per the `pci` node above), not something
  discoverable from the DTB alone -- a guest needs a PCI enumerator, not a
  DT walk, to find it. This is good news for the Windows ARM64 path:
  virtio-pci is the well-supported, mainline transport for Windows'
  virtio-win drivers (vs. the much less common virtio-mmio).
- `/chosen/bootargs = "panic=-1 hostname=VmRun coherent_pool=16384"`,
  `kernel-address`/`kernel-size` under `/config` (0x80200000 / 0x9d8 in this
  dump) — these describe where crosvm loaded the *kernel-mode* payload in
  its own default config, not directly relevant to `bootloader`-mode boot,
  but useful reference for load-address conventions crosvm uses elsewhere.
