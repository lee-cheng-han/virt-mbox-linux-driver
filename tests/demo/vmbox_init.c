// SPDX-License-Identifier: MIT
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <linux/reboot.h>
#include <linux/vmbox.h>

static void say(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);
}

static int load_module(const char *path)
{
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    int ret;

    if (fd < 0) {
        return -1;
    }

    ret = syscall(SYS_finit_module, fd, "", 0);
    close(fd);
    return ret;
}

static int wait_for_path(const char *path, int timeout_ms)
{
    struct timespec req = {
        .tv_sec = 0,
        .tv_nsec = 100 * 1000 * 1000,
    };
    int waited = 0;

    while (waited <= timeout_ms) {
        if (access(path, F_OK) == 0) {
            return 0;
        }
        nanosleep(&req, NULL);
        waited += 100;
    }

    return -1;
}

static int run_vmbox_test(void)
{
    struct pollfd pfd;
    struct vmbox_status status;
    struct vmbox_stats stats;
    const char tx[] = "hello";
    char rx[sizeof(tx)] = { 0 };
    size_t got = 0;
    int fd;
    ssize_t ret;

    fd = open("/dev/vmbox0", O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        say("[FAIL] open /dev/vmbox0: %s\n", strerror(errno));
        return 1;
    }
    say("[PASS] open /dev/vmbox0\n");

    ret = write(fd, tx, strlen(tx));
    if (ret != (ssize_t)strlen(tx)) {
        say("[FAIL] write: ret=%zd errno=%s\n", ret, strerror(errno));
        close(fd);
        return 1;
    }
    say("[PASS] write message\n");

    pfd.fd = fd;
    pfd.events = POLLIN;
    ret = poll(&pfd, 1, 5000);
    if (ret <= 0) {
        say("[FAIL] poll: ret=%zd errno=%s\n", ret, strerror(errno));
        close(fd);
        return 1;
    }
    say("[PASS] poll readable\n");

    while (got < strlen(tx)) {
        ret = read(fd, rx + got, strlen(tx) - got);
        if (ret <= 0) {
            say("[FAIL] read: ret=%zd data='%.*s'\n",
                ret, (int)sizeof(rx), rx);
            close(fd);
            return 1;
        }
        got += ret;
    }

    if (memcmp(rx, "HELLO", strlen(tx)) != 0) {
        say("[FAIL] read data='%.*s'\n", (int)sizeof(rx), rx);
        close(fd);
        return 1;
    }
    say("[PASS] read transformed data: %s\n", rx);

    if (ioctl(fd, VMBOX_IOC_GET_STATUS, &status) != 0) {
        say("[FAIL] ioctl status: %s\n", strerror(errno));
        close(fd);
        return 1;
    }
    say("[PASS] ioctl status fifo_depth=%u tx=%u rx=%u\n",
        status.fifo_depth, status.tx_count, status.rx_count);

    if (ioctl(fd, VMBOX_IOC_GET_STATS, &stats) != 0) {
        say("[FAIL] ioctl stats: %s\n", strerror(errno));
        close(fd);
        return 1;
    }
    say("[PASS] ioctl stats bytes_written=%llu bytes_read=%llu irqs=%llu\n",
        (unsigned long long)stats.bytes_written,
        (unsigned long long)stats.bytes_read,
        (unsigned long long)stats.irqs);

    close(fd);
    return 0;
}

int main(void)
{
    int rc;

    mount("devtmpfs", "/dev", "devtmpfs", 0, "");
    mount("proc", "/proc", "proc", 0, "");
    mount("sysfs", "/sys", "sysfs", 0, "");
    mount("debugfs", "/sys/kernel/debug", "debugfs", 0, "");

    say("vmbox demo init starting\n");

    if (load_module("/vmbox.ko") != 0) {
        say("[FAIL] load /vmbox.ko: %s\n", strerror(errno));
        return 1;
    }
    say("[PASS] loaded vmbox.ko\n");

    if (wait_for_path("/dev/vmbox0", 5000) != 0) {
        say("[FAIL] /dev/vmbox0 did not appear\n");
        return 1;
    }
    say("[PASS] /dev/vmbox0 appeared\n");

    rc = run_vmbox_test();
    say(rc ? "vmbox demo failed\n" : "vmbox demo passed\n");
    sync();
    reboot(LINUX_REBOOT_CMD_POWER_OFF);
    return rc;
}
