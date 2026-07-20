# Demo

The working demo shows the complete hardware/software stack running from a tiny
guest userspace init program down to the QEMU MMIO device model. It applies the
repository payloads to external QEMU and Linux checkouts, builds an ARM64 kernel
and `vmbox.ko`, creates a minimal initramfs, boots `qemu-system-aarch64`, and
runs the mailbox smoke test inside the guest.

## Architecture Diagram

```text
guest /init demo test
        |
        | open/read/write/poll/ioctl
        v
/dev/vmbox0
        |
        | Linux vmbox platform driver
        | sysfs + debugfs observability
        v
MMIO registers + IRQ
        |
        | QEMU virt-mbox SysBusDevice
        v
TX FIFO -> timer processing -> RX FIFO
```

## Run Command

From this repository:

```sh
scripts/run-arm64-demo.sh ~/qemu ~/work/linux
```

Expected external checkouts:

- QEMU source tree: `~/qemu`
- Linux source tree: `~/work/linux`

Required host tool:

- `aarch64-linux-gnu-gcc`

On Ubuntu/WSL:

```sh
sudo apt-get install -y gcc-aarch64-linux-gnu
```

The first run can take a while because it builds the ARM64 kernel. Later runs
are incremental and much faster.

## Demo Coverage

The demo proves:

- QEMU exposes the custom MMIO device.
- Linux probes the platform driver from a devicetree node.
- `/dev/vmbox0` is created.
- userspace can write data and read processed data.
- `poll()` wakes on device readiness.
- ioctl status and stats work.
- QEMU, Linux, the module, and userspace agree on the register contract.

## Captured Output

A successful captured run is stored in `docs/demo-output.txt`.

```text
vmbox demo init starting
vmbox 9100000.mbox: probed fifo_depth=16 irq=22
[PASS] loaded vmbox.ko
[PASS] /dev/vmbox0 appeared
[PASS] open /dev/vmbox0
[PASS] write message
[PASS] poll readable
[PASS] read transformed data: HELLO
[PASS] ioctl status fifo_depth=16 tx=0 rx=0
[PASS] ioctl stats bytes_written=5 bytes_read=5 irqs=5
vmbox demo passed
reboot: Power down
```

Exact IRQ numbering can vary depending on the QEMU machine and guest kernel.

## Demo Implementation

The demo-specific pieces are:

- `scripts/run-arm64-demo.sh`
- `tests/demo/vmbox_init.c`
- `qemu/patches/virt-demo.patch`

The QEMU demo wiring instantiates `virt-mbox` on the ARM `virt` machine at
`0x09100000` and emits a matching `virt,mbox` devicetree node.
`scripts/run-arm64-demo.sh` applies that temporary wiring idempotently; the
patch file is kept as a reviewable reference. The initramfs contains a static
ARM64 `/init` program and `vmbox.ko`.

## Debugfs Sample

```text
$ cat /sys/kernel/debug/vmbox0/stats
bytes_read: 1000
bytes_written: 1000
irqs: 1000
errors: 0
errors_irq_spurious: 0
errors_fifo_overrun: 0
errors_fifo_underrun: 0
tx_full_events: 0
rx_empty_events: 0
```

```text
$ cat /sys/kernel/debug/vmbox0/regs
id: 0x514d424f
version: 0x00010000
control: 0x00000009
status: 0x00000002
irq_status: 0x00000000
irq_enable: 0x0000000f
tx_count: 0
rx_count: 1
fifo_depth: 16
```

## Artifacts To Keep

For a polished project page, keep:

- QEMU command line
- kernel config notes
- device tree snippet
- driver boot logs
- userspace test logs
- captured `docs/demo-output.txt`
- debugfs sample output
- CI run link or screenshot
- short architecture diagram

## Known Limitations

- The QEMU and Linux source payloads are still project-owned and are applied to
  external source trees by helper scripts.
- The ARM64 demo uses `KBUILD_MODPOST_WARN=1` for the single-module build to
  avoid requiring a full clean module-symbol pass before every demo run.
- The current driver supports one device instance and one opener.
- Runtime PM, suspend/resume, DMA, and multi-instance support are future work.

## Future Work

- runtime PM
- suspend/resume support
- IOThread-aware QEMU locking if the execution context changes
- DMA support
- configurable or larger FIFO depth
- multi-instance support
- full syzkaller integration
- stronger timeout/error recovery if IRQs are lost

## Stretch Demo

After the core mailbox project is complete, the same driver architecture can be
extended into a larger simulated accelerator demo. That should remain a stretch
goal until the mailbox stack is stable and tested.
