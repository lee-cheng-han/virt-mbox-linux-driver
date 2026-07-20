#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu

usage() {
    echo "usage: $0 /path/to/qemu /path/to/linux" >&2
}

if [ "$#" -ne 2 ]; then
    usage
    exit 2
fi

qemu_tree="$1"
linux_tree="$2"
repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
out_dir="${VMBOX_DEMO_OUT:-/tmp/vmbox-arm64-demo}"
initramfs="$out_dir/initramfs.cpio"
init_bin="$out_dir/init"

need() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "missing required tool: $1" >&2
        exit 1
    fi
}

need_file() {
    if [ ! -f "$1" ]; then
        echo "missing required file: $1" >&2
        exit 1
    fi
}

apply_qemu_virt_demo() {
    virt_c="$qemu_tree/hw/arm/virt.c"

    need_file "$virt_c"

    python3 - "$virt_c" <<'PY'
from pathlib import Path
import sys

virt_c = Path(sys.argv[1])
text = virt_c.read_text()
changed = False

include = '#include "hw/misc/qemu_mbox.h"\n'
include_marker = '#include "hw/arm/fdt.h"\n'
if include not in text:
    if include_marker not in text:
        raise SystemExit("virt.c marker not found: hw/arm/fdt.h include")
    text = text.replace(include_marker, include_marker + include, 1)
    changed = True

helper = '''static void create_qemu_mbox(const VirtMachineState *vms)
{
    MachineState *ms = MACHINE(vms);
    const hwaddr base = 0x09100000;
    const hwaddr size = QEMU_MBOX_MMIO_SIZE;
    const int irq = 11;
    char *nodename;

    sysbus_create_simple(TYPE_QEMU_MBOX, base, qdev_get_gpio_in(vms->gic, irq));

    nodename = g_strdup_printf("/mbox@%" PRIx64, base);
    qemu_fdt_add_subnode(ms->fdt, nodename);
    qemu_fdt_setprop_string(ms->fdt, nodename, "compatible", "virt,mbox");
    qemu_fdt_setprop_sized_cells(ms->fdt, nodename, "reg", 2, base, 2, size);
    qemu_fdt_setprop_cells(ms->fdt, nodename, "interrupts",
                           gic_fdt_irq_type_spi(vms), irq,
                           GIC_FDT_IRQ_FLAGS_LEVEL_HI);
    g_free(nodename);
}

'''

if "static void create_qemu_mbox(" not in text:
    helper_marker = "static void create_rtc(const VirtMachineState *vms)\n"
    if helper_marker not in text:
        raise SystemExit("virt.c marker not found: create_rtc")
    text = text.replace(helper_marker, helper + helper_marker, 1)
    changed = True

call_marker = "    create_rtc(vms);\n\n    create_pcie(vms);"
call_replacement = "    create_rtc(vms);\n    create_qemu_mbox(vms);\n\n    create_pcie(vms);"
if "    create_qemu_mbox(vms);\n" not in text:
    if call_marker not in text:
        raise SystemExit("virt.c marker not found: create_rtc/create_pcie")
    text = text.replace(call_marker, call_replacement, 1)
    changed = True

if changed:
    virt_c.write_text(text)
    print(f"updated: {virt_c} with virt-mbox demo wiring")
else:
    print(f"already present: virt-mbox demo wiring in {virt_c}")
PY
}

need aarch64-linux-gnu-gcc
need make
need python3

mkdir -p "$out_dir"

"$repo_root/scripts/apply-qemu.sh" "$qemu_tree"
apply_qemu_virt_demo
"$repo_root/scripts/apply-linux.sh" "$linux_tree"

cd "$linux_tree"
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- defconfig
scripts/config --enable OF
scripts/config --enable DEVTMPFS
scripts/config --enable DEVTMPFS_MOUNT
scripts/config --enable BLK_DEV_INITRD
scripts/config --enable DEBUG_FS
scripts/config --module VMBOX
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- olddefconfig
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j"$(nproc)" Image modules_prepare
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- \
    M=drivers/misc vmbox.ko KBUILD_MODPOST_WARN=1

aarch64-linux-gnu-gcc -static -O2 \
    -I"$repo_root/kernel/include/uapi" \
    -o "$init_bin" \
    "$repo_root/tests/demo/vmbox_init.c"

python3 - "$initramfs" "$init_bin" "$linux_tree/drivers/misc/vmbox.ko" <<'PY'
import os
import stat
import sys

out, init_bin, module = sys.argv[1:4]

def pad4(length):
    return b"\0" * ((4 - length % 4) % 4)

ino = 1

def entry(name, data=b"", mode=0o100644):
    global ino
    namesz = len(name) + 1
    fields = [
        ino, mode, 0, 0, 1, 0, len(data),
        0, 0, 0, 0, namesz, 0,
    ]
    ino += 1
    header = b"070701" + "".join(f"{x:08x}" for x in fields).encode()
    filename = name.encode() + b"\0"
    return (header + filename + pad4(len(header) + len(filename)) +
            data + pad4(len(data)))

def directory(name):
    return entry(name, b"", 0o040755)

with open(init_bin, "rb") as f:
    init_data = f.read()
with open(module, "rb") as f:
    module_data = f.read()

blob = b"".join([
    directory("dev"),
    directory("proc"),
    directory("sys"),
    directory("sys/kernel"),
    directory("sys/kernel/debug"),
    entry("init", init_data, 0o100755),
    entry("vmbox.ko", module_data, 0o100644),
    entry("TRAILER!!!"),
])

with open(out, "wb") as f:
    f.write(blob)
PY

ninja -C "$qemu_tree/build"

exec "$qemu_tree/build/qemu-system-aarch64" \
    -machine virt \
    -cpu cortex-a57 \
    -m 1024 \
    -kernel "$linux_tree/arch/arm64/boot/Image" \
    -initrd "$initramfs" \
    -append "console=ttyAMA0 rdinit=/init panic=-1" \
    -no-reboot \
    -nographic
