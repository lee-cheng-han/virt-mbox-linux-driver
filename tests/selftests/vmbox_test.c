// SPDX-License-Identifier: MIT
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "../../kernel/include/uapi/linux/vmbox.h"

#define VMBOX_DEV "/dev/vmbox0"
#define VMBOX_STRESS_MESSAGES 1000

static int failures;

static void check(int condition, const char *name)
{
	if (condition) {
		printf("[PASS] %s\n", name);
		return;
	}

	printf("[FAIL] %s: %s\n", name, strerror(errno));
	failures++;
}

static int open_vmbox(int flags)
{
	int fd = open(VMBOX_DEV, flags);

	if (fd < 0)
		perror("open " VMBOX_DEV);

	return fd;
}

static void test_open_close(void)
{
	int fd = open_vmbox(O_RDWR);

	check(fd >= 0, "open/close");
	if (fd >= 0)
		close(fd);
}

static void test_basic_read_write(void)
{
	char out = 'h';
	char in = 0;
	int fd;
	ssize_t ret;

	fd = open_vmbox(O_RDWR);
	if (fd < 0) {
		check(0, "basic read/write");
		return;
	}

	ret = write(fd, &out, 1);
	if (ret != 1) {
		check(0, "basic read/write write");
		close(fd);
		return;
	}

	ret = read(fd, &in, 1);
	check(ret == 1 && in == 'H', "basic read/write");
	close(fd);
}

static void test_nonblocking_read(void)
{
	char in;
	int fd;
	ssize_t ret;

	fd = open_vmbox(O_RDWR | O_NONBLOCK);
	if (fd < 0) {
		check(0, "non-blocking read");
		return;
	}

	(void)ioctl(fd, VMBOX_IOC_RESET);
	ret = read(fd, &in, 1);
	check(ret < 0 && errno == EAGAIN, "non-blocking read");
	close(fd);
}

static void test_poll_wakeup(void)
{
	struct pollfd pfd;
	char out = 'p';
	int fd;
	int ret;

	fd = open_vmbox(O_RDWR | O_NONBLOCK);
	if (fd < 0) {
		check(0, "poll wakeup");
		return;
	}

	(void)ioctl(fd, VMBOX_IOC_RESET);
	check(write(fd, &out, 1) == 1, "poll write seed");

	pfd.fd = fd;
	pfd.events = POLLIN | POLLOUT;
	pfd.revents = 0;
	ret = poll(&pfd, 1, 1000);
	check(ret > 0 && (pfd.revents & POLLIN), "poll wakeup");
	close(fd);
}

static void test_ioctl_status_stats_reset(void)
{
	struct vmbox_status status;
	struct vmbox_stats stats;
	char out = 's';
	int fd;

	fd = open_vmbox(O_RDWR);
	if (fd < 0) {
		check(0, "ioctl status/stats/reset");
		return;
	}

	check(ioctl(fd, VMBOX_IOC_RESET) == 0, "ioctl reset");
	check(ioctl(fd, VMBOX_IOC_GET_STATUS, &status) == 0 &&
	      status.fifo_depth == 16, "ioctl status");
	check(write(fd, &out, 1) == 1, "ioctl stats seed");
	check(ioctl(fd, VMBOX_IOC_GET_STATS, &stats) == 0 &&
	      stats.bytes_written >= 1, "ioctl stats");
	close(fd);
}

static void test_invalid_ioctl(void)
{
	int fd;

	fd = open_vmbox(O_RDWR);
	if (fd < 0) {
		check(0, "invalid ioctl");
		return;
	}

	check(ioctl(fd, _IO('x', 0x55)) < 0 && errno == ENOTTY,
	      "invalid ioctl");
	close(fd);
}

static void test_stress(void)
{
	char out = 'a';
	char in;
	int fd;
	int i;
	int ok = 1;

	fd = open_vmbox(O_RDWR);
	if (fd < 0) {
		check(0, "stress 1000 messages");
		return;
	}

	(void)ioctl(fd, VMBOX_IOC_RESET);
	for (i = 0; i < VMBOX_STRESS_MESSAGES; i++) {
		if (write(fd, &out, 1) != 1 || read(fd, &in, 1) != 1 ||
		    in != 'A') {
			ok = 0;
			break;
		}
	}

	check(ok, "stress 1000 messages");
	close(fd);
}

int main(void)
{
	test_open_close();
	test_basic_read_write();
	test_nonblocking_read();
	test_poll_wakeup();
	test_ioctl_status_stats_reset();
	test_invalid_ioctl();
	test_stress();

	if (failures) {
		printf("%d test(s) failed.\n", failures);
		return EXIT_FAILURE;
	}

	printf("All tests passed.\n");
	return EXIT_SUCCESS;
}
