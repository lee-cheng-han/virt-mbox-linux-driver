<!-- SPDX-License-Identifier: MIT -->

# End-To-End Bring-Up

This repository contains the project-owned QEMU device, Linux driver source,
userspace tests, and integration fragments. The real end-to-end run happens
after applying those fragments to full QEMU and Linux source trees.

## Expected Flow

```text
QEMU boots Linux
QEMU exposes virt-mbox as an MMIO platform device
Linux probes the vmbox driver
/dev/vmbox0 appears
userspace selftests pass
debugfs/sysfs state is readable
```

## Helper Scripts

From this repository:

```sh
scripts/apply-qemu.sh ~/qemu
scripts/apply-linux.sh ~/work/linux
scripts/build-userspace.sh /tmp/vmbox_test
scripts/run-e2e-checklist.sh
scripts/run-arm64-demo.sh ~/qemu ~/work/linux
```

The apply scripts are intentionally conservative: they copy source payloads and
append integration fragments only when the target checkout does not already
contain the expected marker.

## Working ARM64 Demo

The working end-to-end demo command is:

```sh
scripts/run-arm64-demo.sh ~/qemu ~/work/linux
```

It performs the following steps:

1. Applies the QEMU payload and temporary ARM `virt` demo wiring.
2. Applies the Linux payload to the external Linux checkout.
3. Builds the ARM64 kernel image and `drivers/misc/vmbox.ko`.
4. Builds a static ARM64 init program from `tests/demo/vmbox_init.c`.
5. Creates a tiny initramfs containing `/init` and `/vmbox.ko`.
6. Boots `qemu-system-aarch64 -machine virt`.
7. Loads the module and runs the guest test through `/dev/vmbox0`.

The captured passing output is in `docs/demo-output.txt`.

## QEMU Command Shape

```sh
qemu-system-aarch64 \
  -machine virt \
  -cpu cortex-a57 \
  -m 1024 \
  -kernel ~/work/linux/arch/arm64/boot/Image \
  -initrd /tmp/vmbox-arm64-demo/initramfs.cpio \
  -append "console=ttyAMA0 rdinit=/init panic=-1" \
  -no-reboot \
  -nographic
```

The demo script wraps this command and regenerates the initramfs before boot.

## Guest Validation

```sh
finit_module("/vmbox.ko")
test -e /dev/vmbox0
open("/dev/vmbox0")
write("hello")
poll(POLLIN)
read("HELLO")
ioctl(VMBOX_IOC_GET_STATUS)
ioctl(VMBOX_IOC_GET_STATS)
```

## Sanitizer Boot Notes

For runtime sanitizer coverage, boot a kernel with:

- `CONFIG_KASAN=y`
- `CONFIG_DEBUG_KMEMLEAK=y`
- `CONFIG_LOCKDEP=y`
- `CONFIG_PROVE_LOCKING=y`

After tests, scan:

```sh
dmesg | grep -Ei 'BUG|WARNING|KASAN|kmemleak|Oops|use-after-free'
cat /sys/kernel/debug/kmemleak
```
