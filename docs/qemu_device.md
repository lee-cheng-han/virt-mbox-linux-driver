# QEMU Device Model

## Device name

The emulated hardware device is named:

    qemu-mbox

The Linux-facing driver/device name will later be:

    qemu_mbox

The eventual character device node will be:

    /dev/qemu_mbox0

## Milestone 2 scope

Milestone 2 implements the minimal QEMU MMIO device model.

Implemented:

- QOM/qdev device type
- SysBusDevice parent
- MMIO region
- 32-bit register read/write callbacks
- ID register
- VERSION register
- CONTROL register
- STATUS register
- TX_DATA register
- RX_DATA register
- IRQ_STATUS storage
- IRQ_ENABLE storage
- RESET register

Not implemented yet:

- real TX FIFO
- real RX FIFO
- processing timer
- interrupt line
- QOM debug properties
- qtest coverage
- Linux driver

## Temporary TX/RX behavior

Milestone 2 does not implement a real FIFO yet.

For smoke testing, writes to TX_DATA store one byte, immediately process it, and expose the result through RX_DATA.

The temporary processing rule is:

    'a' - 'z' -> 'A' - 'Z'
    other bytes unchanged

Example:

    write TX_DATA = 'h'
    read RX_DATA  = 'H'

This behavior will later move behind a real TX FIFO, RX FIFO, processing timer, and IRQ path.

## Integration model

The files in this repository are project-owned source files.

They are intended to become a patch against a real QEMU source tree:

    qemu/hw/misc/qemu_mbox.c
    qemu/include/hw/misc/qemu_mbox.h
    qemu/patches/0001-hw-misc-add-qemu-mbox-device.patch

Later milestones will add the Meson/Kconfig integration needed to compile the device inside QEMU.
