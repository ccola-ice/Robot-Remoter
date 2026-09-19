# Storage I/O fault regression

Run `py -3.9 7_Test/host/run_storage_io_tests.py [gcc-path]` from the repair tree.
The test compiles the production FatFs glue and SDIO timeout, completion and
interrupt handlers with host peripheral stubs. It does not maintain duplicate
implementations of these functions.

Coverage includes successful initialization/read/write, failed initialization,
command and CRC failures, absent DMA completion, SDIO-only completion,
persistent peripheral activity/card busy, removal/error state, timeout counter
wrap, simultaneous completion/error IRQs, invalid requests, unaligned buffers,
SD byte addresses above 4 GiB, Flash reads larger than 65535 bytes, and immediate
failure propagation from Flash read/erase/program into FatFs and CTRL_SYNC.

SD waits use DWT and a finite fallback poll budget, including before TIM6 starts.
An SD I/O failure requires reinitialization. Flash errors remain latched until
the existing read-only boot probe resets and validates the device. Hardware
testing is still required for actual card removal, DMA/IRQ timing and flash
power interruption; host tests do not model those electrical conditions.
