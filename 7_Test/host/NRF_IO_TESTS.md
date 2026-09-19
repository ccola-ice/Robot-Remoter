# NRF transport regression

Run `py -3.9 7_Test/host/run_nrf_io_tests.py [gcc-path]` from the repair tree.
The runner compiles the production NRF driver and platform configuration code,
omitting only the hardware GPIO/clock/SPI initialization function. It exercises
real transaction and TX state code using a host register and SPI model.

Coverage: disabled settings readback with 16-bit CRC, TX standby/pulse, ACK,
MAX_RT, no completion flags, 30 ms timeout and cycle-counter wrap, stale payload
flush/cancellation, synchronous compatibility wrapper, TXE/RXNE stalls, missing
module, sticky error boundaries, NULL arguments and CE behavior after faults.
Configuration generation tests cover settings, each mode, failed apply and wrap;
ordinary TX completion and cancellation must not advance the generation.
Run `run_diagnostic_tests.py` for diagnostic configuration restoration, including
failed restoration leaving CE low.

`NRF_TxStart` returns 1 only when a frame was started. `NRF_TxPoll` returns
NRF_TX_PENDING, TX_DS, MAX_RT or ERROR; it never waits on IRQ. The 30 ms deadline
uses DWT, so the caller must poll regularly. `NRF_TxCancel` clears software state
and pending TX data. Applying settings powers down before reconfiguration and
allows 1600 us settling before entering active TX/RX mode.

SPI errors stay latched across settings changes. Only explicit `NRF_SPI_Init`
clears them and reinitializes the transport; callers should recheck the module
before enabling control traffic. Host tests do not validate RF range, over-air
timing, peer failsafe behavior or electrical SPI/IRQ faults.
