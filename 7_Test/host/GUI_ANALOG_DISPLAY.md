# Analog display regression checks

Run from the repository root (MinGW GCC required):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File 7_Test/host/run_gui_analog_tests.ps1
```

The runner compiles the production display filter and extracts the complete
channel/robot rendering functions from `1_App/gui.c`, with mocked LCD writes.
It checks noise rejection, step response, ADC endpoints, channel mapping,
numeric refresh cadence, page re-entry, immediate link transitions, and that
stationary joystick dots are not erased when ADC values change.

Display settings are in `1_App/gui_analog_filter.h`: small changes use a 1/4
low-pass coefficient; changes exceeding 128 ADC counts use 1/2 for faster
tracking. An 8-count hysteresis suppresses minor fluctuations. Numeric values
refresh every four menu frames (about 200 ms at the current 50 ms interval).
The filter affects display values only. Robot telemetry numbers use the same
refresh limit; link changes remain immediate. Dot motion is independent of
numeric refresh and uses a two-pixel hysteresis in `gui_robot_draw_stick`.

On hardware, verify both pages with the sticks released, slow movements,
full-travel movements, and repeated page changes. Confirm stationary dots
remain visible and moving dots leave no trails. Host checks cannot verify
LCD scanout flicker or the actual board's ADC noise amplitude.
