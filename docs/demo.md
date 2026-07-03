# Demo Plan

The final demo should show the complete hardware/software stack working from
userspace down to the QEMU device model.

## Demo Goals

The demo should prove:

- QEMU exposes the custom MMIO device.
- Linux probes the platform driver.
- `/dev/vmbox0` is created.
- userspace can write data and read processed data.
- sysfs exposes stable status and FIFO depth attributes.
- blocking and non-blocking I/O behave correctly.
- `poll()` wakes on device readiness.
- ioctl reset, status, and stats work.
- debugfs exposes useful diagnostic state.

## Expected Boot Log

Target driver log shape:

```text
vmbox 10000000.mbox: device id 0x514d424f
vmbox 10000000.mbox: hardware version 1.0
vmbox 10000000.mbox: fifo depth 16
vmbox 10000000.mbox: irq registered
vmbox 10000000.mbox: registered /dev/vmbox0
```

Exact bus addresses may change depending on the QEMU machine and device tree.

## Expected Test Output

Target userspace test output:

```text
[PASS] basic read/write
[PASS] ioctl reset
[PASS] ioctl status
[PASS] ioctl stats
[PASS] poll wakeup
[PASS] non-blocking read
[PASS] non-blocking write
[PASS] FIFO full handling
[PASS] FIFO empty handling
[PASS] invalid ioctl handling
[PASS] stress 1000 messages
All tests passed.
```

## Artifacts To Keep

For a polished project page, keep:

- QEMU command line
- kernel config notes
- device tree snippet
- driver boot logs
- userspace test logs
- debugfs sample output
- CI run link or screenshot
- short architecture diagram

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
