<!-- SPDX-License-Identifier: MIT -->

# Ioctl Fuzz Test Plan

The `vmbox` userspace test suite should include a bounded manual ioctl fuzzer.

Planned coverage:

- random command numbers
- random argument values
- NULL pointers
- invalid pointers
- misaligned pointers
- valid pointers with invalid data
- nonzero reserved fields
- invalid mode bits

The fuzzer should run for a bounded number of iterations and verify the driver
returns clean errors without warnings, crashes, leaks, or hangs.

Full syzkaller integration is future work.
