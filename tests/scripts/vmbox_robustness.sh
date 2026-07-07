#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu

DEV="${VMBOX_DEV:-/dev/vmbox0}"
MODULE="${VMBOX_MODULE:-vmbox}"
SYSFS_DRIVER="${VMBOX_SYSFS_DRIVER:-}"
DMESG_SINCE="${VMBOX_DMESG_SINCE:-}"

need_root() {
    if [ "$(id -u)" -ne 0 ]; then
        echo "SKIP: robustness tests need root"
        exit 77
    fi
}

scan_dmesg() {
    if [ -n "$DMESG_SINCE" ]; then
        dmesg --since "$DMESG_SINCE" | grep -Ei 'BUG|WARNING|KASAN|kmemleak|Oops|use-after-free' && return 1
    else
        dmesg | grep -Ei 'BUG|WARNING|KASAN|kmemleak|Oops|use-after-free' && return 1
    fi

    return 0
}

reload_module() {
    modprobe -r "$MODULE" || true
    modprobe "$MODULE"
    test -e "$DEV"
}

unbind_rebind_once() {
    if [ -z "$SYSFS_DRIVER" ]; then
        echo "SKIP: set VMBOX_SYSFS_DRIVER for unbind/rebind tests"
        return 0
    fi

    dev="$(basename "$(readlink -f "$SYSFS_DRIVER"/* 2>/dev/null | head -n 1)")"
    if [ -z "$dev" ]; then
        echo "SKIP: no bound vmbox platform device found"
        return 0
    fi

    exec 9<"$DEV"
    printf '%s' "$dev" > "$SYSFS_DRIVER/unbind"
    printf '%s' "$dev" > "$SYSFS_DRIVER/bind"
    exec 9<&-
}

need_root
reload_module
unbind_rebind_once
scan_dmesg
echo "vmbox robustness smoke passed"
